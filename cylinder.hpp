#ifndef CYLINDER_H
#define CYLINDER_H

#include "hittable.hpp"
#include <cmath>
#include <algorithm>

class Cylinder : public Hittable {
public:
    Vec3 center;
    double radius, height;
    Vec3 color;

    Cylinder(const Vec3& center, double radius, double height, const Vec3& color)
        : center(center), radius(radius), height(height), color(color) {}

    const char* type_name() const override { return "cylinder"; }
    Vec3 get_color() const override { return color; }

    bool hit(const Ray& r, double t_min, double t_max, HitRecord& rec) const override {
        Vec3 ro = r.origin() - center;
        Vec3 rd = r.direction();
        double half_h = height * 0.5;

        double a = rd.x * rd.x + rd.z * rd.z;
        double b = 2.0 * (ro.x * rd.x + ro.z * rd.z);
        double c = ro.x * ro.x + ro.z * ro.z - radius * radius;

        double t_hit = t_max;
        Vec3 hit_normal;

        auto try_hit = [&](double t, const Vec3& n) {
            if (t > t_min && t < t_hit) {
                t_hit = t;
                hit_normal = n;
                return true;
            }
            return false;
        };

        double disc = b*b - 4*a*c;
        if (disc >= 0 && a > 1e-8) {
            double sqrtd = std::sqrt(disc);
            double t1 = (-b - sqrtd) / (2.0 * a);
            double t2 = (-b + sqrtd) / (2.0 * a);
            for (double t : {t1, t2}) {
                double y = ro.y + t * rd.y;
                if (y >= -half_h && y <= half_h) {
                    try_hit(t, Vec3(ro.x + t * rd.x, 0, ro.z + t * rd.z) / radius);
                }
            }
        }

        for (int side : {-1, 1}) {
            if (std::abs(rd.y) < 1e-8) continue;
            double yc = side * half_h;
            double t = (yc - ro.y) / rd.y;
            Vec3 p = ro + rd * t;
            if (p.x * p.x + p.z * p.z <= radius * radius) {
                try_hit(t, Vec3(0, (double)side, 0));
            }
        }

        if (t_hit >= t_max) return false;

        rec.t = t_hit;
        rec.p = r.at(t_hit);
        rec.set_face_normal(r, hit_normal);
        rec.color = this->color;
        return true;
    }
};

#endif
