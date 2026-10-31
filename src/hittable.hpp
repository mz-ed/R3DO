#ifndef HITTABLE_H
#define HITTABLE_H

#include "v3.hpp"
#include "ray.hpp"
#include <string>
#include <cmath>

struct HitRecord {
    Vec3 p;
    Vec3 normal;
    double t;
    bool front_face;
    Vec3 color;
    class Hittable* hittable = nullptr;

    void set_face_normal(const Ray& r, const Vec3& outward_normal) {
        front_face = dot(r.direction(), outward_normal) < 0;
        normal = front_face ? outward_normal : -outward_normal;
    }
};

// ---- animation transform math ----
// Euler rotation (radians) composed as M = Rz(z) * Ry(y) * Rx(x).
// Pose of a native point p about its pivot c:
//     world = anim_offset + c + M * (S * (p - c))
// The matching local ray keeps the ray parameter t world-consistent:
//     local(day.dir) = Mt * (d / s), local(o) = c + Mt * ((o - c - off) / s)
inline void anim_euler_matrix(const Vec3& e, double M[3][3], double MT[3][3]) {
    double cx = std::cos(e.x), sx = std::sin(e.x);
    double cy = std::cos(e.y), sy = std::sin(e.y);
    double cz = std::cos(e.z), sz = std::sin(e.z);
    M[0][0] = cz*cy; M[0][1] = cz*sy*sx - sz*cx; M[0][2] = cz*sy*cx + sz*sx;
    M[1][0] = sz*cy; M[1][1] = sz*sy*sx + cz*cx; M[1][2] = sz*sy*cx - cz*sx;
    M[2][0] = -sy;   M[2][1] = cy*sx;            M[2][2] = cy*cx;
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            MT[i][j] = M[j][i];
}

inline Vec3 anim_mat_mul(const double M[3][3], const Vec3& v) {
    return Vec3(
        M[0][0]*v.x + M[0][1]*v.y + M[0][2]*v.z,
        M[1][0]*v.x + M[1][1]*v.y + M[1][2]*v.z,
        M[2][0]*v.x + M[2][1]*v.y + M[2][2]*v.z);
}

class Hittable {
public:
    bool visible_ = true;
    Vec3 anim_offset_{0, 0, 0};
    Vec3 anim_rot_{0, 0, 0};       // radians, XYZ euler
    Vec3 anim_scale_{1, 1, 1};     // multipliers per axis
    Vec3 anim_tint_{1, 1, 1};      // RGB multipliers for the render color
    double rot_m_[3][3] = {};      // forward rotation
    double rot_mt_[3][3] = {};     // transpose (= inverse, orthonormal)

    virtual ~Hittable() = default;
    virtual bool hit(const Ray& r, double t_min, double t_max, HitRecord& rec) const = 0;
    virtual const char* type_name() const { return "unknown"; }
    virtual Vec3 get_color() const { return Vec3(0,0,0); }
    virtual Vec3 get_center() const { return anim_offset_; }
    virtual void translate(const Vec3& d) { (void)d; }
    virtual std::string mesh_path() const { return ""; }
    bool is_visible() const { return visible_; }
    void set_visible(bool v) { visible_ = v; }

    void set_anim_offset(const Vec3& o) { anim_offset_ = o; }
    Vec3 anim_offset() const { return anim_offset_; }

    void set_anim_rot(const Vec3& e) {
        anim_rot_ = e;
        anim_euler_matrix(e, rot_m_, rot_mt_);
        rot_active_ = e.x != 0.0 || e.y != 0.0 || e.z != 0.0;
    }
    Vec3 anim_rot() const { return anim_rot_; }

    void set_anim_scale(const Vec3& s) {
        // clamp so normals/tangents never degenerate
        for (int i = 0; i < 3; i++) {
            double v = (&s.x)[i];
            if (v < 0.01) v = 0.01;
            if (v > 50.0) v = 50.0;
            (&anim_scale_.x)[i] = v;
        }
    }
    Vec3 anim_scale() const { return anim_scale_; }

    void set_anim_tint(const Vec3& t) { anim_tint_ = t; }
    Vec3 anim_tint() const { return anim_tint_; }

    // True when rot/scale need the full transform path in hit(). Pure
    // translation keeps using the cheap offset-only fast paths.
    bool anim_xform_active() const {
        return rot_active_ ||
               anim_scale_.x != 1.0 || anim_scale_.y != 1.0 || anim_scale_.z != 1.0;
    }

    // Native-space ray for a shape whose base pivot is c (object center
    // without animation). Direction is scaled (not normalized) so the hit
    // parameter t stays aligned with the world ray.
    Ray anim_local_ray(const Ray& r, const Vec3& c) const {
        Vec3 s_inv = Vec3(1.0 / anim_scale_.x, 1.0 / anim_scale_.y, 1.0 / anim_scale_.z);
        Vec3 lo = anim_mat_mul(rot_mt_, (r.origin() - c - anim_offset_) * s_inv) + c;
        Vec3 ld = anim_mat_mul(rot_mt_, r.direction() * s_inv);
        return Ray(lo, ld);
    }
    Vec3 anim_world_point(const Vec3& p, const Vec3& c) const {
        return anim_offset_ + c + anim_mat_mul(rot_m_, (p - c) * anim_scale_);
    }
    Vec3 anim_world_normal(const Vec3& n) const {
        Vec3 s_inv = Vec3(1.0 / anim_scale_.x, 1.0 / anim_scale_.y, 1.0 / anim_scale_.z);
        return unit_vector(anim_mat_mul(rot_m_, n * s_inv));
    }
    void anim_color(HitRecord& rec) const {
        if (anim_tint_.x != 1.0 || anim_tint_.y != 1.0 || anim_tint_.z != 1.0)
            rec.color = Vec3(rec.color.x * anim_tint_.x,
                             rec.color.y * anim_tint_.y,
                             rec.color.z * anim_tint_.z);
    }

private:
    bool rot_active_ = false;
};

#endif
