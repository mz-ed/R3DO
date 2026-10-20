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

UI::UI(Grid& g, Camera& c, DisplayWin& dsp, SculptEngine* sculpt)
    : grid(g), cam(c), display(dsp), sculpt_(sculpt) {
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
            {BtnID::SCULPT_RES_INC, "+Res", "Resolution +"},
            {BtnID::SCULPT_RES_DEC, "-Res", "Resolution -"},
            {BtnID::SCULPT_BRUSH_NEXT, "Brush", "Next Brush Type"},
            {BtnID::SCULPT_REMESH, "Remesh", "Subdivide Mesh"},
            {BtnID::SCULPT_INVERT, "Invert", "Toggle Invert/Implode"},
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
    if (render_mode_ >= 0 && render_mode_ <= 4)
        mode_label_ = std::string("Mode: ") + mode_names[render_mode_];
    else
        mode_label_ = std::string("Mode: ") + mode_names[0];
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
        y += 20; // section header height + spacing
        if (!section_collapsed_[s]) {
            y += (int)sections_[s].buttons.size() * (BTN_H + BTN_GAP);
            if (!sections_[s].buttons.empty()) y -= BTN_GAP;
            y += 8; // section spacing
            if (s == 0) y += SML_H + BTN_GAP;
        } else {
            y += 8; // collapsed spacing
        }
    }
    return y;
}

int UI::button_y(int si, int bi) const {
    int y = section_y(si);
    y += 20; // header height + spacing (matches new draw)
    y += bi * (BTN_H + BTN_GAP);
    return y;
}

