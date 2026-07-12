#include "mesh.hpp"
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
