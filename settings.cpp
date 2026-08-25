#include "settings.hpp"
#include <cstdio>
#include <cstring>
#include <cmath>

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

static bool is_known_resolution(int w, int h) {
    for (int i = 0; i < NUM_RESOLUTIONS; i++)
        if (RESOLUTIONS[i].w == w && RESOLUTIONS[i].h == h) return true;
    return false;
}

Settings load_settings() {
    Settings s;

    FILE* f = fopen("settings.cfg", "r");
    if (!f) return s;

    char key[32];
    double a, b, c;
    while (fscanf(f, "%31s", key) == 1) {
        if (strcmp(key, "resolution") == 0 &&
            fscanf(f, "%lf %lf", &a, &b) == 2) {
            int w = (int)a, h = (int)b;
            if (w > 0 && h > 0 && is_known_resolution(w, h)) {
                s.res.w = w;
                s.res.h = h;
            }
        } else if (strcmp(key, "light") == 0 &&
                   fscanf(f, "%lf %lf %lf", &a, &b, &c) == 3) {
            if (std::isfinite(a) && std::isfinite(b) && std::isfinite(c) &&
                (a != 0 || b != 0 || c != 0)) {
                s.light_x = a; s.light_y = b; s.light_z = c;
            }
        } else if ((strcmp(key, "samples") == 0 || strcmp(key, "lq_samples") == 0) &&
                   fscanf(f, "%lf %lf", &a, &b) == 2) {
            int lq = (int)a, hq = (int)b;
            if (lq >= 1 && lq <= 64) s.lq_samples = lq;
            if (hq >= 1 && hq <= 64) s.hq_samples = hq;
        }
    }
    fclose(f);
    return s;
}

void save_settings(const Settings& s) {
    FILE* f = fopen("settings.cfg", "w");
    if (!f) return;
    fprintf(f, "resolution %d %d\n", s.res.w, s.res.h);
    fprintf(f, "light %.3f %.3f %.3f\n", s.light_x, s.light_y, s.light_z);
    fprintf(f, "samples %d %d\n", s.lq_samples, s.hq_samples);
    fclose(f);
}
