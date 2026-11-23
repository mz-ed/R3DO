#include "overhead.hpp"
#include "overhead/view.hpp"
#include "overhead/terrain.hpp"
#include "overhead/markers.hpp"
#include <cmath>

void render_overhead(Grid& grid, Camera& cam, DisplayWin& display) {
    int w = display.width(), h = display.height();
    display.fill_rect(0, 0, w, h, 0x0d0d1a);

    OverScreenMap map = overhead_build_map(display, grid);
    double cs = grid.cell_size;

    // grid lines, projected with the view (zoom/pan included)
    Vec3 gmin, gmax;
    grid.grid_bounds(gmin, gmax);
    for (int i = 0; i <= grid.nx; i++) {
        double wx = gmin.x + i * cs;
        auto top = map.to_screen(wx, gmin.z);
        auto bot = map.to_screen(wx, gmax.z);
        if (top.first >= 0 && top.first < w) {
            int y0 = std::max(0, std::min(top.second, bot.second));
            int y1 = std::min(h, std::max(top.second, bot.second));
            display.fill_rect(top.first, y0, 1, y1 - y0, 0x222244);
        }
    }
    for (int k = 0; k <= grid.nz; k++) {
        double wz = gmin.z + k * cs;
        auto left = map.to_screen(gmin.x, wz);
        auto right = map.to_screen(gmax.x, wz);
        if (left.second >= 0 && left.second < h) {
            int x0 = std::max(0, std::min(left.first, right.first));
            int x1 = std::min(w, std::max(left.first, right.first));
            display.fill_rect(x0, left.second, x1 - x0, 1, 0x222244);
        }
    }

    overhead_draw_terrain(display, grid, map);
    overhead_draw_markers(display, grid, map);

    // camera position + facing marker
    auto cp = map.to_screen(cam.pos.x, cam.pos.z);
    display.fill_rect(cp.first - 3, cp.second - 3, 7, 7, 0xffffff);
    Vec3 fwd = cam.forward();
    auto fp = map.to_screen(cam.pos.x + fwd.x * cs * 2, cam.pos.z + fwd.z * cs * 2);
    display.fill_rect(fp.first - 1, fp.second - 1, 3, 3, 0xff4444);

    display.update();
}
