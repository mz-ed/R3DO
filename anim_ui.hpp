#ifndef ANIM_UI_H
#define ANIM_UI_H

#include "animation.hpp"
#include "display.hpp"
#include <string>
#include <vector>

class AnimUI {
public:
    AnimUI(DisplayWin& display);

    bool visible = false;

    void draw();
    bool handle_click(int mx, int my);
    void handle_key(int keysym);

    Animator& animator() { return animator_; }

    void set_target_free(int free_idx) { target_free_ = free_idx; }
    void set_target_cell(int i, int j, int k) { target_i_ = i; target_j_ = j; target_k_ = k; }

    bool playing() const { return animator_.global_playing; }
    bool key_dialog_open() const { return key_dialog_active_; }

private:
    DisplayWin& display_;
    Animator animator_;

    int selected_property_ = 0;
    int selected_interp_ = 0;
    float scrub_time_ = 0.0f;
    int selected_clip_ = 0;

    int target_free_ = -1;
    int target_i_ = -1, target_j_ = -1, target_k_ = -1;

    bool key_dialog_active_ = false;
    float key_time_ = 0.0f;
    float key_val_[3] = {0, 0, 0};
    int key_field_ = 0;

    static const int PANEL_W = 280;

    void draw_button(int x, int y, int w, int h, const char* label,
                     bool hover, bool active = false);
    bool button_hit(int mx, int my, int x, int y, int w, int h) const;

    void draw_timeline(int x, int y, int w);
    void draw_key_dialog();

public:
    int panel_x() const { return 0; }
};

#endif
