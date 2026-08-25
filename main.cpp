#include "v3.hpp"
#include "ray.hpp"
#include "hittable.hpp"
#include "grid.hpp"
#include "camera.hpp"
#include "display.hpp"
#include "ui.hpp"
#include "saver.hpp"
#include "render.hpp"
#include "startscreen.hpp"
#include "settings.hpp"
#include "billboard.hpp"
#include "overhead.hpp"
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
                const Settings& settings) {
    UI ui(grid, cam, display);
    const Vec3 light_dir(settings.light_x, settings.light_y, settings.light_z);
    const char* SAVE_PATH = "saves/default.r3do";

    auto render = [&](int samples) {
        switch (ui.render_mode()) {
        case 0:
            render_scene(grid, cam, display, display.width(), display.height(),
                         samples > 0 ? samples : settings.lq_samples, light_dir);
            break;
        case 1: render_billboard(grid, cam, display, light_dir); break;
        default: render_overhead(grid, cam, display); break;
        }
    };

    auto render_and_ui = [&]() {
        render(0);
        ui.draw();
        display.draw_crosshair(display.width() / 2, display.height() / 2, 8, 0x00ff00);
    };

    auto autosave = [&]() {
        save_scene(grid, cam, SAVE_PATH, ui.render_mode(), ui.palette_idx());
    };

    std::cout << "Initial render..." << std::endl;
    render_and_ui();

    Vec3 vel(0, 0, 0);
    bool on_ground = true;
    auto last_frame = std::chrono::steady_clock::now();
    bool running = true;

    while (running) {
        bool animating = display.is_mouse_down() ||
                         (ui.ground_mode() && !on_ground) ||
                         ui.is_save_dialog_active();
        if (!animating)
            display.wait_event(50);
        display.process_events();
        if (display.is_closed()) return;

        // Delta time
        auto now = std::chrono::steady_clock::now();
        double dt = std::chrono::duration<double>(now - last_frame).count();
        last_frame = now;
        if (dt > 0.05 || dt <= 0) dt = 0.05;

        // --- Save dialog: modal key + click handling ---
        if (ui.is_save_dialog_active()) {
            int key;
            char c;
            while (display.poll_key(key, c))
                ui.handle_save_dialog_key(key, c);

            if (display.mouse_clicked()) {
                int mx = display.mouse_x(), my = display.mouse_y();
                display.clear_mouse();
                display.clear_mouse_released();
                ui.handle_click(mx, my);
                ui.draw();
                display.draw_crosshair(display.width() / 2, display.height() / 2, 8, 0x00ff00);
            }
            continue;
        }

        // --- Mouse clicks ---
        if (display.mouse_clicked()) {
            int mx = display.mouse_x(), my = display.mouse_y();
            int btn = display.mouse_button();
            display.clear_mouse();
            if (mx >= display.width() - UI::SIDEBAR_W) {
                if (ui.handle_click(mx, my)) {
                    autosave();
                    render_and_ui();
                }
            } else if (btn == 3) {
                // Right-click: quick-place the last used shape
                if (ui.place_last_shape()) {
                    autosave();
                    render_and_ui();
                }
            }
        }

        // --- Drag look ---
        if (display.is_mouse_down() &&
            display.mouse_press_x() < display.width() - UI::SIDEBAR_W) {
            int dx = display.mouse_dx();
            int dy = display.mouse_dy();
            if (dx != 0 || dy != 0) {
                display.clear_mouse_delta();
                cam.rotate(-dx * MOUSE_SENS, dy * MOUSE_SENS);
                render_and_ui();
            }
        }

        // --- Click-to-delete ---
        if (display.mouse_released()) {
            display.clear_mouse_released();
            if (display.mouse_button() == 1 &&
                display.mouse_press_x() < display.width() - UI::SIDEBAR_W) {
                int dx = display.mouse_x() - display.mouse_press_x();
                int dy = display.mouse_y() - display.mouse_press_y();
                int dist = (int)std::sqrt((double)(dx*dx + dy*dy));
                if (dist < 5) {
                    HitRecord rec;
                    if (hit_center(grid, cam, display.width(), display.height(), rec)) {
                        bool deleted = false;
                        // Free object (mesh)?
                        if (rec.hittable) {
                            for (const auto& f : grid.free_objects()) {
                                if (f.get() == rec.hittable) {
                                    ui.snapshot_for_undo();
                                    grid.remove_free(rec.hittable);
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
                            ui.draw();
                            display.draw_crosshair(display.width() / 2,
                                                   display.height() / 2, 8, 0x00ff00);
                        }
                    }
                }
            }
        }

        // --- One-shot keys ---
        bool ctrl_held = display.key_held(XK_Control_L) || display.key_held(XK_Control_R);
        int key;
        char kc;
        while (display.poll_key(key, kc)) {
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
                    ui.set_render_mode((ui.render_mode() + 1) % 3);
                    render_and_ui();
                    break;
                case XK_space:
                    if (!ui.ground_mode()) {
                        std::cout << "Full quality..." << std::endl;
                        render(settings.hq_samples);
                        ui.draw();
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
                default:
                    if (ctrl_held && (key == XK_z || key == XK_Z)) {
                        if (ui.undo()) {
                            autosave();
                            render_and_ui();
                        }
                    }
                    break;
            }
            if (!running) break;
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
        } else {
            std::string name = show_load_screen(display);
            if (name.empty() || display.is_closed()) continue;
            load_scene("saves/" + name + ".r3do", grid, &cam);
        }

        run_editor(grid, cam, display, settings);
        if (display.is_closed()) return 0;
    }
}
