#ifndef DISPLAY_H
#define DISPLAY_H

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/extensions/XShm.h>
#include <deque>

class DisplayWin {
    Display* d;
    Window w;
    GC gc;
    XImage* img;
    Atom wm_delete;
    int width_, height_;
    bool closed_, fullscreen_;
    unsigned int* data_ = nullptr;
    std::deque<int> key_queue_;
    char last_char_ = 0;
    static const int MAX_KEYCODES = 256;
    int held_keysym_[MAX_KEYCODES] = {0};
    int mouse_x_ = 0, mouse_y_ = 0;
    int mouse_dx_ = 0, mouse_dy_ = 0;
    int mouse_press_x_ = 0, mouse_press_y_ = 0;
    int mouse_button_ = 0;
    bool mouse_clicked_ = false;
    bool mouse_down_ = false, mouse_released_ = false;
    XFontStruct* font_;

    // MIT-SHM shared-memory framebuffer
    XShmSegmentInfo shmi_ = {};
    bool shm_ok_ = false;
    bool shm_attached_ = false;

    void free_image();
    void alloc_image(int new_w, int new_h);

public:
    DisplayWin(int width, int height, const char* title);
    ~DisplayWin();
    DisplayWin(const DisplayWin&) = delete;
    DisplayWin& operator=(const DisplayWin&) = delete;

    int width() const { return width_; }
    int height() const { return height_; }
    bool is_closed() const { return closed_; }

    bool poll_key(int& keysym, char& c);
    void clear_keys();
    bool key_held(int keysym) const;

    int mouse_x() const { return mouse_x_; }
    int mouse_y() const { return mouse_y_; }
    int mouse_dx() const { return mouse_dx_; }
    int mouse_dy() const { return mouse_dy_; }
    int mouse_press_x() const { return mouse_press_x_; }
    int mouse_press_y() const { return mouse_press_y_; }
    int mouse_button() const { return mouse_button_; }
    bool mouse_clicked() const { return mouse_clicked_; }
    bool is_mouse_down() const { return mouse_down_; }
    bool mouse_released() const { return mouse_released_; }
    void clear_mouse() { mouse_clicked_ = false; }
    void clear_mouse_delta() { mouse_dx_ = 0; mouse_dy_ = 0; }
    void clear_mouse_released() { mouse_released_ = false; }

    void set_pixel(int x, int y, unsigned int rgb);
    void update();
    void process_events();
    bool wait_event(int timeout_ms);
    void draw_text(int x, int y, const char* text, unsigned long color);
    void fill_rect(int x, int y, int w, int h, unsigned long color);
    void draw_crosshair(int cx, int cy, int size, unsigned long color);
    bool is_fullscreen() const { return fullscreen_; }
    void toggle_fullscreen();
    void resize(int new_w, int new_h);

    Display* xdisplay() const { return d; }
    Window xwindow() const { return w; }
    void grab_pointer();
    void ungrab_pointer();
};

#endif
