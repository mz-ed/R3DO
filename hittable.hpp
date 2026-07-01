#ifndef HITTABLE_H
#define HITTABLE_H

#include "v3.hpp"
#include "ray.hpp"

struct HitRecord {
    Vec3 p;
    Vec3 normal;
    double t;
    bool front_face;
    Vec3 color;

    void set_face_normal(const Ray& r, const Vec3& outward_normal) {
        front_face = dot(r.direction(), outward_normal) < 0;
        normal = front_face ? outward_normal : -outward_normal;
    }
};

class Hittable {
public:
    bool visible_ = true;

    virtual ~Hittable() = default;
    virtual bool hit(const Ray& r, double t_min, double t_max, HitRecord& rec) const = 0;
    virtual const char* type_name() const { return "unknown"; }
    virtual Vec3 get_color() const { return Vec3(0,0,0); }
    bool is_visible() const { return visible_; }
    void set_visible(bool v) { visible_ = v; }
};

#endif
