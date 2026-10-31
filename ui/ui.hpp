#ifndef UI_H
#define UI_H

#include "/home/kali/R3DO/src/v3.hpp"
#include "grid.hpp"
#include "camera.hpp"
#include "display.hpp"
#include "render_modes.hpp"
#include "sculpt_engine.hpp"
#include <X11/keysym.h>
#include <string>
#include <vector>

enum class ShapeType { SPHERE, BOX, CYLINDER, CONE, MESH };

// State container + actions for the Nuklear-drawn panels.
// All pixel drawing / hit-testing lives in nkui.cpp now.
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
    void end_drag() { dragging_ = false; }
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
    void do_remesh();
    void load_terrain();

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

private:
    char save_msg_[64] = {0};
    double save_msg_until_ = 0;
    char save_name_[256] = {0};
    int save_name_len_ = 0;

    std::vector<std::string> mesh_files_;
    int mesh_idx_ = 0;

    int selected_obj_ = -1;

    // --- gizmo drag state ---
    bool dragging_ = false;
    struct DragState {
        int sel = -1;        // index into obj_refs_
        Vec3 grab_hit;       // world point under cursor at press (visual pose)
        Vec3 visual0;        // visual center of the object at press
        bool was_cell = false;
        bool undo_saved = false; // undo snapshot fires on first real motion
        int ci = 0, cj = 0, ck = 0;
    };
    DragState drag_;

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
    void show_msg(const char* msg);
    void scan_meshes();
};

#endif
