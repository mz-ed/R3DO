#include "overhead.hpp"
#include "ui.hpp"
#include <cmath>
#include <algorithm>

namespace {

struct ScreenPt { int x, y; };

} // namespace

void render_overhead(Grid& grid, Camera& cam, DisplayWin& display) {
    int w = display.width(), h = display.height();
    display.fill_rect(0, 0, w, h, 0x0d0d1a);

    int pad = 60;
    int map_w = w - pad * 2 - UI::SIDEBAR_W;
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

    auto to_screen = [&](double wx, double wz) -> ScreenPt {
        double fx = (wx - gmin.x) / (gmax.x - gmin.x);
        double fz = (wz - gmin.z) / (gmax.z - gmin.z);
        return {ox + (int)(fx * total_w), oy + (int)((1.0 - fz) * total_h)};
    };

    for (int i = 0; i <= grid.nx; i++) {
        ScreenPt p = to_screen(gmin.x + i * cs, gmin.z);
        display.fill_rect(p.x, oy, 1, (int)total_h, 0x222244);
    }
    for (int k = 0; k <= grid.nz; k++) {
        ScreenPt p = to_screen(gmin.x, gmin.z + k * cs);
        display.fill_rect(ox, p.y, (int)total_w, 1, 0x222244);
    }

    // Heightfield terrain -> height/slope colored map under the grid.
    if (auto* hf = dynamic_cast<HeightField*>(grid.terrain())) {
        double hf_step = hf->side() / hf->hf_res();
        for (int k = 0; k < hf->hf_res(); k++) {
            for (int i = 0; i < hf->hf_res(); i++) {
                double wx = hf->origin_x() + (i + 0.5) * hf_step;
                double wz = hf->origin_z() + (k + 0.5) * hf_step;
                if (wx < gmin.x - hf_step || wx > gmax.x + hf_step ||
                    wz < gmin.z - hf_step || wz > gmax.z + hf_step)
                    continue;
                double hy = 0.0;
                double hx = 0.0, hz = 0.0;
                hf->column_sample(i, k, hy, hx, hz);
                // slight shade by relative height, water-flat for low
                double tn = hy / hf->max_height();
                unsigned int col;
                if (tn < 0.10) col = 0x2b4a72;
                else if (tn < 0.45) col = 0x3a7a38;
                else if (tn < 0.80) col = 0x8a8f78;
                else col = 0xd8dde8;
                ScreenPt p = to_screen(wx, wz);
                int pw = std::max(2, (int)std::ceil(hf_step * cell_px));
                display.fill_rect(p.x - pw / 2, p.y - pw / 2, pw, pw, col);
            }
        }
    }

    auto shade = [&](const Vec3& color) -> unsigned int {
        int cr = int(255.999 * std::sqrt(color.x));
        int cg = int(255.999 * std::sqrt(color.y));
        int cb = int(255.999 * std::sqrt(color.z));
        return ((unsigned)cr << 16) | ((unsigned)cg << 8) | (unsigned)cb;
    };

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
            ScreenPt p = to_screen(c.x, c.z);
            int pw = std::max(1, (int)(cell_px * cs));
            display.fill_rect(p.x - pw/2, p.y - pw/2, pw, pw, shade(color));
        }
    }

    // Free objects (meshes) → colored diamond markers
    for (const auto& f : grid.free_objects()) {
        if (!f || !f->is_visible()) continue;
        Vec3 pos = f->get_center();
        ScreenPt p = to_screen(pos.x, pos.z);
        Vec3 col = f->get_color();
        unsigned int col24 = shade(col);
        int cr = (col24 >> 16) & 0xff, cg = (col24 >> 8) & 0xff, cb = col24 & 0xff;
        int ms = 6;
        for (int dy = -ms; dy <= ms; dy++) {
            int hw = ms - std::abs(dy);
            for (int dx = -hw; dx <= hw; dx++)
                display.set_pixel(p.x + dx, p.y + dy,
                                  ((unsigned)cr << 16) | ((unsigned)cg << 8) | (unsigned)cb);
        }
    }

    ScreenPt cp = to_screen(cam.pos.x, cam.pos.z);
    display.fill_rect(cp.x - 3, cp.y - 3, 7, 7, 0xffffff);

    Vec3 fwd = cam.forward();
    ScreenPt fp = to_screen(cam.pos.x + fwd.x * cs * 2, cam.pos.z + fwd.z * cs * 2);
    display.fill_rect(fp.x - 1, fp.y - 1, 3, 3, 0xff4444);

    display.update();
}
