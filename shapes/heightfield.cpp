#include "heightfield.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_SIMD
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "vendor/stb/stb_image.h"
#include "vendor/stb/stb_image_write.h"

static const unsigned int MAP_COLOR_MAX  = 0x3f7a3a;
static const unsigned int MAP_COLOR_HIGH = 0x8a8f78;
static const unsigned int MAP_COLOR_WATER = 0x2b4a72;

HeightField::HeightField(double origin_x, double origin_z, double side, int n,
                         const std::vector<float>& heights, const Vec3& col)
    : color(col), origin_x_(origin_x), origin_z_(origin_z), side_(side),
      n_(n) {
    cell_ = side_ / (double)n_;
    h_ = heights;
    // Height normalization anchors: 0 (base/water) .. y_max_ (grows as the
    // brush raises terrain, so painted hills stay on the same ramp).
    double mx = 0.0;
    for (float v : h_) mx = std::max(mx, (double)v);
    y_min_ = 0.0;
    y_max_ = (mx > 1e-6) ? mx : 1e-6;
    path_.clear();
}

Vec3 HeightField::get_center() const {
    return Vec3(origin_x_ + side_ * 0.5, (y_min_ + y_max_) * 0.5,
                origin_z_ + side_ * 0.5);
}

void HeightField::translate(const Vec3& d) {
    origin_x_ += d.x;
    origin_z_ += d.z;
}

double HeightField::height_at(double x, double z) const {
    // Bilinear over column centers.
    double fx = (x - origin_x_) / cell_ - 0.5;
    double fz = (z - origin_z_) / cell_ - 0.5;
    int i0 = (int)std::floor(fx), k0 = (int)std::floor(fz);
    double tx = fx - i0, tz = fz - k0;
    auto at = [&](int i, int k) -> double {
        if (i < 0) { i = 0; }
        if (i >= n_) { i = n_ - 1; }
        if (k < 0) { k = 0; }
        if (k >= n_) { k = n_ - 1; }
        return h_[(size_t)k * n_ + i];
    };
    double a = at(i0, k0) * (1 - tx) + at(i0 + 1, k0) * tx;
    double b = at(i0, k0 + 1) * (1 - tx) + at(i0 + 1, k0 + 1) * tx;
    return a * (1 - tz) + b * tz;
}

Vec3 HeightField::normal_at(double x, double z) const {
    double e = cell_ * 0.5;
    double dhx = (height_at(x + e, z) - height_at(x - e, z)) / (2.0 * e);
    double dhz = (height_at(x, z + e) - height_at(x, z - e)) / (2.0 * e);
    Vec3 n(-dhx, 1.0, -dhz);
    return unit_vector(n);
}

unsigned int HeightField::height_tint(double hy, double slope) const {
    // Water | grass | rock | snow blend by height/slope, gamma-corrected later
    // by the renderer (colors are read raw through rec.color in sRGB space).
    double tn = (hy - y_min_) / std::max(1e-9, y_max_ - y_min_);
    tn = std::clamp(tn, 0.0, 1.0);
    double steep = std::clamp(slope, 0.0, 1.0);

    unsigned int base;
    if (tn < 0.10) base = MAP_COLOR_WATER;
    else if (tn < 0.45) base = MAP_COLOR_MAX;
    else if (tn < 0.80) base = MAP_COLOR_HIGH;
    else base = 0xd8dde8;

    // Slope pushes toward rock.
    if (steep > 0.05) {
        unsigned int rock = 0x6e6254;
        double mix = steep * 0.8;
        int br = (int)(((base >> 16) & 0xff) * (1 - mix) + ((rock >> 16) & 0xff) * mix);
        int bg = (int)(((base >> 8) & 0xff) * (1 - mix) + ((rock >> 8) & 0xff) * mix);
        int bb = (int)((base & 0xff) * (1 - mix) + (rock & 0xff) * mix);
        base = ((unsigned)br << 16) | ((unsigned)bg << 8) | (unsigned)bb;
    }
    return base;
}

