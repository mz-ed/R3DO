#include "pick.hpp"
#include "view.hpp"
#include <cmath>

Hittable* overhead_pick(DisplayWin& display, const Grid& grid, int mx, int my) {
    OverScreenMap map = overhead_build_map(display, grid);
    Vec3 w = map.to_world(mx, my);
    double cs = grid.cell_size;

    Hittable* best = nullptr;
    double best_d = 1e30;

    for (const auto& f : grid.free_objects()) {
        if (!f || !f->is_visible()) continue;
        Vec3 c = f->get_center();
        double dx = w.x - c.x, dz = w.z - c.z;
        double d = dx * dx + dz * dz;
        double r = std::max(f->bound_radius_xz(), cs * 0.3);
        if (d <= r * r && d < best_d) {
            best_d = d;
            best = f.get();
        }
    }
    if (best) return best;

    int i, j, k;
    if (!grid.world_to_cell(w, i, j, k)) return nullptr;
    for (int jj = grid.ny - 1; jj >= 0; jj--) {
        Hittable* obj = grid.get(i, jj, k);
        if (obj && obj->is_visible()) return obj;
    }
    return nullptr;
}
