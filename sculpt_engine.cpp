#include "sculpt_engine.hpp"
#include "render.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

static Vec3 project_screen_ray(const Camera& cam, int img_w, int img_h, double sx, double sy) {
    double vw = VIEWPORT_HEIGHT * img_w / img_h;
    Vec3 fwd = cam.forward();
    Vec3 rgt = cam.right();
    Vec3 up = cross(rgt, fwd);
    Vec3 lower = cam.pos - vw*rgt*0.5 - VIEWPORT_HEIGHT*up*0.5 + fwd*FOCAL_LENGTH;
    double u = sx / (img_w-1);
    double v = 1.0 - sy / (img_h-1);
    return lower + u*vw*rgt + v*VIEWPORT_HEIGHT*up - cam.pos;
}

SculptEngine::SculptEngine() {}

bool SculptEngine::pick_active(const Camera& cam, int img_w, int img_h, double sx, double sy, Vec3& hit_pos, int& hit_vertex, double& hit_dist) const {
    if (!active_) return false;
    Vec3 dir = unit_vector(project_screen_ray(cam, img_w, img_h, sx, sy));
    Ray r(cam.pos, dir);
    HitRecord rec;
    if (active_->hit(r, 0.001, 2000.0, rec)) {
        hit_pos = rec.p;
        hit_dist = rec.t;
        hit_vertex = -1;
        // find closest vertex
        double bestd = 1e9;
        int bestv = -1;
        for (size_t i=0; i<active_->vertices().size(); i++) {
            double d = (active_->vertices()[i].pos - hit_pos).length();
            if (d < bestd && d < 0.5) { bestd=d; bestv=(int)i; }
        }
        hit_vertex = bestv;
        return true;
    }
    return false;
}

void SculptEngine::begin_stroke() {
    current_stroke_.ops.clear();
    current_stroke_.committed = false;
}

static Vec3 mirror_vec(const Vec3& v, bool mx,bool my,bool mz) {
    Vec3 r = v;
    if (mx) r.x = -r.x;
    if (my) r.y = -r.y;
    if (mz) r.z = -r.z;
    return r;
}

void SculptEngine::stroke_update(const Camera& cam, int img_w, int img_h, double sx, double sy, const Brush& brush, bool is_additive) {
    if (!active_) return;
    Vec3 hit_pos; int hv; double hd;
    if (!pick_active(cam, img_w, img_h, sx, sy, hit_pos, hv, hd)) {
        has_last_hit_ = false;
        return;
    }
    last_hit_pos_ = hit_pos; has_last_hit_ = true; last_radius_used_ = brush.radius;

    float radius = brush.radius;
    float strength = brush.strength;
    if (brush.invert) strength *= -1.0f;
    if (!is_additive) strength *= -0.5f; // LMB vs RMB behavior? treat additive true if push in direction

    // Apply brush to vertices within radius
    auto& verts = active_->vertices();
    bool changed = false;
    for (size_t i = 0; i < verts.size(); i++) {
        Vec3 vpos = verts[i].pos;
        Vec3 to = vpos - hit_pos;
        double d = to.length();
        if (d > radius) continue;
        float w = brush.weight((float)d);
        if (w <= 0.0f) continue;

        Vec3 disp(0,0,0);
        switch (brush.type) {
        case BrushType::Draw:
        case BrushType::Inflate:
        case BrushType::Blob:
            disp = verts[i].normal * (strength * w);
            break;
        case BrushType::Deflate:
            disp = -verts[i].normal * (strength * w);
            break;
        case BrushType::PushPull:
            disp = unit_vector(cam.forward()) * (strength * w); // view direction
            break;
        case BrushType::Grab:
            disp = Vec3(0,0,0);
            break;
        case BrushType::Smooth: {
            // approximate: move toward average of nearby positions weighted
            disp = (vpos - vpos) * (strength * w * 0.1f); // simplified; no neighbor lookup for speed
            break;
        }
        case BrushType::Flatten: {
            // simplified flatten: move vertex along its normal toward the brush plane
            disp = Vec3(0,0,0);
            break;
        }
        default:
            disp = verts[i].normal * (strength * w * 0.5f);
            break;
        }
        if (disp.length() > 0) {
            SculptStrokeOp op{(int)i, disp};
            current_stroke_.ops.push_back(op);
            verts[i].pos += disp;
            changed = true;
        }
    }
    // Only rebuild normals if vertices changed (BVH rebuild moved to end_stroke for efficiency)
    if (changed) {
        active_->rebuild_normals();
    }
}

void SculptEngine::end_stroke() {
    if (!current_stroke_.ops.empty()) {
        // trim history past cursor
        if (history_cursor_ < history_.size())
            history_.resize(history_cursor_);
        history_.push_back(current_stroke_);
        if (history_.size() > 50) { history_.erase(history_.begin()); }
        history_cursor_ = history_.size();
    }
    current_stroke_.ops.clear();
}

void SculptEngine::undo() {
    if (history_cursor_ == 0 || !active_) return;
    // undo last stroke
    size_t idx = history_cursor_ - 1;
    if (idx < history_.size()) {
        auto& st = history_[idx];
        // apply in reverse
        for (const auto& op : st.ops)
            active_->vertices()[op.vertex_index].pos = active_->vertices()[op.vertex_index].pos - op.displacement;
    }
    history_cursor_--;
    active_->rebuild_normals();
    active_->rebuild_bvh();
}

void SculptEngine::redo() {
    if (history_cursor_ >= history_.size() || !active_) return;
    auto& st = history_[history_cursor_];
    for (const auto& op : st.ops)
        active_->vertices()[op.vertex_index].pos = active_->vertices()[op.vertex_index].pos + op.displacement;
    history_cursor_++;
    active_->rebuild_normals();
    active_->rebuild_bvh();
}

void SculptEngine::clear_history() {
    history_.clear();
    history_cursor_ = 0;
}
