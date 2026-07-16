#include "mesh.hpp"
#include <algorithm>
#include <cmath>

bool ray_tri_intersect(const Ray& r, const Vec3& v0, const Vec3& v1, const Vec3& v2,
                       double t_min, double t_max, HitRecord& rec) {
    const double EPS = 1e-10;
    Vec3 e1 = v1 - v0;
    Vec3 e2 = v2 - v0;
    Vec3 pvec = cross(r.direction(), e2);
    double det = dot(e1, pvec);
    if (std::abs(det) < EPS) return false;
    double inv_det = 1.0 / det;
    Vec3 tvec = r.origin() - v0;
    double u = dot(tvec, pvec) * inv_det;
    if (u < 0 || u > 1) return false;
    Vec3 qvec = cross(tvec, e1);
    double v = dot(r.direction(), qvec) * inv_det;
    if (v < 0 || u + v > 1) return false;
    double t = dot(e2, qvec) * inv_det;
    if (t < t_min || t > t_max) return false;
    rec.t = t;
    rec.p = r.at(t);
    Vec3 n = cross(e1, e2);
    rec.set_face_normal(r, n);
    return true;
}

// --- AABB --- //
void AABB::expand(const Vec3& p) {
    if (p.x < min.x) min.x = p.x; if (p.x > max.x) max.x = p.x;
    if (p.y < min.y) min.y = p.y; if (p.y > max.y) max.y = p.y;
    if (p.z < min.z) min.z = p.z; if (p.z > max.z) max.z = p.z;
}

void AABB::expand(const AABB& o) { expand(o.min); expand(o.max); }

bool AABB::hit(const Ray& r, double t_min, double t_max) const {
    for (int a = 0; a < 3; a++) {
        double invD = 1.0 / r.direction()[a];
        double t0 = (min[a] - r.origin()[a]) * invD;
        double t1 = (max[a] - r.origin()[a]) * invD;
        if (invD < 0) std::swap(t0, t1);
        t_min = std::max(t0, t_min);
        t_max = std::min(t1, t_max);
        if (t_max <= t_min) return false;
    }
    return true;
}

// --- BVH build --- //
static AABB tri_bbox(const Triangle& t) {
    AABB b; b.expand(t.v0); b.expand(t.v1); b.expand(t.v2);
    return b;
}

static double centroid_axis(const Triangle& t, int axis) {
    return (t.v0[axis] + t.v1[axis] + t.v2[axis]) / 3.0;
}

static void build_bvh_rec(std::vector<Triangle>& tris, int start, int end, BVHNode* node) {
    AABB bbox;
    for (int i = start; i < end; i++)
        bbox.expand(tri_bbox(tris[i]));
    node->bbox = bbox;

    int count = end - start;
    if (count <= 8) {
        node->tri_start = start;
        node->tri_end = end;
        return;
    }

    Vec3 diag = bbox.max - bbox.min;
    int axis = 0;
    if (diag.y > diag.x) axis = 1;
    if (diag.z > diag[axis]) axis = 2;

    std::sort(tris.begin() + start, tris.begin() + end,
        [axis](const Triangle& a, const Triangle& b) {
            return centroid_axis(a, axis) < centroid_axis(b, axis);
        });

    int mid = (start + end) / 2;
    node->left = new BVHNode;
    node->right = new BVHNode;
    build_bvh_rec(tris, start, mid, node->left);
    build_bvh_rec(tris, mid, end, node->right);
}

static BVHNode* build_bvh(std::vector<Triangle>& tris) {
    if (tris.empty()) return nullptr;
    BVHNode* root = new BVHNode;
    build_bvh_rec(tris, 0, tris.size(), root);
    return root;
}

// --- Mesh --- //
Mesh::Mesh(const std::vector<Triangle>& tris, const Vec3& col)
    : triangles(tris), color(col) {
    bvh_root = build_bvh(triangles);
}

Mesh::~Mesh() { delete bvh_root; }

static bool hit_bvh_node(const Ray& r, double t_min, double t_max, HitRecord& rec,
                         const BVHNode* node, const std::vector<Triangle>& tris) {
    if (!node->bbox.hit(r, t_min, t_max)) return false;

    if (!node->left && !node->right) {
        bool hit_any = false;
        double closest = t_max;
        HitRecord temp;
        for (int i = node->tri_start; i < node->tri_end; i++) {
            auto& tri = tris[i];
            if (ray_tri_intersect(r, tri.v0, tri.v1, tri.v2, t_min, closest, temp)) {
                hit_any = true;
                closest = temp.t;
                rec = temp;
            }
        }
        return hit_any;
    }

    bool hit_left = node->left && hit_bvh_node(r, t_min, t_max, rec, node->left, tris);
    double mid = hit_left ? rec.t : t_max;
    bool hit_right = node->right && hit_bvh_node(r, t_min, mid, rec, node->right, tris);
    return hit_left || hit_right;
}

bool Mesh::hit(const Ray& r, double t_min, double t_max, HitRecord& rec) const {
    if (!bvh_root) return false;
    if (hit_bvh_node(r, t_min, t_max, rec, bvh_root, triangles)) {
        rec.color = color;
        return true;
    }
    return false;
}
