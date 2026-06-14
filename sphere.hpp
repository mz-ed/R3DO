#ifndef SPHERE_H
#define SPHERE_H

#include "hittable.hpp"
#include <cmath>

class Sphere : public Hittable {
public:
    Vec3 center;
    double radius;
    Vec3 color;

    Sphere(const Vec3& center, double radius, const Vec3& color)
        : center(center), radius(radius), color(color) {}

    bool hit(const Ray& r, double t_min, double t_max, HitRecord& rec) const override {
        Vec3 oc = r.origin() - center;
        double a = dot(r.direction(), r.direction());
        double b = 2.0 * dot(oc, r.direction());
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
        rec.p = r.at(t);
        Vec3 outward_normal = (rec.p - center) / radius;
        rec.set_face_normal(r, outward_normal);
        rec.color = this->color;
        return true;
    }
};

#endif
