#include "display.hpp"
#include <cstdlib>
#include <cstring>
#include <cstdio>

DisplayWin::DisplayWin(int width, int height, const char* title)
    : width_(width), height_(height), closed_(false), fullscreen_(false), data_(nullptr),
      last_keysym_(0), last_char_(0), mouse_x_(0), mouse_y_(0),
      mouse_dx_(0), mouse_dy_(0), mouse_press_x_(0), mouse_press_y_(0),
      mouse_clicked_(false), mouse_down_(false), mouse_released_(false) {
    d = XOpenDisplay(nullptr);
    if (!d) {
        fprintf(stderr, "Cannot open X display\n");
        exit(1);
    }
    int screen = DefaultScreen(d);
    Window root = RootWindow(d, screen);
    w = XCreateSimpleWindow(d, root, 0, 0, width, height, 1,
        BlackPixel(d, screen), 0x222222);
    XStoreName(d, w, title);
    XSelectInput(d, w, ExposureMask | KeyPressMask | ButtonPressMask | ButtonReleaseMask | ButtonMotionMask | StructureNotifyMask);
    wm_delete = XInternAtom(d, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(d, w, &wm_delete, 1);
    XMapWindow(d, w);

    XEvent e;
    do { XNextEvent(d, &e); } while (e.type != Expose);

    gc = XCreateGC(d, w, 0, nullptr);

    font_ = XLoadQueryFont(d, "fixed");
    if (!font_) font_ = XLoadQueryFont(d, "9x15");
    if (font_) XSetFont(d, gc, font_->fid);

    white_pixel_ = WhitePixel(d, screen);
    black_pixel_ = BlackPixel(d, screen);

    Visual* vis = DefaultVisual(d, screen);
    int depth = DefaultDepth(d, screen);
    img = XCreateImage(d, vis, depth, ZPixmap, 0, nullptr, width, height, 32, 0);
    data_ = new unsigned int[width * height]();
    img->data = (char*)data_;
}

DisplayWin::~DisplayWin() {
    if (font_) XFreeFont(d, font_);
    if (img) {
        img->data = nullptr;
        XDestroyImage(img);
    }
    delete[] data_;
    if (gc) XFreeGC(d, gc);
    if (w) XDestroyWindow(d, w);
    if (d) XCloseDisplay(d);
}

void DisplayWin::set_pixel(int x, int y, unsigned char r, unsigned char g, unsigned char b) {
    if (x < 0 || x >= width_ || y < 0 || y >= height_) return;
    int ry = height_ - 1 - y;
    data_[ry * width_ + x] = (r << 16) | (g << 8) | b;
}

void DisplayWin::update() {
    XPutImage(d, w, gc, img, 0, 0, 0, 0, width_, height_);
    XFlush(d);
}

void DisplayWin::process_events() {
    XEvent e;
    while (XPending(d) > 0) {
        XNextEvent(d, &e);
        if (e.type == KeyPress) {
            KeySym keysym;
            char buf[32];
            XLookupString(&e.xkey, buf, sizeof(buf), &keysym, nullptr);
            last_keysym_ = (int)keysym;
            if (buf[0]) last_char_ = buf[0];
        }
        if (e.type == ButtonPress) {
            mouse_x_ = e.xbutton.x;
            mouse_y_ = e.xbutton.y;
            mouse_clicked_ = true;
            mouse_down_ = true;
            mouse_released_ = false;
            mouse_press_x_ = mouse_x_;
            mouse_press_y_ = mouse_y_;
            mouse_dx_ = 0;
            mouse_dy_ = 0;
        }
        if (e.type == ButtonRelease) {
            mouse_down_ = false;
            mouse_released_ = true;
        }
        if (e.type == MotionNotify) {
            if (mouse_down_) {
                mouse_dx_ += e.xmotion.x - mouse_x_;
                mouse_dy_ += e.xmotion.y - mouse_y_;
            }
            mouse_x_ = e.xmotion.x;
            mouse_y_ = e.xmotion.y;
        }
        if (e.type == DestroyNotify) { closed_ = true; }
        if (e.type == ClientMessage) {
            if ((Atom)e.xclient.data.l[0] == wm_delete) { closed_ = true; }
        }
    }
}

void DisplayWin::draw_text(int x, int y, const char* text, unsigned long color) {
    XSetForeground(d, gc, color);
    XDrawString(d, w, gc, x, y, text, strlen(text));
}

void DisplayWin::fill_rect(int x, int y, int w, int h, unsigned long color) {
    XSetForeground(d, gc, color);
    XFillRectangle(d, this->w, gc, x, y, w, h);
}

void DisplayWin::clear_buffer() {
    memset(data_, 0, width_ * height_ * sizeof(unsigned int));
}

void DisplayWin::draw_crosshair(int cx, int cy, int size, unsigned long color) {
    XSetForeground(d, gc, color);
    XFillRectangle(d, w, gc, cx - 1, cy - size, 3, size * 2 + 1);
    XFillRectangle(d, w, gc, cx - size, cy - 1, size * 2 + 1, 3);
}

void DisplayWin::resize(int new_w, int new_h) {
    if (img) {
        img->data = nullptr;
        XDestroyImage(img);
    }
    delete[] data_;
    data_ = new unsigned int[new_w * new_h]();

    Visual* vis = DefaultVisual(d, DefaultScreen(d));
    int depth = DefaultDepth(d, DefaultScreen(d));
    img = XCreateImage(d, vis, depth, ZPixmap, 0, nullptr, new_w, new_h, 32, 0);
    img->data = (char*)data_;

    width_ = new_w;
    height_ = new_h;
    XResizeWindow(d, w, new_w, new_h);
}

void DisplayWin::toggle_fullscreen() {
    fullscreen_ = !fullscreen_;

    if (fullscreen_) {
        Screen* s = DefaultScreenOfDisplay(d);
        resize(WidthOfScreen(s), HeightOfScreen(s));
    } else {
        resize(800, 600);
    }

    Atom wm_state = XInternAtom(d, "_NET_WM_STATE", False);
    Atom fs = XInternAtom(d, "_NET_WM_STATE_FULLSCREEN", False);
    XEvent xev;
    memset(&xev, 0, sizeof(xev));
    xev.type = ClientMessage;
    xev.xclient.window = w;
    xev.xclient.message_type = wm_state;
    xev.xclient.format = 32;
    xev.xclient.data.l[0] = fullscreen_ ? 1 : 0;
    xev.xclient.data.l[1] = fs;
    xev.xclient.data.l[2] = 0;
    XSendEvent(d, DefaultRootWindow(d), False, SubstructureRedirectMask | SubstructureNotifyMask, &xev);
    XFlush(d);
}
