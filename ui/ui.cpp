#include "ui.hpp"
#include "saver.hpp"
#include "sphere.hpp"
#include "box.hpp"
#include "cylinder.hpp"
#include "cone.hpp"
#include "obj_loader.hpp"
#include "heightfield.hpp"
#include "sculpt_mesh.hpp"
#include "shapes/mesh.hpp"
#include "render.hpp"
#include "overhead/pick.hpp"
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstring>
#include <filesystem>

namespace fs = std::filesystem;

double UI::now_s() const {
    return std::chrono::duration<double>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

UI::UI(Grid& g, Camera& c, DisplayWin& dsp, SculptEngine* sculpt)
    : grid(g), cam(c), display(dsp), sculpt_(sculpt) {
    scan_meshes();
    scan_hmaps();
    update_labels();
}

void UI::update_labels() {
    // Keep the Nuklear combo selection in sync with the active render mode
    // (the sidebar label is looked up via mode_label_idx_).
    if (render_mode_ < 0) render_mode_ = 0;
    if (render_mode_ >= mode_count()) render_mode_ = mode_count() - 1;
    mode_label_idx_ = render_mode_;
}

void UI::scan_meshes() {
    mesh_files_.clear();
    std::error_code ec;
    if (!fs::exists("models")) {
        fs::create_directory("models", ec);
        return;
    }
    for (const auto& entry : fs::directory_iterator("models")) {
        if (!entry.is_regular_file()) continue;
        auto p = entry.path();
        if (p.extension() == ".obj")
            mesh_files_.push_back(p.string());
    }
    std::sort(mesh_files_.begin(), mesh_files_.end());
    if (mesh_idx_ >= (int)mesh_files_.size()) mesh_idx_ = 0;
}

void UI::cycle_mesh(int dir) {
    if (mesh_files_.empty()) return;
    mesh_idx_ = (mesh_idx_ + dir + (int)mesh_files_.size()) % (int)mesh_files_.size();
}

void UI::set_brush_radius(float r) {
    if (r < 0.05f) r = 0.05f;
    if (r > 2.0f) r = 2.0f;
    brush_radius_ = r;
    char buf[64]; snprintf(buf, sizeof(buf), "Radius: %.2f", brush_radius_);
    show_msg(buf);
}

void UI::set_brush_strength(float s) {
    if (s < 0.05f) s = 0.05f;
    if (s > 2.0f) s = 2.0f;
    brush_strength_ = s;
    char buf[64]; snprintf(buf, sizeof(buf), "Strength: %.2f", brush_strength_);
    show_msg(buf);
}

void UI::set_sculpt_res(int segs, int rings) {
    if (segs < 8) segs = 8;
    if (segs > 64) segs = 64;
    if (rings < 6) rings = 6;
    if (rings > 48) rings = 48;
    sculpt_segs_ = segs;
    sculpt_rings_ = rings;
    char buf[64]; snprintf(buf, sizeof(buf), "Res: %dx%d", sculpt_segs_, sculpt_rings_);
    show_msg(buf);
}

Vec3 UI::palette_color(int index) const {
    static const Vec3 palette[] = {
        Vec3(1, 0.2, 0.2), Vec3(0.2, 1, 0.3), Vec3(1, 1, 0.2),
        Vec3(1, 0.5, 0), Vec3(0.2, 0.4, 1), Vec3(0.9, 0.2, 0.9),
        Vec3(0.2, 0.9, 0.9), Vec3(0.8, 0.6, 0.2), Vec3(0.5, 0.3, 1),
        Vec3(1, 0.6, 0.6)
    };
    return palette[index % 10];
}

void UI::push_undo() {
    undo_stack_.push_back(serialize_scene(grid, cam, render_mode_, palette_idx_));
    if (undo_stack_.size() > 50)
        undo_stack_.erase(undo_stack_.begin());
}

bool UI::undo() {
    if (undo_stack_.empty()) return false;
    std::string snapshot = std::move(undo_stack_.back());
    undo_stack_.pop_back();
    deserialize_scene(snapshot, grid, &cam, &render_mode_, &palette_idx_);
    update_labels();
    pose_undo_dirty_ = true; // restored state: next edit pushes a fresh snapshot
    infl_undo_dirty_ = true;
    bone_bind_pending_ = false;
    return true;
}

void UI::clear_scene() {
    push_undo();
    grid.clear();
    show_msg("Scene cleared");
}

void UI::add_sculpt_mesh() {
    // Spawn where the crosshair points so the mesh is immediately visible
    // and targetable with the brush.
    Vec3 target = cam.pos + cam.forward() * 3.0;
    SculptMesh* sm = SculptMesh::create_sphere(target, 0.4,
        sculpt_segs_, sculpt_rings_, Vec3(0.8,0.7,0.5));
    if (sm) {
        grid.add_free(sm);
        if (sculpt_) sculpt_->set_active(sm);
        show_msg("Sculpt mesh added");
    }
}

void UI::add_skeleton_to_selected() {
    int sel = selected_obj_;
    if (sel < 0 || sel >= (int)obj_refs_.size() ||
        obj_refs_[(size_t)sel].type != ObjRef::FREE) {
        show_msg("Select a mesh/sculpt object first");
        return;
    }
    int fi = obj_refs_[(size_t)sel].free_idx;
    if (fi < 0 || fi >= (int)grid.free_objects_.size() || !grid.free_objects_[(size_t)fi]) {
        show_msg("Invalid object");
        return;
    }
    Hittable* obj = grid.free_objects_[(size_t)fi].get();
    if (obj->skeleton()) { show_msg("Already has skeleton"); return; }
    auto* sk = new Skeleton();
    double span = obj->bone_span();
    Vec3 base = obj->bone_anchor();
    sk->add(-1, "root", base);
    sk->add(0, "spine", Vec3(0, span * 0.3, 0));
    sk->add(1, "tip",   Vec3(0, span * 0.3, 0));
    obj->bind_skeleton(sk);
    char buf[64];
    snprintf(buf, sizeof(buf), "Skeleton added: %d bones", (int)sk->bones.size());
    show_msg(buf);
}

Hittable* UI::selected_free_object() {
    int sel = selected_obj_;
    if (sel < 0 || sel >= (int)obj_refs_.size() ||
        obj_refs_[(size_t)sel].type != ObjRef::FREE)
        return nullptr;
    int fi = obj_refs_[(size_t)sel].free_idx;
    if (fi < 0 || fi >= (int)grid.free_objects_.size() ||
        !grid.free_objects_[(size_t)fi])
        return nullptr;
    return grid.free_objects_[(size_t)fi].get();
}

Mesh* UI::selected_mesh() {
    auto* obj = selected_free_object();
    return obj ? dynamic_cast<Mesh*>(obj) : nullptr;
}

void UI::set_subdivide_radius(double r) { if (r >= 0.0) subdivide_radius_ = r; }
void UI::set_subdivide_sharp(double s)  { if (s > 0.0) subdivide_sharp_ = s; }
void UI::set_subdivide_levels(int l)    { if (l >= 1) subdivide_levels_ = l; }
void UI::set_sculpt_awaiting_center(bool v) { subdivide_await_center_ = v; }
void UI::set_sculpt_center(const Vec3& p) { subdivide_center_ = p; subdivide_center_valid_ = true; }

void UI::set_mesh_scale(double s) {
    if (s <= 0.0) s = 0.05;
    auto* sm = selected_mesh();
    if (!sm) return;
    push_undo();
    // apply as a delta from the last slider value so repeated slider moves
    // compound correctly about the object center
    sm->scale(Vec3(s / mesh_scale_prev_, s / mesh_scale_prev_, s / mesh_scale_prev_));
    mesh_scale_prev_ = s;
    mesh_scale_ = s;
}

void UI::subdivide_selected_area() {
    auto* sm = selected_mesh();
    if (!sm) { show_msg("select a mesh object first"); return; }
    if (!subdivide_center_valid_) { show_msg("click Set Center, then click on the object"); return; }
    push_undo();
    sm->subdivide_area(subdivide_center_, subdivide_radius_, subdivide_sharp_, subdivide_levels_);
    char lbl[96];
    snprintf(lbl, sizeof(lbl), "subdivided area r=%.2f sharp=%.2f", subdivide_radius_, subdivide_sharp_);
    show_msg(lbl);
}

void UI::apply_customize_at(const Vec3& center) {
    auto* sm = selected_mesh();
    if (!sm) return;
    push_undo();
    sm->customize(center, customize_radius_, customize_sharp_,
                  customize_strength_, customize_mode_);
    char lbl[64];
    static const char* names[4] = { "Inflate", "Deflate", "Pull", "Push" };
    snprintf(lbl, sizeof(lbl), "customize %s r=%.2f s=%.2f",
             names[customize_mode_], customize_radius_, customize_sharp_);
    show_msg(lbl);
}

const Skeleton* UI::selected_skeleton() const {
    auto* self = const_cast<UI*>(this);
    Hittable* o = self->selected_free_object();
    return o ? const_cast<const Hittable*>(o)->skeleton() : nullptr;
}

void UI::add_bone_to_skeleton() {
    Hittable* o = selected_free_object();
    Skeleton* sk = o ? o->skeleton_mut() : nullptr;
    if (!sk) { show_msg("Select an object with skeleton"); return; }
    int parent = selected_bone_ >= 0 &&
                 selected_bone_ < (int)sk->bones.size()
                     ? selected_bone_ : (int)sk->bones.size() - 1;
    // a new bone hangs below the current selection (root when nothing picked)
    double span = o->bone_span();
    Vec3 off(0, span * 0.15, 0);
    char name[16];
    snprintf(name, sizeof(name), "bone%d", (int)sk->bones.size());
    sk->add(parent, name, off);
    selected_bone_ = (int)sk->bones.size() - 1;
    // fresh bone takes over its nearest verts: re-bind sees the full chain
    o->bind_skeleton(sk);
    show_msg("Bone added (verts re-linked)");
}

void UI::delete_bone_from_skeleton() {
    Hittable* o = selected_free_object();
    Skeleton* sk = o ? o->skeleton_mut() : nullptr;
    if (!sk || sk->bones.empty()) { show_msg("No skeleton"); return; }
    int idx = selected_bone_ >= 0 && selected_bone_ < (int)sk->bones.size()
                  ? selected_bone_ : -1;
    if (idx < 0) { show_msg("Select a bone first"); return; }
    for (const auto& b : sk->bones)
        if (b.parent == idx) { show_msg("Delete child bones first"); return; }
    int replacement = sk->bones[(size_t)idx].parent;
    sk->bones.erase(sk->bones.begin() + idx);
    // removed id -> parent; ids above shift down by one
    o->remap_bone_binds(idx, replacement);
    selected_bone_ = -1;
    show_msg("Bone removed (binds follow parent)");
}

void UI::relink_selected_bones() {
    Hittable* o = selected_free_object();
    Skeleton* sk = o ? o->skeleton_mut() : nullptr;
    if (!sk) { show_msg("No skeleton to relink"); return; }
    o->bind_skeleton(sk);
    show_msg("Vertices re-linked to current chain");
}

// Pose mode: absolute euler sliders on the selected bone. Applied the moment
// a value differs from the last applied triple, so unpicked sliders never
// clobber an existing pose.
void UI::pose_slider_tick() {
    Hittable* obj = selected_free_object();
    if (!obj) return;
    Skeleton* sk = obj->skeleton_mut();
    if (!sk) return;
    int b = selected_bone_;
    if (b < 0 || b >= (int)sk->bones.size()) return;
    Vec3 want(pose_roll_, pose_yaw_, pose_pitch_); // degrees
    Vec3& ap = pose_applied_;
    if (std::abs(want.x - ap.x) < 0.01 && std::abs(want.y - ap.y) < 0.01 &&
        std::abs(want.z - ap.z) < 0.01) return;
    if (!pose_undo_dirty_) {
        // one snapshot per run of slider edits (bonepose lines keep the
        // live pose inside the scene snapshot)
        push_undo();
        pose_undo_dirty_ = true;
    }
    const double D2R = M_PI / 180.0;
    // euler order Rz*Ry*Rx: pitch nods about X (bend), yaw turns about Y,
    // roll spins in the screen plane about Z (the view axis)
    Vec3 rot(pose_pitch_ * D2R, pose_yaw_ * D2R, pose_roll_ * D2R);
    obj->set_bone_pose(b, rot, sk->bones[(size_t)b].off); // keep live offset
    obj->apply_bones();
    pose_applied_ = want;
}

void UI::pose_straighten() {
    Hittable* obj = selected_free_object();
    if (!obj) return;
    push_undo();
    obj->reset_bones();
    // deferred influence re-derivation (set while posed) lands here, where
    // the bind snapshot is valid again
    if (bone_bind_pending_ && obj->skeleton()) {
        bone_bind_pending_ = false;
        obj->bind_skeleton(obj->skeleton_mut());
    }
    obj->apply_bones();
    pose_roll_ = pose_yaw_ = pose_pitch_ = 0.0f;
    pose_applied_ = Vec3(0, 0, 0);
    pose_undo_dirty_ = false; // next pose edit takes its own snapshot
    infl_undo_dirty_ = false;
}

// Per-bone influence area: radius where the bone's falloff reaches (0 =
// unlimited, matching the legacy nearest-joint weights) plus sharpness.
// Writes re-derive the vertex binds so the tint/weights update immediately
// while the chain is at rest; while posed, re-derivation is deferred to
// Straighten (the bind snapshot must stay in bind space).
static Skeleton* ui_edit_skeleton(UI* self, int& sel_bone) {
    Hittable* o = self->selected_free_object();
    Skeleton* sk = o ? o->skeleton_mut() : nullptr;
    sel_bone = self->selected_bone();
    if (!sk || sel_bone < 0 || sel_bone >= (int)sk->bones.size()) return nullptr;
    return sk;
}

void UI::rederive_bone_binds() {
    Hittable* obj = selected_free_object();
    Skeleton* sk = obj ? obj->skeleton_mut() : nullptr;
    if (!sk || sk->bones.empty()) return;
    if (obj->bones_posed()) {
        bone_bind_pending_ = true; // Straighten re-binds with the new radii
        show_msg("Straighten applies influence");
        return;
    }
    obj->bind_skeleton(sk);
    obj->apply_bones();
}

float UI::bone_influence() {
    int b;
    Skeleton* sk = ui_edit_skeleton(this, b);
    if (!sk) return 0.0f;
    return (float)sk->bones[(size_t)b].influence;
}

float UI::bone_sharp() {
    int b;
    Skeleton* sk = ui_edit_skeleton(this, b);
    if (!sk) return 2.0f;
    return (float)sk->bones[(size_t)b].sharp;
}

void UI::set_bone_influence(float v) {
    if (v < 0.0f) v = 0.0f;
    if (v > 8.0f) v = 8.0f;
    int b;
    Skeleton* sk = ui_edit_skeleton(this, b);
    if (!sk) return;
    if (std::fabs(sk->bones[(size_t)b].influence - v) < 1e-4) return;
    if (!infl_undo_dirty_) {
        push_undo(); // bonepose lines keep pose; structure is in the snapshot
        infl_undo_dirty_ = true;
    }
    sk->bones[(size_t)b].influence = v;
    rederive_bone_binds();
}

void UI::set_bone_sharp(float v) {
    if (v < 0.5f) v = 0.5f;
    if (v > 8.0f) v = 8.0f;
    int b;
    Skeleton* sk = ui_edit_skeleton(this, b);
    if (!sk) return;
    if (std::fabs(sk->bones[(size_t)b].sharp - v) < 1e-4) return;
    if (!infl_undo_dirty_) {
        push_undo();
        infl_undo_dirty_ = true;
    }
    sk->bones[(size_t)b].sharp = v;
    rederive_bone_binds();
}

// Key-from-pose: bake the selected bone's live rotation + offset into the
// clip that targets it (FREE_BONE) at the animator's playhead, creating that
// clip when none exists. This is the pose -> key -> scrub -> pose loop.
void UI::key_bone_pose_to_clip(int interp) {
    Hittable* o = selected_free_object();
    Skeleton* sk = o ? o->skeleton_mut() : nullptr;
    if (!sk || selected_bone_ < 0 || selected_bone_ >= (int)sk->bones.size()) {
        show_msg("Select a bone first");
        return;
    }
    Animator* a = animator();
    if (!a) { show_msg("No animator"); return; }
    int fi = obj_refs_[(size_t)selected_obj_].free_idx;

    int found = -1;
    for (size_t c = 0; c < a->clips.size(); c++) {
        const AnimTarget& t = a->clips[c].target;
        if (t.type == AnimTarget::FREE_BONE && t.free_idx == fi &&
            t.bone == selected_bone_) {
            found = (int)c;
            break;
        }
    }
    if (found < 0) {
        Animator::ClipInstance ci;
        ci.target.type = AnimTarget::FREE_BONE;
        ci.target.free_idx = fi;
        ci.target.bone = selected_bone_;
        ci.base_snapped = true; // bones apply directly, no pose-base snap
        ci.clip.name = "Bone " + sk->bones[(size_t)selected_bone_].name;
        a->add_clip(ci.target, ci.clip);
        found = (int)a->clips.size() - 1;
    }
    Animator::ClipInstance& cli = a->clips[(size_t)found];
    const Skeleton::Bone& b = sk->bones[(size_t)selected_bone_];
    float t = a->global_time < 0.0f ? 0.0f : a->global_time;
    cli.clip.add_keyframe(AnimProperty::Rotation, t, bone_euler_rad(b.m),
                          (InterpolationType)interp);
    cli.clip.add_keyframe(AnimProperty::Position, t, b.off,
                          (InterpolationType)interp);
    a->update_max_time();
    char msg[64];
    snprintf(msg, sizeof(msg), "Keyed @ %.2fs (%s)", t, cli.clip.name.c_str());
    show_msg(msg);
}

void UI::rename_selected_bone(const char* name) {
    Hittable* o = selected_free_object();
    Skeleton* sk = o ? o->skeleton_mut() : nullptr;
    if (!sk || selected_bone_ < 0 || selected_bone_ >= (int)sk->bones.size()) {
        show_msg("Select a bone first");
        return;
    }
    sk->bones[(size_t)selected_bone_].name = name ? name : "";
    show_msg("Bone renamed");
}

void UI::do_remesh() {
    if (sculpt_ && sculpt_->active()) {
        sculpt_->active()->remesh_subdivide();
        char buf[64];
        snprintf(buf, sizeof(buf), "Remeshed: %d verts", (int)sculpt_->active()->vertices().size());
        show_msg(buf);
    } else {
        show_msg("No active mesh");
    }
}

void UI::load_terrain() {
    if (mesh_files_.empty()) {
        show_msg("No meshes in models/");
        return;
    }
    const std::string& path = mesh_files_[mesh_idx_];
    Mesh* m = load_obj(path.c_str(), Vec3(0.3, 0.5, 0.25), Vec3(0, 0, 0), 1.0);
    if (m) {
        grid.set_terrain(m, path);
        show_msg("Terrain loaded");
    } else {
        show_msg("Mesh load failed");
    }
}

// ---- heightfield terrain (PNG heightmaps in maps/) ----

void UI::scan_hmaps() {
    hmap_files_.clear();
    std::error_code ec;
    if (!fs::exists("maps")) {
        fs::create_directory("maps", ec);
        return;
    }
    for (const auto& entry : fs::directory_iterator("maps", ec)) {
        if (!entry.is_regular_file()) continue;
        auto p = entry.path();
        auto ext = p.extension().string();
        for (char& c : ext) c = (char)tolower((unsigned char)c);
        if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" ||
            ext == ".bmp" || ext == ".tga")
            hmap_files_.push_back(p.string());
    }
    std::sort(hmap_files_.begin(), hmap_files_.end());
}

