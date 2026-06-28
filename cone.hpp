#ifndef CONE_H
#define CONE_H

#include "hittable.hpp"
#include <cmath>
#include <algorithm>

class Cone : public Hittable {
public:
    Vec3 center;
    double radius, height;
    Vec3 color;

    Cone(const Vec3& center, double radius, double height, const Vec3& color)
        : center(center), radius(radius), height(height), color(color) {}

    const char* type_name() const override { return "cone"; }
    Vec3 get_color() const override { return color; }

    bool hit(const Ray& r, double t_min, double t_max, HitRecord& rec) const override {
        Vec3 ro = r.origin() - center;
        Vec3 rd = r.direction();
        double half_h = height * 0.5;
        double k = radius / height;
        double A = half_h - ro.y;

        double a = rd.x*rd.x + rd.z*rd.z - k*k*rd.y*rd.y;
        double b = 2.0*(ro.x*rd.x + ro.z*rd.z) + 2.0*k*k*A*rd.y;
        double c = ro.x*ro.x + ro.z*ro.z - k*k*A*A;

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
        if (disc >= 0 && std::abs(a) > 1e-8) {
            double sqrtd = std::sqrt(disc);
            double t1 = (-b - sqrtd) / (2.0 * a);
            double t2 = (-b + sqrtd) / (2.0 * a);
            for (double t : {t1, t2}) {
                double y = ro.y + t * rd.y;
                if (y >= -half_h && y <= half_h) {
                    Vec3 p = ro + rd * t;
                    try_hit(t, unit_vector(Vec3(p.x, k*k*(half_h - y), p.z)));
                }
            }
        }

        if (std::abs(rd.y) > 1e-8) {
            double t = (-half_h - ro.y) / rd.y;
            Vec3 p = ro + rd * t;
            if (p.x * p.x + p.z * p.z <= radius * radius) {
                try_hit(t, Vec3(0, -1, 0));
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
