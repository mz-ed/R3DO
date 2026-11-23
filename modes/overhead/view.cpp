#include "view.hpp"
#include "ui.hpp"
#include <X11/keysym.h>
#include <cmath>
#include <algorithm>
#include <chrono>

namespace {

double g_zoom = 1.0;
double g_pan_x = 0.0, g_pan_y = 0.0;
bool g_panning = false;
double g_pan_start_x = 0.0, g_pan_start_y = 0.0;
int g_press_x = 0, g_press_y = 0;

constexpr double ZOOM_MIN = 1.0;
constexpr double ZOOM_MAX = 8.0;
constexpr double ZOOM_STEP = 1.15;

// Keep at least this much of the zoomed map inside the map viewport so a
// wild pan cannot lose the scene entirely.
constexpr double KEEP_PX = 80.0;

void clamp_pan(DisplayWin& display, const Grid& grid) {
    int w = display.width(), h = display.height();
    int pad = 60;
    int vw = w - pad * 2 - UI::SIDEBAR_W;
    int vh = h - pad * 2;

    OverScreenMap m = overhead_build_map(display, grid);
    double zw = m.bw * g_zoom, zh = m.bh * g_zoom;

    if (zw <= vw) g_pan_x = 0.0;
    else {
        double lo = (double)pad - ((double)m.ox + zw) + KEEP_PX;
        double hi = ((double)pad + vw - KEEP_PX) - (double)m.ox;
        g_pan_x = std::clamp(g_pan_x, lo, hi);
    }
    if (zh <= vh) g_pan_y = 0.0;
    else {
        double lo = (double)pad - ((double)m.oy + zh) + KEEP_PX;
        double hi = ((double)pad + vh - KEEP_PX) - (double)m.oy;
        g_pan_y = std::clamp(g_pan_y, lo, hi);
    }
}

} // namespace

OverScreenMap overhead_build_map(DisplayWin& display, const Grid& grid) {
    int w = display.width(), h = display.height();
    int pad = 60;
    int map_w = w - pad * 2 - UI::SIDEBAR_W;
    int map_h = h - pad * 2;

    double cs = grid.cell_size;
    double cell_px_x = (double)map_w / (grid.nx * cs);
    double cell_px_y = (double)map_h / (grid.nz * cs);
    double cell_px = std::min(cell_px_x, cell_px_y);
    if (cell_px < 1) cell_px = 1;

    OverScreenMap m;
    m.bw = grid.nx * cs * cell_px;
    m.bh = grid.nz * cs * cell_px;
    m.ox = pad + (int)((map_w - m.bw) / 2.0);
    m.oy = pad + (int)((map_h - m.bh) / 2.0);
    m.zoom = g_zoom;
    m.pan_x = g_pan_x;
    m.pan_y = g_pan_y;
    grid.grid_bounds(m.gmin, m.gmax);
    return m;
}

void overhead_view_zoom(const Grid& grid, int dir, int px, int py,
                        DisplayWin& display) {
    OverScreenMap m = overhead_build_map(display, grid);
    Vec3 anchor = m.to_world(px, py);
    double fx = (anchor.x - m.gmin.x) / (m.gmax.x - m.gmin.x);
    double fz = (anchor.z - m.gmin.z) / (m.gmax.z - m.gmin.z);

    double nz = g_zoom * (dir > 0 ? ZOOM_STEP : 1.0 / ZOOM_STEP);
    nz = std::clamp(nz, ZOOM_MIN, ZOOM_MAX);
    if (nz == g_zoom) {
        clamp_pan(display, grid);
        return;
    }
    // keep the world point under the cursor fixed while zooming
    g_pan_x = (double)px - (double)m.ox - fx * m.bw * nz;
    g_pan_y = (double)py - (double)m.oy - (1.0 - fz) * m.bh * nz;
    g_zoom = nz;
    clamp_pan(display, grid);
}

bool overhead_view_pan_tick(DisplayWin& display, const Grid& grid) {
    bool mid = display.is_mouse_down() && display.mouse_button() == 2;
    bool space_left = display.is_mouse_down() && display.mouse_button() == 1 &&
                      display.key_held(XK_space);
    if (!mid && !space_left) {
        g_panning = false;
        return false;
    }
    if (!g_panning) {
        g_panning = true;
        g_pan_start_x = g_pan_x;
        g_pan_start_y = g_pan_y;
        g_press_x = display.mouse_press_x();
        g_press_y = display.mouse_press_y();
    }
    g_pan_x = g_pan_start_x + (double)(display.mouse_x() - g_press_x);
    g_pan_y = g_pan_start_y + (double)(display.mouse_y() - g_press_y);
    clamp_pan(display, grid);
    return true;
}

void overhead_view_click(DisplayWin& display, int btn, int mx, int my) {
    (void)display;
    (void)btn;
    static std::chrono::steady_clock::time_point last{};
    static int lx = -9999, ly = -9999;
    auto now = std::chrono::steady_clock::now();
    double dt = std::chrono::duration<double>(now - last).count();
    if (dt < 0.4 && std::abs(mx - lx) < 8 && std::abs(my - ly) < 8) {
        overhead_view_reset();
        last = {};
        lx = ly = -9999;
        return;
    }
    last = now;
    lx = mx;
    ly = my;
}

void overhead_view_reset() {
    g_zoom = 1.0;
    g_pan_x = g_pan_y = 0.0;
    g_panning = false;
}

bool overhead_map_contains(DisplayWin& display, const Grid& grid,
                           int px, int py) {
    OverScreenMap m = overhead_build_map(display, grid);
    double zw = m.bw * m.zoom, zh = m.bh * m.zoom;
    double x0 = (double)m.ox + m.pan_x, y0 = (double)m.oy + m.pan_y;
    return px >= x0 && px <= x0 + zw && py >= y0 && py <= y0 + zh;
}

Vec3 overhead_world_at(DisplayWin& display, const Grid& grid, int px, int py) {
    OverScreenMap m = overhead_build_map(display, grid);
    return m.to_world(px, py);
}
