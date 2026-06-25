#include "ui.hpp"
#include "sphere.hpp"
#include "box.hpp"
#include <cstdio>

UI::UI(Grid& grid, Camera& cam, DisplayWin& display)
    : grid(grid), cam(cam), display(display), palette_idx_(0) {}

Vec3 UI::palette_color(int index) const {
    Vec3 palette[] = {
        Vec3(1, 0.2, 0.2), Vec3(0.2, 1, 0.3), Vec3(1, 1, 0.2),
        Vec3(1, 0.5, 0), Vec3(0.2, 0.4, 1), Vec3(0.9, 0.2, 0.9),
        Vec3(0.2, 0.9, 0.9), Vec3(0.8, 0.6, 0.2), Vec3(0.5, 0.3, 1),
        Vec3(1, 0.6, 0.6)
    };
    return palette[index % 10];
}

bool UI::try_place(bool is_sphere) {
    Vec3 target = cam.pos + cam.forward() * 3.0;
    int i, j, k;
    if (!grid.world_to_cell(target, i, j, k)) {
        std::cerr << "FAIL: target (" << target.x << "," << target.y << "," << target.z << ") outside grid" << std::endl;
        return false;
    }
    if (grid.get(i, j, k)) {
        std::cerr << "FAIL: cell (" << i << "," << j << "," << k << ") occupied" << std::endl;
        return false;
    }

    double cs = grid.cell_size;
    Vec3 c = grid.cell_center(i, j, k);
    if (is_sphere) {
        grid.set(i, j, k, new Sphere(c, cs * 0.45, palette_color(palette_idx_)));
        std::cerr << "Sphere at cell (" << i << "," << j << "," << k << ") pos ("
                  << c.x << "," << c.y << "," << c.z << ")" << std::endl;
    } else {
        Vec3 half(cs * 0.38, cs * 0.38, cs * 0.38);
        grid.set(i, j, k, new Box(c - half, c + half, palette_color(palette_idx_)));
        std::cerr << "Box at cell (" << i << "," << j << "," << k << ")" << std::endl;
    }
    palette_idx_++;
    return true;
}

void UI::clear_grid() {
    for (int i = 0; i < grid.nx; i++)
        for (int j = 0; j < grid.ny; j++)
            for (int k = 0; k < grid.nz; k++)
                grid.set(i, j, k, nullptr);
}

int UI::count_objects() const {
    int count = 0;
    for (int i = 0; i < grid.nx; i++)
        for (int j = 0; j < grid.ny; j++)
            for (int k = 0; k < grid.nz; k++)
                if (grid.get(i, j, k)) count++;
    return count;
}

void UI::draw() {
    display.fill_rect(MENU_X, 0, MENU_W, display.height(), 0x1a1a2e);
    display.fill_rect(MENU_X - 2, 0, 2, display.height(), 0x444466);

    display.draw_text(MENU_X + 10, 22, "R3DO", 0xffffff);
    display.fill_rect(MENU_X + 10, 28, 40, 1, 0x666688);

    Button buttons[] = {
        {MENU_X + 10, 45, 150, 28, "Add Sphere"},
        {MENU_X + 10, 80, 150, 28, "Add Box"},
        {MENU_X + 10, 115, 150, 28, "Clear All"},
    };

    for (auto& btn : buttons) {
        display.fill_rect(btn.x, btn.y, btn.w, btn.h, 0x333355);
        display.fill_rect(btn.x + 1, btn.y + 1, btn.w - 2, btn.h - 2, 0x3d3d6b);
        display.draw_text(btn.x + 10, btn.y + 19, btn.text, 0xccccff);
    }

    char buf[64];
    snprintf(buf, sizeof(buf), "Objects: %d", count_objects());
    display.draw_text(MENU_X + 10, 170, buf, 0xaaaaaa);

    snprintf(buf, sizeof(buf), "Cam: %.1f %.1f %.1f", cam.pos.x, cam.pos.y, cam.pos.z);
    display.draw_text(MENU_X + 10, 190, buf, 0x666688);

    display.draw_text(MENU_X + 10, 220, "Controls:", 0x888888);
    display.draw_text(MENU_X + 10, 237, "WASD move", 0x666688);
    display.draw_text(MENU_X + 10, 252, "Arrows look", 0x666688);
    display.draw_text(MENU_X + 10, 267, "Q/E up/down", 0x666688);
    display.draw_text(MENU_X + 10, 282, "Space: HQ", 0x666688);
    display.draw_text(MENU_X + 10, 297, "Esc: quit", 0x666688);
}

bool UI::handle_click(int mx, int my) {
    if (mx < MENU_X || mx >= MENU_X + MENU_W)
        return false;

    std::cerr << "CLICK at (" << mx << "," << my << ")" << std::endl;

    if (my >= 45 && my < 45 + 28) {
        std::cerr << "Button: Add Sphere" << std::endl;
        return try_place(true);
    } else if (my >= 80 && my < 80 + 28) {
        std::cerr << "Button: Add Box" << std::endl;
        return try_place(false);
    } else if (my >= 115 && my < 115 + 28) {
        std::cerr << "Button: Clear All" << std::endl;
        clear_grid();
        return true;
    }
    std::cerr << "Menu click at y=" << my << " (no button)" << std::endl;
    return false;
}
