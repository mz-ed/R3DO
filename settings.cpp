#include "settings.hpp"
#include <cstdio>
#include <cstring>

const Resolution RESOLUTIONS[] = {
    {800, 600},
    {1024, 768},
    {1280, 720},
    {1366, 768},
    {1600, 900},
    {1920, 1080},
    {2560, 1440},
};

const int NUM_RESOLUTIONS = sizeof(RESOLUTIONS) / sizeof(RESOLUTIONS[0]);

Settings load_settings() {
    Settings s;
    s.res = RESOLUTIONS[5];

    FILE* f = fopen("settings.cfg", "r");
    if (!f) return s;

    char key[32];
    int w, h;
    while (fscanf(f, "%31s %d %d", key, &w, &h) == 3) {
        if (strcmp(key, "resolution") == 0) {
            s.res.w = w;
            s.res.h = h;
        }
    }
    fclose(f);
    return s;
}

void save_settings(const Settings& s) {
    FILE* f = fopen("settings.cfg", "w");
    if (!f) return;
    fprintf(f, "resolution %d %d\n", s.res.w, s.res.h);
    fclose(f);
}
