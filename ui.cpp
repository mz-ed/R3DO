#include "ui.hpp"
#include "saver.hpp"
#include "sphere.hpp"
#include "box.hpp"
#include "cylinder.hpp"
#include "cone.hpp"
#include "obj_loader.hpp"
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>
#include <algorithm>

UI::UI(Grid& grid, Camera& cam, DisplayWin& display)
    : grid(grid), cam(cam), display(display), palette_idx_(0) {
    save_msg_[0] = 0;
    save_msg_timer_ = 0;
    save_dialog_active_ = false;
    save_name_[0] = 0;
    save_name_len_ = 0;
    cursor_counter_ = 0;
    scan_meshes();
}

void UI::scan_meshes() {
    mesh_files_.clear();
    DIR* dir = opendir("models");
    if (!dir) {
        mkdir("models", 0755);
        return;
    }
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        const char* name = entry->d_name;
        int len = (int)strlen(name);
        if (len > 4 && strcmp(name + len - 4, ".obj") == 0) {
            mesh_files_.push_back(std::string("models/") + name);
        }
    }
    closedir(dir);
    std::sort(mesh_files_.begin(), mesh_files_.end());
    if (mesh_idx_ >= (int)mesh_files_.size()) mesh_idx_ = 0;
}

void UI::cycle_mesh(int dir) {
    if (mesh_files_.empty()) return;
    mesh_idx_ = (mesh_idx_ + dir + (int)mesh_files_.size()) % (int)mesh_files_.size();
}

Vec3 UI::palette_color(int index) const {
    Vec3 palette[] = {
        Vec3(1, 0.2, 0.2), Vec3(0.2, 1, 0.3), Vec3(1, 1, 0.2),
        Vec3(1, 0.5, 0), Vec3(0.2, 0.4, 1), Vec3(0.9, 0.2, 0.9),
        Vec3(0.2, 0.9, 0.9), Vec3(0.8, 0.6, 0.2), Vec3(0.5, 0.3, 1),
        Vec3(1, 0.6, 0.6)
    };
    return palette[index % 10];
}

bool UI::try_place(ShapeType type) {
    Vec3 target = cam.pos + cam.forward() * 3.0;
    int i, j, k;

    if (ground_mode_) {
        Vec3 ro = cam.pos;
        Vec3 rd = cam.forward();
        if (grid.has_terrain()) {
            // Raycast forward against terrain mesh
            Ray fwd(ro, rd);
            HitRecord trec;
            if (grid.terrain()->hit(fwd, 0.001, 100.0, trec))
                target = trec.p;
            else
                target = ro + rd * 5.0; // fallback distance
        } else {
            if (rd.y > -1e-10) return false;
            Vec3 gmin, gmax;
            grid.grid_bounds(gmin, gmax);
            double gy = gmin.y;
            double t = (gy - ro.y) / rd.y;
            if (t < 0) return false;
            target = ro + rd * t;
        }
        if (!grid.world_to_cell(target, i, j, k)) {
            std::cerr << "FAIL: ground target (" << target.x << "," << target.y << "," << target.z << ") outside grid" << std::endl;
            return false;
        }
        j = 0;
    } else if (!grid.world_to_cell(target, i, j, k)) {
        std::cerr << "FAIL: target (" << target.x << "," << target.y << "," << target.z << ") outside grid" << std::endl;
        return false;
    }

    if (grid.get(i, j, k)) {
        std::cerr << "FAIL: cell (" << i << "," << j << "," << k << ") occupied" << std::endl;
        return false;
    }

    double cs = grid.cell_size;
    Vec3 c = grid.cell_center(i, j, k);
    Vec3 col = palette_color(palette_idx_);

    switch (type) {
        case ShapeType::SPHERE:
            grid.set(i, j, k, new Sphere(c, cs * 0.45, col));
            std::cerr << "Sphere at cell (" << i << "," << j << "," << k << ")" << std::endl;
            break;
        case ShapeType::BOX: {
            Vec3 half(cs * 0.38, cs * 0.38, cs * 0.38);
            grid.set(i, j, k, new Box(c - half, c + half, col));
            std::cerr << "Box at cell (" << i << "," << j << "," << k << ")" << std::endl;
            break;
        }
        case ShapeType::CYLINDER:
            grid.set(i, j, k, new Cylinder(c, cs * 0.38, cs * 0.8, col));
            std::cerr << "Cylinder at cell (" << i << "," << j << "," << k << ")" << std::endl;
            break;
        case ShapeType::CONE:
            grid.set(i, j, k, new Cone(c, cs * 0.38, cs * 0.8, col));
            std::cerr << "Cone at cell (" << i << "," << j << "," << k << ")" << std::endl;
            break;
        case ShapeType::MESH: {
            if (mesh_files_.empty()) {
                std::cerr << "FAIL: no .obj files in models/" << std::endl;
                return false;
            }
            const std::string& path = mesh_files_[mesh_idx_];
            Mesh* m = load_obj(path.c_str(), col, c, cs * 0.4);
            if (m) {
                grid.add_free(m);
                std::cerr << "Mesh from " << path << " (free object)" << std::endl;
            } else {
                std::cerr << "FAIL: could not load " << path << std::endl;
                return false;
            }
            break;
        }
    }
    palette_idx_++;
    return true;
}