void UI::load_heightfield(int idx) {
    if (idx < 0 || idx >= (int)hmap_files_.size()) {
        show_msg("No PNG in maps/");
        return;
    }
    const std::string& path = hmap_files_[(size_t)idx];
    Vec3 gmin, gmax;
    grid.grid_bounds(gmin, gmax);
    double span_x = gmax.x - gmin.x, span_z = gmax.z - gmin.z;
    double side = std::max(span_x, span_z) + 1.6;
    auto hf = HeightField::from_png(path,
                                    gmin.x - (side - span_x) / 2,
                                    gmin.z - (side - span_z) / 2,
                                    side, 0.0, 3.0,
                                    Vec3(0.3, 0.5, 0.25));
    if (!hf) {
        show_msg("PNG decode failed");
        return;
    }
    grid.set_terrain(hf.release(), path);
    show_msg("Heightmap imported");
}

bool UI::export_heightfield(const char* name) {
    auto* hf = dynamic_cast<HeightField*>(grid.terrain());
    if (!hf) {
        show_msg("No heightmap terrain");
        return false;
    }
    std::string safe(name);
    for (char& ch : safe)
        if (ch == '/' || ch == '\\' || ch == ':' || ch < 32) ch = '_';
    std::string path = std::string("maps/") + safe + ".png";
    if (!hf->export_png(path)) {
        show_msg("PNG write failed");
        return false;
    }
    if (hf->mesh_path().empty()) hf->set_path(path);
    show_msg("Exported");
    scan_hmaps();
    return true;
}

