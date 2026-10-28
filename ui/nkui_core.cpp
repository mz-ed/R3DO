#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_IMPLEMENTATION
#define NK_XLIB_IMPLEMENTATION
// Trigger on press: the editor defers panel draws, so widget state must
// register immediately on the down-transition or the first click gets eaten.
#include "vendor/nuklear/nuklear.h"
#include "vendor/nuklear/nuklear_xlib.h"

#include "nkui.hpp"
#include "ui.hpp"
#include "display.hpp"
#include "v3.hpp"
#include <cstdio>
#include <cstring>

// ---- static event forwarding: DisplayWin hook -> Nuklear input ----
static Display* g_dpy = nullptr;
static int g_screen = 0;
static Window g_win = 0;
static bool g_hook_ready = false;

static void nkui_event_forward(XEvent& ev) {
    if (!g_hook_ready) return;
    if (XFilterEvent(&ev, g_win)) return;
    nk_xlib_handle_event(g_dpy, g_screen, g_win, &ev);
}

// ---- theme: match R3DO dark palette ----
static void nkui_style(struct nk_context* ctx) {
    struct nk_color table[NK_COLOR_COUNT];
    table[NK_COLOR_TEXT]             = nk_rgba(204, 204, 204, 255);
    table[NK_COLOR_WINDOW]           = nk_rgba(30,  30,  30,  255);
    table[NK_COLOR_HEADER]           = nk_rgba(22,  22,  22,  255);
    table[NK_COLOR_BORDER]           = nk_rgba(60,  60,  60,  255);
    table[NK_COLOR_BUTTON]           = nk_rgba(50,  50,  50,  255);
    table[NK_COLOR_BUTTON_HOVER]     = nk_rgba(62,  62,  62,  255);
    table[NK_COLOR_BUTTON_ACTIVE]    = nk_rgba(74, 144, 226, 255);
    table[NK_COLOR_TOGGLE]           = nk_rgba(45,  45,  45,  255);
    table[NK_COLOR_TOGGLE_HOVER]     = nk_rgba(62,  62,  62,  255);
    table[NK_COLOR_TOGGLE_CURSOR]    = nk_rgba(74, 144, 226, 255);
    table[NK_COLOR_SELECT]           = nk_rgba(50,  50,  50,  255);
    table[NK_COLOR_SELECT_ACTIVE]    = nk_rgba(74, 144, 226, 255);
    table[NK_COLOR_SLIDER]           = nk_rgba(45,  45,  45,  255);
    table[NK_COLOR_SLIDER_CURSOR]    = nk_rgba(74, 144, 226, 255);
    table[NK_COLOR_SLIDER_CURSOR_HOVER] = nk_rgba(97, 175, 239, 255);
    table[NK_COLOR_SLIDER_CURSOR_ACTIVE] = nk_rgba(137, 182, 247, 255);
    table[NK_COLOR_PROPERTY]         = nk_rgba(40,  40,  40,  255);
    table[NK_COLOR_EDIT]             = nk_rgba(40,  40,  40,  255);
    table[NK_COLOR_EDIT_CURSOR]      = nk_rgba(204, 204, 204, 255);
    table[NK_COLOR_COMBO]            = nk_rgba(50,  50,  50,  255);
    table[NK_COLOR_CHART]            = nk_rgba(45,  45,  45,  255);
    table[NK_COLOR_CHART_COLOR]      = nk_rgba(74, 144, 226, 255);
    table[NK_COLOR_CHART_COLOR_HIGHLIGHT] = nk_rgba(97, 175, 239, 255);
    table[NK_COLOR_SCROLLBAR]        = nk_rgba(45,  45,  45,  255);
    table[NK_COLOR_SCROLLBAR_CURSOR] = nk_rgba(80,  80,  80,  255);
    table[NK_COLOR_SCROLLBAR_CURSOR_HOVER] = nk_rgba(100, 100, 100, 255);
    table[NK_COLOR_SCROLLBAR_CURSOR_ACTIVE] = nk_rgba(120, 120, 120, 255);
    table[NK_COLOR_TAB_HEADER]       = nk_rgba(38, 38, 38, 255);
    nk_style_from_table(ctx, table);

    ctx->style.window.rounding = 2.0f;
    ctx->style.button.rounding = 2.0f;
    ctx->style.property.rounding = 2.0f;
    ctx->style.edit.rounding = 2.0f;
    ctx->style.combo.rounding = 2.0f;
    ctx->style.progress.rounding = 2.0f;
}

