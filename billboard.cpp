#include "billboard.hpp"
#include <algorithm>
#include <cmath>
#include <vector>

void render_billboard(Grid& grid, Camera& cam, DisplayWin& display, const Vec3& light_dir) {
    int w = display.width(), h = display.height();
    double vh = 2.5;
    double vw = vh * w / h;
    double fl = 2.0;

    Vec3 fwd = cam.forward();
    Vec3 rgt = cam.right();
    Vec3 up = cross(rgt, fwd);

    display.clear_buffer();
    display.fill_rect(0, 0, w, h, 0x0d0d1a);

    struct Sprite {
        int sx, sy, rad;
        Vec3 color;
        double depth;
    };
    std::vector<Sprite> sprites;

    Vec3 light = unit_vector(light_dir);

    for (int i = 0; i < grid.nx; i++) {
        for (int j = 0; j < grid.ny; j++) {
            for (int k = 0; k < grid.nz; k++) {
                Hittable* obj = grid.get(i, j, k);
                if (!obj || !obj->is_visible()) continue;

                Vec3 pos = grid.cell_center(i, j, k);
                Vec3 rel = pos - cam.pos;
                double depth = dot(rel, fwd);
                if (depth <= 0) continue;

                double rx = dot(rel, rgt);
                double ry = dot(rel, up);

                int sx = (int)(w/2.0 + (rx / depth) * fl * w / vw + 0.5);
                int sy = (int)(h/2.0 - (ry / depth) * fl * h / vh + 0.5);

                double r = grid.cell_size * 0.45;
                int rad = (int)(r * fl * w / (vw * depth) + 1);
                if (rad < 1) rad = 1;

                sprites.push_back({sx, sy, rad, obj->get_color(), depth});
            }
        }
    }

    std::sort(sprites.begin(), sprites.end(),
        [](const Sprite& a, const Sprite& b) { return a.depth > b.depth; });

    for (auto& s : sprites) {
        double ambient = 0.3;
        Vec3 n = unit_vector(Vec3(0, 1, 0));
        double diff = std::max(0.0, dot(light, n));
        double intensity = ambient + (1.0 - ambient) * diff;
        int cr = int(255.999 * std::sqrt(s.color.x * intensity));
        int cg = int(255.999 * std::sqrt(s.color.y * intensity));
        int cb = int(255.999 * std::sqrt(s.color.z * intensity));
        unsigned long col = (cr << 16) | (cg << 8) | cb;
        int r2 = s.rad * s.rad;
        for (int dy = -s.rad; dy <= s.rad; dy++) {
            int py = s.sy + dy;
            if (py < 0 || py >= h) continue;
            int dx_max = (int)std::sqrt((double)(r2 - dy * dy));
            for (int dx = -dx_max; dx <= dx_max; dx++) {
                int px = s.sx + dx;
                if (px >= 0 && px < w)
                    display.set_pixel(px, py, cr, cg, cb);
            }
        }
    }

    display.update();
}
