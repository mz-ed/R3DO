#ifndef UI_H
#define UI_H

#include "v3.hpp"
#include "grid.hpp"
#include "camera.hpp"
#include "display.hpp"
#include <X11/keysym.h>
#include <string>
#include <vector>

enum class ShapeType { SPHERE, BOX, CYLINDER, CONE, MESH };

enum class BtnID {
    SPHERE, BOX, CYLINDER, CONE, MESH,
    MESH_PREV, MESH_NEXT,
    CLEAR, SAVE,
    MODE, GROUND, TERRAIN,
    SCULPT_ADD, SCULPT_BRUSH_INC, SCULPT_BRUSH_DEC, SCULPT_STR_INC, SCULPT_STR_DEC,
    NONE
};

class UI {
public:
    static const int SIDEBAR_W = 170;

    UI(Grid& grid, Camera& cam, DisplayWin& display);

    // --- state accessors ---
    bool ground_mode() const { return ground_mode_; }
    void toggle_ground_mode();
    int render_mode() const { return render_mode_; }
    void set_render_mode(int m) { render_mode_ = m; update_labels(); }
    int palette_idx() const { return palette_idx_; }

    void draw();
    bool handle_click(int mx, int my);

    bool is_save_dialog_active() const { return save_dialog_active_; }
    void handle_save_dialog_key(int keysym, char c);

    bool undo();
    bool place_last_shape();
    // Capture state before an external mutation (e.g. click-to-delete).
    void snapshot_for_undo() { push_undo(); }

private:
    Grid& grid;
    Camera& cam;
    DisplayWin& display;

    static const int BTN_H = 27;
    static const int BTN_GAP = 3;
    static const int SML_H = 18;

    struct ButtonDef {
        BtnID id;
        std::string icon;
        std::string label;
    };
    struct Section {
        std::string title;
        std::vector<ButtonDef> buttons;
    };

    std::vector<Section> sections_;
    int mouse_x_ = 0, mouse_y_ = 0;
    BtnID hovered_ = BtnID::NONE;

    int palette_idx_ = 0;
    ShapeType last_shape_ = ShapeType::SPHERE;
    int render_mode_ = 1;
    bool ground_mode_ = false;
    std::string mode_label_ = "Mode: Billboard";
    std::string ground_label_ = "Ground: OFF";

    char save_msg_[64] = {0};
    double save_msg_until_ = 0;   // seconds (steady clock)
    char save_name_[256] = {0};
    int save_name_len_ = 0;
    bool save_dialog_active_ = false;
    int cursor_counter_ = 0;

    std::vector<std::string> mesh_files_;
    int mesh_idx_ = 0;

    std::vector<std::string> undo_stack_;

    int sidebar_x() const { return display.width() - SIDEBAR_W; }
    void build_sections();
    void update_labels();
    int section_y(int section_idx) const;
    int button_y(int section_idx, int btn_idx) const;
    BtnID button_at(int mx, int my) const;
    Vec3 palette_color(int index) const;
    bool try_place(ShapeType type);
    void push_undo();
    void show_msg(const char* msg);
    void cycle_mesh(int dir);
    void scan_meshes();
    void open_save_dialog();
    void cancel_save();
    void confirm_save();
    void draw_save_dialog();
};

#endif
