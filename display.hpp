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

public:
    DisplayWin(int width, int height, const char* title);
    ~DisplayWin();
    int width() const { return width_; }
    int height() const { return height_; }
    void set_pixel(int x, int y, unsigned char r, unsigned char g, unsigned char b);
    void update();
    void process_events();
    bool is_closed() const { return closed_; }
    int get_key() const { return last_keysym_; }
    void clear_key() { last_keysym_ = 0; }
};

#endif
