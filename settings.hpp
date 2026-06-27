#ifndef SETTINGS_H
#define SETTINGS_H

struct Resolution {
    int w, h;
};

struct Settings {
    Resolution res;
};

extern const Resolution RESOLUTIONS[];
extern const int NUM_RESOLUTIONS;

Settings load_settings();
void save_settings(const Settings& s);

#endif