void UI::make_blank_heightfield() {
    Vec3 gmin, gmax;
    grid.grid_bounds(gmin, gmax);
    double span_x = gmax.x - gmin.x, span_z = gmax.z - gmin.z;
    double side = std::max(span_x, span_z) + 1.6;
    auto hf = HeightField::flat(gmin.x - (side - span_x) / 2,
                                gmin.z - (side - span_z) / 2,
                                side, 0.0,
                                Vec3(0.3, 0.5, 0.25));
    grid.set_terrain(hf.release(), "");
    show_msg("Blank heightmap ready");
}

bool UI::heightfield_active() const {
    return dynamic_cast<HeightField*>(grid.terrain()) != nullptr;
}

const std::vector<std::string>* UI::object_list() {
    obj_refs_.clear();
    obj_names_.clear();

    // cells
    for (int i = 0; i < grid.nx; i++) {
        for (int j = 0; j < grid.ny; j++) {
            for (int k = 0; k < grid.nz; k++) {
                Hittable* obj = grid.get(i, j, k);
                if (!obj) continue;
                ObjRef r;
                r.type = ObjRef::CELL;
                r.i = i; r.j = j; r.k = k;
                r.base_center = obj->get_center() - obj->anim_offset();
                obj_refs_.push_back(r);
                char buf[64];
                snprintf(buf, sizeof(buf), "cell %d,%d,%d %s", i, j, k, obj->type_name());
                obj_names_.push_back(buf);
            }
        }
    }
    // free
    const auto& fobjs = grid.free_objects();
    for (int fi = 0; fi < (int)fobjs.size(); fi++) {
        Hittable* obj = fobjs[(size_t)fi].get();
        if (!obj) continue;
        ObjRef r;
        r.type = ObjRef::FREE;
        r.free_idx = fi;
        r.base_center = obj->get_center() - obj->anim_offset();
        obj_refs_.push_back(r);
        char buf[64];
        snprintf(buf, sizeof(buf), "free#%d %s", fi, obj->type_name());
        obj_names_.push_back(buf);
    }
    if (selected_obj_ < 0 && !obj_names_.empty()) selected_obj_ = 0;
    if (selected_obj_ >= (int)obj_names_.size()) selected_obj_ = -1;
    sync_selection_hook(); // refs rebuilt: keep the overlay hook valid
    return &obj_names_;
}

