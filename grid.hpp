#ifndef GRID_H
#define GRID_H

#include "hittable.hpp"
#include "box.hpp"
#include <vector>
#include <limits>

class Grid : public Hittable {
public:
    int nx, ny, nz;
    double cell_size;
    Vec3 origin;
    std::vector<Hittable*> cells;
    int visible_count_ = 0;

    static const int CHUNK_BITS = 2;
    static const int CHUNK_SIZE = 1 << CHUNK_BITS;
    int cnx, cny, cnz;
    std::vector<char> chunk_vis_;

    Grid(int nx, int ny, int nz, double cell_size, const Vec3& origin)
        : nx(nx), ny(ny), nz(nz), cell_size(cell_size), origin(origin) {
        cells.resize(nx * ny * nz, nullptr);
        cnx = (nx + CHUNK_SIZE - 1) >> CHUNK_BITS;
        cny = (ny + CHUNK_SIZE - 1) >> CHUNK_BITS;
        cnz = (nz + CHUNK_SIZE - 1) >> CHUNK_BITS;
        chunk_vis_.resize(cnx * cny * cnz, 0);
    }

    ~Grid() {
        for (auto* c : cells) delete c;
    }

    int idx(int i, int j, int k) const {
        return i + j * nx + k * nx * ny;
    }

    int chunk_flat(int ci, int cj, int ck) const {
        return ci + cj * cnx + ck * cnx * cny;
    }

    void set(int i, int j, int k, Hittable* obj) {
        if (i >= 0 && i < nx && j >= 0 && j < ny && k >= 0 && k < nz) {
            Hittable* old = cells[idx(i, j, k)];
            if (old && old->is_visible()) visible_count_--;
            if (obj && obj->is_visible()) visible_count_++;
            delete old;
            cells[idx(i, j, k)] = obj;
            int ci = i >> CHUNK_BITS, cj = j >> CHUNK_BITS, ck = k >> CHUNK_BITS;
            int cf = chunk_flat(ci, cj, ck);
            chunk_vis_[cf] = 0;
            int si = ci << CHUNK_BITS, ei = std::min(si + CHUNK_SIZE, nx);
            int sj = cj << CHUNK_BITS, ej = std::min(sj + CHUNK_SIZE, ny);
            int sk = ck << CHUNK_BITS, ek = std::min(sk + CHUNK_SIZE, nz);
            for (int kk = sk; kk < ek; kk++)
                for (int jj = sj; jj < ej; jj++)
                    for (int ii = si; ii < ei; ii++) {
                        Hittable* o = cells[idx(ii, jj, kk)];
                        if (o && o->is_visible()) { chunk_vis_[cf] = 1; goto done; }
                    }
            done:;
        }
    }

    Hittable* get(int i, int j, int k) const {
        if (i >= 0 && i < nx && j >= 0 && j < ny && k >= 0 && k < nz)
            return cells[idx(i, j, k)];
        return nullptr;
    }

    Vec3 cell_center(int i, int j, int k) const {
        Vec3 base = origin - Vec3(nx * cell_size, ny * cell_size, nz * cell_size) * 0.5;
        return base + Vec3(i * cell_size + cell_size * 0.5, j * cell_size + cell_size * 0.5, k * cell_size + cell_size * 0.5);
    }

    void cell_bounds(int i, int j, int k, Vec3& cmin, Vec3& cmax) const {
        Vec3 base = origin - Vec3(nx * cell_size, ny * cell_size, nz * cell_size) * 0.5;
        double gap = cell_size * 0.03;
        double half = cell_size * 0.5 - gap;
        Vec3 center = base + Vec3(i * cell_size + cell_size * 0.5, j * cell_size + cell_size * 0.5, k * cell_size + cell_size * 0.5);
        cmin = center - Vec3(half, half, half);
        cmax = center + Vec3(half, half, half);
    }

    void grid_bounds(Vec3& gmin, Vec3& gmax) const {
        Vec3 half = Vec3(nx * cell_size, ny * cell_size, nz * cell_size) * 0.5;
        gmin = origin - half;
        gmax = origin + half;
    }

    bool world_to_cell(const Vec3& p, int& i, int& j, int& k) const {
        Vec3 gmin, gmax;
        grid_bounds(gmin, gmax);
        i = (int)std::floor((p.x - gmin.x) / cell_size);
        j = (int)std::floor((p.y - gmin.y) / cell_size);
        k = (int)std::floor((p.z - gmin.z) / cell_size);
        return i >= 0 && i < nx && j >= 0 && j < ny && k >= 0 && k < nz;
    }

    bool has_visible() const { return visible_count_ > 0; }

