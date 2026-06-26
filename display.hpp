#ifndef DISPLAY_H
#define DISPLAY_H

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

class DisplayWin {
    Display* d;
    Window w;
    GC gc;
    XImage* img;
    Atom wm_delete;
    int width_, height_;
    bool closed_, fullscreen_;
    unsigned int* data_;
    int last_keysym_;
    int mouse_x_, mouse_y_;
    int mouse_dx_, mouse_dy_;
    int mouse_press_x_, mouse_press_y_;
    bool mouse_clicked_;
    bool mouse_down_, mouse_released_;
    XFontStruct* font_;
    unsigned long white_pixel_, black_pixel_;

public:
    DisplayWin(int width, int height, const char* title);
    ~DisplayWin();

    int width() const { return width_; }
    int height() const { return height_; }
    bool is_closed() const { return closed_; }
    int get_key() const { return last_keysym_; }
    void clear_key() { last_keysym_ = 0; }
    int mouse_x() const { return mouse_x_; }
    int mouse_y() const { return mouse_y_; }
    int mouse_dx() const { return mouse_dx_; }
    int mouse_dy() const { return mouse_dy_; }
    int mouse_press_x() const { return mouse_press_x_; }
    int mouse_press_y() const { return mouse_press_y_; }
    bool mouse_clicked() const { return mouse_clicked_; }
    bool is_mouse_down() const { return mouse_down_; }
    bool mouse_released() const { return mouse_released_; }
    void clear_mouse() { mouse_clicked_ = false; }
    void clear_mouse_delta() { mouse_dx_ = 0; mouse_dy_ = 0; }
    void clear_mouse_released() { mouse_released_ = false; }

    void set_pixel(int x, int y, unsigned char r, unsigned char g, unsigned char b);
    void update();
    void process_events();
    void draw_text(int x, int y, const char* text, unsigned long color);
    void fill_rect(int x, int y, int w, int h, unsigned long color);
    void draw_crosshair(int cx, int cy, int size, unsigned long color);
    bool is_fullscreen() const { return fullscreen_; }
    void toggle_fullscreen();
    void resize(int new_w, int new_h);
};

#endif
