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
    void translate(const Vec3& d) override;
    bool hit(const Ray& r, double t_min, double t_max, HitRecord& rec) const override;

    std::vector<Triangle>& get_triangles() { return triangles_; }
    const std::vector<Triangle>& get_triangles() const { return triangles_; }
    void rebuild_bvh();

    // ---- skeleton support ----
    void bind_skeleton(Skeleton* sk) override;
    void set_bone_pose(int bone, const Vec3& rot, const Vec3& off) override;
    void apply_bones() override;
    void reset_bones() override;
    double bone_span() const override;
    void remap_bone_binds(int removed, int replacement) override;

    struct FlatNode {
        float bmin[3], bmax[3];
        int leftFirst;  // leaf: first triangle index; internal: left child index
        int count;      // 0 = internal node; >0 = leaf with count triangles
        unsigned char axis; // split axis for internal nodes
    };

private:
    std::vector<Triangle> triangles_;
    std::vector<FlatNode> nodes_;

    // bind pose snapshot (filled by bind_skeleton): triangle copy plus the
    // three joint bindings per corner
    struct Bind {
        std::vector<Triangle> tris;
        std::vector<int> bone;    // 3 entries per triangle
    };
    std::unique_ptr<Bind> bind_;
    bool bvh_dirty_ = false;
    double last_bvh_rebuild_ = 0.0;

    // SoA float data for SSE intersection
    // Layout per packet of 4 triangles (36 floats):
    //   v0x[4], v0y[4], v0z[4], e1x[4], e1y[4], e1z[4], e2x[4], e2y[4], e2z[4]
    std::vector<float> soa_;

    int build_sah(std::vector<Triangle>& tris, int start, int count);
    void build_soa();
};

#endif