    bool hit(const Ray& r, double t_min, double t_max, HitRecord& rec) const override {
        if (visible_count_ == 0) return false;
        Vec3 gmin, gmax;
        grid_bounds(gmin, gmax);

        double t_enter = t_min, t_exit = t_max;
        for (int a = 0; a < 3; a++) {
            double d = (a == 0) ? r.direction().x : (a == 1) ? r.direction().y : r.direction().z;
            double o = (a == 0) ? r.origin().x : (a == 1) ? r.origin().y : r.origin().z;
            double minv = (a == 0) ? gmin.x : (a == 1) ? gmin.y : gmin.z;
            double maxv = (a == 0) ? gmax.x : (a == 1) ? gmax.y : gmax.z;

            if (std::abs(d) < 1e-10) {
                if (o < minv || o > maxv) return false;
                continue;
            }

            double t0 = (minv - o) / d;
            double t1 = (maxv - o) / d;
            if (d < 0) std::swap(t0, t1);

            if (t0 > t_enter) t_enter = t0;
            if (t1 < t_exit) t_exit = t1;
            if (t_exit <= t_enter) return false;
        }

        double ro[3] = {r.origin().x, r.origin().y, r.origin().z};

        Vec3 entry = r.at(t_enter + 0.0001);
        int i, j, k;
        if (!world_to_cell(entry, i, j, k)) return false;

        double rd[3] = {r.direction().x, r.direction().y, r.direction().z};
        double gm[3] = {gmin.x, gmin.y, gmin.z};
        double tMax[3], tDelta[3];
        int step[3];
        int cell_idx[3] = {i, j, k};
        int chunk_idx[3] = {i >> CHUNK_BITS, j >> CHUNK_BITS, k >> CHUNK_BITS};
        int dim[3] = {nx, ny, nz};

        for (int a = 0; a < 3; a++) {
            if (std::abs(rd[a]) < 1e-12) {
                step[a] = 0;
                tDelta[a] = std::numeric_limits<double>::infinity();
                tMax[a] = std::numeric_limits<double>::infinity();
            } else {
                if (rd[a] > 0) {
                    step[a] = 1;
                    tMax[a] = (gm[a] + (cell_idx[a] + 1.0) * cell_size - ro[a]) / rd[a];
                } else {
                    step[a] = -1;
                    tMax[a] = (gm[a] + cell_idx[a] * cell_size - ro[a]) / rd[a];
                }
                tDelta[a] = cell_size / std::abs(rd[a]);
            }
        }

        HitRecord temp_rec;
        double closest = t_exit;
        int max_steps = nx + ny + nz;

        for (int s = 0; s < max_steps; s++) {
            int cf = chunk_flat(chunk_idx[0], chunk_idx[1], chunk_idx[2]);

            if (!chunk_vis_[cf]) {
                int exit_axis = -1;
                double exit_t = 1e30;
                for (int a = 0; a < 3; a++) {
                    if (step[a] == 0) continue;
                    int bound, steps;
                    if (step[a] > 0) {
                        bound = ((chunk_idx[a] + 1) << CHUNK_BITS);
                        if (bound >= dim[a]) continue;
                        steps = bound - cell_idx[a];
                    } else {
                        bound = chunk_idx[a] << CHUNK_BITS;
                        if (bound <= 0) continue;
                        steps = cell_idx[a] - bound + 1;
                    }
                    double ta = tMax[a] + (steps - 1) * tDelta[a];
                    if (ta < exit_t) { exit_t = ta; exit_axis = a; }
                }
                if (exit_axis < 0) break;
                int steps;
                if (step[exit_axis] > 0) {
                    steps = ((chunk_idx[exit_axis] + 1) << CHUNK_BITS) - cell_idx[exit_axis];
                } else {
                    steps = cell_idx[exit_axis] - (chunk_idx[exit_axis] << CHUNK_BITS) + 1;
                }
                cell_idx[exit_axis] += steps * step[exit_axis];
                chunk_idx[exit_axis] += step[exit_axis];
                if (cell_idx[exit_axis] < 0 || cell_idx[exit_axis] >= dim[exit_axis]) break;
                if (chunk_idx[exit_axis] < 0 || chunk_idx[exit_axis] >= (dim[exit_axis] + CHUNK_SIZE - 1) >> CHUNK_BITS) break;
                tMax[exit_axis] += steps * tDelta[exit_axis];
                continue;
            }

            Hittable* obj = cells[idx(cell_idx[0], cell_idx[1], cell_idx[2])];
            if (obj && obj->is_visible()) {
                if (obj->hit(r, t_min, closest, temp_rec)) {
                    rec = temp_rec;
                    return true;
                }
            }

            int axis = 0;
            if (tMax[1] < tMax[0]) axis = 1;
            if (tMax[2] < tMax[axis]) axis = 2;

            if (tMax[axis] > t_exit) break;

            int prev_chunk = cell_idx[axis] >> CHUNK_BITS;
            cell_idx[axis] += step[axis];
            if (cell_idx[axis] < 0 || cell_idx[axis] >= dim[axis]) break;
            int new_chunk = cell_idx[axis] >> CHUNK_BITS;
            if (new_chunk != prev_chunk) chunk_idx[axis] = new_chunk;
            tMax[axis] += tDelta[axis];
        }

        return false;
    }
};

#endif
