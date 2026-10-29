#include "nkui.hpp"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include "vendor/nuklear/nuklear.h"
#pragma GCC diagnostic pop
#include "ui.hpp"
#include "brush.hpp"
#include "hittable.hpp"
#include <cstdio>
#include <cstring>

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

            // Shared world position: drag, move buttons and the animation
            // panel all flow through the same live base. Editing here moves
            // the object (and the animation follows automatically). Cells
            // re-home into the nearest free cell like the drag gizmo.
            Vec3 wp;
            if (ui.selected_pos(wp)) {
                float px = (float)wp.x, py = (float)wp.y, pz = (float)wp.z;
                nk_layout_row_dynamic(ctx_, 20, 3);
                nk_property_float(ctx_, "pos x", -100.0f, &px, 100.0f, 0.25f, 0.05f);
                nk_property_float(ctx_, "pos y", -100.0f, &py, 100.0f, 0.25f, 0.05f);
                nk_property_float(ctx_, "pos z", -100.0f, &pz, 100.0f, 0.25f, 0.05f);
                if (px != (float)wp.x || py != (float)wp.y || pz != (float)wp.z)
                    ui.set_selected_pos(Vec3((double)px, (double)py, (double)pz));
                nk_layout_row_dynamic(ctx_, 16, 1);
                char posl[96];
                snprintf(posl, sizeof(posl), "pos %.2f %.2f %.2f",
                         (double)wp.x, (double)wp.y, (double)wp.z);
                nk_label_colored(ctx_, posl, NK_TEXT_LEFT, nk_rgba(97, 175, 239, 255));
            }

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
