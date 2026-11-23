#ifndef UI_H
#define UI_H

#include "v3.hpp"
#include "grid.hpp"
#include "camera.hpp"
#include "display.hpp"
#include "render_modes.hpp"
#include "sculpt_engine.hpp"
#include "hittable.hpp"
#include "skeleton.hpp"
#include "heightfield.hpp"
#include "animation.hpp"
#include <X11/keysym.h>
#include <string>
#include <vector>

enum class ShapeType { SPHERE, BOX, CYLINDER, CONE, MESH };

// State container + actions for the Nuklear-drawn panels.
// All pixel drawing / hit-testing lives in nkui.cpp now.
class Mesh;
class UI {
public:
    UI(Grid& grid, Camera& cam, DisplayWin& display, SculptEngine* sculpt = nullptr);

    void set_sculpt_engine(SculptEngine* s) { sculpt_ = s; }

    static constexpr int SIDEBAR_W = 180; // Nuklear sidebar width (docked right)

    // --- state accessors ---
    bool ground_mode() const { return ground_mode_; }
    void toggle_ground_mode();
    void set_ground_mode(bool v);
    int render_mode() const { return render_mode_; }
    void set_render_mode(int m) {
        int max_mode = mode_count() - 1;
        if (m < 0) m = 0;
        if (m > max_mode) m = max_mode;
        render_mode_ = m;
        update_labels();
    }
    // True when the active mode consumes mouse drags for editing (sculpt-style).
    bool is_sculpt_mode() const {
        return get_mode(render_mode_).uses_mouse_for_strokes;
    }
    int palette_idx() const { return palette_idx_; }
    float brush_radius() const { return brush_radius_; }
    void set_brush_radius(float r);
    float brush_strength() const { return brush_strength_; }
    void set_brush_strength(float s);
    BrushType brush_type() const { return brush_type_; }
    void set_brush_type(BrushType t) { brush_type_ = t; }
    bool brush_invert() const { return brush_invert_; }
    void set_brush_invert(bool v) { brush_invert_ = v; }
    int sculpt_segs() const { return sculpt_segs_; }
    int sculpt_rings() const { return sculpt_rings_; }
    void set_sculpt_res(int segs, int rings);

    // mesh picker
    const std::vector<std::string>& mesh_files() const { return mesh_files_; }
    int mesh_idx() const { return mesh_idx_; }
    void set_mesh_idx(int i) {
        if (mesh_files_.empty()) mesh_idx_ = 0;
        else if (i >= 0 && i < (int)mesh_files_.size()) mesh_idx_ = i;
    }
    void rescan_meshes() { scan_meshes(); }
    void cycle_mesh(int dir);

    // object selection & movement
    const std::vector<std::string>* object_list();
    int selected_object() const { return selected_obj_; }
    void select_object(int i);
    void move_selected(const Vec3& d);
    void center_selected_above();
    void reset_selected();

    // mouse drag gizmo: press on an object to grab it, drag to move on the
    // camera-facing plane through the grab point, release to commit.
    bool start_drag(int mx, int my);
    void drag_update(int mx, int my, bool shift_held);
    void end_drag(); // commits object drags; re-binds after a joint drag
    void cancel_drag();
    bool is_dragging() const { return dragging_; }
    bool drag_center(Vec3& out) const; // visual center while dragging

    // shared world position (read/write) — the sidebar editor and animation
    // panel both resolve through these so every source shares one value
    bool selected_pos(Vec3& out) const;
    void set_selected_pos(const Vec3& p);

    // actions (invoked by NkUI widgets)
    bool place_shape(ShapeType type) { return try_place(type); }
    ShapeType last_shape() const { return last_shape_; }
    void clear_scene();
    void add_sculpt_mesh();
    void add_skeleton_to_selected();
    Hittable* selected_free_object();
    const Skeleton* selected_skeleton() const;
    void add_bone_to_skeleton();
    void delete_bone_from_skeleton();
    void relink_selected_bones();
    void rename_selected_bone(const char* name);

