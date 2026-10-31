#ifndef BOX_H
#define BOX_H

#include "hittable.hpp"
#include <algorithm>
#include <limits>

class Box : public Hittable {
public:
    Vec3 min, max;
    Vec3 color;

    Box(const Vec3& min_, const Vec3& max_, const Vec3& color_)
        : min(min_), max(max_), color(color_) {}

    const char* type_name() const override { return "box"; }
    Vec3 get_color() const override { return color; }
    Vec3 get_center() const override { return (min + max) * 0.5 + anim_offset(); }
    void translate(const Vec3& d) override { min += d; max += d; }

    bool hit(const Ray& r, double t_min, double t_max, HitRecord& rec) const override {
        if (t_max <= t_min) return false;
        Ray lr(r.origin() - anim_offset_, r.direction());
        Vec3 pivot = (min + max) * 0.5;
        bool xf = anim_xform_active();
        if (xf) lr = anim_local_ray(r, pivot);
        double tmin = t_min;
        double tmax = t_max;
        int hit_axis = 0;
        double hit_sign = 1;

        for (int a = 0; a < 3; a++) {
            double d = (a == 0) ? lr.direction().x : (a == 1) ? lr.direction().y : lr.direction().z;
            double o = (a == 0) ? lr.origin().x : (a == 1) ? lr.origin().y : lr.origin().z;
            double minv = (a == 0) ? min.x : (a == 1) ? min.y : min.z;
            double maxv = (a == 0) ? max.x : (a == 1) ? max.y : max.z;

            if (std::abs(d) < 1e-8) {
                if (o < minv || o > maxv) return false;
                continue;
            }

            double t0 = (minv - o) / d;
            double t1 = (maxv - o) / d;
            if (d < 0) std::swap(t0, t1);

            if (t0 > tmin) { tmin = t0; hit_axis = a; hit_sign = (d > 0) ? -1 : 1; }
            if (t1 < tmax) tmax = t1;
            if (tmax <= tmin) return false;
        }

        rec.t = tmin;
        Vec3 outward_normal(0, 0, 0);
        if (hit_axis == 0) outward_normal.x = hit_sign;
        else if (hit_axis == 1) outward_normal.y = hit_sign;
        else outward_normal.z = hit_sign;
        if (xf) {
            rec.p = anim_world_point(lr.at(tmin), pivot);
            rec.set_face_normal(r, anim_world_normal(outward_normal));
        } else {
            rec.p = lr.at(tmin) + anim_offset_;
            rec.set_face_normal(r, outward_normal);
        }
        rec.color = this->color;
        anim_color(rec);
        return true;
    }
};

#endif
