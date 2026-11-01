#ifndef HEIGHTFIELD_H
#define HEIGHTFIELD_H

#include "hittable.hpp"
#include <string>
#include <vector>
#include <memory>

// Grid-based terrain backed by a PNG heightmap (grayscale 8-bit; luminance
// -> height). Ray-hit is a ray march over bilinearly sampled columns, so
// painting only rewrites a few floats - no acceleration rebuild like Mesh.
//
// World mapping (square field):
//   origin + (x, 0, z) with x,z in [0, side).
// Image top row (y=0) maps to +z (far edge from the default eye); bottom
// row maps to the near edge. Heights split the range [y_min, y_max].
class HeightField : public Hittable {
public:
    HeightField(double origin_x, double origin_z, double side, int n,
                const std::vector<float>& heights, const Vec3& col);
    ~HeightField() override = default;

    const char* type_name() const override { return "heightfield"; }
    Vec3 get_color() const override { return color; }
    Vec3 get_center() const override;
    void translate(const Vec3& d) override;
    bool hit(const Ray& r, double t_min, double t_max, HitRecord& rec) const override;

    std::string mesh_path() const override { return path_; }
    void set_path(const std::string& p) { path_ = p; }

    // Bilinear surface height at a world (x, z) position.
    double height_at(double x, double z) const;
    // World-space footprint (for clipping / drawing).
    double origin_x() const { return origin_x_; }
    double origin_z() const { return origin_z_; }
    double side() const { return side_; }
    double max_height() const { return y_max_; }
    int hf_res() const { return n_; }
    // Column (i, k): world center + height there.
    void column_sample(int i, int k, double& hy, double& x, double& z) const {
        x = origin_x_ + (i + 0.5) * cell_;
        z = origin_z_ + (k + 0.5) * cell_;
        hy = h_[(size_t)k * n_ + i];
    }

    // Brush: amount > 0 raises, < 0 lowers. mode 0 = add, 1 = smooth.
    void paint(double x, double z, double radius, double amount, int mode);

    // Write the map back out as a grayscale PNG (paint persistence).
    bool export_png(const std::string& path) const;

    // Build from a PNG heightmap covering the given footprint. Returns
    // nullptr on failure (bad file / corrupt image).
    static std::unique_ptr<HeightField> from_png(const std::string& path,
                                                 double origin_x, double origin_z,
                                                 double side,
                                                 double y_min, double y_max,
                                                 const Vec3& col,
                                                 int max_res = 160);

    // Flat terrain at y_min - the blank canvas for painting.
    static std::unique_ptr<HeightField> flat(double origin_x, double origin_z,
                                             double side, double y_min,
                                             const Vec3& col,
                                             std::string blank_path = "");

private:
    std::string path_;
    Vec3 color;
    double origin_x_, origin_z_, side_;
    double y_min_, y_max_;
    int n_;              // columns per side
    double cell_;        // world units per column
    std::vector<float> h_; // n_ * n_ column heights (world y at centers)

    std::pair<double, double> march_bounds(const Ray& r, double t_min,
                                           double t_max) const;
    Vec3 normal_at(double x, double z) const;
    unsigned int height_tint(double hy, double slope) const;
};

#endif