    // bone gizmo (Bones mode): press near a joint to grab the bone. Drag
    // action follows bone_drag_mode_: STRUCTURE edits bind offsets (re-binds
    // on release), POSE moves points with the bone (live off), BEND rotates
    // the bone toward the cursor (drag-to-bend).
    enum { BONEDRAG_STRUCTURE = 0, BONEDRAG_POSE = 1, BONEDRAG_BEND = 2 };
    bool start_bone_drag(int mx, int my);
    bool is_bone_dragging() const { return bone_dragging_; }
    void bone_drag_update(int mx, int my);
    int bone_drag_mode() const { return bone_drag_mode_; }
    void set_bone_drag_mode(int m) {
        if (m >= BONEDRAG_STRUCTURE && m <= BONEDRAG_BEND) bone_drag_mode_ = m;
    }
    // what a POSE-mode stretch does past the skin's safe limit
    enum { STRETCH_LIMIT = 0, STRETCH_REMESH = 1 };
    int stretch_mode() const { return stretch_mode_; }
    void set_stretch_mode(int m) {
        if (m >= STRETCH_LIMIT && m <= STRETCH_REMESH) stretch_mode_ = m;
    }

    // pose mode: absolute euler sliders (degrees) on the selected bone, plus
    // straighten. pose_slider_tick applies only when a value changed.
    float pose_roll() const { return pose_roll_; }   // x axis
    float pose_yaw() const { return pose_yaw_; }     // y axis
    float pose_pitch() const { return pose_pitch_; } // z axis
    void set_pose_roll(float v) { pose_roll_ = v; }
    void set_pose_yaw(float v) { pose_yaw_ = v; }
    void set_pose_pitch(float v) { pose_pitch_ = v; }
    void pose_slider_tick();
    void pose_straighten();
    // writes the selected bone's live pose (euler + offset) into the clip
    // targeting it as FREE_BONE keys at the playhead; creates that clip when
    // none exists. interp: InterpolationType index from the anim panel.
    void key_bone_pose_to_clip(int interp);

    // per-bone influence area: falloff radius (0 = unlimited, legacy look)
    // and sharpness exponent. Changing them re-derives the vertex binds.
    float bone_influence();
    float bone_sharp();
    void set_bone_influence(float v);
    void set_bone_sharp(float v);
    void rederive_bone_binds();

    int selected_bone() const { return selected_bone_; }
    void set_selected_bone(int b) { selected_bone_ = b; sync_selection_hook(); }
    // writes the overlay-selection hook globals (modes/render_modes.hpp) so
    // the bones renderer highlights the picked joint and tints its verts
    void sync_selection_hook();
    void select_bone_with_name(int b) {
        selected_bone_ = b;
        sync_selection_hook();
        const Skeleton* sk = selected_skeleton();
        bone_name_[0] = 0; bone_name_len_ = 0;
        if (sk && b >= 0 && b < (int)sk->bones.size()) {
            const std::string& n = sk->bones[(size_t)b].name;
            bone_name_len_ = (int)n.copy(bone_name_, sizeof(bone_name_) - 1);
            bone_name_[bone_name_len_] = 0;
        }
    }
    char* bone_name_buf() { return bone_name_; }
    int* bone_name_len() { return &bone_name_len_; }
    void do_remesh();
    void load_terrain();

    // heightfield terrain (PNG import + paint brush)
    void scan_hmaps();
    const std::vector<std::string>& hmap_files() const { return hmap_files_; }
    void load_heightfield(int idx);
    bool export_heightfield(const char* name);
    void make_blank_heightfield();
    char* hf_name_buf() { return hf_name_; }
    int* hf_name_len() { return &hf_name_len_; }
    bool heightfield_active() const;

    // Scene save/load carries animation clips when an animator is attached.
    void set_animator(Animator* a) { animator_ = a; }
    Animator* animator() { return animator_; }

    // heightmap import dialog state (driven by NkUI modal)
    bool hmap_dialog_active_ = false;
    bool is_hmap_dialog_active() const { return hmap_dialog_active_; }
    void open_hmap_dialog() {
        scan_hmaps();
        hmap_dialog_active_ = true;
    }
    void cancel_hmap_dialog() { hmap_dialog_active_ = false; }

