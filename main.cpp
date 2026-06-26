#include "v3.hpp"
#include "ray.hpp"
#include "hittable.hpp"
#include "sphere.hpp"
#include "box.hpp"
#include "grid.hpp"
#include "camera.hpp"
#include "display.hpp"
#include "ui.hpp"
#include "saver.hpp"
#include <iostream>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <dirent.h>
#include <vector>
#include <string>
#include <algorithm>

Vec3 ray_color(const Ray& r, const Grid& grid, const Vec3& light_dir) {
    HitRecord rec;
    Vec3 light = unit_vector(light_dir);
    Vec3 bg(0.05, 0.05, 0.15);

    if (grid.hit(r, 0.001, 1000.0, rec)) {
        double diff = std::max(0.0, dot(rec.normal, light));
        double ambient = 0.3;
        double intensity = ambient + (1.0 - ambient) * diff;
        return rec.color * intensity;
    }

    return bg;
}

void render_scene(const Grid& grid, const Camera& cam, DisplayWin& display,
                  int image_width, int image_height, int samples,
                  const Vec3& light_dir) {
    double viewport_height = 2.5;
    double viewport_width = viewport_height * image_width / image_height;
    double focal_length = 2.0;

    Vec3 fwd = cam.forward();
    Vec3 rgt = cam.right();
    Vec3 up = cross(rgt, fwd);

    Vec3 horizontal = viewport_width * rgt;
    Vec3 vertical = viewport_height * up;
    Vec3 lower_left = cam.pos - horizontal/2 - vertical/2 + fwd * focal_length;

    for (int j = image_height - 1; j >= 0; --j) {
        for (int i = 0; i < image_width; ++i) {
            Vec3 color;
            for (int s = 0; s < samples; ++s) {
                double u_off = (s % 2) * 0.5 + 0.25;
                double v_off = (s / 2) * 0.5 + 0.25;
                double u = (i + u_off) / (image_width - 1);
                double v = (j + v_off) / (image_height - 1);
                Ray r(cam.pos, lower_left + u*horizontal + v*vertical - cam.pos);
                color = color + ray_color(r, grid, light_dir);
            }
            color = color / samples;
            int ir = int(255.999 * std::sqrt(color.x));
            int ig = int(255.999 * std::sqrt(color.y));
            int ib = int(255.999 * std::sqrt(color.z));
            display.set_pixel(i, j, ir, ig, ib);
        }
        if (j % 100 == 0)
            std::cout << "\r" << (100 * (image_height - j) / image_height) << "% " << std::flush;
    }
    display.update();
    std::cout << "\r   \r" << std::flush;
}

bool hit_center(const Grid& grid, const Camera& cam,
                int image_width, int image_height, HitRecord& rec) {
    double viewport_height = 2.5;
    double viewport_width = viewport_height * image_width / image_height;
    double focal_length = 2.0;

    Vec3 fwd = cam.forward();
    Vec3 rgt = cam.right();
    Vec3 up = cross(rgt, fwd);

    Vec3 horizontal = viewport_width * rgt;
    Vec3 vertical = viewport_height * up;
    Vec3 lower_left = cam.pos - horizontal/2 - vertical/2 + fwd * focal_length;

    Ray r(cam.pos, lower_left + 0.5*horizontal + 0.5*vertical - cam.pos);
    return grid.hit(r, 0.001, 1000.0, rec);
}

// ── Start screen ──────────────────────────────────────────────────

enum class StartAction { NEW_SCENE, LOAD_SCENE, QUIT };

static std::vector<std::string> list_saves() {
    std::vector<std::string> saves;
    DIR* dir = opendir("saves");
    if (!dir) return saves;
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string name = entry->d_name;
        if (name.size() > 5 && name.substr(name.size() - 5) == ".r3do") {
            saves.push_back(name.substr(0, name.size() - 5));
        }
    }
    closedir(dir);
    std::sort(saves.begin(), saves.end());
    return saves;
}

static StartAction show_start_screen(DisplayWin& display) {
    int w = display.width(), h = display.height();
    const int bw = 220, bh = 44;
    const int bx = (w - bw) / 2;
    const int by_new = 290, by_load = by_new + bh + 12;

    while (true) {
        display.process_events();
        if (display.is_closed()) return StartAction::QUIT;

        int key = display.get_key();
        display.clear_key();
        if (key == XK_Escape) return StartAction::QUIT;

        display.fill_rect(0, 0, w, h, 0x0d0d1a);

        display.draw_text(w/2 - 32, 160, "R3DO", 0xffffff);
        display.draw_text(w/2 - 82, 180, "Interactive 3D Ray Tracer", 0x666688);

        display.fill_rect(bx, by_new, bw, bh, 0x333355);
        display.fill_rect(bx + 1, by_new + 1, bw - 2, bh - 2, 0x3d3d6b);
        display.draw_text(bx + 55, by_new + 28, "New Scene", 0xccccff);

        display.fill_rect(bx, by_load, bw, bh, 0x333355);
        display.fill_rect(bx + 1, by_load + 1, bw - 2, bh - 2, 0x3d3d6b);
        display.draw_text(bx + 50, by_load + 28, "Load Scene", 0xccccff);

        display.draw_text(w/2 - 100, h - 30, "Arrow keys / WASD to move  |  Esc to quit", 0x444466);

        if (display.mouse_clicked()) {
            int mx = display.mouse_x();
            int my = display.mouse_y();
            display.clear_mouse();

            if (mx >= bx && mx < bx + bw && my >= by_new && my < by_new + bh)
                return StartAction::NEW_SCENE;
            if (mx >= bx && mx < bx + bw && my >= by_load && my < by_load + bh)
                return StartAction::LOAD_SCENE;
        }

        usleep(10000);
    }
}

