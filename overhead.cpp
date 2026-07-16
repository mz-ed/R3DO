#include "overhead.hpp"
#include <cmath>
#include <algorithm>

void render_overhead(Grid& grid, Camera& cam, DisplayWin& display) {
    int w = display.width(), h = display.height();
    display.fill_rect(0, 0, w, h, 0x0d0d1a);

    int pad = 60;
    int map_w = w - pad * 2 - 170;
    int map_h = h - pad * 2;
    int map_x = pad;
    int map_y = pad;

    double cs = grid.cell_size;
    double cell_px_x = (double)map_w / (grid.nx * cs);
    double cell_px_y = (double)map_h / (grid.nz * cs);
    double cell_px = std::min(cell_px_x, cell_px_y);
    if (cell_px < 1) cell_px = 1;

    double total_w = grid.nx * cs * cell_px;
    double total_h = grid.nz * cs * cell_px;
    int ox = map_x + (int)((map_w - total_w) / 2);
    int oy = map_y + (int)((map_h - total_h) / 2);

    Vec3 gmin, gmax;
    grid.grid_bounds(gmin, gmax);

    auto to_screen = [&](double wx, double wz) -> int* {
        static int p[2];
        double fx = (wx - gmin.x) / (gmax.x - gmin.x);
        double fz = (wz - gmin.z) / (gmax.z - gmin.z);
        p[0] = ox + (int)(fx * total_w);
        p[1] = oy + (int)((1.0 - fz) * total_h);
        return p;
    };

    for (int i = 0; i <= grid.nx; i++) {
        int* p = to_screen(gmin.x + i * cs, gmin.z);
        display.fill_rect(p[0], oy, 1, (int)total_h, 0x222244);
    }
    for (int k = 0; k <= grid.nz; k++) {
        int* p = to_screen(gmin.x, gmin.z + k * cs);
        display.fill_rect(ox, p[1], (int)total_w, 1, 0x222244);
    }

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
            int* p = to_screen(c.x, c.z);
            int px = p[0] - (int)(cell_px * cs / 2);
            int py = p[1] - (int)(cell_px * cs / 2);
            int pw = (int)(cell_px * cs);
            int ph = (int)(cell_px * cs);
            int cr = int(255.999 * std::sqrt(color.x));
            int cg = int(255.999 * std::sqrt(color.y));
            int cb = int(255.999 * std::sqrt(color.z));
            unsigned long col = (cr << 16) | (cg << 8) | cb;
            display.fill_rect(px, py, pw, ph, col);
        }
    }

    // Free objects (meshes) → colored diamond markers
    for (auto* f : grid.free_objects()) {
        if (!f || !f->is_visible()) continue;
        Vec3 pos = f->get_center();
        int* p = to_screen(pos.x, pos.z);
        Vec3 col = f->get_color();
        int cr = int(255.999 * std::sqrt(col.x));
        int cg = int(255.999 * std::sqrt(col.y));
        int cb = int(255.999 * std::sqrt(col.z));
        unsigned long col24 = (cr << 16) | (cg << 8) | cb;
        int ms = 6;
        for (int dy = -ms; dy <= ms; dy++) {
            int hw = ms - std::abs(dy);
            for (int dx = -hw; dx <= hw; dx++)
                display.set_pixel(p[0] + dx, p[1] + dy, cr, cg, cb);
        }
    }

    int* cp = to_screen(cam.pos.x, cam.pos.z);
    display.fill_rect(cp[0] - 3, cp[1] - 3, 7, 7, 0xffffff);

    Vec3 fwd = cam.forward();
    int* fp = to_screen(cam.pos.x + fwd.x * cs * 2, cam.pos.z + fwd.z * cs * 2);
    display.fill_rect(fp[0] - 1, fp[1] - 1, 3, 3, 0xff4444);

    display.update();
}
