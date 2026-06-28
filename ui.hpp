#ifndef UI_H
#define UI_H

#include "v3.hpp"
#include "grid.hpp"
#include "camera.hpp"
#include "display.hpp"
#include <X11/keysym.h>

enum class ShapeType { SPHERE, BOX, CYLINDER, CONE };

class UI {
    Grid& grid;
    Camera& cam;
    DisplayWin& display;
    int palette_idx_;

    static const int MENU_W = 170;

    struct Button {
        int x, y, w, h;
        const char* text;
    };

    Vec3 palette_color(int index) const;
    bool try_place(ShapeType type);
    void clear_grid();
    int count_objects() const;
    char save_msg_[64];
    int save_msg_timer_;
    char save_name_[256];
    int save_name_len_;
    bool save_dialog_active_;
    int cursor_counter_;

public:
    UI(Grid& grid, Camera& cam, DisplayWin& display);

    void draw();
    bool handle_click(int mx, int my);
    void draw_save_dialog();
    bool is_save_dialog_active() const { return save_dialog_active_; }
    void handle_save_dialog_key(int keysym, char c);
    void open_save_dialog();
    void cancel_save();
    void confirm_save();
};

#endif
