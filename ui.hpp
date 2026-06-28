#ifndef UI_H
#define UI_H

#include "v3.hpp"
#include "grid.hpp"
#include "camera.hpp"
#include "display.hpp"

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

public:
    UI(Grid& grid, Camera& cam, DisplayWin& display);

    void draw();
    bool handle_click(int mx, int my);
};

#endif
