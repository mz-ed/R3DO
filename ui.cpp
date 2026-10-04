#include "ui.hpp"
#include "saver.hpp"
#include "sphere.hpp"
#include "box.hpp"
#include "cylinder.hpp"
#include "cone.hpp"
#include "obj_loader.hpp"
#include "sculpt_mesh.hpp"
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <algorithm>
#include <chrono>

namespace fs = std::filesystem;

static double now_s() {
    return std::chrono::duration<double>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

UI::UI(Grid& g, Camera& c, DisplayWin& dsp)
    : grid(g), cam(c), display(dsp) {
    scan_meshes();
    build_sections();
}

void UI::build_sections() {
    sections_ = {
        {"Add", {
            {BtnID::SPHERE,    "O", "Sphere"},
            {BtnID::BOX,       "#", "Box"},
            {BtnID::CYLINDER,  "|", "Cylinder"},
            {BtnID::CONE,      "/\\", "Cone"},
            {BtnID::MESH,      "M", "Mesh"},
        }},
        {"Sculpt", {
            {BtnID::SCULPT_ADD, "Add Sphere", "Add Sculpt Mesh"},
            {BtnID::SCULPT_BRUSH_INC, "+R", "Brush Radius +"},
            {BtnID::SCULPT_BRUSH_DEC, "-R", "Brush Radius -"},
            {BtnID::SCULPT_STR_INC, "+S", "Strength +"},
            {BtnID::SCULPT_STR_DEC, "-S", "Strength -"},
        }},
        {"Actions", {
            {BtnID::CLEAR,     "X", "Clear All"},
            {BtnID::SAVE,      "S", "Save"},
        }},
        {"Settings", {
            {BtnID::MODE,      "", mode_label_},
            {BtnID::GROUND,    "", ground_label_},
            {BtnID::TERRAIN,   "", "Set Terrain"},
        }},
    };
}

void UI::update_labels() {
    static const char* mode_names[] = {"Raytrace", "Billboard", "Overhead", "Sculpt", "Static"};
    if (render_mode_ >= 0 && render_mode_ <= 3)
        mode_label_ = std::string("Mode: ") + mode_names[render_mode_];
    else
        mode_label_ = std::string("Mode: ") + mode_names[1];
    ground_label_ = ground_mode_ ? "Ground: ON" : "Ground: OFF";
    // Sections hold copies of the label strings — keep them in sync.
    for (auto& sec : sections_) {
        for (auto& btn : sec.buttons) {
            if (btn.id == BtnID::MODE) btn.label = mode_label_;
            else if (btn.id == BtnID::GROUND) btn.label = ground_label_;
        }
    }
}

int UI::section_y(int si) const {
    int y = 30; // start below title
    for (int s = 0; s < si; s++) {
        y += 16; // section header
        y += (int)sections_[s].buttons.size() * (BTN_H + BTN_GAP);
        if (!sections_[s].buttons.empty()) y -= BTN_GAP;
        y += 8; // section spacing
        // mesh selector space after Add section
        if (s == 0) y += SML_H + BTN_GAP;
    }
    return y;
}

int UI::button_y(int si, int bi) const {
    int y = section_y(si);
    y += 16; // header
    y += bi * (BTN_H + BTN_GAP);
    return y;
}

BtnID UI::button_at(int mx, int my) const {
    int sx = sidebar_x();
    if (mx < sx || mx >= sx + SIDEBAR_W) return BtnID::NONE;

    for (size_t si = 0; si < sections_.size(); si++) {
        for (size_t bi = 0; bi < sections_[si].buttons.size(); bi++) {
            int bx = sx + 10;
            int by = button_y((int)si, (int)bi);
            const int BTN_W = UI::SIDEBAR_W - 20;
            const int SML_BTN_W = 20;

            // Mesh section: also check mesh prev/next smaller buttons
            if (sections_[si].buttons[bi].id == BtnID::MESH && !mesh_files_.empty()) {
                int my2 = by + BTN_H + BTN_GAP;
                if (my >= my2 && my < my2 + SML_H) {
                    if (mx >= bx && mx < bx + SML_BTN_W) return BtnID::MESH_PREV;
                    if (mx >= bx + BTN_W - SML_BTN_W && mx < bx + BTN_W) return BtnID::MESH_NEXT;
                }
            }

            if (my >= by && my < by + BTN_H) return sections_[si].buttons[bi].id;
        }
    }
    return BtnID::NONE;
}

void UI::scan_meshes() {
    mesh_files_.clear();
    std::error_code ec;
    if (!fs::exists("models")) {
        fs::create_directory("models", ec);
        return;
    }
    for (const auto& entry : fs::directory_iterator("models")) {
        if (!entry.is_regular_file()) continue;
        auto p = entry.path();
        if (p.extension() == ".obj")
            mesh_files_.push_back(p.string());
    }
    std::sort(mesh_files_.begin(), mesh_files_.end());
    if (mesh_idx_ >= (int)mesh_files_.size()) mesh_idx_ = 0;
}

void UI::cycle_mesh(int dir) {
    if (mesh_files_.empty()) return;
    mesh_idx_ = (mesh_idx_ + dir + (int)mesh_files_.size()) % (int)mesh_files_.size();
}

Vec3 UI::palette_color(int index) const {
    static const Vec3 palette[] = {
        Vec3(1, 0.2, 0.2), Vec3(0.2, 1, 0.3), Vec3(1, 1, 0.2),
        Vec3(1, 0.5, 0), Vec3(0.2, 0.4, 1), Vec3(0.9, 0.2, 0.9),
        Vec3(0.2, 0.9, 0.9), Vec3(0.8, 0.6, 0.2), Vec3(0.5, 0.3, 1),
        Vec3(1, 0.6, 0.6)
    };
    return palette[index % 10];
}

void UI::push_undo() {
    undo_stack_.push_back(serialize_scene(grid, cam, render_mode_, palette_idx_));
    if (undo_stack_.size() > 50)
        undo_stack_.erase(undo_stack_.begin());
}

bool UI::undo() {
    if (undo_stack_.empty()) return false;
    std::string snapshot = std::move(undo_stack_.back());
    undo_stack_.pop_back();
    deserialize_scene(snapshot, grid, &cam, &render_mode_, &palette_idx_);
    update_labels();
    return true;
}

bool UI::place_last_shape() {
    return try_place(last_shape_);
}

void UI::show_msg(const char* msg) {
    snprintf(save_msg_, sizeof(save_msg_), "%s", msg);
    save_msg_until_ = now_s() + 3.0;
}

bool UI::try_place(ShapeType type) {
    Vec3 target = cam.pos + cam.forward() * 3.0;
    int i, j, k;

    if (ground_mode_) {
        Vec3 ro = cam.pos;
        Vec3 rd = cam.forward();
        if (grid.has_terrain()) {
            Ray fwd(ro, rd);
            HitRecord trec;
            if (grid.terrain()->hit(fwd, 0.001, 100.0, trec))
                target = trec.p;
            else
                target = ro + rd * 5.0;
        } else {
            if (rd.y > -1e-10) return false;
            Vec3 gmin, gmax;
            grid.grid_bounds(gmin, gmax);
            double gy = gmin.y;
            double t = (gy - ro.y) / rd.y;
            if (t < 0) return false;
            target = ro + rd * t;
        }
        if (!grid.world_to_cell(target, i, j, k)) return false;
        j = 0;
    } else if (!grid.world_to_cell(target, i, j, k)) {
        return false;
    }

    if (grid.get(i, j, k)) return false;

    push_undo();

    double cs = grid.cell_size;
    Vec3 c = grid.cell_center(i, j, k);
    Vec3 col = palette_color(palette_idx_);

    switch (type) {
        case ShapeType::SPHERE:
            grid.set(i, j, k, new Sphere(c, cs * 0.45, col));
            break;
        case ShapeType::BOX: {
            Vec3 half(cs * 0.38, cs * 0.38, cs * 0.38);
            grid.set(i, j, k, new Box(c - half, c + half, col));
            break;
        }
        case ShapeType::CYLINDER:
            grid.set(i, j, k, new Cylinder(c, cs * 0.38, cs * 0.8, col));
            break;
        case ShapeType::CONE:
            grid.set(i, j, k, new Cone(c, cs * 0.38, cs * 0.8, col));
            break;
        case ShapeType::MESH: {
            if (mesh_files_.empty()) return false;
            const std::string& path = mesh_files_[mesh_idx_];
            Mesh* m = load_obj(path.c_str(), col, c, cs * 0.4);
            if (!m) return false;
            grid.add_free(m);
            break;
        }
    }

    last_shape_ = type;
    palette_idx_++;
    return true;
}

void UI::toggle_ground_mode() {
    ground_mode_ = !ground_mode_;
    update_labels();
}

bool UI::handle_click(int mx, int my) {
    if (save_dialog_active_) {
        int dw = 360, dh = 180;
        int dx = (display.width() - dw) / 2;
        int dy = (display.height() - dh) / 2;
        if (mx >= dx + 20 && mx < dx + 170 && my >= dy + 120 && my < dy + 150) {
            confirm_save();
        } else if (mx >= dx + 190 && mx < dx + 340 && my >= dy + 120 && my < dy + 150) {
            cancel_save();
        } else {
            cancel_save();
        }
        return true;
    }

    if (mx < sidebar_x() || mx >= sidebar_x() + SIDEBAR_W)
        return false;

    BtnID id = button_at(mx, my);

    switch (id) {
        case BtnID::SPHERE:    return try_place(ShapeType::SPHERE);
        case BtnID::BOX:       return try_place(ShapeType::BOX);
        case BtnID::CYLINDER:  return try_place(ShapeType::CYLINDER);
        case BtnID::CONE:      return try_place(ShapeType::CONE);
        case BtnID::MESH:      return try_place(ShapeType::MESH);
        case BtnID::MESH_PREV: cycle_mesh(-1); return true;
        case BtnID::MESH_NEXT: cycle_mesh(1); return true;
        case BtnID::CLEAR:
            push_undo();
            grid.clear();
            show_msg("Scene cleared");
            return true;
        case BtnID::SAVE:      open_save_dialog(); return true;
        case BtnID::MODE:      render_mode_ = (render_mode_ + 1) % 4; update_labels(); return true;
        case BtnID::SCULPT_ADD: {
            // Add sphere sculpt mesh at origin of last pick? add at camera forward offset
            SculptMesh* sm = SculptMesh::create_sphere(Vec3(0,0,0), 0.3, 16, 12, Vec3(0.8,0.7,0.5));
            if (sm) { grid.add_free(sm); show_msg("Sculpt mesh added"); }
            return true;
        }
        case BtnID::SCULPT_BRUSH_INC: show_msg("Brush +"); return true;
        case BtnID::SCULPT_BRUSH_DEC: show_msg("Brush -"); return true;
        case BtnID::SCULPT_STR_INC:   show_msg("Strength +"); return true;
        case BtnID::SCULPT_STR_DEC:   show_msg("Strength -"); return true;
        case BtnID::GROUND:    toggle_ground_mode(); return true;
        case BtnID::TERRAIN: {
            if (mesh_files_.empty()) return true;
            const std::string& path = mesh_files_[mesh_idx_];
            Mesh* m = load_obj(path.c_str(), Vec3(0.3, 0.5, 0.25), Vec3(0, 0, 0), 1.0);
            if (m) {
                grid.set_terrain(m, path);
                show_msg("Terrain loaded");
            }
            return true;
        }
        default:
            return false;
    }
}

void UI::open_save_dialog() {
    save_dialog_active_ = true;
    save_name_[0] = 0;
    save_name_len_ = 0;
    cursor_counter_ = 0;
}

void UI::cancel_save() {
    save_dialog_active_ = false;
}

// Strip anything that could escape saves/ or break the filesystem.
static void sanitize_name(char* name) {
    char* out = name;
    for (char* p = name; *p; p++) {
        unsigned char c = (unsigned char)*p;
        bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                  (c >= '0' && c <= '9') || c == '-' || c == '_' || c == ' ';
        if (ok) *out++ = (char)c;
    }
    *out = 0;
    // Trim leading spaces / dots-only names
    while (out > name && out[-1] == ' ') *--out = 0;
    if (name[0] == '.') name[0] = 0;
}

void UI::confirm_save() {
    sanitize_name(save_name_);
    save_name_len_ = (int)strlen(save_name_);
    if (save_name_len_ > 0) {
        std::string path = "saves/" + std::string(save_name_) + ".r3do";
        save_scene(grid, cam, path, render_mode_, palette_idx_);
        show_msg(("Saved: " + std::string(save_name_)).c_str());
    }
    save_dialog_active_ = false;
}

void UI::handle_save_dialog_key(int keysym, char c) {
    if (!save_dialog_active_) return;
    if (keysym == XK_Escape) {
        cancel_save();
    } else if (keysym == XK_Return || keysym == XK_KP_Enter) {
        confirm_save();
    } else if (keysym == XK_BackSpace) {
        if (save_name_len_ > 0) {
            save_name_[--save_name_len_] = 0;
        }
    } else if ((unsigned char)c >= 32 && (unsigned char)c <= 126 && save_name_len_ < 255) {
        save_name_[save_name_len_++] = c;
        save_name_[save_name_len_] = 0;
    }
}

void UI::draw() {
    const int BTN_W = UI::SIDEBAR_W - 20;
    const int SML_BTN_W = 20;
    int sx = sidebar_x();
    mouse_x_ = display.mouse_x();
    mouse_y_ = display.mouse_y();

    // Sidebar background
    display.fill_rect(sx, 0, SIDEBAR_W, display.height(), 0x1a1a2e);
    display.fill_rect(sx - 2, 0, 2, display.height(), 0x444466);

    // Title
    display.draw_text(sx + 10, 14, "R3DO", 0xffffff);
    display.fill_rect(sx + 10, 20, 40, 1, 0x666688);

    // Determine hover
    hovered_ = button_at(mouse_x_, mouse_y_);

    // Draw sections
    for (size_t si = 0; si < sections_.size(); si++) {
        int sy = section_y((int)si);

        // Section header
        display.draw_text(sx + 10, sy, sections_[si].title.c_str(), 0x888888);
        display.fill_rect(sx + 10, sy + 14, BTN_W, 1, 0x333355);

        // Draw buttons
        for (size_t bi = 0; bi < sections_[si].buttons.size(); bi++) {
            const auto& btn = sections_[si].buttons[bi];
            int by = button_y((int)si, (int)bi);
            int bx = sx + 10;
            bool hover = (hovered_ == btn.id);

            unsigned long bg = hover ? 0x44446e : 0x333355;
            unsigned long bg2 = hover ? 0x505080 : 0x3d3d6b;

            // Special styling for mode/ground/terrain
            unsigned long fg = 0xccccff;
            if (btn.id == BtnID::MODE) fg = 0x55aaff;
            else if (btn.id == BtnID::GROUND) fg = ground_mode_ ? 0x55ff55 : 0xff5555;
            else if (btn.id == BtnID::TERRAIN) fg = grid.has_terrain() ? 0x55ff55 : 0xccccff;

            // Button background
            display.fill_rect(bx, by, BTN_W, BTN_H, bg);
            display.fill_rect(bx + 1, by + 1, BTN_W - 2, BTN_H - 2, bg2);

            // Icon + label
            char label[96];
            if (!btn.icon.empty()) {
                snprintf(label, sizeof(label), "%s %s", btn.icon.c_str(), btn.label.c_str());
            } else {
                snprintf(label, sizeof(label), "%s", btn.label.c_str());
            }
            display.draw_text(bx + 8, by + 18, label, fg);

            // Mesh selector below MESH button
            if (btn.id == BtnID::MESH && !mesh_files_.empty()) {
                int my2 = by + BTN_H + BTN_GAP;
                const std::string& name = mesh_files_[mesh_idx_];
                size_t slash = name.find_last_of('/');
                std::string shortname = (slash != std::string::npos)
                    ? name.substr(slash + 1) : name;

                // "<" button
                bool prev_hover = (hovered_ == BtnID::MESH_PREV);
                unsigned long pbg = prev_hover ? 0x44446e : 0x333355;
                unsigned long pbg2 = prev_hover ? 0x505080 : 0x3d3d6b;
                display.fill_rect(bx, my2, SML_BTN_W, SML_H, pbg);
                display.fill_rect(bx + 1, my2 + 1, SML_BTN_W - 2, SML_H - 2, pbg2);
                display.draw_text(bx + 6, my2 + 13, "<", 0xccccff);

                // filename
                char flabel[128];
                snprintf(flabel, sizeof(flabel), "[%zu/%zu] %s",
                         (size_t)(mesh_idx_ + 1), mesh_files_.size(),
                         shortname.substr(0, 90).c_str());
                display.draw_text(bx + SML_BTN_W + 4, my2 + 13, flabel, 0x88aaff);

                // ">" button
                bool next_hover = (hovered_ == BtnID::MESH_NEXT);
                unsigned long nbg = next_hover ? 0x44446e : 0x333355;
                unsigned long nbg2 = next_hover ? 0x505080 : 0x3d3d6b;
                int rx = bx + BTN_W - SML_BTN_W;
                display.fill_rect(rx, my2, SML_BTN_W, SML_H, nbg);
                display.fill_rect(rx + 1, my2 + 1, SML_BTN_W - 2, SML_H - 2, nbg2);
                display.draw_text(rx + 6, my2 + 13, ">", 0xccccff);
            }
        }
    }

    // Terrain status (below terrain btn)
    int terrain_by = button_y(2, 2);
    int status_y = terrain_by + BTN_H + BTN_GAP + 4;
    {
        char tbuf[64];
        snprintf(tbuf, sizeof(tbuf), "Terrain: %s", grid.has_terrain() ? "loaded" : "none");
        display.draw_text(sx + 10, status_y, tbuf, grid.has_terrain() ? 0x55aa55 : 0x555555);
    }

    // Info section (below settings)
    int info_y = status_y + 18;
    display.fill_rect(sx + 10, info_y, BTN_W, 1, 0x333355);
    info_y += 8;

    char buf[64];
    snprintf(buf, sizeof(buf), "Objects: %d", grid.object_count());
    display.draw_text(sx + 10, info_y, buf, 0xaaaaaa);
    info_y += 18;

    snprintf(buf, sizeof(buf), "Cam: %.1f %.1f %.1f", cam.pos.x, cam.pos.y, cam.pos.z);
    display.draw_text(sx + 10, info_y, buf, 0x666688);
    info_y += 18;

    if (now_s() < save_msg_until_) {
        display.draw_text(sx + 10, info_y, save_msg_, 0x55ff55);
        info_y += 18;
    }

    info_y += 4;
    display.fill_rect(sx + 10, info_y, BTN_W, 1, 0x333355);
    info_y += 8;

    display.draw_text(sx + 10, info_y, "WASD move", 0x666688); info_y += 16;
    display.draw_text(sx + 10, info_y, "Arrows look", 0x666688); info_y += 16;
    display.draw_text(sx + 10, info_y, "Q/E up/down", 0x666688); info_y += 16;
    display.draw_text(sx + 10, info_y, "Space HQ/jump", 0x666688); info_y += 16;
    display.draw_text(sx + 10, info_y, "G ground  B mode", 0x666688); info_y += 16;
    display.draw_text(sx + 10, info_y, "Ctrl+Z undo", 0x666688); info_y += 16;
    display.draw_text(sx + 10, info_y, "RClick place  Esc menu", 0x666688);

    if (save_dialog_active_) {
        draw_save_dialog();
        cursor_counter_++;
    }
}

void UI::draw_save_dialog() {
    int w = display.width(), h = display.height();
    int dw = 360, dh = 180;
    int dx = (w - dw) / 2, dy = (h - dh) / 2;

    display.fill_rect(0, 0, w, h, 0x111122);
    display.fill_rect(dx, dy, dw, dh, 0x222244);
    display.fill_rect(dx + 1, dy + 1, dw - 2, dh - 2, 0x2a2a50);
    display.draw_text(dx + 20, dy + 28, "Save Scene", 0xffffff);
    display.fill_rect(dx + 20, dy + 34, 60, 1, 0x666688);

    display.fill_rect(dx + 20, dy + 55, dw - 40, 36, 0x111122);
    display.fill_rect(dx + 21, dy + 56, dw - 42, 34, 0x1a1a33);

    char display_name[258];
    snprintf(display_name, sizeof(display_name), "%s%s",
             save_name_, (cursor_counter_ / 20) % 2 ? " " : "_");
    display.draw_text(dx + 28, dy + 78, display_name, 0xccccff);

    display.fill_rect(dx + 20, dy + 120, 150, 28, 0x336633);
    display.fill_rect(dx + 21, dy + 121, 148, 26, 0x3d7a3d);
    display.draw_text(dx + 65, dy + 139, "Save", 0xffffff);

    display.fill_rect(dx + 190, dy + 120, 150, 28, 0x663333);
    display.fill_rect(dx + 191, dy + 121, 148, 26, 0x7a3d3d);
    display.draw_text(dx + 228, dy + 139, "Cancel", 0xffffff);
}