    // save dialog state (driven by NkUI modal)
    bool save_dialog_active_ = false;
    bool is_save_dialog_active() const { return save_dialog_active_; }
    void open_save_dialog() {
        save_dialog_active_ = true;
        save_name_[0] = 0;
        save_name_len_ = 0;
    }
    const char* save_name() const { return save_name_; }
    void set_save_name(const char* s);
    void confirm_save();
    void cancel_save() { save_dialog_active_ = false; }

    const char* msg() const { return save_msg_; }
    bool msg_fresh() const { return now_s() < save_msg_until_; }

    bool undo();
    bool place_last_shape() { return try_place(last_shape_); }
    void snapshot_for_undo() { push_undo(); }

private:
    Grid& grid;
    Camera& cam;
    DisplayWin& display;
    SculptEngine* sculpt_ = nullptr;
    Animator* animator_ = nullptr;

    int render_mode_ = 1;
    bool ground_mode_ = false;
    float brush_radius_ = 0.3f;
    float brush_strength_ = 0.2f;
    BrushType brush_type_ = BrushType::Draw;
    int sculpt_segs_ = 24;
    int sculpt_rings_ = 16;
    bool brush_invert_ = false;
    ShapeType last_shape_ = ShapeType::SPHERE;
    int palette_idx_ = 0;

public:
    int mode_label_idx_ = 1;

    // --- gizmo (Unity-style axis constraints at the selected object) ---
    // gizmo_axis_: -1 off, 0 = X (screen right), 1 = Y (screen up),
    //              2 = Z (depth close+far line through the object)
    int gizmo_axis_ = -1;
    bool gizmo_dragging_ = false;
    int gizmo_axis() const { return gizmo_axis_; }
    void gizmo_axis(int a) {
        if (a < -1) a = -1;
        if (a > 2) a = 2;
        gizmo_axis_ = a;
    }
    Hittable* selected_obj() const;
    int gizmo_axis_at(int mx, int my, const Camera& c, int w, int h);
    void draw_gizmo(DisplayWin& d, const Camera& c);

    // ---- sculpt-area modeling: subdivide polygons in an influence area ----
    // falloff per vertex uses the same bone-influence technique (radius +
    // sharp): subdivide_radius_ == 0 means unlimited reach (whole object)
    double subdivide_radius_ = 0.5;
    double subdivide_sharp_ = 2.0;
    int subdivide_levels_ = 1;
    bool subdivide_await_center_ = false;
    Vec3 subdivide_center_;
    bool subdivide_center_valid_ = false;
    double subdivide_radius() const { return subdivide_radius_; }
    void set_subdivide_radius(double r);
    double subdivide_sharp() const { return subdivide_sharp_; }
    void set_subdivide_sharp(double s);
    int subdivide_levels() const { return subdivide_levels_; }
    void set_subdivide_levels(int l);
    bool sculpt_awaiting_center() const { return subdivide_await_center_; }
    void set_sculpt_awaiting_center(bool v);
    Vec3 sculpt_center() const { return subdivide_center_; }
    bool sculpt_center_valid() const { return subdivide_center_valid_; }
    void set_sculpt_center(const Vec3& p);
    Mesh* selected_mesh();
    double mesh_scale_ = 1.0;
    double mesh_scale_prev_ = 1.0;
    int mesh_scale_sel_ = -1;
    double mesh_scale() const { return mesh_scale_; }
    void set_mesh_scale(double s);
    void subdivide_selected_area();

