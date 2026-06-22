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
    int lq_width = image_width, lq_height = image_height;
    Vec3 light_dir(1, 2, 1);

    DisplayWin display(image_width, image_height, "R3DO - 3D Space");
    Camera cam(Vec3(3.0, 1.5, 4.0), 0, -0.15);

    std::cout << "Initial render..." << std::endl;
    render_scene(grid, cam, display, lq_width, lq_height, lq_samples, light_dir);
    std::cout << "WASD=move  Arrows=look  Q/E=up/down  Space=HQ  Esc=quit\n";

    bool running = true;

    while (running) {
        display.process_events();
        if (display.is_closed()) break;

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
                    break;
                case XK_Escape: running = false; break;
            }

            if (moved && running) {
                std::cout << cam.pos.x << "," << cam.pos.y << "," << cam.pos.z << " " << std::flush;
                render_scene(grid, cam, display, lq_width, lq_height, lq_samples, light_dir);
                std::cout << "done" << std::endl;
            }
        }

        usleep(10000);
    }

    return 0;
}
