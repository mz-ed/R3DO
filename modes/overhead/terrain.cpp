#include "terrain.hpp"
#include "heightfield.hpp"
#include <cmath>
#include <vector>

namespace {

struct OverTerrainCache {
    const void* id = nullptr;
    unsigned version = 0;
    bool valid = false;
    double step = 0.0; // world units per tile
    std::vector<std::pair<double, double>> centers; // world xz per tile
    std::vector<unsigned char> bands;               // palette band per tile
    std::vector<double> shades;                     // 0..1 hillshade factor
};

OverTerrainCache g_cache;

unsigned char band_of(double hy, double maxh) {
    double tn = hy / maxh;
    if (tn < 0.10) return 0; // water / flats
    if (tn < 0.45) return 1;
    if (tn < 0.80) return 2;
    return 3;
}

unsigned int band_color(unsigned char b) {
    switch (b) {
    case 0: return 0x2b4a72;
    case 1: return 0x3a7a38;
    case 2: return 0x8a8f78;
    default: return 0xd8dde8;
    }
}

void resample(const HeightField& hf) {
    int n = hf.hf_res();
    g_cache.step = hf.side() / (double)n;
    double maxh = hf.max_height();
    if (maxh <= 1e-6) maxh = 1.0;

    // full height grid first so slopes can use neighbor columns
    std::vector<double> h((size_t)n * n);
    double hy, sx, sz;
    for (int k = 0; k < n; k++)
        for (int i = 0; i < n; i++) {
            hf.column_sample(i, k, hy, sx, sz);
            h[(size_t)k * n + i] = hy;
        }

    g_cache.centers.clear();
    g_cache.bands.clear();
    g_cache.shades.clear();
    g_cache.centers.reserve((size_t)n * n);
    g_cache.bands.reserve((size_t)n * n);
    g_cache.shades.reserve((size_t)n * n);
    for (int k = 0; k < n; k++) {
        for (int i = 0; i < n; i++) {
            double wx = hf.origin_x() + (i + 0.5) * g_cache.step;
            double wz = hf.origin_z() + (k + 0.5) * g_cache.step;
            double hy2 = h[(size_t)k * n + i];
            unsigned char band = band_of(hy2, maxh);
            double shade = 1.0;
            if (band != 0) {
                double hx = (i > 0 ? h[(size_t)k * n + i - 1]
                                   : hy2) -
                            (i < n - 1 ? h[(size_t)k * n + i + 1] : hy2);
                double hz = (k > 0 ? h[(size_t)(k - 1) * n + i]
                                   : hy2) -
                            (k < n - 1 ? h[(size_t)(k + 1) * n + i] : hy2);
                hx /= (2.0 * g_cache.step);
                hz /= (2.0 * g_cache.step);
                // light from the top-left of the map (nw facing bright)
                double slope = std::sqrt(hx * hx + hz * hz);
                double facing = (hx * 0.707 + hz * 0.707) / (slope + 1e-9);
                double mag = std::min(1.0, slope * 2.0 / maxh);
                shade = 1.0 - 0.30 * mag + 0.18 * mag * facing;
                shade = std::clamp(shade, 0.55, 1.15);
            }
            g_cache.centers.push_back({wx, wz});
            g_cache.bands.push_back(band);
            g_cache.shades.push_back(shade);
        }
    }
}

} // namespace

void overhead_draw_terrain(DisplayWin& display, const Grid& grid,
                           const OverScreenMap& map) {
    auto* hf = dynamic_cast<HeightField*>(grid.terrain());
    if (!hf) {
        g_cache.valid = false;
        return;
    }
    if (!g_cache.valid || g_cache.id != (const void*)hf ||
        g_cache.version != hf->version()) {
        g_cache.id = (const void*)hf;
        g_cache.version = hf->version();
        resample(*hf);
        g_cache.valid = true;
    }

    int w = display.width(), h = display.height();

    double px_per_world = map.base_px_per_world() * map.zoom;
    int pw = std::max(2, (int)std::ceil(g_cache.step * px_per_world));

    Vec3 gmin, gmax;
    grid.grid_bounds(gmin, gmax);
    for (size_t t = 0; t < g_cache.centers.size(); t++) {
        double wx = g_cache.centers[t].first;
        double wz = g_cache.centers[t].second;
        if (wx < gmin.x - g_cache.step || wx > gmax.x + g_cache.step ||
            wz < gmin.z - g_cache.step || wz > gmax.z + g_cache.step)
            continue;
        auto sp = map.to_screen(wx, wz);
        if (sp.first + pw / 2 < 0 || sp.first - pw / 2 > w ||
            sp.second + pw / 2 < 0 || sp.second - pw / 2 > h)
            continue;
        unsigned int base = band_color(g_cache.bands[t]);
        double sh = g_cache.shades[t];
        int cr = (int)std::min(255.0, ((base >> 16) & 0xff) * sh);
        int cg = (int)std::min(255.0, ((base >> 8) & 0xff) * sh);
        int cb = (int)std::min(255.0, (base & 0xff) * sh);
        display.fill_rect(sp.first - pw / 2, sp.second - pw / 2, pw, pw,
                          ((unsigned)cr << 16) | ((unsigned)cg << 8) | (unsigned)cb);
    }
}
