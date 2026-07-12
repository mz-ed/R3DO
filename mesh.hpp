#ifndef MESH_H
#define MESH_H

#include "hittable.hpp"
#include <vector>

struct Triangle {
    Vec3 v0, v1, v2;
};

bool ray_tri_intersect(const Ray& r, const Vec3& v0, const Vec3& v1, const Vec3& v2,
                       double t_min, double t_max, HitRecord& rec);

class Mesh : public Hittable {
public:
    std::vector<Triangle> triangles;
    Vec3 color;

    Mesh(const std::vector<Triangle>& tris, const Vec3& col)
        : triangles(tris), color(col) {}

    const char* type_name() const override { return "mesh"; }
    Vec3 get_color() const override { return color; }

    bool hit(const Ray& r, double t_min, double t_max, HitRecord& rec) const override {
        bool hit_any = false;
        double closest = t_max;
        HitRecord temp;
        for (auto& tri : triangles) {
            if (ray_tri_intersect(r, tri.v0, tri.v1, tri.v2, t_min, closest, temp)) {
                hit_any = true;
                closest = temp.t;
                rec = temp;
            }
        }
        return hit_any;
    }
};

#endif
