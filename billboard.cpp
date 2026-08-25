#include "billboard.hpp"
#include "render.hpp"
#include <algorithm>
#include <cmath>
#include <vector>

void render_billboard(Grid& grid, Camera& cam, DisplayWin& display, const Vec3& light_dir) {
    int w = display.width(), h = display.height();
    double vw = VIEWPORT_HEIGHT * w / h;

    Vec3 fwd = cam.forward();
    Vec3 rgt = cam.right();
    Vec3 up = cross(rgt, fwd);

    display.fill_rect(0, 0, w, h, 0x0d0d1a);

    struct Sprite {
        int sx, sy, rad;
        Vec3 color;
        double depth;
        char shape; // 's'=sphere 'b'=box 'c'=cylinder 'o'=cone 'm'=mesh
    };
    std::vector<Sprite> sprites;

    auto project = [&](Hittable* obj) {
        Vec3 pos = obj->get_center();
        Vec3 rel = pos - cam.pos;
        double depth = dot(rel, fwd);
        if (depth <= 0) return;

        double rx = dot(rel, rgt);
        double ry = dot(rel, up);

        int sx = (int)(w/2.0 + (rx / depth) * FOCAL_LENGTH * w / vw + 0.5);
        int sy = (int)(h/2.0 - (ry / depth) * FOCAL_LENGTH * h / VIEWPORT_HEIGHT + 0.5);

        double r = grid.cell_size * 0.45;
        int rad = (int)(r * FOCAL_LENGTH * w / (vw * depth) + 1);
        if (rad < 1) rad = 1;

        sprites.push_back({sx, sy, rad, obj->get_color(), depth, obj->type_name()[0]});
    };

    for (const auto& cell : grid.cells)
        if (cell && cell->is_visible())
            project(cell.get());

    for (const auto& f : grid.free_objects())
        if (f && f->is_visible())
            project(f.get());

    std::sort(sprites.begin(), sprites.end(),
        [](const Sprite& a, const Sprite& b) { return a.depth > b.depth; });

    Vec3 light = unit_vector(light_dir);
    double ambient = 0.3;
    Vec3 n(0, 1, 0);
    double intensity = ambient + (1.0 - ambient) * std::max(0.0, dot(light, n));

    auto rgb = [&](const Vec3& color) -> unsigned int {
        int cr = int(255.999 * std::sqrt(color.x * intensity));
        int cg = int(255.999 * std::sqrt(color.y * intensity));
        int cb = int(255.999 * std::sqrt(color.z * intensity));
        return ((unsigned)cr << 16) | ((unsigned)cg << 8) | (unsigned)cb;
    };

    for (auto& s : sprites) {
        unsigned int col = rgb(s.color);

        switch (s.shape) {
        case 's': { // sphere → circle
            int r2 = s.rad * s.rad;
            for (int dy = -s.rad; dy <= s.rad; dy++) {
                int py = s.sy + dy;
                if (py < 0 || py >= h) continue;
                int dx_max = (int)std::sqrt((double)(r2 - dy * dy));
                for (int dx = -dx_max; dx <= dx_max; dx++)
                    display.set_pixel(s.sx + dx, py, col);
            }
            break;
        }
        case 'b': { // box → filled square
            for (int dy = -s.rad; dy <= s.rad; dy++) {
                int py = s.sy + dy;
                if (py < 0 || py >= h) continue;
                for (int dx = -s.rad; dx <= s.rad; dx++)
                    display.set_pixel(s.sx + dx, py, col);
            }
            break;
        }
        case 'c': { // cylinder → filled diamond
            int r = s.rad;
            for (int dy = -r; dy <= r; dy++) {
                int py = s.sy + dy;
                if (py < 0 || py >= h) continue;
                int hw = r - std::abs(dy);
                for (int dx = -hw; dx <= hw; dx++)
                    display.set_pixel(s.sx + dx, py, col);
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
                for (int dx = -hw; dx <= hw; dx++)
                    display.set_pixel(s.sx + dx, py, col);
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
                for (int dx = -hw; dx <= hw; dx++)
                    display.set_pixel(s.sx + dx, py, col);
            }
            break;
        }
        }
    }

    display.update();
}