void UI::sync_selection_hook() {
    g_sel_bone = selected_bone_;
    g_sel_object = selected_free_object();
    // switching bones: baseline the pose sliders at what they display so a
    // stale "applied" value can't skip the first change on the new bone
    pose_applied_ = Vec3(pose_roll_, pose_yaw_, pose_pitch_);
    pose_undo_dirty_ = false; // new bone: next edit run pushes a fresh snapshot
    infl_undo_dirty_ = false;
}

void UI::select_object(int i) {
    if (i < 0) { selected_obj_ = -1; sync_selection_hook(); mesh_scale_prev_ = 1.0; mesh_scale_sel_ = -1; return; }
    if (i >= (int)obj_names_.size()) return;
    if (i != mesh_scale_sel_) { mesh_scale_prev_ = 1.0; mesh_scale_sel_ = i; }
    selected_obj_ = i;
}

// Move a cell-based object: take (preserve geometry/color), translate, and
// re-home into the cell containing newc. If newc falls outside the grid, the
// object is converted to a free object so raytracing still finds it.
void UI::relocate_cell(int sel, const Vec3& newc) {
    if (sel < 0 || sel >= (int)obj_refs_.size()) return;
    ObjRef& r = obj_refs_[(size_t)sel];
    if (r.type != ObjRef::CELL) return;
    Hittable* obj = grid.take_cell(r.i, r.j, r.k);
    if (!obj) return;
    Vec3 oldc = obj->get_center() - obj->anim_offset();
    obj->set_anim_offset(Vec3(0, 0, 0));
    obj->translate(newc - oldc);
    int ni, nj, nk;
    if (grid.world_to_cell(newc, ni, nj, nk)) {
        grid.set(ni, nj, nk, obj);
        r.i = ni; r.j = nj; r.k = nk;
        r.base_center = obj->get_center();
        char buf[64];
        snprintf(buf, sizeof(buf), "cell %d,%d,%d %s", ni, nj, nk, obj->type_name());
        obj_names_[(size_t)sel] = buf;
    } else {
        grid.add_free(obj);
        r.type = ObjRef::FREE;
        r.free_idx = (int)grid.free_objects().size() - 1;
        r.i = r.j = r.k = -1;
        r.base_center = obj->get_center();
        char buf[64];
        snprintf(buf, sizeof(buf), "free#%d %s", r.free_idx, obj->type_name());
        obj_names_[(size_t)sel] = buf;
    }
}

// ================= bone gizmo (Bones mode) =================

// Press-resolve for the joint gizmo: find the chain joint nearest to the
// mouse (projected like the overlay does) within a comfortable pick radius.
// On hit: select the owning object + that bone and arm the drag.
bool UI::start_bone_drag(int mx, int my) {
    object_list(); // refresh refs so select_object below stays valid
    int w = display.width(), h = display.height();
    double vw = VIEWPORT_HEIGHT * w / h;
    Vec3 fwd = cam.forward();
    Vec3 rgt = cam.right();
    Vec3 upv = cross(rgt, fwd);

    Skeleton* best_sk = nullptr;
    Hittable* best_obj = nullptr;
    int best_bone = -1, best_fi = -1;
    double best_d2 = 16.0 * 16.0; // ~16 px pick radius

    // hit-test against POSED joint positions (chain transforms include live
    // rotations/offsets) so grabbing works on a moving/posed rig too
    std::vector<Vec3> wjoints;
    std::vector<double> wrot;
    const auto& fobjs = grid.free_objects();
    for (size_t fi = 0; fi < fobjs.size(); fi++) {
        Hittable* obj = fobjs[fi].get();
        Skeleton* sk = obj ? obj->skeleton_mut() : nullptr;
        if (!sk || sk->bones.empty()) continue;
        bone_chain_transforms(*sk, wjoints, wrot);
        for (int bi = 0; bi < (int)sk->bones.size(); bi++) {
            const Vec3& jp = wjoints[(size_t)bi];
            Vec3 rel = jp - cam.pos;
            double depth = dot(rel, fwd);
            if (depth <= 0.05) continue;
            double rx = dot(rel, rgt), ry = dot(rel, upv);
            double sx = w / 2.0 + (rx / depth) * FOCAL_LENGTH * w / vw;
            double sy = h / 2.0 - (ry / depth) * FOCAL_LENGTH * h / VIEWPORT_HEIGHT;
            double dx = sx - mx, dy = sy - my;
            double d2 = dx * dx + dy * dy;
            if (d2 < best_d2) {
                best_d2 = d2;
                best_sk = sk; best_obj = obj; best_bone = bi; best_fi = (int)fi;
            }
        }
    }
    if (!best_sk) return false;

    // bonepose lines keep the pose inside scene snapshots, so one push per
    // grab covers both structural and pose drags (idle clicks are cheap)
    push_undo();

    select_object(-1);
    for (int si = 0; si < (int)obj_refs_.size(); si++) {
        const ObjRef& r = obj_refs_[(size_t)si];
        if (r.type == ObjRef::FREE && r.free_idx == best_fi) {
            select_object(si);
            break;
        }
    }
    select_bone_with_name(best_bone);

    bone_dragging_ = true;
    bone_drag_obj_ = best_obj;
    bone_drag_sk_ = best_sk;
    bone_drag_bone_ = best_bone;
    bone_drag_mx_ = mx;
    bone_drag_my_ = my;
    bone_drag_mx0_ = mx;
    bone_drag_my0_ = my;
    bone_drag_e0_ = bone_euler_rad(
        best_sk->bones[(size_t)(best_sk->bones[(size_t)best_bone].parent >= 0
                                    ? best_sk->bones[(size_t)best_bone].parent
                                    : best_bone)].m);
    // STRUCTURE edits the rig at rest (re-binds on release); POSE/BEND edit
    // the live pose (points follow, never re-bind)
    bone_drag_pose_ = bone_drag_mode_ != BONEDRAG_STRUCTURE;
    // clips would fight the drag: pause playback for its duration
    bone_drag_anim_paused_ = false;
    if (Animator* a = animator(); a && a->global_playing) {
        a->global_playing = false;
        bone_drag_anim_paused_ = true;
    }
    dragging_ = true; // release path routes through end_drag like object drags
    return true;
}

