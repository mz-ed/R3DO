#include "startscreen.hpp"
#include <unistd.h>
#include <dirent.h>
#include <algorithm>
#include <cstdio>
#include <cstring>

std::vector<std::string> list_saves() {
    std::vector<std::string> saves;
    DIR* dir = opendir("saves");
    if (!dir) return saves;
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string name = entry->d_name;
        if (name.size() > 5 && name.substr(name.size() - 5) == ".r3do") {
            saves.push_back(name.substr(0, name.size() - 5));
        }
    }
    closedir(dir);
    std::sort(saves.begin(), saves.end());
    return saves;
}

StartAction show_start_screen(DisplayWin& display) {
    int w = display.width(), h = display.height();
    const int bw = 220, bh = 44;
    const int bx = (w - bw) / 2;
    const int by_new = h / 2 - 30, by_load = by_new + bh + 12;

    while (true) {
        display.process_events();
        if (display.is_closed()) return StartAction::QUIT;

        int key = display.get_key();
        display.clear_key();
        if (key == XK_Escape) return StartAction::QUIT;

        display.fill_rect(0, 0, w, h, 0x0d0d1a);

        display.draw_text(w/2 - 32, h * 2 / 7, "R3DO", 0xffffff);
        display.draw_text(w/2 - 82, h * 2 / 7 + 22, "Interactive 3D Ray Tracer", 0x666688);

        display.fill_rect(bx, by_new, bw, bh, 0x333355);
        display.fill_rect(bx + 1, by_new + 1, bw - 2, bh - 2, 0x3d3d6b);
        display.draw_text(bx + 55, by_new + 28, "New Scene", 0xccccff);

        display.fill_rect(bx, by_load, bw, bh, 0x333355);
        display.fill_rect(bx + 1, by_load + 1, bw - 2, bh - 2, 0x3d3d6b);
        display.draw_text(bx + 50, by_load + 28, "Load Scene", 0xccccff);

        display.draw_text(w/2 - 100, h - 30, "Arrow keys / WASD to move  |  Esc to quit", 0x444466);

        if (display.mouse_clicked()) {
            int mx = display.mouse_x();
            int my = display.mouse_y();
            display.clear_mouse();

            if (mx >= bx && mx < bx + bw && my >= by_new && my < by_new + bh)
                return StartAction::NEW_SCENE;
            if (mx >= bx && mx < bx + bw && my >= by_load && my < by_load + bh)
                return StartAction::LOAD_SCENE;
        }

        usleep(10000);
    }
}

std::string show_load_screen(DisplayWin& display) {
    auto saves = list_saves();
    int w = display.width(), h = display.height();
    const int bw = 320, bh = 36;
    const int bx = (w - bw) / 2;

    while (true) {
        display.process_events();
        if (display.is_closed()) return "";

        int key = display.get_key();
        display.clear_key();
        if (key == XK_Escape) return "";

        display.fill_rect(0, 0, w, h, 0x0d0d1a);
        display.draw_text(w/2 - 48, h / 6, "Load Scene", 0xffffff);

        int y = h / 5;
        if (saves.empty()) {
            display.draw_text(bx, y, "No saves found", 0x888888);
            y += 40;
        } else {
            for (size_t i = 0; i < saves.size(); i++) {
                display.fill_rect(bx, y, bw, bh, 0x333355);
                display.fill_rect(bx + 1, y + 1, bw - 2, bh - 2, 0x3d3d6b);
                display.draw_text(bx + 10, y + 24, saves[i].c_str(), 0xccccff);
                y += bh + 6;
            }
        }

        int back_y = y + 12;
        display.fill_rect(bx, back_y, bw, bh, 0x333355);
        display.fill_rect(bx + 1, back_y + 1, bw - 2, bh - 2, 0x3d3d6b);
        display.draw_text(bx + 135, back_y + 24, "Back", 0xccccff);

        if (display.mouse_clicked()) {
            int mx = display.mouse_x();
            int my = display.mouse_y();
            display.clear_mouse();

            y = h / 5;
            for (size_t i = 0; i < saves.size(); i++) {
                if (mx >= bx && mx < bx + bw && my >= y && my < y + bh)
                    return saves[i];
                y += bh + 6;
            }
            if (mx >= bx && mx < bx + bw && my >= back_y && my < back_y + bh)
                return "";
        }

        usleep(10000);
    }
}
