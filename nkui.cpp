#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_IMPLEMENTATION
#define NK_XLIB_IMPLEMENTATION
#define NK_BUTTON_TRIGGER_ON_RELEASE
#include "vendor/nuklear/nuklear.h"
#include "vendor/nuklear/nuklear_xlib.h"

#include "nkui.hpp"
#include "ui.hpp"
#include "animation.hpp"
#include "display.hpp"
#include "v3.hpp"
#include "grid.hpp"
#include "hittable.hpp"
#include "brush.hpp"
#include <cstdio>
#include <cstring>
#include <vector>
#include <string>

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
    table[NK_COLOR_BUTTON_ACTIVE]    = nk_rgba(74,  144, 226, 255);
    table[NK_COLOR_TOGGLE]           = nk_rgba(45,  45,  45,  255);
    table[NK_COLOR_TOGGLE_HOVER]     = nk_rgba(62,  62,  62,  255);
    table[NK_COLOR_TOGGLE_CURSOR]    = nk_rgba(74,  144, 226, 255);
    table[NK_COLOR_SELECT]           = nk_rgba(50,  50,  50,  255);
    table[NK_COLOR_SELECT_ACTIVE]    = nk_rgba(74,  144, 226, 255);
    table[NK_COLOR_SLIDER]           = nk_rgba(45,  45,  45,  255);
    table[NK_COLOR_SLIDER_CURSOR]    = nk_rgba(74,  144, 226, 255);
    table[NK_COLOR_SLIDER_CURSOR_HOVER] = nk_rgba(97, 175, 239, 255);
    table[NK_COLOR_SLIDER_CURSOR_ACTIVE] = nk_rgba(137, 182, 247, 255);
    table[NK_COLOR_PROPERTY]         = nk_rgba(40,  40,  40,  255);
    table[NK_COLOR_EDIT]             = nk_rgba(40,  40,  40,  255);
    table[NK_COLOR_EDIT_CURSOR]      = nk_rgba(204, 204, 204, 255);
    table[NK_COLOR_COMBO]            = nk_rgba(50,  50,  50,  255);
    table[NK_COLOR_CHART]            = nk_rgba(45,  45,  45,  255);
    table[NK_COLOR_CHART_COLOR]      = nk_rgba(74,  144, 226, 255);
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
    g_hook_ready = true;
    dw.set_event_hook(&nkui_event_forward);
    return true;
}

void NkUI::shutdown() {
    if (!ctx_) return;
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
    if (!ctx_ || !g_hook_ready) return;
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

static const char* mode_names[] = {"Raytrace", "Billboard", "Overhead", "Sculpt", "Static"};
static const char* brush_names[] = {
    "Draw", "Inflate", "Deflate", "Carve", "Implode", "Smooth",
    "Flatten", "Grab", "PushPull", "Pinch", "Crease", "Clay", "Blob"
};
static const int brush_count = (int)(sizeof(brush_names) / sizeof(brush_names[0]));
static const BrushType brush_types[] = {
    BrushType::Draw, BrushType::Inflate, BrushType::Deflate,
    BrushType::Carve, BrushType::Implode, BrushType::Smooth,
    BrushType::Flatten, BrushType::Grab, BrushType::PushPull,
    BrushType::Pinch, BrushType::Crease, BrushType::Clay, BrushType::Blob
};

// expand rect by margin (shadow spill tolerance)
static void expand(int& x, int& y, int& w, int& h, int ww, int wh, int m) {
    x -= m; y -= m; w += 2 * m; h += 2 * m;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > ww) w = ww - x;
    if (y + h > wh) h = wh - y;
}