// Per-frame mouse update for the joint gizmo: convert the screen delta into
// a world-space move along the camera plane (same projection as the overlay)
// and apply it to the bone's bind offset — or live pose translation when a
// pose is active. Children follow: later joints sum their parents' offsets.
void UI::bone_drag_update(int mx, int my) {
    Hittable* obj = bone_drag_obj_;
    Skeleton* sk = bone_drag_sk_;
    int bi = bone_drag_bone_;
    if (!obj || !sk || obj->skeleton() != sk || bi < 0 ||
        bi >= (int)sk->bones.size()) {
        bone_dragging_ = false;
        dragging_ = false;
        return;
    }
    int h = display.height();
    Vec3 fwd = cam.forward();
    Vec3 rgt = cam.right();
    Vec3 upv = cross(rgt, fwd);

    // depth scale from the POSED joint (where the dot actually is)
    std::vector<Vec3> wjoints;
    std::vector<double> wrot;
    bone_chain_transforms(*sk, wjoints, wrot);
    const Vec3& jp = wjoints[(size_t)bi];
    Vec3 rel = jp - cam.pos;
    double depth = dot(rel, fwd);
    if (depth <= 0.05) {
        bone_drag_mx_ = mx; bone_drag_my_ = my;
        return;
    }
    // per-pixel world size at this joint's depth (inverse of project())
    double f = depth * VIEWPORT_HEIGHT / (FOCAL_LENGTH * h);
    int ddx = mx - bone_drag_mx_, ddy = my - bone_drag_my_;
    bool shift_held = display.key_held(XK_Shift_L) || display.key_held(XK_Shift_R);
    Vec3 delta = shift_held
        ? upv * (-ddy * f)                       // vertical only, like objects
        : rgt * (ddx * f) + upv * (-ddy * f);

    Skeleton::Bone& b = sk->bones[(size_t)bi];
    if (bone_drag_mode_ == BONEDRAG_BEND) {
        // drag-to-bend: rotating the grabbed bone itself would only swing
        // the chain BELOW its joint (the grabbed dot would stay put), so the
        // pivot is the parent bone — the grabbed dot then arcs with the
        // cursor. Roots bend themselves. Absolute from the press euler, so
        // no per-move drift; screen-plane roll for horizontal motion, pitch
        // for vertical (signs chosen so the dot follows the cursor).
        int pivot = b.parent >= 0 ? b.parent : bi;
        const double KP = 0.5 * M_PI / 180.0; // radians per pixel (~180°/360px)
        Vec3 e = bone_drag_e0_;
        e.z -= (mx - bone_drag_mx0_) * KP;
        e.x += (my - bone_drag_my0_) * KP;
        obj->set_bone_pose(pivot, e, sk->bones[(size_t)pivot].off);
        obj->apply_bones();
    } else if (bone_drag_pose_) {
        Vec3 off = b.off + delta;
        double cap = obj->stretch_cap();
        // LIMIT stretch mode: the drag dead-ends just before the skin
        // tears. A pose already past the cap (keys / REMESH mode) is left
        // alone instead of snapping back on grab.
        if (cap > 0.0 && off.length() > cap && b.off.length() <= cap)
            off = off * (cap / off.length());
        b.off = off;
        sk->touch_pose();             // version so idle renders follow too
        obj->apply_bones();           // preview bends under the cursor
        if (stretch_mode_ == STRETCH_REMESH) {
            if (auto* scm = dynamic_cast<SculptMesh*>(obj))
                scm->remesh_if_stretched(1.6, 400000);
        }
    } else {
        b.offset = b.offset + delta; // rest-pose rig edit (bind space)
    }
    bone_drag_mx_ = mx;
    bone_drag_my_ = my;
}

// ================= mouse drag gizmo =================

// Map a raycast hit to an index into obj_refs_ by pointer identity.
// terrain hits carry hittable == nullptr and resolve to -1.
int UI::resolve_pick(void* hit_ptr, const Vec3& hit_p) {
    auto* h = static_cast<Hittable*>(hit_ptr);
    if (!h) return -1;
    object_list(); // rebuild obj_refs_ so selections stay valid
    for (int si = 0; si < (int)obj_refs_.size(); si++) {
        const ObjRef& r = obj_refs_[(size_t)si];
        if (r.type == ObjRef::FREE) {
            if (r.free_idx < 0 || r.free_idx >= (int)grid.free_objects().size()) continue;
            if (grid.free_objects()[(size_t)r.free_idx].get() == h) return si;
        } else {
            if (grid.get(r.i, r.j, r.k) == h) return si;
        }
    }
    // Pointer not in refs (e.g. a cell object parked outside the grid):
    // fall back to the cell containing the hit point.
    int i, j, k;
    if (grid.world_to_cell(hit_p, i, j, k) && grid.get(i, j, k) == h) {
        object_list(); // reconcile refs with the actual grid contents
        for (int si = 0; si < (int)obj_refs_.size(); si++) {
            const ObjRef& r = obj_refs_[(size_t)si];
            if (r.type == ObjRef::CELL && r.i == i && r.j == j && r.k == k) return si;
        }
    }
    return -1;
}

// The object currently being dragged (validates against the live scene).
Hittable* UI::drag_obj() {
    if (!dragging_ || drag_.sel < 0 || drag_.sel >= (int)obj_refs_.size()) return nullptr;
    const ObjRef& r = obj_refs_[(size_t)drag_.sel];
    if (r.type == ObjRef::FREE) {
        if (r.free_idx < 0 || r.free_idx >= (int)grid.free_objects().size()) return nullptr;
        return grid.free_objects()[(size_t)r.free_idx].get();
    }
    return grid.get(r.i, r.j, r.k);
}

// Nearest empty cell (world-space center) to the target point, searched in a
// small ring around the cell that would contain the target.
bool UI::nearest_free_cell(const Vec3& target, Vec3& snapped) {
    int ti, tj, tk;
    if (!grid.world_to_cell(target, ti, tj, tk)) return false;
    static const int RAD = 4;
    double best = 1e30;
    bool found = false;
    for (int dj = -RAD; dj <= RAD; dj++)
        for (int dk = -RAD; dk <= RAD; dk++)
            for (int di = -RAD; di <= RAD; di++) {
                int i = ti + di, j = tj + dj, k = tk + dk;
                if (i < 0 || i >= grid.nx || j < 0 || j >= grid.ny ||
                    k < 0 || k >= grid.nz) continue;
                if (grid.get(i, j, k)) continue; // occupied
                Vec3 c = grid.cell_center(i, j, k);
                double d = (c.x - target.x) * (c.x - target.x) +
                           (c.y - target.y) * (c.y - target.y) +
                           (c.z - target.z) * (c.z - target.z);
                if (d < best) { best = d; snapped = c; found = true; }
            }
    return found;
}

// Move a free object back into a cell (undoing a drag that left the grid).
bool UI::rehome_free(int sel, int i, int j, int k) {
    if (sel < 0 || sel >= (int)obj_refs_.size()) return false;
    const ObjRef* rr = &obj_refs_[(size_t)sel];
    if (rr->type != ObjRef::FREE) return false;
    if (rr->free_idx < 0 || rr->free_idx >= (int)grid.free_objects().size())
        return false;
    Hittable* obj = grid.free_objects()[(size_t)rr->free_idx].get();
    if (!obj || grid.get(i, j, k)) return false; // target cell taken
    // Deformable meshes stretch beyond one cell: keep them on the free list
    // so the raytracer's cell walk can never cull deformed geometry.
    if (obj->skeleton()) return false;
    int taken = rr->free_idx;
    Hittable* raw = grid.take_free(obj);
    if (!raw) return false;
    // Free-list indices shifted: fix every other ref after the removal.
    for (auto& o : obj_refs_)
        if (o.type == ObjRef::FREE && o.free_idx > taken) o.free_idx--;
    grid.set(i, j, k, raw);
    ObjRef& rm = obj_refs_[(size_t)sel];
    rm.type = ObjRef::CELL;
    rm.i = i; rm.j = j; rm.k = k;
    rm.base_center = raw->get_center();
    char buf[64];
    snprintf(buf, sizeof(buf), "cell %d,%d,%d %s", i, j, k, raw->type_name());
    obj_names_[(size_t)sel] = buf;
    return true;
}

