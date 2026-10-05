#ifndef SCULPT_MESH_H
#define SCULPT_MESH_H

#include "hittable.hpp"
#include "brush.hpp"
#include <vector>
#include <string>
#include <functional>

struct SculptVertex {
    Vec3 pos;
    Vec3 normal;
};

struct SculptTriangle {
    int v0, v1, v2;
};

struct BN {
    float bmin[3], bmax[3];
    int start, count, left, right, axis;
    bool leaf;
};

struct FN2 {
    float bmin[3], bmax[3];
    int leftFirst, rightFirst, count, start;
    unsigned char axis;
    bool leaf;
};

class SculptMesh : public Hittable {
public:
    SculptMesh();
    ~SculptMesh() override = default;

    static SculptMesh* create_sphere(const Vec3& center, double radius, int segments, int rings, const Vec3& color);
    static SculptMesh* create_cube(const Vec3& center, double size, const Vec3& color);
    static SculptMesh* create_plane(const Vec3& center, double size, int divisions, const Vec3& color);
    static SculptMesh* create_torus(const Vec3& center, double major, double minor, int major_segs, int minor_segs, const Vec3& color);

    std::vector<SculptVertex>& vertices() { return verts_; }
    const std::vector<SculptVertex>& vertices() const { return verts_; }
    std::vector<SculptTriangle>& faces() { return faces_; }
    const std::vector<SculptTriangle>& faces() const { return faces_; }

    void set_color(const Vec3& c) { color_ = c; }
    Vec3 get_color() const override { return color_; }
    Vec3 get_center() const override;
    const char* type_name() const override { return "sculpt"; }
    std::string mesh_path() const override { return path_; }
    void set_path(const std::string& p) { path_ = p; }

    void rebuild_normals();
    void rebuild_topology(bool recompute_adjacency = true);
    void rebuild_bvh();

    bool hit(const Ray& r, double t_min, double t_max, HitRecord& rec) const override;
    bool export_obj(const std::string& filename) const;
    void translate(const Vec3& delta);

    const std::vector<std::vector<int>>& adj_faces() const { return adj_faces_; }

private:
    std::vector<SculptVertex> verts_;
    std::vector<SculptTriangle> faces_;
    std::vector<std::vector<int>> adj_faces_;
    std::vector<BN> bnodes_;
    std::vector<FN2> fn2_;
    std::vector<int> tri_list_;
    Vec3 color_;
    std::string path_;
};

#endif
