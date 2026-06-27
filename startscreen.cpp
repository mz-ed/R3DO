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

StartAction show_start_screen(DisplayWin& display, Settings& settings) {
    int w = display.width(), h = display.height();
    const int bw = 220, bh = 44;
    const int bx = (w - bw) / 2;
    const int by_new = h / 2 - 60;
    const int by_load = by_new + bh + 10;
    const int by_settings = by_load + bh + 10;

    while (true) {
        display.process_events();
        if (display.is_closed()) return StartAction::QUIT;

        int key = display.get_key();
        display.clear_key();
        if (key == XK_Escape) return StartAction::QUIT;

        display.fill_rect(0, 0, w, h, 0x0d0d1a);

        display.draw_text(w/2 - 32, h * 2 / 7, "R3DO", 0xffffff);
        display.draw_text(w/2 - 82, h * 2 / 7 + 22, "Interactive 3D Ray Tracer", 0x666688);

        auto draw_btn = [&](int by, const char* text, int text_off) {
            display.fill_rect(bx, by, bw, bh, 0x333355);
            display.fill_rect(bx + 1, by + 1, bw - 2, bh - 2, 0x3d3d6b);
            display.draw_text(bx + text_off, by + 28, text, 0xccccff);
        };

        draw_btn(by_new, "New Scene", 55);
        draw_btn(by_load, "Load Scene", 50);
        draw_btn(by_settings, "Settings", 65);

        display.draw_text(w/2 - 100, h - 30, "F11 fullscreen  |  Esc to quit", 0x444466);

        if (display.mouse_clicked()) {
            int mx = display.mouse_x();
            int my = display.mouse_y();
            display.clear_mouse();

            if (mx >= bx && mx < bx + bw && my >= by_new && my < by_new + bh)
                return StartAction::NEW_SCENE;
            if (mx >= bx && mx < bx + bw && my >= by_load && my < by_load + bh)
                return StartAction::LOAD_SCENE;
            if (mx >= bx && mx < bx + bw && my >= by_settings && my < by_settings + bh)
                show_settings_screen(display, settings);
        }

        usleep(10000);
    }
}

void show_settings_screen(DisplayWin& display, Settings& settings) {
    int w = display.width(), h = display.height();
    const int bw = 280, bh = 40;
    const int bx = (w - bw) / 2;

    while (true) {
        display.process_events();
        if (display.is_closed()) return;

        int key = display.get_key();
        display.clear_key();
        if (key == XK_Escape) return;

        display.fill_rect(0, 0, w, h, 0x0d0d1a);
        display.draw_text(w/2 - 48, h / 6, "Settings", 0xffffff);
        display.draw_text(w/2 - 72, h / 6 + 22, "Resolution", 0x888888);

        int y = h / 4;
        for (int i = 0; i < NUM_RESOLUTIONS; i++) {
            bool active = RESOLUTIONS[i].w == settings.res.w && RESOLUTIONS[i].h == settings.res.h;
            display.fill_rect(bx, y, bw, bh, active ? 0x555588 : 0x333355);
            display.fill_rect(bx + 1, y + 1, bw - 2, bh - 2, active ? 0x5d5d9b : 0x3d3d6b);

            char label[32];
            snprintf(label, sizeof(label), "%d x %d", RESOLUTIONS[i].w, RESOLUTIONS[i].h);
            display.draw_text(bx + 10, y + 27, label, active ? 0xffffff : 0xccccff);
            y += bh + 6;
        }

        int back_y = y + 12;
        display.fill_rect(bx, back_y, bw, 40, 0x333355);
        display.fill_rect(bx + 1, back_y + 1, bw - 2, 38, 0x3d3d6b);
        display.draw_text(bx + 115, back_y + 27, "Back", 0xccccff);

        if (display.mouse_clicked()) {
            int mx = display.mouse_x();
            int my = display.mouse_y();
            display.clear_mouse();

            y = h / 4;
            for (int i = 0; i < NUM_RESOLUTIONS; i++) {
                if (mx >= bx && mx < bx + bw && my >= y && my < y + bh) {
                    settings.res = RESOLUTIONS[i];
                    save_settings(settings);
                    display.resize(settings.res.w, settings.res.h);
                    w = display.width();
                    h = display.height();
                }
                y += bh + 6;
            }
            if (mx >= bx && mx < bx + bw && my >= back_y && my < back_y + 40)
                return;
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
