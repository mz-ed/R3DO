#include "display.hpp"
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <sys/select.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>
#include <X11/XKBlib.h>

DisplayWin::DisplayWin(int width, int height, const char* title)
    : d(nullptr), w(0), gc(nullptr), img(nullptr), width_(width), height_(height),
      closed_(false), fullscreen_(false), font_(nullptr) {
    d = XOpenDisplay(nullptr);
    if (!d) {
        fprintf(stderr, "Cannot open X display\n");
        exit(1);
    }
    Bool detectable = False;
    XkbSetDetectableAutoRepeat(d, True, &detectable);

    int screen = DefaultScreen(d);
    Window root = RootWindow(d, screen);
    w = XCreateSimpleWindow(d, root, 0, 0, width, height, 1,
        BlackPixel(d, screen), 0x222222);
    XStoreName(d, w, title);
    XSelectInput(d, w, ExposureMask | KeyPressMask | KeyReleaseMask |
                 ButtonPressMask | ButtonReleaseMask | ButtonMotionMask |
                 StructureNotifyMask);
    wm_delete = XInternAtom(d, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(d, w, &wm_delete, 1);
    XMapWindow(d, w);

    XEvent e;
    while (XCheckTypedEvent(d, Expose, &e) == False && XPending(d) > 0)
        XNextEvent(d, &e);

    gc = XCreateGC(d, w, 0, nullptr);

    font_ = XLoadQueryFont(d, "fixed");
    if (!font_) font_ = XLoadQueryFont(d, "9x15");
    if (font_) XSetFont(d, gc, font_->fid);

    // Probe MIT-SHM availability
    if (XShmQueryExtension(d))
        shm_ok_ = true;

    alloc_image(width, height);
}

void DisplayWin::free_image() {
    if (!img) return;
    if (shm_attached_) {
        XShmDetach(d, &shmi_);
        img->data = nullptr;
        XDestroyImage(img);
        shmdt(shmi_.shmaddr);
        shmctl(shmi_.shmid, IPC_RMID, nullptr);
        shm_attached_ = false;
    } else {
        img->data = nullptr;
        XDestroyImage(img);
        delete[] data_;
    }
    img = nullptr;
    data_ = nullptr;
}

void DisplayWin::alloc_image(int new_w, int new_h) {
    free_image();

    Visual* vis = DefaultVisual(d, DefaultScreen(d));
    int depth = DefaultDepth(d, DefaultScreen(d));

    // Try MIT-SHM path
    if (shm_ok_) {
        img = XShmCreateImage(d, vis, depth, ZPixmap, nullptr, &shmi_, new_w, new_h);
        if (img) {
            shmi_.shmid = shmget(IPC_PRIVATE, img->bytes_per_line * img->height, IPC_CREAT | 0600);
            if (shmi_.shmid >= 0) {
                shmi_.shmaddr = (char*)shmat(shmi_.shmid, nullptr, 0);
                if (shmi_.shmaddr != (void*)-1) {
                    img->data = shmi_.shmaddr;
                    shmi_.readOnly = False;
                    if (XShmAttach(d, &shmi_)) {
                        data_ = (unsigned int*)shmi_.shmaddr;
                        shm_attached_ = true;
                        width_ = new_w;
                        height_ = new_h;
                        XSync(d, False);
                        return;
                    }
                    shmdt(shmi_.shmaddr);
                }
                shmctl(shmi_.shmid, IPC_RMID, nullptr);
            }
            img->data = nullptr;
            XDestroyImage(img);
            img = nullptr;
        }
        shm_ok_ = false; // don't retry
    }

    // Fallback: regular X11 image
    data_ = new unsigned int[new_w * new_h]();
    img = XCreateImage(d, vis, depth, ZPixmap, 0, (char*)data_, new_w, new_h, 32, 0);
    shm_attached_ = false;
    width_ = new_w;
    height_ = new_h;
}

DisplayWin::~DisplayWin() {
    free_image();
    if (font_) XFreeFont(d, font_);
    if (gc) XFreeGC(d, gc);
    if (w) XDestroyWindow(d, w);
    if (d) XCloseDisplay(d);
}

void DisplayWin::set_pixel(int x, int y, unsigned int rgb) {
    if (x < 0 || x >= width_ || y < 0 || y >= height_) return;
    data_[y * width_ + x] = rgb;
}

void DisplayWin::update() {
    if (shm_attached_) {
        XShmPutImage(d, w, gc, img, 0, 0, 0, 0, width_, height_, False);
        XSync(d, False);
    } else {
        XPutImage(d, w, gc, img, 0, 0, 0, 0, width_, height_);
        XFlush(d);
    }
}

bool DisplayWin::wait_event(int timeout_ms) {
    fd_set fds;
    FD_ZERO(&fds);
    int fd = ConnectionNumber(d);
    FD_SET(fd, &fds);
    timeval tv{timeout_ms / 1000, (timeout_ms % 1000) * 1000};
    if (select(fd + 1, &fds, nullptr, nullptr, &tv) <= 0) return false;
    return XPending(d) > 0;
}

void DisplayWin::process_events() {
    XEvent e;
    while (XPending(d) > 0) {
        XNextEvent(d, &e);
        switch (e.type) {
        case KeyPress: {
            KeySym keysym;
            char buf[32];
            XLookupString(&e.xkey, buf, sizeof(buf), &keysym, nullptr);
            int kc = e.xkey.keycode;
            if (kc >= 0 && kc < MAX_KEYCODES) held_keysym_[kc] = (int)keysym;
            key_queue_.push_back((int)keysym);
            if (buf[0]) last_char_ = buf[0];
            break;
        }
        case KeyRelease: {
            int kc = e.xkey.keycode;
            if (kc >= 0 && kc < MAX_KEYCODES) held_keysym_[kc] = 0;
            break;
        }
        case ButtonPress:
            mouse_x_ = e.xbutton.x;
            mouse_y_ = e.xbutton.y;
            mouse_button_ = e.xbutton.button;
            mouse_clicked_ = true;
            mouse_down_ = true;
            mouse_released_ = false;
            mouse_press_x_ = mouse_x_;
            mouse_press_y_ = mouse_y_;
            mouse_dx_ = 0;
            mouse_dy_ = 0;
            break;
        case ButtonRelease:
            mouse_down_ = false;
            mouse_released_ = true;
            break;
        case MotionNotify:
            if (mouse_down_) {
                mouse_dx_ += e.xmotion.x - mouse_x_;
                mouse_dy_ += e.xmotion.y - mouse_y_;
            }
            mouse_x_ = e.xmotion.x;
            mouse_y_ = e.xmotion.y;
            break;
        case ConfigureNotify:
            if (e.xconfigure.width != width_ || e.xconfigure.height != height_)
                alloc_image(e.xconfigure.width, e.xconfigure.height);
            break;
        case DestroyNotify:
            closed_ = true;
            break;
        case ClientMessage:
            if ((Atom)e.xclient.data.l[0] == wm_delete) closed_ = true;
            break;
        }
    }
}

bool DisplayWin::poll_key(int& keysym, char& c) {
    if (key_queue_.empty()) return false;
    keysym = key_queue_.front();
    key_queue_.pop_front();
    c = last_char_;
    last_char_ = 0;
    return true;
}

void DisplayWin::clear_keys() {
    key_queue_.clear();
    last_char_ = 0;
}

bool DisplayWin::key_held(int keysym) const {
    for (int i = 0; i < MAX_KEYCODES; i++)
        if (held_keysym_[i] == keysym) return true;
    return false;
}

void DisplayWin::draw_text(int x, int y, const char* text, unsigned long color) {
    if (!font_) return;
    XSetForeground(d, gc, color);
    XDrawString(d, w, gc, x, y, text, strlen(text));
}

void DisplayWin::fill_rect(int x, int y, int fw, int fh, unsigned long color) {
    XSetForeground(d, gc, color);
    XFillRectangle(d, this->w, gc, x, y, fw, fh);
    int x0 = std::max(x, 0);
    int y0 = std::max(y, 0);
    int x1 = std::min(x + fw, width_);
    int y1 = std::min(y + fh, height_);
    for (int py = y0; py < y1; py++)
        for (int px = x0; px < x1; px++)
            data_[py * width_ + px] = (unsigned int)color;
}

void DisplayWin::draw_crosshair(int cx, int cy, int size, unsigned long color) {
    XSetForeground(d, gc, color);
    XFillRectangle(d, w, gc, cx - 1, cy - size, 3, size * 2 + 1);
    XFillRectangle(d, w, gc, cx - size, cy - 1, size * 2 + 1, 3);
}

void DisplayWin::resize(int new_w, int new_h) {
    alloc_image(new_w, new_h);
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
