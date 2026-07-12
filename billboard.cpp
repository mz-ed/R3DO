#include "billboard.hpp"
#include <algorithm>
#include <cmath>
#include <vector>
#include <cstring>

void render_billboard(Grid& grid, Camera& cam, DisplayWin& display, const Vec3& light_dir) {
    int w = display.width(), h = display.height();
    double vh = 2.5;
    double vw = vh * w / h;
    double fl = 2.0;

    Vec3 fwd = cam.forward();
    Vec3 rgt = cam.right();
    Vec3 up = cross(rgt, fwd);

    display.fill_rect(0, 0, w, h, 0x0d0d1a);

    struct Sprite {
        int sx, sy, rad;
        Vec3 color;
        double depth;
        char shape; // 's'=sphere 'b'=box 'c'=cylinder 'o'=cone
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

                const char* tn = obj->type_name();
                sprites.push_back({sx, sy, rad, obj->get_color(), depth, tn[0]});
            }
        }
    }

    std::sort(sprites.begin(), sprites.end(),
        [](const Sprite& a, const Sprite& b) { return a.depth > b.depth; });

    auto shade = [&](const Vec3& color) -> unsigned long {
        double ambient = 0.3;
        Vec3 n = unit_vector(Vec3(0, 1, 0));
        double diff = std::max(0.0, dot(light, n));
        double intensity = ambient + (1.0 - ambient) * diff;
        int cr = int(255.999 * std::sqrt(color.x * intensity));
        int cg = int(255.999 * std::sqrt(color.y * intensity));
        int cb = int(255.999 * std::sqrt(color.z * intensity));
        return (cr << 16) | (cg << 8) | cb;
    };

    for (auto& s : sprites) {
        unsigned long col = shade(s.color);
        int cr = (col >> 16) & 0xff;
        int cg = (col >> 8) & 0xff;
        int cb = col & 0xff;

        switch (s.shape) {
        case 's': { // sphere → circle
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
            break;
        }
        case 'b': { // box → filled square
            for (int dy = -s.rad; dy <= s.rad; dy++) {
                int py = s.sy + dy;
                if (py < 0 || py >= h) continue;
                for (int dx = -s.rad; dx <= s.rad; dx++) {
                    int px = s.sx + dx;
                    if (px >= 0 && px < w)
                        display.set_pixel(px, py, cr, cg, cb);
                }
            }
            break;
        }
        case 'c': { // cylinder → filled diamond
            int r = s.rad;
            for (int dy = -r; dy <= r; dy++) {
                int py = s.sy + dy;
                if (py < 0 || py >= h) continue;
                int hw = r - std::abs(dy);
                for (int dx = -hw; dx <= hw; dx++) {
                    int px = s.sx + dx;
                    if (px >= 0 && px < w)
                        display.set_pixel(px, py, cr, cg, cb);
                }
            }
            break;
        }
        case 'o': { // cone → filled triangle pointing up
            int r = s.rad;
            int hh = s.rad;
            for (int dy = 0; dy <= hh; dy++) {
                int py = s.sy - hh + dy;
                if (py < 0 || py >= h) continue;
                int hw = r - (r * dy) / hh;
                for (int dx = -hw; dx <= hw; dx++) {
                    int px = s.sx + dx;
                    if (px >= 0 && px < w)
                        display.set_pixel(px, py, cr, cg, cb);
                }
            }
            break;
        }
        case 'm': { // mesh → small hexagon
            int r = s.rad;
            for (int dy = -r; dy <= r; dy++) {
                int py = s.sy + dy;
                if (py < 0 || py >= h) continue;
                int hw = r;
                if (dy < -r/2) hw = r - (r - (-dy - r/2));
                else if (dy > r/2) hw = r - (r - (dy - r/2));
                for (int dx = -hw; dx <= hw; dx++) {
                    int px = s.sx + dx;
                    if (px >= 0 && px < w)
                        display.set_pixel(px, py, cr, cg, cb);
                }
            }
            break;
        }
        }
    }

    display.update();
}