std::pair<double, double> HeightField::march_bounds(const Ray& r,
                                                    double t_min,
                                                    double t_max) const {
    // Clip the ray to the terrain AABB [origin, origin+side] x [ymin, ymax].
    double x0 = origin_x_, x1 = origin_x_ + side_;
    double z0 = origin_z_, z1 = origin_z_ + side_;
    double entry = t_min;
    // x/z sides must be inside eventually: find max of the entry T's
    const double eps = 1e-9;
    for (int ax = 0; ax < 2; ax++) {
        double d = (ax == 0) ? r.direction().x : r.direction().z;
        double o = (ax == 0) ? r.origin().x : r.origin().z;
        double lo = (ax == 0) ? x0 : z0, hi = (ax == 0) ? x1 : z1;
        if (std::fabs(d) < eps) {
            if (o < lo || o > hi) return {0.0, -1.0};
            continue;
        }
        double t0 = (lo - o) / d, t1 = (hi - o) / d;
        if (t0 > t1) std::swap(t0, t1);
        entry = std::max(entry, t0);
        t_max = std::min(t_max, t1);
    }
    if (entry > t_max) return {0.0, -1.0};
    return {entry, t_max};
}

bool HeightField::hit(const Ray& r, double t_min, double t_max, HitRecord& rec) const {
    auto tb = march_bounds(r, t_min, t_max);
    double t = tb.first, tend = tb.second;
    if (tend <= t) return false;

    const int steps = 220;
    double step = (tend - t) / steps;
    double prev_t = t;
    double prev_dy = r.origin().y + r.direction().y * t - height_at(r.origin().x + r.direction().x * t,
                                                             r.origin().z + r.direction().z * t);

    for (int i = 1; i <= steps; i++) {
        t = tb.first + step * i;
        if (t > tend) t = tend;
        double px = r.origin().x + r.direction().x * t;
        double pz = r.origin().z + r.direction().z * t;
        double py = r.origin().y + r.direction().y * t;
        double surf = height_at(px, pz);
        double dy = py - surf;
        if (dy <= 0.0) {
            // bisect between prev_t and t for a tighter hit point
            double a = prev_t, b = t;
            for (int it = 0; it < 7; it++) {
                double mid = 0.5 * (a + b);
                double mx_ = r.origin().x + r.direction().x * mid;
                double mz_ = r.origin().z + r.direction().z * mid;
                double my_ = r.origin().y + r.direction().y * mid;
                double dm = my_ - height_at(mx_, mz_);
                if (dm <= 0.0) { b = mid; }
                else { a = mid; }
            }
            double ht = 0.5 * (a + b);
            double hx = r.origin().x + r.direction().x * ht;
            double hz = r.origin().z + r.direction().z * ht;
            double hy = height_at(hx, hz);

            Vec3 nrm = normal_at(hx, hz);
            // slope = 1 - |n.y| (0 flat .. 1 vertical)
            double slope = 1.0 - std::fabs(nrm.y);
            rec.p = Vec3(hx, hy, hz);
            rec.set_face_normal(r, nrm);
            rec.t = ht;
            rec.color = Vec3(((height_tint(hy, slope) >> 16) & 0xff) / 255.999,
                             ((height_tint(hy, slope) >> 8) & 0xff) / 255.999,
                             (height_tint(hy, slope) & 0xff) / 255.999);
            rec.hittable = nullptr;
            return true;
        }
        prev_t = t; prev_dy = dy;
    }
    return false;
}