BtnID UI::button_at(int mx, int my) const {
    int sx = sidebar_x();
    if (mx < sx || mx >= sx + SIDEBAR_W) return BtnID::NONE;

    for (size_t si = 0; si < sections_.size(); si++) {
        if (section_collapsed_[si]) continue;
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

            if (my >= by - 3 && my < by + BTN_H + 3 && mx >= bx - 2 && mx < bx + BTN_W + 2) return sections_[si].buttons[bi].id;
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

    // Check section headers for collapse/expand
    int sx = sidebar_x();
    int y = 30;
    for (size_t si = 0; si < sections_.size(); si++) {
        int hy = y;
        int hh = 20;
        if (mx >= sx + 5 && mx < sx + SIDEBAR_W - 5 && my >= hy && my < hy + hh) {
            section_collapsed_[si] = !section_collapsed_[si];
            return true;
        }
        y += hh;
        if (!section_collapsed_[si]) {
            y += (int)sections_[si].buttons.size() * (BTN_H + BTN_GAP);
            if (!sections_[si].buttons.empty()) y -= BTN_GAP;
            y += 8;
            if (si == 0) y += SML_H + BTN_GAP;
        } else {
            y += 8;
        }
    }

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
        case BtnID::MODE:      render_mode_ = (render_mode_ + 1) % 5; update_labels(); return true;
        case BtnID::SCULPT_ADD: {
            SculptMesh* sm = SculptMesh::create_sphere(Vec3(0,0,0), 0.4, sculpt_segs_, sculpt_rings_, Vec3(0.8,0.7,0.5));
            if (sm) {
                grid.add_free(sm);
                if (sculpt_) sculpt_->set_active(sm);
                show_msg("Sculpt mesh added");
            }
            return true;
        }
        case BtnID::SCULPT_BRUSH_INC: {
            brush_radius_ += 0.05f;
            if (brush_radius_ > 2.0f) brush_radius_ = 2.0f;
            char buf[64]; snprintf(buf, sizeof(buf), "Radius: %.2f", brush_radius_);
            show_msg(buf);
            return true;
        }
        case BtnID::SCULPT_BRUSH_DEC: {
            brush_radius_ -= 0.05f;
            if (brush_radius_ < 0.05f) brush_radius_ = 0.05f;
            char buf[64]; snprintf(buf, sizeof(buf), "Radius: %.2f", brush_radius_);
            show_msg(buf);
            return true;
        }
        case BtnID::SCULPT_STR_INC: {
            brush_strength_ += 0.05f;
            if (brush_strength_ > 2.0f) brush_strength_ = 2.0f;
            char buf[64]; snprintf(buf, sizeof(buf), "Strength: %.2f", brush_strength_);
            show_msg(buf);
            return true;
        }
        case BtnID::SCULPT_STR_DEC: {
            brush_strength_ -= 0.05f;
            if (brush_strength_ < 0.05f) brush_strength_ = 0.05f;
            char buf[64]; snprintf(buf, sizeof(buf), "Strength: %.2f", brush_strength_);
            show_msg(buf);
            return true;
        }
        case BtnID::SCULPT_RES_INC: {
            sculpt_segs_ += 4; sculpt_rings_ += 3;
            if (sculpt_segs_ > 64) sculpt_segs_ = 64;
            if (sculpt_rings_ > 48) sculpt_rings_ = 48;
            char buf[64]; snprintf(buf, sizeof(buf), "Res: %dx%d", sculpt_segs_, sculpt_rings_);
            show_msg(buf);
            return true;
        }
        case BtnID::SCULPT_RES_DEC: {
            sculpt_segs_ -= 4; sculpt_rings_ -= 3;
            if (sculpt_segs_ < 8) sculpt_segs_ = 8;
            if (sculpt_rings_ < 6) sculpt_rings_ = 6;
            char buf[64]; snprintf(buf, sizeof(buf), "Res: %dx%d", sculpt_segs_, sculpt_rings_);
            show_msg(buf);
            return true;
        }
        case BtnID::SCULPT_BRUSH_NEXT: {
            static const BrushType types[] = {
                BrushType::Draw, BrushType::Inflate, BrushType::Deflate,
                BrushType::Carve, BrushType::Implode, BrushType::Smooth,
                BrushType::Flatten, BrushType::Grab, BrushType::PushPull,
                BrushType::Pinch, BrushType::Crease, BrushType::Clay, BrushType::Blob
            };
            static const char* names[] = {
                "Draw", "Inflate", "Deflate", "Carve", "Implode", "Smooth",
                "Flatten", "Grab", "PushPull", "Pinch", "Crease", "Clay", "Blob"
            };
            static const int n = sizeof(types) / sizeof(types[0]);
            static int cur = 0;
            cur = (cur + 1) % n;
            brush_type_ = types[cur];
            char buf[64]; snprintf(buf, sizeof(buf), "Brush: %s", names[cur]);
            show_msg(buf);
            return true;
        }
        case BtnID::SCULPT_REMESH: {
            if (sculpt_ && sculpt_->active()) {
                sculpt_->active()->remesh_subdivide();
                char buf[64];
                snprintf(buf, sizeof(buf), "Remeshed: %d verts", (int)sculpt_->active()->vertices().size());
                show_msg(buf);
            } else {
                show_msg("No active mesh");
            }
            return true;
        }
        case BtnID::SCULPT_INVERT: {
            brush_invert_ = !brush_invert_;
            show_msg(brush_invert_ ? "Invert: ON (implode)" : "Invert: OFF");
            return true;
        }
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

    display.fill_rect(sx, 0, SIDEBAR_W, display.height(), 0x1e1e1e);
    display.fill_rect(sx - 1, 0, 1, display.height(), 0x333333);

    display.draw_text(sx + 12, 16, "R3DO", 0xffffff);
    display.fill_rect(sx + 12, 22, 40, 1, 0x2d2d2d);

    hovered_ = button_at(mouse_x_, mouse_y_);

    int sy_cur = 30;
    for (size_t si = 0; si < sections_.size(); si++) {
        const bool collapsed = section_collapsed_[si];
        char header[128];
        snprintf(header, sizeof(header), "%s %s", collapsed ? "+" : "-", sections_[si].title.c_str());
        display.draw_text(sx + 12, sy_cur + 12, header, 0xffffff);
        display.fill_rect(sx + 12, sy_cur + 18, BTN_W, 1, 0x2d2d2d);
        sy_cur += 20;
        if (collapsed) { sy_cur += 8; continue; }
        for (size_t bi = 0; bi < sections_[si].buttons.size(); bi++) {
            const auto& btn = sections_[si].buttons[bi];
            bool hover = (hovered_ == btn.id);
            unsigned long bg = hover ? 0x3e3e3e : 0x323232;
            unsigned long bg2 = hover ? 0x4a4a4a : 0x2d2d2d;
            unsigned long fg = 0xcccccc;
            if (btn.id == BtnID::MODE) fg = 0x61afef;
            else if (btn.id == BtnID::GROUND) fg = ground_mode_ ? 0x4a90e2 : 0xcccccc;
            else if (btn.id == BtnID::TERRAIN) fg = grid.has_terrain() ? 0x4a90e2 : 0xcccccc;
            else if (hover) fg = 0xffffff;
            display.fill_rect(sx + 10, sy_cur, BTN_W, BTN_H, bg);
            display.fill_rect(sx + 11, sy_cur + 1, BTN_W - 2, BTN_H - 2, bg2);
            char label[128];
            if (!btn.icon.empty()) snprintf(label, sizeof(label), "%s %s", btn.icon.c_str(), btn.label.c_str());
            else snprintf(label, sizeof(label), "%s", btn.label.c_str());
            display.draw_text(sx + 18, sy_cur + 16, label, fg);
            sy_cur += BTN_H + BTN_GAP;
            if (btn.id == BtnID::MESH && !mesh_files_.empty()) {
                const std::string& name = mesh_files_[mesh_idx_];
                size_t slash = name.find_last_of('/');
                std::string shortname = (slash != std::string::npos) ? name.substr(slash + 1) : name;
                bool prev_hover = (hovered_ == BtnID::MESH_PREV);
                unsigned long pbg = prev_hover ? 0x3e3e3e : 0x323232;
                unsigned long pbg2 = prev_hover ? 0x4a4a4a : 0x2d2d2d;
                display.fill_rect(sx + 10, sy_cur, SML_BTN_W, SML_H, pbg);
                display.fill_rect(sx + 11, sy_cur + 1, SML_BTN_W - 2, SML_H - 2, pbg2);
                display.draw_text(sx + 16, sy_cur + 12, "<", 0xcccccc);
                char flabel[128];
                snprintf(flabel, sizeof(flabel), "[%zu/%zu] %s", (size_t)(mesh_idx_ + 1), mesh_files_.size(), shortname.substr(0, 60).c_str());
                display.draw_text(sx + 10 + SML_BTN_W + 4, sy_cur + 12, flabel, 0x61afef);
                bool next_hover = (hovered_ == BtnID::MESH_NEXT);
                pbg = next_hover ? 0x3e3e3e : 0x323232;
                pbg2 = next_hover ? 0x4a4a4a : 0x2d2d2d;
                display.fill_rect(sx + BTN_W + 10 - SML_BTN_W, sy_cur, SML_BTN_W, SML_H, pbg);
                display.fill_rect(sx + BTN_W + 11 - SML_BTN_W, sy_cur + 1, SML_BTN_W - 2, SML_H - 2, pbg2);
                display.draw_text(sx + BTN_W + 10 - SML_BTN_W + 6, sy_cur + 12, ">", 0xcccccc);
                sy_cur += SML_H + BTN_GAP;
            }
        }
        if (!sections_[si].buttons.empty()) sy_cur -= BTN_GAP;
        sy_cur += 12;
    }

    // Footer status
    {
        char tbuf[64];
        snprintf(tbuf, sizeof(tbuf), "Terrain: %s", grid.has_terrain() ? "loaded" : "none");
        display.draw_text(sx + 12, sy_cur + 10, tbuf, grid.has_terrain() ? 0x4a90e2 : 0x666666);
        sy_cur += 18;
    }
    char buf[64];
    snprintf(buf, sizeof(buf), "Objects: %d", grid.object_count());
    display.draw_text(sx + 12, sy_cur + 10, buf, 0xaaaaaa);
    sy_cur += 16;
    if (now_s() < save_msg_until_) {
        display.draw_text(sx + 12, sy_cur + 10, save_msg_, 0x4a90e2);
        sy_cur += 16;
    }
    sy_cur += 4;
    display.fill_rect(sx + 12, sy_cur, BTN_W, 1, 0x2d2d2d);
    sy_cur += 8;
    display.draw_text(sx + 12, sy_cur + 10, "Ctrl+Z undo | B mode", 0x666666);
    sy_cur += 16;
    display.draw_text(sx + 12, sy_cur + 10, "Esc menu | Alt+Shift grab", 0x666666);

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