void UI::clear_grid() {
    for (int i = 0; i < grid.nx; i++)
        for (int j = 0; j < grid.ny; j++)
            for (int k = 0; k < grid.nz; k++)
                grid.set(i, j, k, nullptr);
    while (!grid.free_objects().empty()) {
        Hittable* f = grid.free_objects()[0];
        grid.remove_free(f);
        delete f;
    }
    grid.set_terrain(nullptr, "");
}

int UI::count_objects() const {
    int count = 0;
    for (int i = 0; i < grid.nx; i++)
        for (int j = 0; j < grid.ny; j++)
            for (int k = 0; k < grid.nz; k++)
                if (grid.get(i, j, k)) count++;
    count += (int)grid.free_objects().size();
    return count;
}

void UI::draw() {
    display.fill_rect((display.width() - MENU_W), 0, MENU_W, display.height(), 0x1a1a2e);
    display.fill_rect((display.width() - MENU_W) - 2, 0, 2, display.height(), 0x444466);

    display.draw_text((display.width() - MENU_W) + 10, 22, "R3DO", 0xffffff);
    display.fill_rect((display.width() - MENU_W) + 10, 28, 40, 1, 0x666688);

    Button buttons[] = {
        {(display.width() - MENU_W) + 10, 45, 150, 28, "Add Sphere"},
        {(display.width() - MENU_W) + 10, 80, 150, 28, "Add Box"},
        {(display.width() - MENU_W) + 10, 115, 150, 28, "Add Cylinder"},
        {(display.width() - MENU_W) + 10, 150, 150, 28, "Add Cone"},
        {(display.width() - MENU_W) + 10, 185, 150, 28, "Add Mesh"},
        {(display.width() - MENU_W) + 10, 220, 150, 28, "Clear All"},
        {(display.width() - MENU_W) + 10, 255, 150, 28, "Save"},
    };

    for (auto& btn : buttons) {
        display.fill_rect(btn.x, btn.y, btn.w, btn.h, 0x333355);
        display.fill_rect(btn.x + 1, btn.y + 1, btn.w - 2, btn.h - 2, 0x3d3d6b);
        display.draw_text(btn.x + 10, btn.y + 19, btn.text, 0xccccff);
    }

    // Mesh file selector
    if (!mesh_files_.empty()) {
        int mx = (display.width() - MENU_W) + 10;
        int my = 217;
        const std::string& name = mesh_files_[mesh_idx_];
        const char* shortname = name.c_str();
        const char* slash = strrchr(shortname, '/');
        if (slash) shortname = slash + 1;
        // "<" button
        display.fill_rect(mx, my, 20, 18, 0x333355);
        display.fill_rect(mx + 1, my + 1, 18, 16, 0x3d3d6b);
        display.draw_text(mx + 6, my + 14, "<", 0xccccff);
        // filename
        char label[128];
        snprintf(label, sizeof(label), " [%zu/%zu] %s", mesh_idx_ + 1, mesh_files_.size(), shortname);
        display.draw_text(mx + 24, my + 14, label, 0x88aaff);
        // ">" button
        int rx = mx + 150 - 20;
        display.fill_rect(rx, my, 20, 18, 0x333355);
        display.fill_rect(rx + 1, my + 1, 18, 16, 0x3d3d6b);
        display.draw_text(rx + 6, my + 14, ">", 0xccccff);
    }

    char buf[64];
    snprintf(buf, sizeof(buf), "Objects: %d", count_objects());
    display.draw_text((display.width() - MENU_W) + 10, 265, buf, 0xaaaaaa);

    snprintf(buf, sizeof(buf), "Cam: %.1f %.1f %.1f", cam.pos.x, cam.pos.y, cam.pos.z);
    display.draw_text((display.width() - MENU_W) + 10, 285, buf, 0x666688);

    if (save_msg_timer_ > 0) {
        save_msg_timer_--;
        display.draw_text((display.width() - MENU_W) + 10, 310, save_msg_, 0x55ff55);
    }

    display.draw_text((display.width() - MENU_W) + 10, 335, "Controls:", 0x888888);
    display.draw_text((display.width() - MENU_W) + 10, 352, "WASD move", 0x666688);
    display.draw_text((display.width() - MENU_W) + 10, 367, "Arrows look", 0x666688);
    display.draw_text((display.width() - MENU_W) + 10, 382, "Q/E up/down", 0x666688);
    display.draw_text((display.width() - MENU_W) + 10, 397, "Space: HQ jump", 0x666688);
    display.draw_text((display.width() - MENU_W) + 10, 412, "G: ground", 0x666688);
    display.draw_text((display.width() - MENU_W) + 10, 427, "Esc: quit", 0x666688);
    display.fill_rect((display.width() - MENU_W) + 10, 432, 150, 28, 0x333355);
    display.fill_rect((display.width() - MENU_W) + 11, 433, 148, 26, 0x3d3d6b);
    display.draw_text((display.width() - MENU_W) + 15, 451, mode_label_, 0x55aaff);
    display.fill_rect((display.width() - MENU_W) + 10, 466, 150, 28, 0x333355);
    display.fill_rect((display.width() - MENU_W) + 11, 467, 148, 26, 0x3d3d6b);
    display.draw_text((display.width() - MENU_W) + 15, 485, ground_label_, ground_mode_ ? 0x55ff55 : 0xff5555);

    // Terrain
    display.fill_rect((display.width() - MENU_W) + 10, 500, 150, 28, 0x333355);
    display.fill_rect((display.width() - MENU_W) + 11, 501, 148, 26, 0x3d3d6b);
    display.draw_text((display.width() - MENU_W) + 15, 519, "Set Terrain", grid.has_terrain() ? 0x55ff55 : 0xccccff);
    {
        char tbuf[128];
        if (grid.has_terrain())
            snprintf(tbuf, sizeof(tbuf), "Terrain: loaded");
        else
            snprintf(tbuf, sizeof(tbuf), "Terrain: none");
        display.draw_text((display.width() - MENU_W) + 10, 540, tbuf, grid.has_terrain() ? 0x55aa55 : 0x555555);
    }

    if (save_dialog_active_) {
        draw_save_dialog();
        cursor_counter_++;
    }
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

    if (mx < (display.width() - MENU_W) || mx >= (display.width() - MENU_W) + MENU_W)
        return false;

    std::cerr << "CLICK at (" << mx << "," << my << ")" << std::endl;

    if (my >= 45 && my < 45 + 28)
        return try_place(ShapeType::SPHERE);
    else if (my >= 80 && my < 80 + 28)
        return try_place(ShapeType::BOX);
    else if (my >= 115 && my < 115 + 28)
        return try_place(ShapeType::CYLINDER);
    else if (my >= 150 && my < 150 + 28)
        return try_place(ShapeType::CONE);
    else if (my >= 185 && my < 185 + 28)
        return try_place(ShapeType::MESH);
    else if (!mesh_files_.empty() && my >= 217 && my < 217 + 18) {
        int sidebar_x = display.width() - MENU_W;
        if (mx >= sidebar_x + 10 && mx < sidebar_x + 30) {
            cycle_mesh(-1);
            return true;
        } else if (mx >= sidebar_x + 10 + 150 - 20 && mx < sidebar_x + 10 + 150) {
            cycle_mesh(1);
            return true;
        }
    }
    else if (my >= 220 && my < 220 + 28) {
        clear_grid();
        return true;
    } else if (my >= 255 && my < 255 + 28) {
        open_save_dialog();
        return true;
    } else if (my >= 432 && my < 432 + 28) {
        mode_clicked_ = true;
        return false;
    } else if (my >= 466 && my < 466 + 28) {
        ground_clicked_ = true;
        return false;
    } else if (my >= 500 && my < 500 + 28) {
        if (mesh_files_.empty()) {
            std::cerr << "FAIL: no .obj files to load as terrain" << std::endl;
            return true;
        }
        const std::string& path = mesh_files_[mesh_idx_];
        Mesh* m = load_obj(path.c_str(), Vec3(0.3, 0.5, 0.25), Vec3(0, 0, 0), 1.0);
        if (m) {
            grid.set_terrain(m, path);
            std::cerr << "Terrain set from " << path << std::endl;
        } else {
            std::cerr << "FAIL: could not load terrain from " << path << std::endl;
        }
        return true;
    }
    std::cerr << "Menu click at y=" << my << " (no button)" << std::endl;
    return false;
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

void UI::confirm_save() {
    if (save_name_len_ > 0) {
        std::string path = "saves/" + std::string(save_name_) + ".r3do";
        save_scene(grid, path);
        snprintf(save_msg_, sizeof(save_msg_), "Saved: %s", save_name_);
        save_msg_timer_ = 60;
        std::cout << "  -> " << path << std::endl;
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
    } else if (c >= 32 && c <= 126 && save_name_len_ < 255) {
        save_name_[save_name_len_++] = c;
        save_name_[save_name_len_] = 0;
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

    char display_name[256];
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
