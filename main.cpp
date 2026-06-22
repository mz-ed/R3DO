#include "v3.hpp"
#include "ray.hpp"
#include "hittable.hpp"
#include "sphere.hpp"
#include "box.hpp"
#include "grid.hpp"
#include "camera.hpp"
#include "display.hpp"
#include <iostream>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <unistd.h>

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

// --- UI ---
#define MENU_X 630
#define MENU_W 170

struct Button {
    int x, y, w, h;
    const char* text;
};

int count_objects(const Grid& grid) {
    int count = 0;
    for (int i = 0; i < grid.nx; i++)
        for (int j = 0; j < grid.ny; j++)
            for (int k = 0; k < grid.nz; k++)
                if (grid.get(i, j, k)) count++;
    return count;
}

Vec3 palette_color(int index) {
    Vec3 palette[] = {
        Vec3(1, 0.2, 0.2), Vec3(0.2, 1, 0.3), Vec3(1, 1, 0.2),
        Vec3(1, 0.5, 0), Vec3(0.2, 0.4, 1), Vec3(0.9, 0.2, 0.9),
        Vec3(0.2, 0.9, 0.9), Vec3(0.8, 0.6, 0.2), Vec3(0.5, 0.3, 1),
        Vec3(1, 0.6, 0.6)
    };
    return palette[index % 10];
}

bool try_place(Grid& grid, const Camera& cam, const Vec3& color, bool is_sphere) {
    Vec3 target = cam.pos + cam.forward() * 3.0;
    int i, j, k;
    if (!grid.world_to_cell(target, i, j, k)) {
        std::cerr << "FAIL: target (" << target.x << "," << target.y << "," << target.z << ") outside grid" << std::endl;
        return false;
    }
    if (grid.get(i, j, k)) {
        std::cerr << "FAIL: cell (" << i << "," << j << "," << k << ") occupied" << std::endl;
        return false;
    }

    double cs = grid.cell_size;
    Vec3 c = grid.cell_center(i, j, k);
    if (is_sphere) {
        grid.set(i, j, k, new Sphere(c, cs * 0.45, color));
        std::cerr << "Sphere at cell (" << i << "," << j << "," << k << ") pos ("
                  << c.x << "," << c.y << "," << c.z << ")" << std::endl;
    } else {
        Vec3 half(cs * 0.38, cs * 0.38, cs * 0.38);
        grid.set(i, j, k, new Box(c - half, c + half, color));
        std::cerr << "Box at cell (" << i << "," << j << "," << k << ")" << std::endl;
    }
    return true;
}

void clear_grid(Grid& grid) {
    for (int i = 0; i < grid.nx; i++)
        for (int j = 0; j < grid.ny; j++)
            for (int k = 0; k < grid.nz; k++)
                grid.set(i, j, k, nullptr);
}

void draw_ui(DisplayWin& display, const Grid& grid, int obj_count, const Vec3& cam_pos) {
    display.fill_rect(MENU_X, 0, MENU_W, display.height(), 0x1a1a2e);
    display.fill_rect(MENU_X - 2, 0, 2, display.height(), 0x444466);

    display.draw_text(MENU_X + 10, 22, "R3DO", 0xffffff);
    display.fill_rect(MENU_X + 10, 28, 40, 1, 0x666688);

    Button buttons[] = {
        {MENU_X + 10, 45, 150, 28, "Add Sphere"},
        {MENU_X + 10, 80, 150, 28, "Add Box"},
        {MENU_X + 10, 115, 150, 28, "Clear All"},
    };

    for (auto& btn : buttons) {
        display.fill_rect(btn.x, btn.y, btn.w, btn.h, 0x333355);
        display.fill_rect(btn.x + 1, btn.y + 1, btn.w - 2, btn.h - 2, 0x3d3d6b);
        display.draw_text(btn.x + 10, btn.y + 19, btn.text, 0xccccff);
    }

    char buf[64];
    snprintf(buf, sizeof(buf), "Objects: %d", obj_count);
    display.draw_text(MENU_X + 10, 170, buf, 0xaaaaaa);

    snprintf(buf, sizeof(buf), "Cam: %.1f %.1f %.1f", cam_pos.x, cam_pos.y, cam_pos.z);
    display.draw_text(MENU_X + 10, 190, buf, 0x666688);

    display.draw_text(MENU_X + 10, 220, "Controls:", 0x888888);
    display.draw_text(MENU_X + 10, 237, "WASD move", 0x666688);
    display.draw_text(MENU_X + 10, 252, "Arrows look", 0x666688);
    display.draw_text(MENU_X + 10, 267, "Q/E up/down", 0x666688);
    display.draw_text(MENU_X + 10, 282, "Space: HQ", 0x666688);
    display.draw_text(MENU_X + 10, 297, "Esc: quit", 0x666688);
}

