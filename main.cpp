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

    DisplayWin display(800, 600, "R3DO - 3D Space");
    display.toggle_fullscreen();
    const char* SAVE_PATH = "saves/default.r3do";

    Grid grid(nx, ny, nz, cell_size, grid_center);

    while (true) {
        StartAction action = show_start_screen(display);
        if (action == StartAction::QUIT) return 0;

        if (action == StartAction::NEW_SCENE) break;

        std::string name = show_load_screen(display);
        if (name.empty()) continue;
        load_scene("saves/" + name + ".r3do", grid);
        break;
    }

    Camera cam(Vec3(3.0, 1.5, 4.0), 0, -0.15);
    UI ui(grid, cam, display);

    std::cout << "Initial render..." << std::endl;
    render_scene(grid, cam, display, display.width(), display.height(), lq_samples, light_dir);
    ui.draw();
    display.draw_crosshair(display.width() / 2, display.height() / 2, 8, 0x00ff00);

    bool running = true;

    while (running) {
        display.process_events();
        if (display.is_closed()) break;

        if (display.mouse_clicked()) {
            int mx = display.mouse_x();
            int my = display.mouse_y();
            display.clear_mouse();
            if (mx >= display.width() - 170 && ui.handle_click(mx, my)) {
                save_scene(grid, SAVE_PATH);
                render_scene(grid, cam, display, display.width(), display.height(), lq_samples, light_dir);
                ui.draw();
                display.draw_crosshair(display.width() / 2, display.height() / 2, 8, 0x00ff00);
            }
        }

        if (display.is_mouse_down() && display.mouse_press_x() < display.width() - 170) {
            int dx = display.mouse_dx();
            int dy = display.mouse_dy();
            if (dx != 0 || dy != 0) {
                display.clear_mouse_delta();
                double sens = 0.005;
                cam.rotate(-dx * sens, dy * sens);
                render_scene(grid, cam, display, display.width(), display.height(), lq_samples, light_dir);
                ui.draw();
                display.draw_crosshair(display.width() / 2, display.height() / 2, 8, 0x00ff00);
            }
        }

        if (display.mouse_released()) {
            display.clear_mouse_released();
            if (display.mouse_press_x() < display.width() - 170) {
                int dx = display.mouse_x() - display.mouse_press_x();
                int dy = display.mouse_y() - display.mouse_press_y();
                int dist = (int)std::sqrt((double)(dx*dx + dy*dy));
                if (dist < 5) {
                    HitRecord rec;
                    if (hit_center(grid, cam, display.width(), display.height(), rec)) {
                        int i, j, k;
                        if (grid.world_to_cell(rec.p, i, j, k) && grid.get(i, j, k)) {
                            grid.set(i, j, k, nullptr);
                            save_scene(grid, SAVE_PATH);
                            render_scene(grid, cam, display, display.width(), display.height(), lq_samples, light_dir);
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
                case XK_w: cam.move_fwd(move_speed); moved = true; break;
                case XK_s: cam.move_fwd(-move_speed); moved = true; break;
                case XK_a: cam.move_right(-move_speed); moved = true; break;
                case XK_d: cam.move_right(move_speed); moved = true; break;
                case XK_q: cam.move_up(-move_speed); moved = true; break;
                case XK_e: cam.move_up(move_speed); moved = true; break;
                case XK_Left: cam.rotate(rot_speed, 0); moved = true; break;
                case XK_Right: cam.rotate(-rot_speed, 0); moved = true; break;
                case XK_Up: cam.rotate(0, rot_speed); moved = true; break;
                case XK_Down: cam.rotate(0, -rot_speed); moved = true; break;
                case XK_F11:
                    display.toggle_fullscreen();
                    render_scene(grid, cam, display, display.width(), display.height(), lq_samples, light_dir);
                    ui.draw();
                    display.draw_crosshair(display.width() / 2, display.height() / 2, 8, 0x00ff00);
                    break;
                case XK_space:
                    std::cout << "Full quality..." << std::endl;
                    render_scene(grid, cam, display, display.width(), display.height(), hq_samples, light_dir);
                    ui.draw();
                    display.draw_crosshair(display.width() / 2, display.height() / 2, 8, 0x00ff00);
                    break;
                case XK_Escape: running = false; break;
            }

            if (moved && running) {
                std::cout << cam.pos.x << "," << cam.pos.y << "," << cam.pos.z << " " << std::flush;
                render_scene(grid, cam, display, display.width(), display.height(), lq_samples, light_dir);
                ui.draw();
                display.draw_crosshair(display.width() / 2, display.height() / 2, 8, 0x00ff00);
                std::cout << "done" << std::endl;
            }
        }

        usleep(10000);
    }

    return 0;
}