void NkUI::draw_sidebar(UI& ui, int ww, int wh) {
    bool open = nk_begin(ctx_, "Sidebar", nk_rect((float)(ww - UI::SIDEBAR_W), 0, (float)UI::SIDEBAR_W, (float)wh),
                         NK_WINDOW_BORDER | NK_WINDOW_TITLE);
    if (!open) { nk_end(ctx_); return; }
    ctx_->current->layout->flags &= ~(nk_flags)NK_WINDOW_ROM;
    ctx_->current->flags &= ~(nk_flags)NK_WINDOW_ROM;

    // ---- Add ----
    nk_layout_row_dynamic(ctx_, 20, 1);
    if (nk_button_label(ctx_, sec_add_ ? "-  ADD" : "+  ADD")) {
        sec_add_ = !sec_add_;
    }
    if (sec_add_) {
        nk_layout_row_dynamic(ctx_, 24, 3);
        if (nk_button_label(ctx_, "Sphere"))  ui.place_shape(ShapeType::SPHERE);
        if (nk_button_label(ctx_, "Box"))     ui.place_shape(ShapeType::BOX);
        if (nk_button_label(ctx_, "Cyl"))     ui.place_shape(ShapeType::CYLINDER);
        if (nk_button_label(ctx_, "Cone"))    ui.place_shape(ShapeType::CONE);
        if (nk_button_label(ctx_, "Mesh"))    ui.place_shape(ShapeType::MESH);

        const auto& files = ui.mesh_files();
        if (!files.empty()) {
            if (ui.mesh_idx() >= (int)files.size()) ui.set_mesh_idx(0);
            std::string name = files[(size_t)ui.mesh_idx()];
            size_t slash = name.find_last_of('/');
            std::string shortname = (slash != std::string::npos) ? name.substr(slash + 1) : name;
            nk_layout_row_dynamic(ctx_, 20, 1);
            if (nk_combo_begin_label(ctx_, shortname.c_str(), nk_vec2((float)UI::SIDEBAR_W - 30, 160))) {
                nk_layout_row_dynamic(ctx_, 18, 1);
                for (int i = 0; i < (int)files.size(); i++)
                    if (nk_combo_item_label(ctx_, files[(size_t)i].c_str(), NK_TEXT_LEFT))
                        ui.set_mesh_idx(i);
                nk_combo_end(ctx_);
            }
        }
    }

    nk_spacing(ctx_, 1);

    // ---- Sculpt ----
    nk_layout_row_dynamic(ctx_, 20, 1);
    if (nk_button_label(ctx_, sec_sculpt_ ? "-  SCULPT" : "+  SCULPT")) sec_sculpt_ = !sec_sculpt_;

    if (sec_sculpt_) {
        nk_layout_row_dynamic(ctx_, 24, 1);
        if (nk_button_label(ctx_, "Add Sculpt Mesh")) ui.add_sculpt_mesh();

        nk_layout_row_dynamic(ctx_, 20, 1);
        float rad = ui.brush_radius();
        if (nk_slider_float(ctx_, 1.0f, &rad, 60.0f, 0.5f)) ui.set_brush_radius(rad);
        nk_layout_row_dynamic(ctx_, 16, 1);
        { char lbl[64]; snprintf(lbl, sizeof(lbl), "Radius: %.1f", (double)rad);
          nk_label(ctx_, lbl, NK_TEXT_LEFT); }

        nk_layout_row_dynamic(ctx_, 20, 1);
        float str = ui.brush_strength();
        if (nk_slider_float(ctx_, 0.0f, &str, 3.0f, 0.05f)) ui.set_brush_strength(str);
        nk_layout_row_dynamic(ctx_, 16, 1);
        { char lbl[64]; snprintf(lbl, sizeof(lbl), "Strength: %.2f", (double)str);
          nk_label(ctx_, lbl, NK_TEXT_LEFT); }

        int segs = ui.sculpt_segs(), rings = ui.sculpt_rings();
        nk_layout_row_dynamic(ctx_, 20, 1);
        nk_property_int(ctx_, "res segs", 8, &segs, 64, 4, 1);
        nk_layout_row_dynamic(ctx_, 20, 1);
        nk_property_int(ctx_, "res rings", 6, &rings, 48, 3, 1);
        if (segs != ui.sculpt_segs() || rings != ui.sculpt_rings())
            ui.set_sculpt_res(segs, rings);

        int bi = 0;
        for (int i = 0; i < brush_count; i++)
            if (ui.brush_type() == brush_types[i]) { bi = i; break; }
        nk_layout_row_dynamic(ctx_, 20, 1);
        if (nk_combo_begin_label(ctx_, brush_names[bi], nk_vec2((float)UI::SIDEBAR_W - 30, 180))) {
            nk_layout_row_dynamic(ctx_, 18, 1);
            for (int i = 0; i < brush_count; i++)
                if (nk_combo_item_label(ctx_, brush_names[i], NK_TEXT_LEFT))
                    ui.set_brush_type(brush_types[i]);
            nk_combo_end(ctx_);
        }

        nk_bool inv = ui.brush_invert() ? nk_true : nk_false;
        nk_layout_row_dynamic(ctx_, 18, 1);
        if (nk_checkbox_label(ctx_, "Invert (implode)", &inv))
            ui.set_brush_invert(inv != nk_false);

        nk_layout_row_dynamic(ctx_, 24, 1);
        if (nk_button_label(ctx_, "Remesh")) ui.do_remesh();
    }

    nk_spacing(ctx_, 1);

    // ---- Objects ----
    nk_layout_row_dynamic(ctx_, 20, 1);
    if (nk_button_label(ctx_, sec_objects_ ? "-  OBJECTS" : "+  OBJECTS")) sec_objects_ = !sec_objects_;
    if (sec_objects_) {
        int sel = ui.selected_object();
        const auto* objs = ui.object_list();
        if (!objs || objs->empty()) {
            nk_layout_row_dynamic(ctx_, 16, 1);
            nk_label(ctx_, "(no objects)", NK_TEXT_LEFT);
        } else {
            if (sel < 0 || sel >= (int)objs->size()) sel = 0;
            const char* cur = (*objs)[(size_t)sel].c_str();
            nk_layout_row_dynamic(ctx_, 20, 1);
            if (nk_combo_begin_label(ctx_, cur, nk_vec2((float)UI::SIDEBAR_W - 30, 160))) {
                nk_layout_row_dynamic(ctx_, 18, 1);
                for (int i = 0; i < (int)objs->size(); i++)
                    if (nk_combo_item_label(ctx_, (*objs)[(size_t)i].c_str(), NK_TEXT_LEFT))
                        sel = i;
                nk_combo_end(ctx_);
            }
            ui.select_object(sel);
            sel = ui.selected_object();

            nk_layout_row_dynamic(ctx_, 22, 3);
            if (nk_button_label(ctx_, "Up")) { ui.move_selected(Vec3(0, 0.5, 0)); }
            if (nk_button_label(ctx_, "Down")) { ui.move_selected(Vec3(0, -0.5, 0)); }
            if (nk_button_label(ctx_, "Reset")) { ui.reset_selected(); }

            nk_layout_row_dynamic(ctx_, 22, 3);
            if (nk_button_label(ctx_, "+X")) { ui.move_selected(Vec3(0.5, 0, 0)); }
            if (nk_button_label(ctx_, "-X")) { ui.move_selected(Vec3(-0.5, 0, 0)); }
            if (nk_button_label(ctx_, "+Z")) { ui.move_selected(Vec3(0, 0, 0.5)); }

            nk_layout_row_dynamic(ctx_, 22, 3);
            if (nk_button_label(ctx_, "-Z")) { ui.move_selected(Vec3(0, 0, -0.5)); }
            if (nk_button_label(ctx_, "+Y")) { ui.move_selected(Vec3(0, 0.5, 0)); }
            if (nk_button_label(ctx_, "-Y")) { ui.move_selected(Vec3(0, -0.5, 0)); }

            nk_layout_row_dynamic(ctx_, 18, 1);
            if (nk_button_label(ctx_, "Center Above")) { ui.center_selected_above(); }
        }
    }

    nk_spacing(ctx_, 1);

    // ---- Actions ----
    nk_layout_row_dynamic(ctx_, 20, 1);
    if (nk_button_label(ctx_, sec_actions_ ? "-  ACTIONS" : "+  ACTIONS")) sec_actions_ = !sec_actions_;
    if (sec_actions_) {
        nk_layout_row_dynamic(ctx_, 24, 2);
        if (nk_button_label(ctx_, "Clear")) ui.clear_scene();
        if (nk_button_label(ctx_, "Save")) {
            ui.open_save_dialog();
            save_buf_[0] = 0;
            save_len_ = 0;
        }
    }

    nk_spacing(ctx_, 1);

    // ---- Settings ----
    nk_layout_row_dynamic(ctx_, 20, 1);
    if (nk_button_label(ctx_, sec_settings_ ? "-  SETTINGS" : "+  SETTINGS")) sec_settings_ = !sec_settings_;
    if (sec_settings_) {
        nk_layout_row_dynamic(ctx_, 20, 1);
        if (nk_combo_begin_label(ctx_, mode_names[ui.mode_label_idx_], nk_vec2((float)UI::SIDEBAR_W - 30, 130))) {
            nk_layout_row_dynamic(ctx_, 18, 1);
            for (int i = 0; i < 5; i++)
                if (nk_combo_item_label(ctx_, mode_names[i], NK_TEXT_LEFT))
                    ui.set_render_mode(i);
            nk_combo_end(ctx_);
        }
        nk_layout_row_dynamic(ctx_, 24, 1);
        if (nk_button_label(ctx_, "Set Terrain")) ui.load_terrain();
    }

    nk_spacing(ctx_, 1);
    nk_rule_horizontal(ctx_, nk_rgba(45, 45, 45, 255), nk_false);

    // ---- footer ----
    if (ui.msg_fresh()) {
        nk_layout_row_dynamic(ctx_, 20, 1);
        nk_label_colored(ctx_, ui.msg(), NK_TEXT_LEFT, nk_rgba(74, 144, 226, 255));
    }

    nk_end(ctx_);
}