int main() {
    int nx = 10, ny = 10, nz = 10;
    double cell_size = 0.8;
    Vec3 grid_center(0, 0, 0);

    Grid grid(nx, ny, nz, cell_size, grid_center);

    grid.set(0, 0, 0, new Sphere(grid.cell_center(0, 0, 0), cell_size * 0.45, Vec3(1, 0.2, 0.2)));
    grid.set(9, 9, 9, new Sphere(grid.cell_center(9, 9, 9), cell_size * 0.45, Vec3(0.2, 1, 0.3)));
    grid.set(0, 9, 0, new Sphere(grid.cell_center(0, 9, 0), cell_size * 0.45, Vec3(1, 1, 0.2)));
    grid.set(9, 0, 9, new Sphere(grid.cell_center(9, 0, 9), cell_size * 0.45, Vec3(1, 0.5, 0)));
    grid.set(4, 4, 4, new Sphere(grid.cell_center(4, 4, 4), cell_size * 0.45, Vec3(0.2, 0.4, 1)));
    grid.set(5, 5, 5, new Sphere(grid.cell_center(5, 5, 5), cell_size * 0.45, Vec3(0.9, 0.2, 0.9)));
    grid.set(2, 7, 3, new Sphere(grid.cell_center(2, 7, 3), cell_size * 0.45, Vec3(0.2, 0.9, 0.9)));
    grid.set(7, 2, 6, new Sphere(grid.cell_center(7, 2, 6), cell_size * 0.45, Vec3(0.8, 0.6, 0.2)));
    grid.set(1, 1, 8, new Sphere(grid.cell_center(1, 1, 8), cell_size * 0.45, Vec3(0.5, 0.3, 1)));
    grid.set(8, 8, 1, new Sphere(grid.cell_center(8, 8, 1), cell_size * 0.45, Vec3(1, 0.6, 0.6)));

    Vec3 box_half = Vec3(cell_size * 0.38, cell_size * 0.38, cell_size * 0.38);
    grid.set(2, 2, 2, new Box(grid.cell_center(2, 2, 2) - box_half, grid.cell_center(2, 2, 2) + box_half, Vec3(0.1, 0.8, 0.3)));
    grid.set(7, 7, 7, new Box(grid.cell_center(7, 7, 7) - box_half, grid.cell_center(7, 7, 7) + box_half, Vec3(0.8, 0.2, 0.3)));
    grid.set(3, 8, 2, new Box(grid.cell_center(3, 8, 2) - box_half, grid.cell_center(3, 8, 2) + box_half, Vec3(0.3, 0.5, 1)));

    int image_width = 800;
    int image_height = 600;
    int hq_samples = 4;
    int lq_samples = 1;
    Vec3 light_dir(1, 2, 1);

    DisplayWin display(image_width, image_height, "R3DO - 3D Space");
    Camera cam(Vec3(3.0, 1.5, 4.0), 0, -0.15);
    int palette_idx = 0;

    std::cout << "Initial render..." << std::endl;
    render_scene(grid, cam, display, image_width, image_height, lq_samples, light_dir);
    draw_ui(display, grid, count_objects(grid), cam.pos);

    bool running = true;

    while (running) {
        display.process_events();
        if (display.is_closed()) break;

        if (display.mouse_clicked()) {
            int mx = display.mouse_x();
            int my = display.mouse_y();
            display.clear_mouse();
            std::cerr << "CLICK at (" << mx << "," << my << ")" << std::endl;

            if (mx >= MENU_X && mx < MENU_X + MENU_W) {
                bool need_render = false;

                if (my >= 45 && my < 45 + 28) {
                    std::cerr << "Button: Add Sphere" << std::endl;
                    if (try_place(grid, cam, palette_color(palette_idx), true)) {
                        palette_idx++;
                        need_render = true;
                    }
                } else if (my >= 80 && my < 80 + 28) {
                    std::cerr << "Button: Add Box" << std::endl;
                    if (try_place(grid, cam, palette_color(palette_idx), false)) {
                        palette_idx++;
                        need_render = true;
                    }
                } else if (my >= 115 && my < 115 + 28) {
                    std::cerr << "Button: Clear All" << std::endl;
                    clear_grid(grid);
                    need_render = true;
                } else {
                    std::cerr << "Menu click at y=" << my << " (no button)" << std::endl;
                }

                if (need_render) {
                    render_scene(grid, cam, display, image_width, image_height, lq_samples, light_dir);
                    draw_ui(display, grid, count_objects(grid), cam.pos);
                }
            } else {
                std::cerr << "Click outside menu (x=" << mx << ")" << std::endl;
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
                    draw_ui(display, grid, count_objects(grid), cam.pos);
                    break;
                case XK_Escape: running = false; break;
            }

            if (moved && running) {
                std::cout << cam.pos.x << "," << cam.pos.y << "," << cam.pos.z << " " << std::flush;
                render_scene(grid, cam, display, image_width, image_height, lq_samples, light_dir);
                draw_ui(display, grid, count_objects(grid), cam.pos);
                std::cout << "done" << std::endl;
            }
        }

        usleep(10000);
    }

    return 0;
}
