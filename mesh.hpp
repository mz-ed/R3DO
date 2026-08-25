#ifndef MESH_H
#define MESH_H

#include "hittable.hpp"
#include <vector>

struct Triangle {
    Vec3 v0, v1, v2;
};

bool ray_tri_intersect(const Ray& r, const Vec3& v0, const Vec3& v1, const Vec3& v2,
                       double t_min, double t_max, HitRecord& rec);

class Mesh : public Hittable {
public:
    std::string path_;
    Vec3 color;

    explicit Mesh(std::vector<Triangle> tris, const Vec3& col);
    ~Mesh() override = default;
    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;

    const char* type_name() const override { return "mesh"; }
    Vec3 get_color() const override { return color; }
    std::string mesh_path() const override { return path_; }
    void set_path(const std::string& p) { path_ = p; }
    Vec3 get_center() const override;
    bool hit(const Ray& r, double t_min, double t_max, HitRecord& rec) const override;

    std::vector<Triangle>& get_triangles() { return triangles_; }
    const std::vector<Triangle>& get_triangles() const { return triangles_; }

    struct FlatNode {
        float bmin[3], bmax[3];
        int leftFirst;  // leaf: first triangle index; internal: left child index
        int count;      // 0 = internal node; >0 = leaf with count triangles
        unsigned char axis; // split axis for internal nodes
    };

private:
    std::vector<Triangle> triangles_;
    std::vector<FlatNode> nodes_;

    // SoA float data for SSE intersection
    // Layout per packet of 4 triangles (36 floats):
    //   v0x[4], v0y[4], v0z[4], e1x[4], e1y[4], e1z[4], e2x[4], e2y[4], e2z[4]
    std::vector<float> soa_;

    int build_sah(std::vector<Triangle>& tris, int start, int count);
    void build_soa();
};

#endif
