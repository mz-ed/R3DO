#ifndef NKUI_H
#define NKUI_H

#include <X11/Xlib.h>
#include <string>
#include <vector>

class DisplayWin;
class Animator;
class UI;
class Hittable;
struct Camera;
struct nk_context;

// Nuklear-drawn UI, split one implementation file per feature:
//   ui/nkui_core.cpp    - X11 event hook, init/shutdown, frame loop, save dialog
//   ui/nkui_sidebar.cpp - docked main sidebar (right, non-movable)
//   ui/nkui_anim.cpp    - animation panel (left, T toggles)
// Region-limited blits keep the raytraced framebuffer visible outside panels.
class NkUI {
public:
    NkUI() = default;
    ~NkUI();
    NkUI(const NkUI&) = delete;
    NkUI& operator=(const NkUI&) = delete;

    bool init(DisplayWin& dw);
    void shutdown();

    void begin();
    void end_frame();
    void render();

    bool visible() const { return visible_; }
    void set_visible(bool v) { visible_ = v; }
    bool wants_keys() const { return any_active_ || save_modal_open_; }
    bool hit(int mx, int my) const;

    void draw(UI& ui, Animator& anim, Camera& cam,
              const std::vector<std::string>& target_names,
              const std::vector<Hittable*>& target_objs);

    nk_context* ctx() { return ctx_; }

    // Count + published rects of the live panels (incl. popups/modals) —
    // the renderer needs them so it never writes UI-underneath pixels.
    int panel_rects(const int*& px, const int*& py, const int*& pw, const int*& ph) const {
        px = px_; py = py_; pw = pw_; ph = ph_;
        return panel_n_;
    }

private:
    static const int PANEL_W = 300;

    struct nk_context* ctx_ = nullptr;
    Display* dpy_ = nullptr;
    Window win_ = 0;
    int screen_ = 0;
    bool visible_ = false;          // animation panel visibility
    bool any_active_ = false;
    bool save_modal_open_ = false;
    bool hooked_ = false;           // event hook registered

    // panel rects, filled during draw
    static const int MAX_PANELS = 8;
    int panel_n_ = 0;
    int px_[MAX_PANELS], py_[MAX_PANELS], pw_[MAX_PANELS], ph_[MAX_PANELS];

    // sidebar section collapse state
    bool sec_add_ = true;
    bool sec_sculpt_ = true;
    bool sec_actions_ = true;
    bool sec_settings_ = true;
    bool sec_objects_ = true;

    // save modal buffer
    char save_buf_[64] = {0};
    int save_len_ = 0;

    // animation panel state
    int sel_clip_ = 0;
    int sel_prop_ = 0;      // AnimProperty index
    int sel_interp_ = 0;    // InterpolationType index
    float key_time_ = 0.0f;
    float key_x_ = 0.0f, key_y_ = 0.0f, key_z_ = 0.0f;
    int cell_i_ = 1, cell_j_ = 1, cell_k_ = 1;   // CELL target picker
    // params applied to the next modifier attached
    float proc_amp_ = 0.5f, proc_freq_ = 1.0f, proc_speed_ = 1.0f;

    void draw_sidebar(UI& ui, int ww, int wh);
    void draw_anim_panel(Animator& anim, Camera& cam,
                         const std::vector<std::string>& target_names,
                         const std::vector<Hittable*>& target_objs, int wh);
    void draw_save_modal(UI& ui, int ww, int wh);
};

#endif
