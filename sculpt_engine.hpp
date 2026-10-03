#ifndef SCULPT_ENGINE_H
#define SCULPT_ENGINE_H

#include "sculpt_mesh.hpp"
#include "brush.hpp"
#include "ray.hpp"
#include "camera.hpp"
#include <vector>
#include <functional>
#include <memory>

struct SculptStrokeOp {
    int vertex_index;
    Vec3 displacement;
};

struct SculptStroke {
    std::vector<SculptStrokeOp> ops;
    bool committed = false;
};

class SculptEngine {
public:
    SculptEngine();
    void set_active(SculptMesh* m) { active_ = m; }
    SculptMesh* active() const { return active_; }

    // Picking: raycast from camera through screen coords (normalized or pixel)
    bool pick_active(const Camera& cam, int img_w, int img_h, double sx, double sy, Vec3& hit_pos, int& hit_vertex, double& hit_dist) const;

    // Stroke
    void begin_stroke();
    void stroke_update(const Camera& cam, int img_w, int img_h, double sx, double sy, const Brush& brush, bool is_additive);
    void end_stroke();
    void undo();
    void redo();
    void clear_history();

private:
    SculptMesh* active_ = nullptr;
    std::vector<SculptStroke> history_;
    size_t history_cursor_ = 0;
    SculptStroke current_stroke_;
    Vec3 last_hit_pos_;
    bool has_last_hit_ = false;
    float last_radius_used_ = 0.0f;

    Vec3 get_normal_averaged(int v) const;
};

#endif
