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
        grab_active_ = false;
        last_sx_ = sx; last_sy_ = sy;
        return;
    }
    last_hit_pos_ = hit_pos; has_last_hit_ = true; last_radius_used_ = brush.radius;
    last_sx_ = sx; last_sy_ = sy;

    float radius = brush.radius;
    float strength = brush.strength;
    if (brush.invert) strength *= -1.0f;
    if (!is_additive) strength *= -0.5f;

    auto& verts = active_->vertices();
    bool changed = false;
    
    // For Grab brush, track initial state
    if (brush.type == BrushType::Grab && !grab_active_) {
        grab_active_ = true;
        grab_center_ = hit_pos;
    }
    
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
            disp = unit_vector(cam.forward()) * (strength * w);
            break;
        case BrushType::Grab:
            // Simple grab: translate vertices along the movement of hit point
            if (has_last_hit_) {
                // Use the delta from when grab started? Or simpler approximation
                disp = ((hit_pos - last_hit_pos_).length() > 1e-8 ? (hit_pos - last_hit_pos_) : Vec3(0,0,0));
                disp = disp * strength * w * 0.5f;
            }
            break;
        case BrushType::Smooth: {
            // Average positions of adjacent vertices
            Vec3 avg(0,0,0);
            int cnt = 0;
            const auto& adj = active_->adj_faces();
            if (i < adj.size()) {
                // Collect unique neighbor vertex positions
                // For each adjacent face, get the other 2 verts
                for (int fi : adj[i]) {
                    const auto& f = active_->faces()[fi];
                    int nv[3] = {f.v0, f.v1, f.v2};
                    for (int j = 0; j < 3; j++) {
                        if (nv[j] != (int)i && nv[j] >= 0 && nv[j] < (int)verts.size()) {
                            avg += verts[nv[j]].pos;
                            cnt++;
                        }
                    }
                }
            }
            if (cnt > 0) {
                avg = avg / (double)cnt;
                disp = (avg - vpos) * (strength * w);
            }
            break;
        }
        case BrushType::Flatten: {
            // Compute average plane center and normal of vertices in radius
            Vec3 c(0,0,0), n(0,0,0);
            int k = 0;
            for (size_t j = 0; j < verts.size(); j++) {
                double dj = (verts[j].pos - hit_pos).length();
                if (dj <= radius) {
                    c += verts[j].pos;
                    n += verts[j].normal;
                    k++;
                }
            }
            if (k > 0) {
                c = c / (double)k;
                n = unit_vector(n / (double)k);
                // Project vertex onto plane (along normal direction)
                double dist_to_plane = dot(vpos - c, n);
                disp = -n * dist_to_plane * (strength * w);
            }
            break;
        }
        default:
            disp = verts[i].normal * (strength * w * 0.5f);
            break;
        }
        if (disp.length_sq() > 1e-12) {
            SculptStrokeOp op{(int)i, disp};
            current_stroke_.ops.push_back(op);
            verts[i].pos += disp;
            changed = true;
            // Apply mirror symmetry
            if (brush.mirror_x || brush.mirror_y || brush.mirror_z) {
                Vec3 orig_pos = vpos;
                Vec3 mir_pos = orig_pos;
                if (brush.mirror_x) mir_pos.x = -orig_pos.x;
                if (brush.mirror_y) mir_pos.y = -orig_pos.y;
                if (brush.mirror_z) mir_pos.z = -orig_pos.z;
                // Find closest vertex to mirrored position
                double bestd = 1e-3; // small threshold
                int bestv = -1;
                for (size_t j = 0; j < verts.size(); j++) {
                    if ((int)j == (int)i) continue;
                    Vec3 mp = verts[j].pos;
                    Vec3 t = mp;
                    if (brush.mirror_x) t.x = -mp.x;
                    if (brush.mirror_y) t.y = -mp.y;
                    if (brush.mirror_z) t.z = -mp.z;
                    // Check if this vertex maps to near orig_pos under mirror?
                    double d = (t - orig_pos).length();
                    if (d < bestd) {
                        bestd = d;
                        bestv = (int)j;
                    }
                }
                if (bestv >= 0) {
                    Vec3 mir_disp = disp;
                    if (brush.mirror_x) mir_disp.x = -disp.x;
                    if (brush.mirror_y) mir_disp.y = -disp.y;
                    if (brush.mirror_z) mir_disp.z = -disp.z;
                    SculptStrokeOp op2{bestv, mir_disp};
                    current_stroke_.ops.push_back(op2);
                    verts[bestv].pos += mir_disp;
                    changed = true;
                }
            }
        }
    }
    if (changed) {
        active_->rebuild_normals();
        active_->rebuild_bvh();
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
