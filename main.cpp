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
#include <unistd.h>

int main() {
    int nx = 10, ny = 10, nz = 10;
    double cell_size = 0.8;
    Vec3 grid_center(0, 0, 0);
    int hq_samples = 4;
    int lq_samples = 1;
    Vec3 light_dir(1, 2, 1);

    Settings settings = load_settings();

    DisplayWin display(settings.res.w, settings.res.h, "R3DO - 3D Space");
    const char* SAVE_PATH = "saves/default.r3do";

    Grid grid(nx, ny, nz, cell_size, grid_center);

    while (true) {
        StartAction action = show_start_screen(display, settings);
        if (action == StartAction::QUIT) return 0;

        if (action == StartAction::NEW_SCENE) break;

        std::string name = show_load_screen(display);
        if (name.empty()) continue;
        load_scene("saves/" + name + ".r3do", grid);
        break;
    }

    Camera cam(Vec3(3.0, 1.5, 4.0), 0, -0.15);
    UI ui(grid, cam, display);
    int render_mode = 1; // 0=raytrace 1=billboard 2=overhead
    const char* mode_names[] = {"Raytrace", "Billboard", "Overhead"};
    auto render_auto = [&]() { ui.set_mode_label(mode_names[render_mode]); };
    render_auto();

    auto render = [&](int samples) {
        if (render_mode == 0)
            render_scene(grid, cam, display, display.width(), display.height(), samples, light_dir);
        else if (render_mode == 1)
            render_billboard(grid, cam, display, light_dir);
        else
            render_overhead(grid, cam, display);
    };

    auto render_and_ui = [&]() {
        render(lq_samples);
        ui.draw();
        display.draw_crosshair(display.width() / 2, display.height() / 2, 8, 0x00ff00);
    };

    std::cout << "Initial render..." << std::endl;
    render_and_ui();

    const double eye_height = 1.5;
    const double gravity_accel = -0.015;
    const double jump_speed = 0.3;
    Vec3 vel(0, 0, 0);
    bool on_ground = true;

    bool running = true;

    while (running) {
        display.process_events();
        if (display.is_closed()) break;

        if (ui.is_save_dialog_active()) {
            int key = display.get_key();
            char c = display.get_char();
            display.clear_key();
            ui.handle_save_dialog_key(key, c);
            ui.draw();
            display.draw_crosshair(display.width() / 2, display.height() / 2, 8, 0x00ff00);
        }

        if (display.mouse_clicked()) {
            int mx = display.mouse_x();
            int my = display.mouse_y();
            display.clear_mouse();
            if (ui.is_save_dialog_active()) {
                ui.handle_click(mx, my);
                display.clear_mouse_released();
                ui.draw();
                display.draw_crosshair(display.width() / 2, display.height() / 2, 8, 0x00ff00);
            } else if (mx >= display.width() - 170 && ui.handle_click(mx, my)) {
                render_and_ui();
            }
            if (ui.mode_was_clicked()) {
                render_mode = (render_mode + 1) % 3;
                render_auto();
                render_and_ui();
            }
            if (ui.ground_was_clicked()) {
                ui.ground_mode_ = !ui.ground_mode_;
                ui.set_ground_label(ui.ground_mode_ ? "Ground: ON" : "Ground: OFF");
                if (ui.ground_mode_) { cam.pos.y = grid.get_ground_height(cam.pos.x, cam.pos.z) + eye_height; vel.y = 0; on_ground = true; }
                render_and_ui();
            }
        }

        if (display.is_mouse_down() && display.mouse_press_x() < display.width() - 170) {
            int dx = display.mouse_dx();
            int dy = display.mouse_dy();
            if (dx != 0 || dy != 0) {
                display.clear_mouse_delta();
                double sens = 0.005;
                cam.rotate(-dx * sens, dy * sens);
                render_and_ui();
            }
        }

        if (display.mouse_released()) {
            display.clear_mouse_released();
            if (ui.is_save_dialog_active()) {
                // ignore
            } else if (display.mouse_press_x() < display.width() - 170) {
                int dx = display.mouse_x() - display.mouse_press_x();
                int dy = display.mouse_y() - display.mouse_press_y();
                int dist = (int)std::sqrt((double)(dx*dx + dy*dy));
                if (dist < 5) {
                    HitRecord rec;
                    if (hit_center(grid, cam, display.width(), display.height(), rec)) {
                        bool need_render = false;
                        // Check if hit object is a free object (mesh)
                        if (rec.hittable) {
                            auto& free = grid.free_objects();
                            for (size_t fi = 0; fi < free.size(); fi++) {
                                if (free[fi] == rec.hittable) {
                                    grid.remove_free(rec.hittable);
                                    delete rec.hittable;
                                    need_render = true;
                                    break;
                                }
                            }
                        }
                        // Otherwise, cell-based deletion
                        if (!need_render) {
                            int i, j, k;
                            if (grid.world_to_cell(rec.p, i, j, k) && grid.get(i, j, k)) {
                                grid.set(i, j, k, nullptr);
                                need_render = true;
                            }
                        }
                        if (need_render) {
                            save_scene(grid, SAVE_PATH);
                            render(lq_samples);
                            ui.draw();
                            display.draw_crosshair(display.width() / 2, display.height() / 2, 8, 0x00ff00);
                        }
                    }
                }
            }
        }

        int key = display.get_key();
        display.clear_key();

        if (key) {
            double move_speed = 0.5;
            double rot_speed = 0.06;
            bool moved = false;

            switch (key) {
                case XK_w:
                    if (ui.ground_mode_) {
                        Vec3 f = cam.forward();
                        Vec3 hf = unit_vector(Vec3(f.x, 0, f.z));
                        if (std::isfinite(hf.x)) cam.pos = cam.pos + hf * move_speed;
                    } else {
                        cam.move_fwd(move_speed);
                    }
                    moved = true;
                    break;
                case XK_s:
                    if (ui.ground_mode_) {
                        Vec3 f = cam.forward();
                        Vec3 hf = unit_vector(Vec3(f.x, 0, f.z));
                        if (std::isfinite(hf.x)) cam.pos = cam.pos - hf * move_speed;
                    } else {
                        cam.move_fwd(-move_speed);
                    }
                    moved = true;
                    break;
                case XK_a:
                    if (ui.ground_mode_) cam.pos = cam.pos - cam.right() * move_speed;
                    else cam.move_right(-move_speed);
                    moved = true;
                    break;
                case XK_d:
                    if (ui.ground_mode_) cam.pos = cam.pos + cam.right() * move_speed;
                    else cam.move_right(move_speed);
                    moved = true;
                    break;
                case XK_q:
                    if (!ui.ground_mode_) cam.move_up(-move_speed);
                    moved = !ui.ground_mode_;
                    break;
                case XK_e:
                    if (!ui.ground_mode_) cam.move_up(move_speed);
                    moved = !ui.ground_mode_;
                    break;
                case XK_Left: cam.rotate(rot_speed, 0); moved = true; break;
                case XK_Right: cam.rotate(-rot_speed, 0); moved = true; break;
                case XK_Up: cam.rotate(0, rot_speed); moved = true; break;
                case XK_Down: cam.rotate(0, -rot_speed); moved = true; break;
                case XK_F11:
                    display.toggle_fullscreen();
                    render_and_ui();
                    break;
                case XK_g:
                case XK_G:
                    ui.ground_mode_ = !ui.ground_mode_;
                    ui.set_ground_label(ui.ground_mode_ ? "Ground: ON" : "Ground: OFF");
                    if (ui.ground_mode_) { cam.pos.y = grid.get_ground_height(cam.pos.x, cam.pos.z) + eye_height; vel.y = 0; on_ground = true; }
                    render_and_ui();
                    break;
                case XK_space:
                    if (ui.ground_mode_) {
                        if (on_ground) {
                            vel.y = jump_speed;
                            on_ground = false;
                            moved = true;
                        }
                    } else {
                        std::cout << "Full quality..." << std::endl;
                        render(hq_samples);
                        ui.draw();
                        display.draw_crosshair(display.width() / 2, display.height() / 2, 8, 0x00ff00);
                    }
                    break;
                case XK_Escape: running = false; break;
                case XK_b:
                case XK_B:
                    render_mode = (render_mode + 1) % 3;
                    render_auto();
                    render_and_ui();
                    break;
            }

            if (moved && running) {
                render_and_ui();
            }
        }

        // Physics tick (ground mode only)
        if (ui.ground_mode_ && !on_ground) {
            vel.y += gravity_accel;
            cam.pos.y += vel.y;
            double gh = grid.get_ground_height(cam.pos.x, cam.pos.z);
            if (cam.pos.y <= gh + eye_height) {
                cam.pos.y = gh + eye_height;
                vel.y = 0;
                on_ground = true;
            }
            render_and_ui();
        }

        usleep(10000);
    }

    return 0;
}
