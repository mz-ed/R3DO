#include "v3.hpp"
#include "ray.hpp"
#include "hittable.hpp"
#include "grid.hpp"
#include "camera.hpp"
#include "display.hpp"
#include "ui.hpp"
#include "saver.hpp"
#include "render.hpp"
#include "ui/startscreen.hpp"
#include "settings.hpp"
#include "billboard.hpp"
#include "overhead.hpp"
#include "sculpt_engine.hpp"
#include "sculpt_mesh.hpp"
#include "animation.hpp"
#include "ui/nkui.hpp"
#include <iostream>
#include <cmath>
#include <chrono>

namespace {

constexpr double EYE_HEIGHT = 1.5;
constexpr double GRAVITY = -1.5;      // units/s^2
constexpr double JUMP_SPEED = 3.0;    // units/s
constexpr double MOVE_SPEED = 3.0;    // units/s
constexpr double ROT_SPEED = 1.2;     // rad/s
constexpr double MOUSE_SENS = 0.005;

// Would a camera at p be inside a solid cell?
bool blocked_at(const Grid& grid, const Vec3& p) {
    int i, j, k;
    return grid.world_to_cell(p, i, j, k) && grid.get(i, j, k) != nullptr;
}

void run_editor(Grid& grid, Camera& cam, DisplayWin& display,
                const Settings& settings, int initial_mode = 1) {
    SculptEngine sculpt;
    UI ui(grid, cam, display, &sculpt);
    NkUI nkui;
    Animator animator;
    if (!nkui.init(display))
        std::cerr << "Nuklear UI init failed (font missing?)" << std::endl;
    bool mouse_captured = false;

    ui.set_render_mode(initial_mode);
    // Re-attach an existing sculpt mesh (e.g. from the start screen) so the
    // brush works immediately; if none exists, spawn one in front of the eye.
    for (auto& f : grid.free_objects()) {
        if (std::string(f->type_name()) == "sculpt") {
            sculpt.set_active(static_cast<SculptMesh*>(f.get()));
            break;
        }
    }
    if (initial_mode == 3 && !sculpt.active())
        ui.add_sculpt_mesh();

    const Vec3 light_dir(settings.light_x, settings.light_y, settings.light_z);
    const char* SAVE_PATH = "saves/default.r3do";

    auto render = [&](int samples) {
        switch (ui.render_mode()) {
        case 0:
            render_scene(grid, cam, display, display.width(), display.height(),
                         samples > 0 ? samples : settings.lq_samples, light_dir);
            break;
        case 1: render_billboard(grid, cam, display, light_dir); break;
        case 2: render_overhead(grid, cam, display); break;
        case 3: render_scene(grid, cam, display, display.width(), display.height(),
                         samples > 0 ? samples : settings.lq_samples, light_dir);
            break;
        case 4: {
            // Static rendering: each pixel gets a fixed color, no ray casting
            int w = display.width();
            int h = display.height();
            for (int j = 0; j < h; j++) {
                for (int i = 0; i < w; i++) {
                    // Fixed color pattern: gradient based on pixel position
                    unsigned int c = ((unsigned)(i * 255 / w) << 16) |
                                    ((unsigned)(j * 255 / h) << 8) |
                                    (unsigned)(128);
                    display.set_pixel(i, j, c);
                }
            }
            display.update();
            break;
        }
        default: render_billboard(grid, cam, display, light_dir); break;
        }
    };

    auto build_targets = [&](std::vector<std::string>& names,
                             std::vector<Hittable*>& objs) {
        names.clear();
        objs.clear();
        for (auto& f : grid.free_objects()) {
            names.push_back(std::string("free#") +
                            std::to_string((long)objs.size()) + " " +
                            f->type_name());
            objs.push_back(f.get());
        }
        // CAMERA pseudo-entry: the combo picks it via index == free count.
        names.push_back("camera");
        objs.push_back(nullptr);
        // Out-of-range clip targets are dead, not "the last object": mark them
        // so the panel shows a warning instead of silently animating nothing.
        for (auto& ci : animator.clips)
            if (ci.target.type == AnimTarget::FREE_OBJECT &&
                (ci.target.free_idx < 0 ||
                 ci.target.free_idx >= (int)objs.size()))
                ci.target.free_idx = -1;
    };

    auto refresh_ui = [&]() {
        std::vector<std::string> tnames;
        std::vector<Hittable*> tobjs;
        build_targets(tnames, tobjs);
        nkui.draw(ui, animator, cam, tnames, tobjs);
        nkui.render();
    };

    // Widgets are drawn first so button actions (place shape, change mode,
    // add clip, ...) have already mutated the scene before it is rendered;
    // the completed Nuklear buffer is then blitted over the panels.
    auto render_and_ui = [&]() {
        std::vector<std::string> tnames;
        std::vector<Hittable*> tobjs;
        build_targets(tnames, tobjs);
        nkui.draw(ui, animator, cam, tnames, tobjs);
        render(0);
        nkui.render();
        display.draw_crosshair(display.width() / 2, display.height() / 2, 8, 0x00ff00);
    };

    // Per-frame animation application: keys + procedural offsets drive either
    // a free object, a grid cell occupant, or the camera pose.
    auto apply_animator = [&]() {
        auto& fobjs = grid.free_objects();
        for (auto& ci : animator.clips) {
            const AnimTarget& tg = ci.target;
            if (tg.type == AnimTarget::NONE) continue;
            Hittable* obj = nullptr;
            if (tg.type == AnimTarget::FREE_OBJECT) {
                if (tg.free_idx < 0 || tg.free_idx >= (int)fobjs.size()) continue;
                obj = fobjs[(size_t)tg.free_idx].get();
            } else if (tg.type == AnimTarget::CELL) {
                obj = grid.get(tg.i, tg.j, tg.k);
            }

            // First application after a change snaps the pose base.
            if (!ci.base_snapped) {
                ci.base_snapped = true;
                if (tg.type == AnimTarget::CAMERA) {
                    ci.base_pos = cam.pos;
                    ci.base_yaw = (float)cam.yaw;
                    ci.base_pitch = (float)cam.pitch;
                } else if (obj) {
                    ci.base_pos = obj->get_center() - obj->anim_offset();
                }
            }

            float t = animator.clip_time(ci);
            Vec3 dp(0, 0, 0), dr(0, 0, 0);
            for (auto& pm : ci.procs) {
                if (!pm.enabled) continue;
                Vec3 v = pm.evaluate(t);
                if (pm.type == ProceduralType::SpinRotate) dr = dr + v;
                else dp = dp + v;
            }
            dp = dp + ci.clip.evaluate(AnimProperty::Position, t);

            if (tg.type == AnimTarget::CAMERA) {
                Vec3 drk = ci.clip.evaluate(AnimProperty::Rotation, t);
                cam.pos = ci.base_pos + dp;
                cam.yaw = (double)ci.base_yaw + (double)drk.x + (double)dr.x;
                cam.pitch = (double)ci.base_pitch + (double)drk.y + (double)dr.y;
                if (cam.pitch > 1.5) cam.pitch = 1.5;
                if (cam.pitch < -1.5) cam.pitch = -1.5;
                continue;
            }
            if (!obj) continue;

            obj->set_anim_offset(dp);
            obj->set_anim_rot(ci.clip.evaluate(AnimProperty::Rotation, t) + dr);
            obj->set_anim_scale(ci.clip.evaluate(AnimProperty::Scale, t));
            obj->set_anim_tint(ci.clip.evaluate(AnimProperty::Color, t));
        }
    };

    // Reset only objects clips actually bind to (cheap: a handful of clips).
    auto reset_clip_targets = [&]() {
        auto& fobjs = grid.free_objects();
        for (auto& ci : animator.clips) {
            Hittable* obj = nullptr;
            if (ci.target.type == AnimTarget::FREE_OBJECT) {
                if (ci.target.free_idx < 0 || ci.target.free_idx >= (int)fobjs.size()) continue;
                obj = fobjs[(size_t)ci.target.free_idx].get();
            } else if (ci.target.type == AnimTarget::CELL) {
                obj = grid.get(ci.target.i, ci.target.j, ci.target.k);
            }
            if (!obj) continue;
            obj->set_anim_offset(Vec3(0, 0, 0));
            obj->set_anim_rot(Vec3(0, 0, 0));
            obj->set_anim_scale(Vec3(1, 1, 1));
            obj->set_anim_tint(Vec3(1, 1, 1));
        }
        // camera clips: restore base pose lazily; nothing to undo per-object
    };

    auto set_panel_visible = [&](bool v) {
        nkui.set_visible(v);
        if (!v) reset_clip_targets();
        render_and_ui();
    };
    (void)set_panel_visible;

    auto autosave = [&]() {
        save_scene(grid, cam, SAVE_PATH, ui.render_mode(), ui.palette_idx());
    };

    std::cout << "Initial render..." << std::endl;
    render_and_ui();

    Vec3 vel(0, 0, 0);
    bool on_ground = true;
    auto last_frame = std::chrono::steady_clock::now();
    bool running = true;
    float last_applied_time = -1.0f;  // seek-preview gate (persists across frames)

    while (running) {
        bool animating = display.is_mouse_down() ||
                         (ui.ground_mode() && !on_ground) ||
                         ui.is_save_dialog_active() ||
                         animator.global_playing;
        nkui.begin();
        if (!animating)
            display.wait_event(50);
        display.process_events();
        nkui.end_frame();
        if (display.is_closed()) return;

        // Delta time
        auto now = std::chrono::steady_clock::now();
        double dt = std::chrono::duration<double>(now - last_frame).count();
        last_frame = now;
        if (dt > 0.05 || dt <= 0) dt = 0.05;

        // Animation tick. When a click is pending, let the input handling
        // below draw the UI once instead of drawing here as well.
        if (nkui.visible() && animator.global_playing) {
            animator.update((float)dt);
            apply_animator();
            last_applied_time = animator.global_time;
            if (!display.mouse_clicked() && !display.mouse_released())
                render_and_ui();
        } else if (!nkui.visible()) {
            reset_clip_targets();
        } else if (last_applied_time != animator.global_time) {
            // Paused with the panel open: seeking on the timeline previews
            // the pose immediately instead of waiting for PLAY.
            apply_animator();
            last_applied_time = animator.global_time;
            render_and_ui();
        }

        // Panel clicks are deferred and drawn at most once per iteration so a
        // batched press+release cannot fire the same button twice.
        bool ui_click_pending = false;

        if (display.mouse_clicked()) {
            int mx = display.mouse_x(), my = display.mouse_y();
            int btn = display.mouse_button();
            display.clear_mouse();
            if (nkui.hit(mx, my)) {
                ui_click_pending = true;
            } else if (btn == 3) {
                // Right-click: quick-place the last used shape
                if (ui.place_last_shape()) {
                    autosave();
                    render_and_ui();
                }
            }
        }

        // Begin sculpt stroke
        static bool sculpt_stroke_active = false;
        bool sculpt_mouse_down = (display.mouse_button() == 1 || display.mouse_button() == 3);
        if (ui.render_mode() == 3 && display.is_mouse_down() && sculpt_mouse_down &&
            !sculpt_stroke_active &&
            !nkui.hit(display.mouse_press_x(), display.mouse_press_y())) {
            sculpt.begin_stroke();
            sculpt_stroke_active = true;
        }
        if (!(ui.render_mode() == 3 && display.is_mouse_down() && sculpt_mouse_down)) {
            if (sculpt_stroke_active) { sculpt.end_stroke(); sculpt_stroke_active = false; }
        }

        // --- Drag look ---
        if (display.is_mouse_down() &&
            !nkui.hit(display.mouse_press_x(), display.mouse_press_y())) {
            // Sculpt mode: LMB or RMB strokes
            if (ui.render_mode() == 3 && sculpt_mouse_down) {
                Brush b;
                b.radius = ui.brush_radius();
                b.strength = ui.brush_strength();
                b.type = ui.brush_type();
                b.invert = ui.brush_invert() || display.mouse_button() == 3; // RMB also inverts
                sculpt.stroke_update(cam, display.width(), display.height(),
                                     display.mouse_x(), display.mouse_y(), b, true);
                render_and_ui();
                display.clear_mouse_delta();
            } else {
                int dx = display.mouse_dx();
                int dy = display.mouse_dy();
                if (dx != 0 || dy != 0) {
                    display.clear_mouse_delta();
                    cam.rotate(-dx * MOUSE_SENS, dy * MOUSE_SENS);
                    render_and_ui();
                }
            }
        }

        // --- Release: panel buttons + click-to-delete ---
        if (display.mouse_released()) {
            display.clear_mouse_released();
            bool over_panel = nkui.hit(display.mouse_press_x(), display.mouse_press_y());
            if (over_panel) {
                // Deferred draw below: fires the button exactly once even when
                // press and release arrive in the same iteration.
                ui_click_pending = true;
            } else if (ui.render_mode() != 3 && display.mouse_button() == 1) {
                int dx = display.mouse_x() - display.mouse_press_x();
                int dy = display.mouse_y() - display.mouse_press_y();
                int dist = (int)std::sqrt((double)(dx*dx + dy*dy));
                if (dist < 5) {
                    HitRecord rec;
                    if (hit_center(grid, cam, display.width(), display.height(), rec)) {
                        bool deleted = false;
                        // Free object (mesh)?
                        if (rec.hittable) {
                            auto& fobjs = grid.free_objects();
                            for (size_t fi = 0; fi < fobjs.size(); fi++) {
                                if (fobjs[fi].get() == rec.hittable) {
                                    ui.snapshot_for_undo();
                                    grid.remove_free(rec.hittable);
                                    // Renumber clips so a deletion never
                                    // silently re-targets a different object.
                                    for (auto& ci : animator.clips) {
                                        if (ci.target.type != AnimTarget::FREE_OBJECT) continue;
                                        if (ci.target.free_idx == (int)fi) ci.target.free_idx = -1;
                                        else if (ci.target.free_idx > (int)fi) ci.target.free_idx--;
                                    }
                                    deleted = true;
                                    break;
                                }
                            }
                        }
                        // Cell-based deletion
                        if (!deleted) {
                            int i, j, k;
                            if (rec.hittable &&
                                grid.world_to_cell(rec.p, i, j, k) &&
                                grid.get(i, j, k)) {
                                ui.snapshot_for_undo();
                                grid.set(i, j, k, nullptr);
                                deleted = true;
                            }
                        }
                        if (deleted) {
                            autosave();
                            render(settings.lq_samples);
                            refresh_ui();
                            display.draw_crosshair(display.width() / 2,
                                                   display.height() / 2, 8, 0x00ff00);
                        }
                    }
                }
            }
        }

        if (ui_click_pending)
            render_and_ui();

        // --- One-shot keys ---
        bool ctrl_held = display.key_held(XK_Control_L) || display.key_held(XK_Control_R);
        int key;
        char kc;
        while (display.poll_key(key, kc)) {
            bool nk_eats_all = nkui.wants_keys();
            if (nk_eats_all) {
                // typing inside a Nuklear widget / over a panel: no editor hotkeys
                refresh_ui();
                continue;
            }
            switch (key) {
                case XK_F11:
                    display.toggle_fullscreen();
                    render_and_ui();
                    break;
                case XK_g: case XK_G:
                    ui.toggle_ground_mode();
                    if (ui.ground_mode()) {
                        cam.pos.y = grid.get_ground_height(cam.pos.x, cam.pos.z) + EYE_HEIGHT;
                        vel.y = 0;
                        on_ground = true;
                    }
                    autosave();
                    render_and_ui();
                    break;
                case XK_b: case XK_B:
                    ui.set_render_mode((ui.render_mode() + 1) % 5);
                    render_and_ui();
                    break;
                case XK_t: case XK_T:
                    set_panel_visible(!nkui.visible());
                    break;
                case XK_space:
                    if (!ui.ground_mode()) {
                        std::cout << "Full quality..." << std::endl;
                        render(settings.hq_samples);
                        refresh_ui();
                        display.draw_crosshair(display.width() / 2,
                                               display.height() / 2, 8, 0x00ff00);
                    } else if (on_ground) {
                        vel.y = JUMP_SPEED;
                        on_ground = false;
                    }
                    break;
                case XK_Escape:
                    running = false;
                    break;
                case XK_Shift_L:
                case XK_Shift_R:
                case XK_Alt_L:
                case XK_Alt_R:
                    // allow toggle check? defer
                    break;
                default:
                    if (ctrl_held && (key == XK_z || key == XK_Z)) {
                        if (ui.render_mode() == 3) {
                            sculpt.undo();
                            render_and_ui();
                            break;
                        }
                        if (ui.undo()) {
                            autosave();
                            render_and_ui();
                        }
                    } else if (ctrl_held && (key == XK_y || key == XK_Y)) {
                        if (ui.render_mode() == 3) {
                            sculpt.redo();
                            render_and_ui();
                            break;
                        }
                    }
                    break;
            }
            if (!running) break;
        }

        // Toggle mouse capture with Alt+Shift
        {
            bool alt_held = display.key_held(XK_Alt_L) || display.key_held(XK_Alt_R);
            bool shift_held = display.key_held(XK_Shift_L) || display.key_held(XK_Shift_R);
            static bool last_toggle = false;
            bool both = alt_held && shift_held;
            if (both && !last_toggle) {
                mouse_captured = !mouse_captured;
                if (mouse_captured)
                    display.grab_pointer();
                else
                    display.ungrab_pointer();
                last_toggle = true;
            }
            if (!both) last_toggle = false;
        }

        // --- Continuous movement (held keys, delta-time) ---
        if (running) {
            double fwd_amt = 0, strafe_amt = 0, up_amt = 0;
            if (display.key_held(XK_w)) fwd_amt   += MOVE_SPEED * dt;
            if (display.key_held(XK_s)) fwd_amt   -= MOVE_SPEED * dt;
            if (display.key_held(XK_d)) strafe_amt += MOVE_SPEED * dt;
            if (display.key_held(XK_a)) strafe_amt -= MOVE_SPEED * dt;
            if (!ui.ground_mode()) {
                if (display.key_held(XK_e)) up_amt += MOVE_SPEED * dt;
                if (display.key_held(XK_q)) up_amt -= MOVE_SPEED * dt;
            }

            bool rotate = false;
            if (display.key_held(XK_Left))  { cam.rotate( ROT_SPEED * dt, 0); rotate = true; }
            if (display.key_held(XK_Right)) { cam.rotate(-ROT_SPEED * dt, 0); rotate = true; }
            if (display.key_held(XK_Up))    { cam.rotate(0,  ROT_SPEED * dt); rotate = true; }
            if (display.key_held(XK_Down))  { cam.rotate(0, -ROT_SPEED * dt); rotate = true; }

            bool moved = false;
            if (fwd_amt != 0 || strafe_amt != 0 || up_amt != 0) {
                Vec3 step = cam.forward() * fwd_amt + cam.right() * strafe_amt;
                if (ui.ground_mode()) {
                    // Flatten movement onto the horizontal plane
                    Vec3 f = cam.forward();
                    Vec3 hf(f.x, 0, f.z);
                    double len = hf.length();
                    hf = (len > 1e-9) ? hf * (1.0 / len) : Vec3(0, 0, 0);
                    step = hf * fwd_amt + cam.right() * strafe_amt;
                    step.y = 0;
                } else {
                    step.y += up_amt;
                }
                // Per-axis slide so we don't stick on walls
                Vec3 np = cam.pos;
                np.x += step.x;
                if (!blocked_at(grid, np)) cam.pos.x = np.x;
                np = cam.pos;
                np.z += step.z;
                if (!blocked_at(grid, np)) cam.pos.z = np.z;
                np = cam.pos;
                np.y += step.y;
                if (!blocked_at(grid, np)) cam.pos.y = np.y;
                moved = true;
            }

            // --- Ground-mode physics (fall off ledges, jump gravity) ---
            if (ui.ground_mode()) {
                double gh = grid.get_ground_height(cam.pos.x, cam.pos.z);
                if (on_ground && cam.pos.y > gh + EYE_HEIGHT + 0.02)
                    on_ground = false;   // walked off a ledge
                if (!on_ground) {
                    vel.y += GRAVITY * dt;
                    cam.pos.y += vel.y * dt;
                    gh = grid.get_ground_height(cam.pos.x, cam.pos.z);
                    if (cam.pos.y <= gh + EYE_HEIGHT) {
                        cam.pos.y = gh + EYE_HEIGHT;
                        vel.y = 0;
                        on_ground = true;
                    }
                    moved = true;
                } else if (cam.pos.y < gh + EYE_HEIGHT) {
                    cam.pos.y = gh + EYE_HEIGHT;   // terrain rose under us
                }
            }

            if (moved || rotate)
                render_and_ui();
        }
    }

    autosave();
}

} // namespace

int main() {
    Settings settings = load_settings();

    DisplayWin display(settings.res.w, settings.res.h, "R3DO - 3D Space");

    const int nx = 10, ny = 10, nz = 10;
    Grid grid(nx, ny, nz, 0.8, Vec3(0, 0, 0));
    Camera cam(Vec3(3.0, 1.5, 4.0), 0, -0.15);

    while (true) {
        StartAction action = show_start_screen(display, settings);
        if (action == StartAction::QUIT || display.is_closed())
            return 0;

        if (action == StartAction::NEW_SCENE) {
            grid.clear();
            run_editor(grid, cam, display, settings);
        } else if (action == StartAction::SCULPT) {
            grid.clear();
            // run_editor spawns/attaches a sculpt mesh and starts in Sculpt mode
            run_editor(grid, cam, display, settings, 3);
        } else {
            std::string name = show_load_screen(display);
            if (name.empty() || display.is_closed()) continue;
            load_scene("saves/" + name + ".r3do", grid, &cam);
            run_editor(grid, cam, display, settings);
        }

        if (display.is_closed()) return 0;
    }
}