bool UI::start_drag(int mx, int my) {
    // Bones modes (billboard + raytrace variants): the mouse may be on a
    // joint — those belong to the bone gizmo, not the object-drag gizmo.
    if ((render_mode() == MODE_BONES || render_mode() == MODE_BONES_RAYTRACE) &&
        start_bone_drag(mx, my)) return true;
    // Overhead map: pick in map space (nearest marker / occupied column).
    // Selection only — the 3D gizmo drag machinery has no meaning here.
    if (render_mode() == MODE_OVERHEAD) {
        Hittable* hit = overhead_pick(display, grid, mx, my);
        if (!hit) {
            select_object(-1);
            return false;
        }
        int sel = resolve_pick(hit, hit->get_center());
        if (sel < 0) return false;
        select_object(sel);
        return false;
    }
    // --- gizmo axis drag (Unity-style): X screen right, Y screen up,
    //     Z depth close+far line through the selected object's center ---
    if (gizmo_axis_ >= 0) {
        Hittable* hit = selected_obj();
        if (hit) {
            dragging_ = true;
            gizmo_dragging_ = true;
            drag_.sel = selected_obj_;
            drag_.undo_saved = false;
            drag_.mx0 = mx;
            drag_.my0 = my;
            drag_.visual0 = hit->get_center();
            return true;
        }
    }
    HitRecord rec;
    if (!pick_at_mouse(grid, cam, display.width(), display.height(), mx, my, rec))
        return false;
    int sel = resolve_pick(rec.hittable, rec.p);
    if (sel < 0) return false;
    Hittable* obj = nullptr;
    {
        const ObjRef& r = obj_refs_[(size_t)sel];
        if (r.type == ObjRef::FREE) {
            if (r.free_idx < 0 || r.free_idx >= (int)grid.free_objects().size()) return false;
            obj = grid.free_objects()[(size_t)r.free_idx].get();
        } else {
            obj = grid.get(r.i, r.j, r.k);
        }
    }
    if (!obj) return false;

    select_object(sel); // sidebar follows the drag
    dragging_ = true;
    drag_.sel = sel;
    drag_.undo_saved = false;
    drag_.grab_hit = rec.p - obj->anim_offset(); // base-plane point
    drag_.visual0 = obj->get_center();
    const ObjRef& r0 = obj_refs_[(size_t)sel];
    drag_.was_cell = (r0.type == ObjRef::CELL);
    drag_.ci = r0.i; drag_.cj = r0.j; drag_.ck = r0.k;
    return true;
}

void UI::drag_update(int mx, int my, bool shift_held) {
    if (!dragging_) return;
    // Unity-style gizmo axis drag: translate the selected object along one
    // screen-space axis (X right, Y up, Z depth close+far).
    if (gizmo_axis_ >= 0 && drag_.sel >= 0 &&
        drag_.sel < (int)obj_refs_.size()) {
        Hittable* obj = drag_obj();
        if (!obj) {
            dragging_ = false;
            return;
        }
        Vec3 center;
        if (!selected_pos(center)) {
            dragging_ = false;
            return;
        }
        int h = display.height();
        Vec3 fwd = cam.forward(), rgt = cam.right(), upv = cross(rgt, fwd);
        Vec3 rel = center - cam.pos;
        double depth = dot(rel, fwd);
        if (depth <= 0.05) {
            drag_.mx0 = mx;
            drag_.my0 = my;
            return;
        }
        // per-pixel world size at the object's depth (inverse of project())
        double f = depth * VIEWPORT_HEIGHT / (FOCAL_LENGTH * h);
        int ddx = mx - drag_.mx0, ddy = my - drag_.my0;
        Vec3 delta;
        switch (gizmo_axis_) {
        case 0: delta = rgt * (ddx * f); break;        // X: screen right
        case 1: delta = upv * (-ddy * f); break;       // Y: screen up
        default: delta = fwd * (-ddy * f); break;      // Z: close+far depth
        }
        if (!drag_.undo_saved) {
            drag_.undo_saved = true;
            snapshot_for_undo();
        }
        obj->translate(delta);
        ObjRef& rr = obj_refs_[(size_t)drag_.sel];
        rr.base_center = obj->get_center() - obj->anim_offset();
        drag_.mx0 = mx;
        drag_.my0 = my;
        return;
    }
    // bone gizmo: separate path, uses screen delta instead of a grab ray
    if (bone_dragging_) { bone_drag_update(mx, my); return; }
    Hittable* obj = drag_obj();
    if (!obj) { dragging_ = false; return; }
    ObjRef& r = obj_refs_[(size_t)drag_.sel];

    Ray ray = mouse_ray(cam, display.width(), display.height(), mx, my);
    Vec3 n = cam.forward(); // grab plane perpendicular to the view
    double denom = dot(ray.direction(), n);
    if (std::abs(denom) < 1e-8) return;
    double t = dot(drag_.grab_hit - ray.origin(), n) / denom;
    if (t < 0) return;
    Vec3 target = ray.origin() + ray.direction() * t; // new grab-point position
    if (shift_held) target = drag_.grab_hit + Vec3(0, (target.y - drag_.grab_hit.y), 0);

    // Undo snapshot once, on the first frame the grab point actually moves.
    if (!drag_.undo_saved && (target - drag_.grab_hit).length() > 0.01) {
        drag_.undo_saved = true;
        snapshot_for_undo();
    }

    if (r.type == ObjRef::FREE) {
        // Move the object by the same delta as the grab point (keeps the
        // pick offset, so the object slides instead of snapping to cursor).
        Vec3 vis_target = drag_.visual0 + (target - drag_.grab_hit);
        obj->translate(vis_target - obj->get_center());
        r.base_center = obj->get_center() - obj->anim_offset();
        // Objects that started as cells re-home once their pose is back
        // inside the grid with an empty cell available.
        if (drag_.was_cell) {
            int ci2, cj2, ck2;
            if (grid.world_to_cell(r.base_center, ci2, cj2, ck2) &&
                !grid.get(ci2, cj2, ck2) && rehome_free(drag_.sel, ci2, cj2, ck2))
                return; // next frame continues as a cell drag
        }
    } else {
        Vec3 vis_target = drag_.visual0 + (target - drag_.grab_hit);
        Vec3 base_target = vis_target - obj->anim_offset();
        int bi, bj, bk;
        if (grid.world_to_cell(base_target, bi, bj, bk)) {
            // Inside the grid: magnetize to the nearest free cell so we
            // never overwrite an existing occupant.
            Vec3 snapped;
            if (!nearest_free_cell(base_target, snapped)) return; // hold
            base_target = snapped;
        }
        relocate_cell(drag_.sel, base_target); // outside grid -> becomes free
    }
}

void UI::end_drag() {
    if (bone_dragging_) {
        bone_dragging_ = false;
        Hittable* obj = bone_drag_obj_;
        Skeleton* sk = bone_drag_sk_;
        bone_drag_obj_ = nullptr;
        bone_drag_sk_ = nullptr;
        bone_drag_bone_ = -1;
        dragging_ = false;
        // bind-edit drag done at rest: re-bind so the moved joint re-claims
        // the nearest verts (safe: re-binding our own skeleton no longer
        // frees it). Pose drags never re-bind — that would snapshot the
        // posed verts and break the rig.
        if (obj && sk && obj->skeleton() == sk && !bone_drag_pose_)
            obj->bind_skeleton(sk);
        if (bone_drag_anim_paused_) {
            bone_drag_anim_paused_ = false;
            if (Animator* a = animator()) a->global_playing = true;
        }
        return;
    }
    if (gizmo_dragging_) {
        gizmo_dragging_ = false;
        gizmo_axis_ = -1;
    }
    dragging_ = false;
}

