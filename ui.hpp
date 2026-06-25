#ifndef UI_H
#define UI_H

#include "v3.hpp"
#include "grid.hpp"
#include "camera.hpp"
#include "display.hpp"

class UI {
    Grid& grid;
    Camera& cam;
    DisplayWin& display;
    int palette_idx_;

    static const int MENU_X = 630;
    static const int MENU_W = 170;

    struct Button {
        int x, y, w, h;
        const char* text;
    };

    Vec3 palette_color(int index) const;
    bool try_place(bool is_sphere);
    void clear_grid();
    int count_objects() const;

public:
    UI(Grid& grid, Camera& cam, DisplayWin& display);

    void draw();
    bool handle_click(int mx, int my);
};

#endif
