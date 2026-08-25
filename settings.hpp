#ifndef SETTINGS_H
#define SETTINGS_H

struct Resolution {
    int w, h;
};

struct Settings {
    Resolution res{1920, 1080};
    double light_x = 1.0, light_y = 2.0, light_z = 1.0;
    int lq_samples = 1;
    int hq_samples = 4;

    bool operator==(const Settings& o) const { return res.w == o.res.w && res.h == o.res.h; }
};

extern const Resolution RESOLUTIONS[];
extern const int NUM_RESOLUTIONS;

Settings load_settings();
void save_settings(const Settings& s);

#endif