bool NkUI::init(DisplayWin& dw) {
    dpy_ = dw.xdisplay();
    win_ = dw.xwindow();
    screen_ = XDefaultScreen(dpy_);

    XFont* font = nk_xfont_create(dpy_, "fixed");
    if (!font) return false;
    ctx_ = nk_xlib_init(font, dpy_, screen_, win_,
                        (unsigned)dw.width(), (unsigned)dw.height());
    if (!ctx_) return false;

    nkui_style(ctx_);
    nk_xlib_push_font(font);

    g_dpy = dpy_; g_screen = screen_; g_win = win_;
    hooked_ = true;
    g_hook_ready = true;
    dw.set_event_hook(&nkui_event_forward);
    return true;
}

void NkUI::shutdown() {
    if (!ctx_) return;
    hooked_ = false;
    g_hook_ready = false;
    nk_xlib_shutdown();
    ctx_ = nullptr;
}

NkUI::~NkUI() { shutdown(); }

void NkUI::begin() {
    if (ctx_) nk_input_begin(ctx_);
}

void NkUI::end_frame() {
    if (ctx_) nk_input_end(ctx_);
}

void NkUI::render() {
    if (!ctx_ || !hooked_) return;
    nk_xlib_set_blit_rects(panel_n_, px_, py_, pw_, ph_);
    nk_xlib_render(win_, nk_rgb(30, 30, 30));
}

bool NkUI::hit(int mx, int my) const {
    for (int i = 0; i < panel_n_; i++)
        if (mx >= px_[i] && mx < px_[i] + pw_[i] &&
            my >= py_[i] && my < py_[i] + ph_[i])
            return true;
    return false;
}

// expand rect by margin (shadow spill tolerance)
static void expand(int& x, int& y, int& w, int& h, int ww, int wh, int m) {
    x -= m; y -= m; w += 2 * m; h += 2 * m;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > ww) w = ww - x;
    if (y + h > wh) h = wh - y;
}

void NkUI::draw(UI& ui, Animator& anim, Camera& cam,
                const std::vector<std::string>& target_names,
                const std::vector<Hittable*>& target_objs) {
    if (!ctx_) return;
    any_active_ = nk_item_is_any_active(ctx_) != 0;

    int ww = 800, wh = 600;
    XWindowAttributes attr;
    if (XGetWindowAttributes(dpy_, win_, &attr)) {
        if (attr.width > 0) ww = attr.width;
        if (attr.height > 0) wh = attr.height;
    }

    draw_sidebar(ui, ww, wh);
    draw_anim_panel(anim, cam, target_names, target_objs, wh);
    draw_save_modal(ui, ww, wh);

    // publish rects (expanded a bit for border/shadow spill)
    panel_n_ = 0;
    for (struct nk_window* it = ctx_->begin; it; it = it->next) {
        if (it->flags & NK_WINDOW_HIDDEN) continue;
        if (panel_n_ >= MAX_PANELS) break;
        struct nk_window* popups[2] = {it, it->popup.win};
        for (auto* w : popups) {
            if (!w || (w != it && (w->flags & NK_WINDOW_HIDDEN))) continue;
            if (panel_n_ >= MAX_PANELS) break;
            int x = (int)w->bounds.x, y = (int)w->bounds.y;
            int wdd = (int)w->bounds.w, hgt = (int)w->bounds.h;
            expand(x, y, wdd, hgt, ww, wh, 4);
            px_[panel_n_] = x; py_[panel_n_] = y; pw_[panel_n_] = wdd; ph_[panel_n_] = hgt;
            panel_n_++;
        }
    }
}

void NkUI::draw_save_modal(UI& ui, int ww, int wh) {
    save_modal_open_ = false;
    if (!ui.is_save_dialog_active()) return;
    save_modal_open_ = true;

    int mw = 330, mh = 160;
    bool open = nk_begin(ctx_, "Save Scene",
                         nk_rect((float)((ww - mw) / 2), (float)((wh - mh) / 3), (float)mw, (float)mh),
                         NK_WINDOW_BORDER | NK_WINDOW_TITLE);
    if (!open) { nk_end(ctx_); return; }
    ctx_->current->layout->flags &= ~(nk_flags)NK_WINDOW_ROM;
    ctx_->current->flags &= ~(nk_flags)NK_WINDOW_ROM;

    nk_layout_row_dynamic(ctx_, 24, 1);
    nk_edit_focus(ctx_, NK_EDIT_ALWAYS_INSERT_MODE);
    nk_flags act = nk_edit_string(ctx_, NK_EDIT_FIELD, save_buf_, &save_len_, 63, nk_filter_ascii);
    nk_layout_row_dynamic(ctx_, 24, 2);
    bool do_save = nk_button_label(ctx_, "Save") != 0;
    bool do_cancel = nk_button_label(ctx_, "Cancel") != 0;
    if (act & NK_EDIT_COMMITED) do_save = true;
    if (do_save) {
        ui.set_save_name(save_buf_);
        ui.confirm_save();
        nk_edit_unfocus(ctx_);
    } else if (do_cancel) {
        ui.cancel_save();
        nk_edit_unfocus(ctx_);
    }

    nk_end(ctx_);
}