    // ---- object customization: area-of-affection deformation (bone-style
    // radius + sharp falloff around a hover center) ----
    enum { CUSTOMIZE_INFLATE = 0, CUSTOMIZE_DEFLATE = 1,
           CUSTOMIZE_PULL = 2, CUSTOMIZE_PUSH = 3 };
    int customize_mode_ = CUSTOMIZE_INFLATE;
    double customize_radius_ = 0.4;
    double customize_sharp_ = 2.0;
    double customize_strength_ = 0.08;
    bool customize_hover_ = false;
    Vec3 customize_center_;
    int customize_mode() const { return customize_mode_; }
    void set_customize_mode(int m) {
        if (m >= CUSTOMIZE_INFLATE && m <= CUSTOMIZE_PUSH) customize_mode_ = m;
    }
    double customize_radius() const { return customize_radius_; }
    void set_customize_radius(double r) { if (r >= 0.0) customize_radius_ = r; }
    double customize_sharp() const { return customize_sharp_; }
    void set_customize_sharp(double s) { if (s > 0.0) customize_sharp_ = s; }
    double customize_strength() const { return customize_strength_; }
    void set_customize_strength(double s) { if (s >= 0.0) customize_strength_ = s; }
    bool customize_hovering() const { return customize_hover_; }
    void set_customize_hovering(bool v) { customize_hover_ = v; }
    Vec3 customize_center() const { return customize_center_; }
    void set_customize_center(const Vec3& p) { customize_center_ = p; customize_hover_ = true; }
    void customize_clear_hover() { customize_hover_ = false; }
    void apply_customize_at(const Vec3& center);
    void show_msg(const char* msg);

private:
    char save_msg_[64] = {0};
    double save_msg_until_ = 0;
    char save_name_[256] = {0};
    int save_name_len_ = 0;

    std::vector<std::string> mesh_files_;
    int mesh_idx_ = 0;

    std::vector<std::string> hmap_files_;
    char hf_name_[64] = "blank";
    int hf_name_len_ = 5;

    int selected_obj_ = -1;
    int selected_bone_ = -1;
    char bone_name_[64] = "";
    int bone_name_len_ = 0;

    // --- gizmo drag state ---
    bool dragging_ = false;
    struct DragState {
        int sel = -1;        // index into obj_refs_
        Vec3 grab_hit;       // world point under cursor at press (visual pose)
        Vec3 visual0;        // visual center of the object at press
        bool was_cell = false;
        bool undo_saved = false; // undo snapshot fires on first real motion
        int ci = 0, cj = 0, ck = 0;
        int mx0 = 0, my0 = 0; // last screen pos (gizmo axis drag reference)
    };
    DragState drag_;

    // bone gizmo drag state (see start_bone_drag)
    bool bone_dragging_ = false;
    int bone_drag_bone_ = -1;
    Hittable* bone_drag_obj_ = nullptr;
    Skeleton* bone_drag_sk_ = nullptr;
    int bone_drag_mx_ = 0, bone_drag_my_ = 0;
    int bone_drag_mx0_ = 0, bone_drag_my0_ = 0; // press point (BEND reference)
    Vec3 bone_drag_e0_;                         // euler at press (BEND)
    bool bone_drag_pose_ = false;  // true: drag edits live pose off (non-structural)
    int bone_drag_mode_ = BONEDRAG_POSE;        // session default: POSE
    int stretch_mode_ = STRETCH_LIMIT;          // session default: capped
    bool bone_drag_anim_paused_ = false;        // clip playback paused for drag

    // pose sliders (degrees) + last values actually applied
    float pose_roll_ = 0, pose_yaw_ = 0, pose_pitch_ = 0;
    Vec3 pose_applied_ = Vec3(0, 0, 0);
    // true once the current run of pose edits has its undo snapshot; reset
    // on bone switch / straighten / undo so the next run pushes a fresh one
    bool pose_undo_dirty_ = false;
    bool infl_undo_dirty_ = false;   // same pattern for influence sliders
    bool bone_bind_pending_ = false; // influence changed while posed: re-derive on straighten

    struct ObjRef {
        enum Type { CELL, FREE } type;
        int i = -1, j = -1, k = -1;
        int free_idx = -1;
        Vec3 base_center;
    };
    std::vector<ObjRef> obj_refs_;
    std::vector<std::string> obj_names_;

    void relocate_cell(int sel, const Vec3& newc);

    // gizmo helpers
    int resolve_pick(void* hit_ptr, const Vec3& hit_p);
    Hittable* drag_obj();
    bool nearest_free_cell(const Vec3& target, Vec3& snapped);
    bool rehome_free(int sel, int i, int j, int k); // free -> cell (owned swap)

    std::vector<std::string> undo_stack_;

    double now_s() const;

    void update_labels();
    Vec3 palette_color(int index) const;
    bool try_place(ShapeType type);
    void push_undo();
    void scan_meshes();
};

#endif
