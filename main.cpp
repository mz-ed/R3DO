#include "v3.hpp"
#include "ray.hpp"
#include "hittable.hpp"
#include "sphere.hpp"
#include "box.hpp"
#include "grid.hpp"
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

struct Camera {
    Vec3 pos;
    double yaw;
    double pitch;

    Camera(Vec3 p, double y, double pi) : pos(p), yaw(y), pitch(pi) {}

    Vec3 forward() const {
        return Vec3(-sin(yaw) * cos(pitch), sin(pitch), -cos(yaw) * cos(pitch));
    }

    Vec3 right() const {
        return Vec3(cos(yaw), 0, -sin(yaw));
    }

    void move_fwd(double d) { pos = pos + forward() * d; }
    void move_right(double d) { pos = pos + right() * d; }
    void move_up(double d) { pos.y += d; }
    void rotate(double dyaw, double dpitch) {
        yaw += dyaw;
        pitch += dpitch;
        if (pitch > 1.5) pitch = 1.5;
        if (pitch < -1.5) pitch = -1.5;
    }
};

void render_scene(const Grid& grid, const Camera& cam, DisplayWin& display,
                  int image_width, int image_height, int samples,
                  const Vec3& light_dir) {
    double viewport_height = 3.0;
    double viewport_width = viewport_height * image_width / image_height;
    double focal_length = 1.5;

    Vec3 fwd = cam.forward();
    Vec3 rgt = cam.right();
    Vec3 up = cross(rgt, fwd);

    Vec3 horizontal = viewport_width * rgt;
    Vec3 vertical = viewport_height * up;
    Vec3 lower_left = cam.pos - horizontal/2 - vertical/2 - fwd * focal_length;

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
    int nx = 6, ny = 6, nz = 6;
    double cell_size = 0.4;
    Vec3 grid_center(0, 0, 0);

    Grid grid(nx, ny, nz, cell_size, grid_center);

    grid.set(2, 2, 2, new Sphere(grid.cell_center(2, 2, 2), cell_size * 0.35, Vec3(1, 0.2, 0.2)));
    grid.set(0, 0, 0, new Sphere(grid.cell_center(0, 0, 0), cell_size * 0.3, Vec3(0.2, 0.4, 1)));
    grid.set(5, 5, 5, new Sphere(grid.cell_center(5, 5, 5), cell_size * 0.3, Vec3(0.2, 1, 0.3)));
    grid.set(0, 5, 0, new Sphere(grid.cell_center(0, 5, 0), cell_size * 0.3, Vec3(1, 1, 0.2)));
    grid.set(5, 0, 5, new Sphere(grid.cell_center(5, 0, 5), cell_size * 0.3, Vec3(1, 0.5, 0)));
    grid.set(3, 1, 4, new Sphere(grid.cell_center(3, 1, 4), cell_size * 0.25, Vec3(0.8, 0.3, 0.8)));
    grid.set(1, 4, 2, new Sphere(grid.cell_center(1, 4, 2), cell_size * 0.25, Vec3(0.2, 0.9, 0.9)));

    int image_width = 800;
    int image_height = 600;
    int hq_samples = 4;
    int lq_samples = 1;
    int lq_width = image_width, lq_height = image_height;
    Vec3 light_dir(1, 2, 1);

    DisplayWin display(image_width, image_height, "R3DO - 3D Space");
    Camera cam(Vec3(2.5, 2.0, 4.5), 0, -0.15);

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
            double move_speed = 0.2;
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
