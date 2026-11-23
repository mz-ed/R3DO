#ifndef OVERHEAD_VIEW_H
#define OVERHEAD_VIEW_H

#include "grid.hpp"
#include "display.hpp"
#include "v3.hpp"
#include <utility>

// Screen-space projection of the world XZ plane used by the Overhead map.
// Built per call from the window size, the grid, and the session view state
// (zoom + pan); every overhead sub-renderer projects through this struct so
// zoom/pan/pick/brush all share one transform.
struct OverScreenMap {
    int ox = 0, oy = 0;      // pixel origin of the zoom=1 fitted map
    double bw = 0, bh = 0;   // base (zoom=1) map size in pixels
    double zoom = 1.0;       // 1 = fit, up to 8
    double pan_x = 0, pan_y = 0;
    Vec3 gmin, gmax;

    std::pair<int, int> to_screen(double wx, double wz) const {
        double fx = (wx - gmin.x) / (gmax.x - gmin.x);
        double fz = (wz - gmin.z) / (gmax.z - gmin.z);
        double sx = ox + fx * bw * zoom + pan_x;
        double sy = oy + (1.0 - fz) * bh * zoom + pan_y;
        return {(int)sx, (int)sy};
    }
    Vec3 to_world(int px, int py) const {
        double bx = ((double)px - ox - pan_x) / zoom;
        double by = ((double)py - oy - pan_y) / zoom;
        double fx = bx / bw;
        double fz = 1.0 - by / bh;
        return Vec3(gmin.x + fx * (gmax.x - gmin.x), 0.0,
                    gmin.z + fz * (gmax.z - gmin.z));
    }
    double base_px_per_world() const {
        return bw / ((gmax.x - gmin.x) + 1e-9);
    }
};

OverScreenMap overhead_build_map(DisplayWin& display, const Grid& grid);

// ---- session view state / input (not saved) ----
void overhead_view_zoom(const Grid& grid, int dir, int px, int py,
                        DisplayWin& display);
bool overhead_view_pan_tick(DisplayWin& display, const Grid& grid);
void overhead_view_click(DisplayWin& display, int btn, int mx, int my);
void overhead_view_reset();

bool overhead_map_contains(DisplayWin& display, const Grid& grid,
                           int px, int py);
Vec3 overhead_world_at(DisplayWin& display, const Grid& grid, int px, int py);

#endif
