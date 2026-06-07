#include "v3.hpp"
#include "ray.hpp"
#include <iostream>
// 1. Sphere hit function
bool hit_sphere(const Vec3& center, double radius, const Ray& r) {
    // Solve quadratic: t²(B·B) + 2t(B·(A-C)) + (A-C)·(A-C) - r² = 0
    Vec3 oc = r.origin() - center;           // A - C
    double a = dot(r.direction(), r.direction());
    double b = 2.0 * dot(oc, r.direction());
    double c = dot(oc, oc) - radius*radius;
    double discriminant = b*b - 4*a*c;
    return discriminant > 0;
}
// 2. Ray color function
Vec3 ray_color(const Ray& r) {
    Vec3 sphere_center(0, 0, -1);
    if (hit_sphere(sphere_center, 0.5, r))
        return Vec3(1, 0, 0);  // red sphere
    // Sky gradient: lerp white → blue based on y direction
    Vec3 unit_dir = unit_vector(r.direction());
    double t = 0.5 * (unit_dir.y + 1.0);
    return (1.0 - t) * Vec3(1, 1, 1) + t * Vec3(0.5, 0.7, 1.0);
}
// 3. Main
int main() {
    // Image config
    int image_width = 400;
    int image_height = 225;
    // PPM header
    std::cout << "P3\n" << image_width << ' ' << image_height << "\n255\n";
    // Camera
    double viewport_height = 2.0;
    double viewport_width = viewport_height * image_width / image_height;
    double focal_length = 1.0;
    Vec3 origin(0, 0, 0);
    Vec3 horizontal(viewport_width, 0, 0);
    Vec3 vertical(0, viewport_height, 0);
    Vec3 lower_left = origin - horizontal/2 - vertical/2 - Vec3(0, 0, focal_length);
    // Render loop (rows top→bottom, cols left→right)
    for (int j = image_height-1; j >= 0; --j) {
        for (int i = 0; i < image_width; ++i) {
            double u = double(i) / (image_width - 1);
            double v = double(j) / (image_height - 1);
            Ray r(origin, lower_left + u*horizontal + v*vertical - origin);
            Vec3 color = ray_color(r);
            // Write pixel as 0-255 RGB
            int ir = int(255.999 * color.x);
            int ig = int(255.999 * color.y);
            int ib = int(255.999 * color.z);
            std::cout << ir << ' ' << ig << ' ' << ib << '\n';
        }
    }
}
