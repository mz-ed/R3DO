#include "markers.hpp"
#include "render_modes.hpp"
#include <cmath>
#include <algorithm>

namespace {

unsigned int shade_color(const Vec3& color) {
    int cr = (int)(255.999 * std::sqrt(color.x));
    int cg = (int)(255.999 * std::sqrt(color.y));
    int cb = (int)(255.999 * std::sqrt(color.z));
    return ((unsigned)cr << 16) | ((unsigned)cg << 8) | (unsigned)cb;
}

void fill_circle(DisplayWin& display, int cx, int cy, int r, unsigned int col) {
    if (r < 1) {
        display.fill_rect(cx, cy, 1, 1, col);
        return;
    }
    for (int dy = -r; dy <= r; dy++) {
        int dx = (int)std::sqrt((double)(r * r - dy * dy));
        display.fill_rect(cx - dx, cy + dy, dx * 2 + 1, 1, col);
    }
}

void ring_circle(DisplayWin& display, int cx, int cy, int r, unsigned int col) {
    for (int dy = -r; dy <= r; dy++) {
        int dx = (int)std::sqrt((double)(r * r - dy * dy));
        display.fill_rect(cx - dx, cy + dy, 1, 1, col);
        display.fill_rect(cx + dx, cy + dy, 1, 1, col);
    }
    for (int dx = -r; dx <= r; dx++) {
        int dy = (int)std::sqrt((double)(r * r - dx * dx));
        display.fill_rect(cx + dx, cy - dy, 1, 1, col);
        display.fill_rect(cx + dx, cy + dy, 1, 1, col);
    }
}

void draw_arrow(DisplayWin& display, int cx, int cy, double dx, double dz,
                double len, unsigned int col) {
    // world XZ dir -> screen dir (screen y flips z)
    double sx = dx, sy = -dz;
    double l = std::sqrt(sx * sx + sy * sy);
    if (l < 1e-6) return;
    sx /= l; sy /= l;
    int ex = cx + (int)(sx * len), ey = cy + (int)(sy * len);
    display.draw_line(cx, cy, ex, ey, col);
    double hx1 = sx * 0.866 + sy * 0.5, hy1 = -sx * 0.5 + sy * 0.866;
    double hx2 = sx * 0.866 - sy * 0.5, hy2 = sx * 0.5 + sy * 0.866;
    int head = std::max(4, (int)(len * 0.28));
    display.draw_line(ex, ey, ex - (int)(hx1 * head), ey - (int)(hy1 * head), col);
    display.draw_line(ex, ey, ex - (int)(hx2 * head), ey - (int)(hy2 * head), col);
}

} // namespace

void overhead_draw_markers(DisplayWin& display, const Grid& grid,
                           const OverScreenMap& map) {
    double cs = grid.cell_size;
    double px_per_world = map.base_px_per_world() * map.zoom;

    for (int i = 0; i < grid.nx; i++) {
        for (int k = 0; k < grid.nz; k++) {
            bool has_visible = false;
            Vec3 color;
            for (int j = 0; j < grid.ny; j++) {
                Hittable* obj = grid.get(i, j, k);
                if (obj && obj->is_visible()) {
                    has_visible = true;
                    color = obj->get_color();
                    break;
                }
            }
            if (!has_visible) continue;
            Vec3 c = grid.cell_center(i, 0, k);
            auto p = map.to_screen(c.x, c.z);
            int pw = std::max(1, (int)(cs * px_per_world));
            display.fill_rect(p.first - pw / 2, p.second - pw / 2, pw, pw,
                              shade_color(color));
        }
    }

    for (const auto& f : grid.free_objects()) {
        if (!f || !f->is_visible()) continue;
        Vec3 pos = f->get_center();
        auto p = map.to_screen(pos.x, pos.z);
        unsigned int col = shade_color(f->get_color());
        int r = std::max(2, (int)(f->bound_radius_xz() * px_per_world));
        fill_circle(display, p.first, p.second, r, col);
        double yaw = f->anim_rot().y;
        if (std::abs(yaw) > 0.01 && r >= 3)
            draw_arrow(display, p.first, p.second, std::sin(yaw), std::cos(yaw),
                       r + 5.0, 0xffffff);
        if (g_sel_object == f.get() && r >= 2)
            ring_circle(display, p.first, p.second, r + 2, 0xffe066);
    }
}
