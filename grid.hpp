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

    Grid(int nx, int ny, int nz, double cell_size, const Vec3& origin)
        : nx(nx), ny(ny), nz(nz), cell_size(cell_size), origin(origin) {
        cells.resize(nx * ny * nz, nullptr);
    }

    ~Grid() {
        for (auto* c : cells) {
            delete c;
        }
    }

    int idx(int i, int j, int k) const {
        return i + j * nx + k * nx * ny;
    }

    void set(int i, int j, int k, Hittable* obj) {
        if (i >= 0 && i < nx && j >= 0 && j < ny && k >= 0 && k < nz) {
            delete cells[idx(i, j, k)];
            cells[idx(i, j, k)] = obj;
        }
    }

    Hittable* get(int i, int j, int k) const {
        if (i >= 0 && i < nx && j >= 0 && j < ny && k >= 0 && k < nz) {
            return cells[idx(i, j, k)];
        }
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

    bool hit(const Ray& r, double t_min, double t_max, HitRecord& rec) const override {
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

        Vec3 faint(0.25, 0.25, 0.35);
        HitRecord temp_rec;
        bool hit_anything = false;
        double closest = t_exit;
        int max_steps = nx + ny + nz;

        for (int s = 0; s < max_steps; s++) {
            Hittable* obj = cells[idx(cell_idx[0], cell_idx[1], cell_idx[2])];
            if (obj) {
                if (obj->hit(r, t_min, closest, temp_rec)) {
                    hit_anything = true;
                    closest = temp_rec.t;
                    rec = temp_rec;
                }
            } else {
                Vec3 cmin, cmax;
                cell_bounds(cell_idx[0], cell_idx[1], cell_idx[2], cmin, cmax);
                Box cell_box(cmin, cmax, faint);
                if (cell_box.hit(r, t_min, closest, temp_rec)) {
                    hit_anything = true;
                    closest = temp_rec.t;
                    rec = temp_rec;
                }
            }

            int axis = 0;
            if (tMax[1] < tMax[0]) axis = 1;
            if (tMax[2] < tMax[axis]) axis = 2;

            if (tMax[axis] > t_exit) {
                break;
            }

            cell_idx[axis] += step[axis];
            if (cell_idx[axis] < 0 || cell_idx[axis] >= dim[axis]) {
                break;
            }
            tMax[axis] += tDelta[axis];
        }

        return hit_anything;
    }
};

#endif