void NkUI::draw_anim_panel(Animator& anim,
                           const std::vector<std::string>& target_names,
                           const std::vector<Hittable*>& target_objs, int wh) {
    if (!visible_) return;
    bool open = nk_begin(ctx_, "Animation", nk_rect(0, 0, (float)PANEL_W, (float)wh),
                         NK_WINDOW_BORDER | NK_WINDOW_TITLE);
    if (!open) { nk_end(ctx_); return; }
    ctx_->current->layout->flags &= ~(nk_flags)NK_WINDOW_ROM;
    ctx_->current->flags &= ~(nk_flags)NK_WINDOW_ROM;

    // ---- playback ----
    nk_layout_row_dynamic(ctx_, 24, 4);
    if (nk_button_label(ctx_, "PLAY")) anim.play();
    if (nk_button_label(ctx_, "PAUSE")) anim.pause();
    if (nk_button_label(ctx_, "STOP")) anim.stop();
    {
        bool any_loop = false;
        for (auto& c : anim.clips) if (c.clip.loop) { any_loop = true; break; }
        if (nk_button_label(ctx_, any_loop ? "LOOP+" : "LOOP-"))
            for (auto& c : anim.clips) c.clip.loop = !any_loop;
    }

    nk_layout_row_dynamic(ctx_, 20, 1);
    nk_property_float(ctx_, "speed", 0.1f, &anim.global_speed, 5.0f, 0.1f, 0.05f);

    char tl[80];
    snprintf(tl, sizeof(tl), "%.2f / %.2f", anim.global_time, anim.max_time);
    nk_layout_row_begin(ctx_, NK_DYNAMIC, 22, 2);
    nk_layout_row_push(ctx_, 0.60f);
    if (anim.max_time > 0.01f) {
        float pct = anim.global_time / anim.max_time;
        float old = pct;
        nk_slider_float(ctx_, 0.0f, &pct, 1.0f, 0.002f);
        if (pct != old) anim.seek(pct * anim.max_time);
    } else {
        nk_label(ctx_, "(no keys)", NK_TEXT_CENTERED);
    }
    nk_layout_row_push(ctx_, 0.40f);
    nk_label(ctx_, tl, NK_TEXT_RIGHT);
    nk_layout_row_end(ctx_);

    nk_spacing(ctx_, 1);

    // helper: object bound to the selected clip's target
    auto tgt_obj = [&]() -> Hittable* {
        if (anim.clips.empty()) return nullptr;
        if (sel_clip_ < 0 || sel_clip_ >= (int)anim.clips.size()) return nullptr;
        const auto& t = anim.clips[(size_t)sel_clip_].target;
        if (t.type != AnimTarget::FREE_OBJECT) return nullptr;
        if (t.free_idx < 0 || t.free_idx >= (int)target_objs.size()) return nullptr;
        return target_objs[(size_t)t.free_idx];
    };
    // snapshot a target's base position into its clip
    auto snap_base = [&](int ci, Hittable* obj) {
        if (!obj || ci < 0 || ci >= (int)anim.clips.size()) return;
        anim.clips[(size_t)ci].base_pos =
            obj->get_center() - obj->anim_offset();
    };

    if (!anim.clips.empty()) {
        if (sel_clip_ >= (int)anim.clips.size()) sel_clip_ = (int)anim.clips.size() - 1;
        if (sel_clip_ < 0) sel_clip_ = 0;

        nk_layout_row_dynamic(ctx_, 24, 2);
        if (nk_button_label(ctx_, "NEW CLIP")) {
            AnimationClip nc;
            nc.name = "Clip " + std::to_string(anim.clips.size() + 1);
            AnimTarget t;
            if (!target_names.empty()) { t.type = AnimTarget::FREE_OBJECT; t.free_idx = 0; }
            anim.add_clip(t, nc);
            sel_clip_ = (int)anim.clips.size() - 1;
            snap_base(sel_clip_, tgt_obj());
        }
        if (nk_button_label(ctx_, "DEL CLIP")) {
            anim.remove_clip(sel_clip_);
            if (sel_clip_ >= (int)anim.clips.size())
                sel_clip_ = (int)anim.clips.size() - 1;
            anim.update_max_time();
        }

        nk_layout_row_dynamic(ctx_, 20, 1);
        if (nk_combo_begin_label(ctx_, anim.clips[(size_t)sel_clip_].clip.name.c_str(), nk_vec2((float)PANEL_W - 30, 160))) {
            nk_layout_row_dynamic(ctx_, 18, 1);
            for (int i = 0; i < (int)anim.clips.size(); i++)
                if (nk_combo_item_label(ctx_, anim.clips[(size_t)i].clip.name.c_str(), NK_TEXT_LEFT))
                    sel_clip_ = i;
            nk_combo_end(ctx_);
        }

        if (!target_names.empty()) {
            int cur = anim.clips[(size_t)sel_clip_].target.free_idx;
            const char* tcur = (cur >= 0 && cur < (int)target_names.size())
                                   ? target_names[(size_t)cur].c_str()
                                   : "^ pick target";
            nk_layout_row_dynamic(ctx_, 20, 1);
            if (nk_combo_begin_label(ctx_, tcur, nk_vec2((float)PANEL_W - 30, 160))) {
                nk_layout_row_dynamic(ctx_, 18, 1);
                for (int i = 0; i < (int)target_names.size(); i++)
                    if (nk_combo_item_label(ctx_, target_names[(size_t)i].c_str(), NK_TEXT_LEFT)) {
                        anim.clips[(size_t)sel_clip_].target.type = AnimTarget::FREE_OBJECT;
                        anim.clips[(size_t)sel_clip_].target.free_idx = i;
                        snap_base(sel_clip_, target_objs.empty() ? nullptr : target_objs[(size_t)i]);
                    }
                nk_combo_end(ctx_);
            }
        }
    } else {
        nk_layout_row_dynamic(ctx_, 24, 1);
        if (nk_button_label(ctx_, "+ NEW CLIP")) {
            AnimationClip nc;
            nc.name = "Clip 1";
            AnimTarget t;
            if (!target_names.empty()) { t.type = AnimTarget::FREE_OBJECT; t.free_idx = 0; }
            anim.add_clip(t, nc);
            snap_base(0, tgt_obj());
        }
    }

    if (!anim.clips.empty()) {
        nk_spacing(ctx_, 1);
        static const char* kprops[] = { "Position", "Rotation", "Scale", "Color" };
        static const char* kinterps[] = { "Linear", "EaseIn", "EaseOut", "EaseInOut", "Step" };
        nk_layout_row_begin(ctx_, NK_DYNAMIC, 20, 2);
        nk_layout_row_push(ctx_, 0.5f);
        if (nk_combo_begin_label(ctx_, kprops[sel_prop_], nk_vec2(140, 130))) {
            nk_layout_row_dynamic(ctx_, 18, 1);
            for (int i = 0; i < 4; i++)
                if (nk_combo_item_label(ctx_, kprops[i], NK_TEXT_LEFT)) sel_prop_ = i;
            nk_combo_end(ctx_);
        }
        nk_layout_row_push(ctx_, 0.5f);
        if (nk_combo_begin_label(ctx_, kinterps[sel_interp_], nk_vec2(140, 150))) {
            nk_layout_row_dynamic(ctx_, 18, 1);
            for (int i = 0; i < 5; i++)
                if (nk_combo_item_label(ctx_, kinterps[i], NK_TEXT_LEFT)) sel_interp_ = i;
            nk_combo_end(ctx_);
        }
        nk_layout_row_end(ctx_);

        nk_layout_row_begin(ctx_, NK_DYNAMIC, 22, 2);
        nk_layout_row_push(ctx_, 0.62f);
        nk_slider_float(ctx_, 0.0f, &key_time_, 10.0f, 0.05f);
        nk_layout_row_push(ctx_, 0.38f);
        nk_property_float(ctx_, "time", 0.0f, &key_time_, 120.0f, 0.1f, 0.05f);
        nk_layout_row_end(ctx_);
        nk_layout_row_dynamic(ctx_, 22, 3);
        if (nk_button_label(ctx_, "KEY @ CUR")) {
            key_time_ = anim.global_time;
            if (key_time_ < 0.0f) key_time_ = 0.0f;
        }
        if (nk_button_label(ctx_, "SYNC FROM OBJ")) {
            Hittable* o = tgt_obj();
            if (o) {
                Vec3 v = o->get_center() - anim.clips[(size_t)sel_clip_].base_pos;
                key_x_ = (float)v.x; key_y_ = (float)v.y; key_z_ = (float)v.z;
                sel_prop_ = 0; // Position
            }
        }
        if (nk_button_label(ctx_, "KEY @ OBJ")) {
            Hittable* o = tgt_obj();
            if (o) {
                Vec3 v = o->get_center() - anim.clips[(size_t)sel_clip_].base_pos;
                AnimationClip& c = anim.clips[(size_t)sel_clip_].clip;
                c.add_keyframe(AnimProperty::Position, key_time_, v,
                               (InterpolationType)sel_interp_);
                anim.update_max_time();
                key_x_ = (float)v.x; key_y_ = (float)v.y; key_z_ = (float)v.z;
                sel_prop_ = 0;
            }
        }

        nk_property_float(ctx_, "key x", -50.0f, &key_x_, 50.0f, 0.5f, 0.05f);
        nk_property_float(ctx_, "key y", -50.0f, &key_y_, 50.0f, 0.5f, 0.05f);
        nk_property_float(ctx_, "key z", -50.0f, &key_z_, 50.0f, 0.5f, 0.05f);
        {
            Hittable* o = tgt_obj();
            if (o) {
                Vec3 v = o->get_center() - anim.clips[(size_t)sel_clip_].base_pos;
                nk_layout_row_dynamic(ctx_, 16, 1);
                char ob[128];
                snprintf(ob, sizeof(ob), "obj offset now: %.2f %.2f %.2f",
                         (double)v.x, (double)v.y, (double)v.z);
                nk_label_colored(ctx_, ob, NK_TEXT_LEFT, nk_rgba(150, 150, 150, 255));
            }
        }

        nk_layout_row_dynamic(ctx_, 22, 2);
        if (nk_button_label(ctx_, "+ KEY")) {
            AnimationClip& c = anim.clips[(size_t)sel_clip_].clip;
            c.add_keyframe((AnimProperty)sel_prop_, key_time_,
                           Vec3(key_x_, key_y_, key_z_), (InterpolationType)sel_interp_);
            anim.update_max_time();
        }
        if (nk_button_label(ctx_, "CLEAR")) {
            anim.clips[(size_t)sel_clip_].clip.clear();
            anim.update_max_time();
        }

        auto& tr = anim.clips[(size_t)sel_clip_].clip.tracks;
        if (!tr.empty()) {
            nk_layout_row_dynamic(ctx_, 96, 1);
            if (nk_group_begin(ctx_, "tracks", NK_WINDOW_BORDER)) {
                for (int i = 0; i < (int)tr.size(); i++) {
                    char row[96];
                    snprintf(row, sizeof(row), "%s  (%d keys)",
                             kprops[(int)tr[(size_t)i].property],
                             (int)tr[(size_t)i].keyframes.size());
                    nk_layout_row_begin(ctx_, NK_DYNAMIC, 18, 2);
                    nk_layout_row_push(ctx_, 0.88f);
                    nk_label(ctx_, row, NK_TEXT_LEFT);
                    nk_layout_row_push(ctx_, 0.12f);
                    if (nk_button_label(ctx_, "x")) {
                        tr.erase(tr.begin() + i);
                        anim.update_max_time();
                        break;
                    }
                    nk_layout_row_end(ctx_);
                }
                nk_group_end(ctx_);
            }
        }

        nk_layout_row_dynamic(ctx_, 16, 1);
        nk_label_colored(ctx_, "rot/scale/color: preview only",
                         NK_TEXT_LEFT, nk_rgba(150, 150, 150, 255));
    }

    nk_end(ctx_);
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

void NkUI::draw(UI& ui, Animator& anim,
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
    draw_anim_panel(anim, target_names, target_objs, wh);
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
