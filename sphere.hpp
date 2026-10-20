#ifndef SPHERE_H
#define SPHERE_H

#include "hittable.hpp"
#include <cmath>

class Sphere : public Hittable {
public:
    Vec3 center;
    double radius;
    Vec3 color;

    Sphere(const Vec3& center_, double radius_, const Vec3& color_)
        : center(center_), radius(radius_), color(color_) {}

    const char* type_name() const override { return "sphere"; }
    Vec3 get_color() const override { return color; }

    bool hit(const Ray& r, double t_min, double t_max, HitRecord& rec) const override {
        Ray lr(r.origin() - anim_offset_, r.direction());
        Vec3 oc = lr.origin() - center;
        double a = dot(lr.direction(), lr.direction());
        double b = 2.0 * dot(oc, lr.direction());
        double c = dot(oc, oc) - radius * radius;
        double discriminant = b*b - 4*a*c;
        if (discriminant < 0) return false;
        double sqrtd = std::sqrt(discriminant);
        double t = (-b - sqrtd) / (2.0 * a);
        if (t < t_min || t > t_max) {
            t = (-b + sqrtd) / (2.0 * a);
            if (t < t_min || t > t_max) return false;
        }
        rec.t = t;
        rec.p = lr.at(t) + anim_offset_;
        Vec3 outward_normal = (lr.at(t) - center) / radius;
        rec.set_face_normal(lr, outward_normal);
        rec.color = this->color;
        return true;
    }
};

#endif
