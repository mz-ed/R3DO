#ifndef MESH_H
#define MESH_H

#include "hittable.hpp"
#include <vector>
#include <cmath>

struct Triangle {
    Vec3 v0, v1, v2;
};

bool ray_tri_intersect(const Ray& r, const Vec3& v0, const Vec3& v1, const Vec3& v2,
                       double t_min, double t_max, HitRecord& rec);

struct AABB {
    Vec3 min, max;
    AABB() : min(INFINITY, INFINITY, INFINITY), max(-INFINITY, -INFINITY, -INFINITY) {}
    AABB(const Vec3& a, const Vec3& b) : min(a), max(b) {}
    void expand(const Vec3& p);
    void expand(const AABB& o);
    bool hit(const Ray& r, double t_min, double t_max) const;
};

struct BVHNode {
    AABB bbox;
    BVHNode* left = nullptr;
    BVHNode* right = nullptr;
    int tri_start = 0, tri_end = 0;
    ~BVHNode() { delete left; delete right; }
};

class Mesh : public Hittable {
public:
    std::vector<Triangle> triangles;
    Vec3 color;
    BVHNode* bvh_root;

    Mesh(const std::vector<Triangle>& tris, const Vec3& col);
    ~Mesh() override;

    const char* type_name() const override { return "mesh"; }
    Vec3 get_color() const override { return color; }
    Vec3 get_center() const override {
        if (bvh_root) return (bvh_root->bbox.min + bvh_root->bbox.max) * 0.5;
        return Vec3(0,0,0);
    }
    bool hit(const Ray& r, double t_min, double t_max, HitRecord& rec) const override;
};

#endif
