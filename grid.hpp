#ifndef GRID_H
#define GRID_H

#include "hittable.hpp"
#include "box.hpp"
#include <vector>

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

    bool hit(const Ray& r, double t_min, double t_max, HitRecord& rec) const override {
        HitRecord temp_rec;
        bool hit_anything = false;
        double closest = t_max;
        Vec3 faint(0.06, 0.06, 0.1);

        for (int i = 0; i < nx; i++) {
            for (int j = 0; j < ny; j++) {
                for (int k = 0; k < nz; k++) {
                    Hittable* obj = cells[idx(i, j, k)];

                    if (obj) {
                        if (obj->hit(r, t_min, closest, temp_rec)) {
                            hit_anything = true;
                            closest = temp_rec.t;
                            rec = temp_rec;
                        }
                    } else {
                        Vec3 cmin, cmax;
                        cell_bounds(i, j, k, cmin, cmax);
                        Box cell_box(cmin, cmax, faint);
                        if (cell_box.hit(r, t_min, closest, temp_rec)) {
                            hit_anything = true;
                            closest = temp_rec.t;
                            rec = temp_rec;
                        }
                    }
                }
            }
        }

        return hit_anything;
    }
};

#endif