void UI::cancel_drag() {
    dragging_ = false;
    if (bone_dragging_) {
        // cancel restores playback like a normal end does
        bone_dragging_ = false;
        bone_drag_obj_ = nullptr;
        bone_drag_sk_ = nullptr;
        bone_drag_bone_ = -1;
        if (bone_drag_anim_paused_) {
            bone_drag_anim_paused_ = false;
            if (Animator* a = animator()) a->global_playing = true;
        }
    }
    Hittable* obj = nullptr;
    // validate sel after dragging_ is cleared (drag_obj requires dragging_)
    if (drag_.sel >= 0 && drag_.sel < (int)obj_refs_.size()) {
        const ObjRef& r = obj_refs_[(size_t)drag_.sel];
        if (r.type == ObjRef::FREE &&
            r.free_idx >= 0 && r.free_idx < (int)grid.free_objects().size())
            obj = grid.free_objects()[(size_t)r.free_idx].get();
        else obj = grid.get(r.i, r.j, r.k);
    }
    if (!obj) { drag_.sel = -1; return; }
    obj->translate(drag_.visual0 - obj->get_center());
    ObjRef& r = obj_refs_[(size_t)drag_.sel];
    r.base_center = obj->get_center() - obj->anim_offset();
    if (drag_.was_cell && r.type == ObjRef::FREE) {
        // Dragged outside the grid and then cancelled: re-home into the
        // original cell if that cell is still empty (restores cell status).
        int i, j, k;
        if (grid.world_to_cell(r.base_center, i, j, k))
            rehome_free(drag_.sel, i, j, k);
    }
    if (gizmo_dragging_) {
        gizmo_dragging_ = false;
        gizmo_axis_ = -1;
    }
    drag_.sel = -1;
}

bool UI::drag_center(Vec3& out) const {
    if (!dragging_ || drag_.sel < 0 || drag_.sel >= (int)obj_refs_.size()) return false;
    const ObjRef& r = obj_refs_[(size_t)drag_.sel];
    Hittable* obj = nullptr;
    if (r.type == ObjRef::FREE) {
        if (r.free_idx < 0 || r.free_idx >= (int)grid.free_objects().size()) return false;
        obj = grid.free_objects()[(size_t)r.free_idx].get();
    } else {
        obj = grid.get(r.i, r.j, r.k);
    }
    if (!obj) return false;
    out = obj->get_center();
    return true;
}

// Live world position of the selected object (visual pose, offset included).
bool UI::selected_pos(Vec3& out) const {
    if (selected_obj_ < 0 || selected_obj_ >= (int)obj_refs_.size()) return false;
    const ObjRef& r = obj_refs_[(size_t)selected_obj_];
    Hittable* obj = nullptr;
    if (r.type == ObjRef::FREE) {
        if (r.free_idx < 0 || r.free_idx >= (int)grid.free_objects().size()) return false;
        obj = grid.free_objects()[(size_t)r.free_idx].get();
    } else {
        obj = grid.get(r.i, r.j, r.k);
    }
    if (!obj) return false;
    out = obj->get_center();
    return true;
}

// Teleport the selected object's visual center to p (shared with drag,
// buttons and animation: the base moves, current key offsets ride along).
void UI::set_selected_pos(const Vec3& p) {
    if (selected_obj_ < 0 || selected_obj_ >= (int)obj_refs_.size()) return;
    ObjRef& r = obj_refs_[(size_t)selected_obj_];
    if (r.type == ObjRef::FREE) {
        if (r.free_idx < 0 || r.free_idx >= (int)grid.free_objects().size()) return;
        Hittable* obj = grid.free_objects()[(size_t)r.free_idx].get();
        if (!obj) return;
        obj->translate(p - obj->get_center());
        r.base_center = obj->get_center() - obj->anim_offset();
        show_msg("Position set");
        return;
    }
    Hittable* obj = grid.get(r.i, r.j, r.k);
    if (!obj) return;
    Vec3 base_target = p - obj->anim_offset();
    int bi, bj, bk;
    if (grid.world_to_cell(base_target, bi, bj, bk)) {
        // inside the grid: keep the never-overwrite rule
        Vec3 snapped;
        if (!nearest_free_cell(base_target, snapped)) return; // no free cell
        base_target = snapped;
    }
    relocate_cell(selected_obj_, base_target);
    show_msg("Position set");
}

void UI::move_selected(const Vec3& d) {
    if (selected_obj_ < 0 || selected_obj_ >= (int)obj_refs_.size()) return;
    ObjRef& r = obj_refs_[(size_t)selected_obj_];
    if (r.type == ObjRef::FREE) {
        if (r.free_idx < 0 || r.free_idx >= (int)grid.free_objects().size()) return;
        Hittable* obj = grid.free_objects()[(size_t)r.free_idx].get();
        if (obj) {
            push_undo();
            obj->translate(d);
            r.base_center = obj->get_center() - obj->anim_offset();
            show_msg("Moved object");
        }
        return;
    }
    if (r.type == ObjRef::CELL) {
        Hittable* obj = grid.get(r.i, r.j, r.k);
        if (!obj) return;
        push_undo();
        Vec3 oldc = obj->get_center() - obj->anim_offset();
        relocate_cell(selected_obj_, oldc + d);
        show_msg("Moved object");
    }
}

void UI::center_selected_above() {
    if (selected_obj_ < 0 || selected_obj_ >= (int)obj_refs_.size()) return;
    ObjRef& r = obj_refs_[(size_t)selected_obj_];
    if (r.type == ObjRef::FREE) {
        if (r.free_idx < 0 || r.free_idx >= (int)grid.free_objects().size()) return;
        Hittable* obj = grid.free_objects()[(size_t)r.free_idx].get();
        if (obj) {
            push_undo();
            Vec3 c = obj->get_center() - obj->anim_offset();
            double gh = grid.get_ground_height(c.x, c.z);
            Vec3 target(c.x, gh + 1.2, c.z);
            obj->translate(target - c);
            r.base_center = obj->get_center() - obj->anim_offset();
            show_msg("Centered above");
        }
        return;
    }
    if (r.type == ObjRef::CELL) {
        Hittable* obj = grid.get(r.i, r.j, r.k);
        if (!obj) return;
        push_undo();
        Vec3 oldc = obj->get_center() - obj->anim_offset();
        double gh = grid.get_ground_height(oldc.x, oldc.z);
        relocate_cell(selected_obj_, Vec3(oldc.x, gh + 0.6, oldc.z));
        show_msg("Centered above");
    }
}

void UI::reset_selected() {
    if (selected_obj_ < 0 || selected_obj_ >= (int)obj_refs_.size()) return;
    const auto& r = obj_refs_[(size_t)selected_obj_];
    if (r.type == ObjRef::FREE) {
        if (r.free_idx < 0 || r.free_idx >= (int)grid.free_objects().size()) return;
        Hittable* obj = grid.free_objects()[(size_t)r.free_idx].get();
        if (obj) {
            push_undo();
            Vec3 c = obj->get_center() - obj->anim_offset();
            obj->translate(r.base_center - c);
            show_msg("Reset to original");
        }
    }
}

void UI::show_msg(const char* msg) {
    snprintf(save_msg_, sizeof(save_msg_), "%s", msg);
    save_msg_until_ = now_s() + 3.0;
}

bool UI::try_place(ShapeType type) {
    Vec3 target = cam.pos + cam.forward() * 3.0;
    int i, j, k;

    if (ground_mode_) {
        Vec3 ro = cam.pos;
        Vec3 rd = cam.forward();
        if (grid.has_terrain()) {
            Ray fwd(ro, rd);
            HitRecord trec;
            if (grid.terrain()->hit(fwd, 0.001, 100.0, trec))
                target = trec.p;
            else
                target = ro + rd * 5.0;
        } else {
            if (rd.y > -1e-10) return false;
            Vec3 gmin, gmax;
            grid.grid_bounds(gmin, gmax);
            double gy = gmin.y;
            double t = (gy - ro.y) / rd.y;
            if (t < 0) return false;
            target = ro + rd * t;
        }
        if (!grid.world_to_cell(target, i, j, k)) return false;
        j = 0;
    } else if (!grid.world_to_cell(target, i, j, k)) {
        return false;
    }

    if (grid.get(i, j, k)) return false;

    push_undo();

    double cs = grid.cell_size;
    Vec3 c = grid.cell_center(i, j, k);
    Vec3 col = palette_color(palette_idx_);

    switch (type) {
        case ShapeType::SPHERE:
            grid.set(i, j, k, new Sphere(c, cs * 0.45, col));
            break;
        case ShapeType::BOX: {
            Vec3 half(cs * 0.38, cs * 0.38, cs * 0.38);
            grid.set(i, j, k, new Box(c - half, c + half, col));
            break;
        }
        case ShapeType::CYLINDER:
            grid.set(i, j, k, new Cylinder(c, cs * 0.38, cs * 0.8, col));
            break;
        case ShapeType::CONE:
            grid.set(i, j, k, new Cone(c, cs * 0.38, cs * 0.8, col));
            break;
        case ShapeType::MESH: {
            if (mesh_files_.empty()) return false;
            const std::string& path = mesh_files_[mesh_idx_];
            Mesh* m = load_obj(path.c_str(), col, c, cs * 0.4);
            if (!m) return false;
            m->set_path(path);
            grid.add_free(m);
            break;
        }
    }

    last_shape_ = type;
    palette_idx_++;
    return true;
}