void HeightField::paint(double x, double z, double radius, double amount, int mode) {
    // Collect column indices in the disc around (x, z).
    int i0 = (int)((x - radius - origin_x_) / cell_);
    int k0 = (int)((z - radius - origin_z_) / cell_);
    int i1 = (int)((x + radius - origin_x_) / cell_) + 1;
    int k1 = (int)((z + radius - origin_z_) / cell_) + 1;
    for (int k = k0; k <= k1; k++) {
        if (k < 0 || k >= n_) continue;
        for (int i = i0; i <= i1; i++) {
            if (i < 0 || i >= n_) continue;
            double wx = origin_x_ + (i + 0.5) * cell_;
            double wz = origin_z_ + (k + 0.5) * cell_;
            double dx = wx - x, dz = wz - z;
            double dist = std::sqrt(dx * dx + dz * dz);
            if (dist > radius) continue;
            double falloff = 0.5 * (1.0 + std::cos(M_PI * dist / radius)); // 1..0
            float& hv = h_[(size_t)k * n_ + i];
            if (mode == 0) {
                hv = (float)std::clamp((double)hv + amount * falloff, 0.0, 8.0);
                if (hv > y_max_) y_max_ = hv;
            } else if (mode == 1) {
                // average of neighbors relaxed toward at a rate from amount
                int cnt = 0;
                double sum = 0.0;
                for (int kk = std::max(0, k - 1); kk <= std::min(n_ - 1, k + 1); kk++)
                    for (int ii = std::max(0, i - 1); ii <= std::min(n_ - 1, i + 1); ii++)
                        { sum += h_[(size_t)kk * n_ + ii]; cnt++; }
                double target = sum / cnt;
                hv = (float)std::clamp((double)hv + (target - (double)hv) * falloff * 0.8, 0.0, 8.0);
            }
        }
    }
}

bool HeightField::export_png(const std::string& path) const {
    unsigned char* buf = new unsigned char[n_ * n_];
    double span = std::max(1e-9, y_max_ - y_min_);
    for (size_t idx = 0; idx < h_.size(); idx++) {
        double tn = std::clamp(((double)h_[idx] - y_min_) / span, 0.0, 1.0);
        buf[idx] = (unsigned char)(tn * 255.0 + 0.5);
    }
    int ok = stbi_write_png(path.c_str(), n_, n_, 1, buf, n_);
    delete[] buf;
    return ok != 0;
}

std::unique_ptr<HeightField> HeightField::from_png(const std::string& path,
                                                   double origin_x, double origin_z,
                                                   double side,
                                                   double y_min, double y_max,
                                                   const Vec3& col,
                                                   int max_res) {
    int w, hgt, comp;
    unsigned char* data = stbi_load(path.c_str(), &w, &hgt, &comp, 0);
    if (!data || w < 2 || hgt < 2) {
        if (data) stbi_image_free(data);
        return nullptr;
    }
    // Downsample factor: keep n_ <= max_res for paint/hit speed.
    int step = 1;
    while ((w / step) > max_res || (hgt / step) > max_res) step++;
    int n = std::max(2, w / step);
    // Terrain is square in world space: resample the source into n x n.
    std::vector<float> hs((size_t)n * n, (float)y_min);
    double hspan = (y_max - y_min > 1e-9) ? (y_max - y_min) : 1.0;
    for (int kk = 0; kk < n; kk++) {
        int sy = (int)(((double)(2 * kk + 1) / (2.0 * n)) * hgt);
        if (sy >= hgt) sy = hgt - 1;
        for (int ii = 0; ii < n; ii++) {
            int sx = (int)(((double)(2 * ii + 1) / (2.0 * n)) * w);
            if (sx >= w) sx = w - 1;
            const unsigned char* px = data + ((size_t)sy * w + sx) * comp;
            // luminance
            double lum;
            if (comp == 1) lum = px[0];
            else if (comp == 3 || comp == 4)
                lum = 0.299 * px[0] + 0.587 * px[1] + 0.114 * px[2];
            else lum = px[0];
            double tn = lum / 255.0;
            // PNG row 0 = far edge (+z). Flat array is [k][i] where k grows
            // toward +z: direct mapping.
            hs[(size_t)kk * n + ii] = (float)(y_min + tn * hspan);
        }
    }
    stbi_image_free(data);

    auto hf = std::make_unique<HeightField>(origin_x, origin_z, side, n, hs, col);
    hf->path_ = path;
    return hf;
}

std::unique_ptr<HeightField> HeightField::flat(double origin_x, double origin_z,
                                               double side, double y_min,
                                               const Vec3& col,
                                               std::string blank_path) {
    int n = 96;
    std::vector<float> hs((size_t)n * n, (float)y_min);
    auto hf = std::make_unique<HeightField>(origin_x, origin_z, side, n, hs, col);
    hf->path_ = blank_path;
    return hf;
}
