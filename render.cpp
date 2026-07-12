#include "render.hpp"
#include <iostream>
#include <cmath>

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

    // Ground plane at bottom of grid
    Vec3 gmin, gmax;
    grid.grid_bounds(gmin, gmax);
    double ground_y = gmin.y;
    const Vec3& ro = r.origin();
    const Vec3& rd = r.direction();
    if (rd.y < -1e-10) {
        double t = (ground_y - ro.y) / rd.y;
        if (t > 0.001 && t < 1000.0) {
            Vec3 p = r.at(t);
            int cx = (int)std::floor(p.x / 0.8);
            int cz = (int)std::floor(p.z / 0.8);
            bool light_sq = ((cx + cz) & 1) == 0;
            double ambient = 0.3;
            Vec3 n(0, 1, 0);
            double diff = std::max(0.0, dot(light, n));
            double intensity = ambient + (1.0 - ambient) * diff;
            Vec3 col = light_sq ? Vec3(0.25, 0.25, 0.3) : Vec3(0.15, 0.15, 0.2);
            return col * intensity;
        }
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