static std::string show_load_screen(DisplayWin& display) {
    auto saves = list_saves();
    int w = display.width(), h = display.height();
    const int bw = 320, bh = 36;
    const int bx = (w - bw) / 2;

    while (true) {
        display.process_events();
        if (display.is_closed()) return "";

        int key = display.get_key();
        display.clear_key();
        if (key == XK_Escape) return "";

        display.fill_rect(0, 0, w, h, 0x0d0d1a);
        display.draw_text(w/2 - 48, 80, "Load Scene", 0xffffff);

        int y = 120;
        if (saves.empty()) {
            display.draw_text(bx, y, "No saves found", 0x888888);
            y += 40;
        } else {
            for (size_t i = 0; i < saves.size(); i++) {
                display.fill_rect(bx, y, bw, bh, 0x333355);
                display.fill_rect(bx + 1, y + 1, bw - 2, bh - 2, 0x3d3d6b);
                display.draw_text(bx + 10, y + 24, saves[i].c_str(), 0xccccff);
                y += bh + 6;
            }
        }

        int back_y = y + 12;
        display.fill_rect(bx, back_y, bw, bh, 0x333355);
        display.fill_rect(bx + 1, back_y + 1, bw - 2, bh - 2, 0x3d3d6b);
        display.draw_text(bx + 135, back_y + 24, "Back", 0xccccff);

        if (display.mouse_clicked()) {
            int mx = display.mouse_x();
            int my = display.mouse_y();
            display.clear_mouse();

            y = 120;
            for (size_t i = 0; i < saves.size(); i++) {
                if (mx >= bx && mx < bx + bw && my >= y && my < y + bh)
                    return saves[i];
                y += bh + 6;
            }
            if (mx >= bx && mx < bx + bw && my >= back_y && my < back_y + bh)
                return "";
        }

        usleep(10000);
    }
}

// ── Main ───────────────────────────────────────────────────────────

int main() {
    int nx = 10, ny = 10, nz = 10;
    double cell_size = 0.8;
    Vec3 grid_center(0, 0, 0);
    int image_width = 800;
    int image_height = 600;
    int hq_samples = 4;
    int lq_samples = 1;
    Vec3 light_dir(1, 2, 1);

    DisplayWin display(image_width, image_height, "R3DO - 3D Space");
    const char* SAVE_PATH = "saves/default.r3do";

    Grid grid(nx, ny, nz, cell_size, grid_center);

    // ── Start screen loop (back returns here) ──
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
    render_scene(grid, cam, display, image_width, image_height, lq_samples, light_dir);
    ui.draw();
    display.draw_crosshair(image_width/2, image_height/2, 8, 0x00ff00);

    bool running = true;

    while (running) {
        display.process_events();
        if (display.is_closed()) break;

        if (display.mouse_clicked()) {
            int mx = display.mouse_x();
            int my = display.mouse_y();
            display.clear_mouse();
            if (mx >= 630 && ui.handle_click(mx, my)) {
                save_scene(grid, SAVE_PATH);
                render_scene(grid, cam, display, image_width, image_height, lq_samples, light_dir);
                ui.draw();
                display.draw_crosshair(image_width/2, image_height/2, 8, 0x00ff00);
            }
        }

        if (display.is_mouse_down() && display.mouse_press_x() < 630) {
            int dx = display.mouse_dx();
            int dy = display.mouse_dy();
            if (dx != 0 || dy != 0) {
                display.clear_mouse_delta();
                double sens = 0.005;
                cam.rotate(-dx * sens, dy * sens);
                render_scene(grid, cam, display, image_width, image_height, lq_samples, light_dir);
                ui.draw();
                display.draw_crosshair(image_width/2, image_height/2, 8, 0x00ff00);
            }
        }

        if (display.mouse_released()) {
            display.clear_mouse_released();
            if (display.mouse_press_x() < 630) {
                int dx = display.mouse_x() - display.mouse_press_x();
                int dy = display.mouse_y() - display.mouse_press_y();
                int dist = (int)std::sqrt((double)(dx*dx + dy*dy));
                if (dist < 5) {
                    HitRecord rec;
                    if (hit_center(grid, cam, image_width, image_height, rec)) {
                        int i, j, k;
                        if (grid.world_to_cell(rec.p, i, j, k) && grid.get(i, j, k)) {
                            grid.set(i, j, k, nullptr);
                            save_scene(grid, SAVE_PATH);
                            render_scene(grid, cam, display, image_width, image_height, lq_samples, light_dir);
                            ui.draw();
                            display.draw_crosshair(image_width/2, image_height/2, 8, 0x00ff00);
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
                case XK_space:
                    std::cout << "Full quality..." << std::endl;
                    render_scene(grid, cam, display, image_width, image_height, hq_samples, light_dir);
                    ui.draw();
                    display.draw_crosshair(image_width/2, image_height/2, 8, 0x00ff00);
                    break;
                case XK_Escape: running = false; break;
            }

            if (moved && running) {
                std::cout << cam.pos.x << "," << cam.pos.y << "," << cam.pos.z << " " << std::flush;
                render_scene(grid, cam, display, image_width, image_height, lq_samples, light_dir);
                ui.draw();
                display.draw_crosshair(image_width/2, image_height/2, 8, 0x00ff00);
                std::cout << "done" << std::endl;
            }
        }

        usleep(10000);
    }

    return 0;
}
