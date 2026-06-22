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
    bool closed_;
    unsigned int* data_;
    int last_keysym_;
    int mouse_x_, mouse_y_;
    bool mouse_clicked_;
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
    bool mouse_clicked() const { return mouse_clicked_; }
    void clear_mouse() { mouse_clicked_ = false; }

    void set_pixel(int x, int y, unsigned char r, unsigned char g, unsigned char b);
    void update();
    void process_events();
    void draw_text(int x, int y, const char* text, unsigned long color);
    void fill_rect(int x, int y, int w, int h, unsigned long color);
};

#endif