void UI::toggle_ground_mode() {
    ground_mode_ = !ground_mode_;
    update_labels();
}

void UI::set_ground_mode(bool v) {
    if (ground_mode_ == v) return;
    toggle_ground_mode();
}

void UI::set_save_name(const char* s) {
    snprintf(save_name_, sizeof(save_name_), "%s", s ? s : "");
}

void UI::confirm_save() {
    std::string name(save_name_);
    // Strip anything that could escape saves/ or break the filesystem.
    std::string clean;
    for (char c : name) {
        unsigned char u = (unsigned char)c;
        bool ok = (u >= 'a' && u <= 'z') || (u >= 'A' && u <= 'Z') ||
                  (u >= '0' && u <= '9') || c == '-' || c == '_' || c == ' ';
        if (ok) clean += c;
    }
    while (!clean.empty() && clean.back() == ' ') clean.pop_back();
    if (!clean.empty() && (clean[0] == '.' || clean == "saves")) {
        clean.clear();
    }
    if (!clean.empty()) {
        std::string path = "saves/" + clean + ".r3do";
        save_scene(grid, cam, path, render_mode_, palette_idx_, animator_);
        show_msg(("Saved: " + clean).c_str());
    }
    save_dialog_active_ = false;
}

// ---- Unity-style axis gizmo at the selected object ----

Hittable* UI::selected_obj() const {
    if (selected_obj_ < 0 || selected_obj_ >= (int)obj_refs_.size())
        return nullptr;
    const ObjRef& r = obj_refs_[(size_t)selected_obj_];
    if (r.type == ObjRef::FREE) {
        if (r.free_idx < 0 || r.free_idx >= (int)grid.free_objects().size())
            return nullptr;
        return grid.free_objects()[(size_t)r.free_idx].get();
    }
    return grid.get(r.i, r.j, r.k);
}

// Which gizmo axis (if any) lies under the mouse: X = screen right,
// Y = screen up, Z = depth close+far line through the object center.
int UI::gizmo_axis_at(int mx, int my, const Camera& c, int ww, int hh) {
    Hittable* obj = selected_obj();
    if (!obj) return -1;
    Vec3 center = obj->get_center();
    int sx, sy;
    if (!project_point(c, ww, hh, center, sx, sy)) return -1;
    Vec3 rel = center - c.pos;
    double depth = dot(rel, c.forward());
    if (depth <= 0.05) return -1;
    double f = depth * VIEWPORT_HEIGHT / (FOCAL_LENGTH * hh);
    double len = 45.0 * f; // ~45 px at this depth
    Vec3 rgt = c.right(), upv = cross(rgt, c.forward()), fwd = c.forward();
    int ax, ay, bx, by, zfx, zfy, zcx, zcy;
    if (!project_point(c, ww, hh, center + rgt * len, ax, ay)) return -1;
    if (!project_point(c, ww, hh, center + upv * len, bx, by)) return -1;
    if (!project_point(c, ww, hh, center + fwd * len, zfx, zfy)) return -1;
    if (!project_point(c, ww, hh, center - fwd * len, zcx, zcy)) return -1;
    auto seg_d2 = [&](int x0, int y0, int x1, int y1, int px, int py) -> double {
        double dx = x1 - x0, dy = y1 - y0;
        double l2 = dx * dx + dy * dy;
        if (l2 < 1e-6) {
            double ux = px - x0, uy = py - y0;
            return ux * ux + uy * uy;
        }
        double t = ((px - x0) * dx + (py - y0) * dy) / l2;
        if (t < 0.0) t = 0.0;
        if (t > 1.0) t = 1.0;
        double qx = x0 + t * dx, qy = y0 + t * dy;
        double ux = px - qx, uy = py - qy;
        return ux * ux + uy * uy;
    };
    int best = -1;
    double best_d = 10.0 * 10.0; // ~10 px pick radius
    double d = seg_d2(sx, sy, ax, ay, mx, my); // X: screen right
    if (d < best_d) { best_d = d; best = 0; }
    d = seg_d2(sx, sy, bx, by, mx, my);        // Y: screen up
    if (d < best_d) { best_d = d; best = 1; }
    d = seg_d2(zfx, zfy, zcx, zcy, mx, my);   // Z: close+far depth
    if (d < best_d) best = 2;
    return best;
}

// Draw the axis gizmo (red X, green Y, blue close+far Z) at the selected
// object's projected center; the constrained axis draws in white while a
// gizmo drag is active.
void UI::draw_gizmo(DisplayWin& d, const Camera& c) {
    if (is_sculpt_mode()) return;
    Hittable* obj = selected_obj();
    if (!obj) return;
    Vec3 center = obj->get_center();
    int ww = d.width(), hh = d.height();
    int sx, sy;
    if (!project_point(c, ww, hh, center, sx, sy)) return;
    Vec3 rel = center - c.pos;
    double depth = dot(rel, c.forward());
    if (depth <= 0.05) return;
    double f = depth * VIEWPORT_HEIGHT / (FOCAL_LENGTH * hh);
    double len = 45.0 * f;
    Vec3 rgt = c.right(), upv = cross(rgt, c.forward()), fwd = c.forward();
    int ax, ay, bx, by, zfx, zfy, zcx, zcy;
    if (!project_point(c, ww, hh, center + rgt * len, ax, ay)) return;
    if (!project_point(c, ww, hh, center + upv * len, bx, by)) return;
    if (!project_point(c, ww, hh, center + fwd * len, zfx, zfy)) return;
    if (!project_point(c, ww, hh, center - fwd * len, zcx, zcy)) return;

    auto arrow = [&](int x0, int y0, int x1, int y1, unsigned col) {
        d.draw_line(x0, y0, x1, y1, col);
        double dx = x1 - x0, dy = y1 - y0;
        double l = std::sqrt(dx * dx + dy * dy);
        if (l < 2.0) return;
        dx /= l; dy /= l;
        int head = 7;
        int hx = (int)(x1 - dx * head + dy * 3);
        int hy = (int)(y1 - dy * head - dx * 3);
        int hx2 = (int)(x1 - dx * head - dy * 3);
        int hy2 = (int)(y1 - dy * head + dx * 3);
        d.draw_line(x1, y1, hx, hy, col);
        d.draw_line(x1, y1, hx2, hy2, col);
    };

    d.draw_crosshair(sx, sy, 3, 0xffffff);
    // X: red (screen right), Y: green (screen up), Z: blue (close+far depth)
    arrow(sx, sy, ax, ay, 0xff0000);
    arrow(sx, sy, bx, by, 0x00cc00);
    arrow(zfx, zfy, zcx, zcy, 0x0000ff);
    if (gizmo_dragging_) {
        int hx, hy;
        switch (gizmo_axis_) {
        case 0: hx = ax; hy = ay; break;
        case 1: hx = bx; hy = by; break;
        default: hx = zfx; hy = zfy; break;
        }
        arrow(sx, sy, hx, hy, 0xffffff); // constrained axis highlight
    }
}

