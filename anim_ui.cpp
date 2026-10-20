#include "anim_ui.hpp"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <X11/keysym.h>

static const unsigned long COL_BG     = 0x1a1a2e;
static const unsigned long COL_PANEL  = 0x16213e;
static const unsigned long COL_BTN   = 0x0f3460;
static const unsigned long COL_BTN_H = 0x533483;
static const unsigned long COL_ACC   = 0xe94560;
static const unsigned long COL_TEXT  = 0xeeeeee;
static const unsigned long COL_DIM   = 0x888899;
static const unsigned long COL_LINE  = 0x333355;

AnimUI::AnimUI(DisplayWin& display) : display_(display) {}

void AnimUI::draw_button(int x, int y, int w, int h, const char* label,
                         bool hover, bool active) {
    unsigned long bg = active ? COL_ACC : (hover ? COL_BTN_H : COL_BTN);
    display_.fill_rect(x, y, w, h, bg);
    display_.fill_rect(x + 1, y + 1, w - 2, h - 2, active ? COL_ACC : bg);
    display_.draw_text(x + 8, y + h / 2 + 4, label, COL_TEXT);
}

bool AnimUI::button_hit(int mx, int my, int x, int y, int w, int h) const {
    return mx >= x && mx < x + w && my >= y && my < y + h;
}

void AnimUI::draw() {
    if (!visible) return;
    int sx = panel_x();
    int W = PANEL_W;
    int H = display_.height();

    display_.fill_rect(sx, 0, W, H, COL_BG);
    display_.fill_rect(sx - 1, 0, 1, H, COL_LINE);

    int x = sx + 12;
    int y = 20;
    int bw = W - 24;
    int bh = 28;
    int gap = 4;

    display_.draw_text(x, y, "ANIMATION", COL_ACC);
    y += 18;
    display_.fill_rect(x, y, bw, 1, COL_LINE);
    y += 10;

    int mx = display_.mouse_x();
    int my = display_.mouse_y();

    char buf[128];

    snprintf(buf, sizeof(buf), "Clip: %d / %d", selected_clip_ + 1,
             (int)animator_.clips.size());
    display_.draw_text(x, y + 14, buf, COL_DIM);
    y += 20;

    if (!animator_.clips.empty() && selected_clip_ < (int)animator_.clips.size()) {
        auto& clip = animator_.clips[selected_clip_].clip;
        snprintf(buf, sizeof(buf), "Duration: %.2fs  Tracks: %d", clip.duration, (int)clip.tracks.size());
        display_.draw_text(x, y + 14, buf, COL_DIM);
        y += 20;
    }

    draw_button(x, y, 56, bh, "PLAY", button_hit(mx, my, x, y, 56, bh), animator_.global_playing);
    draw_button(x + 60, y, 56, bh, "PAUSE", button_hit(mx, my, x + 60, y, 56, bh), !animator_.global_playing && animator_.global_time > 0);
    draw_button(x + 120, y, 56, bh, "STOP", button_hit(mx, my, x + 120, y, 56, bh));
    bool any_loop = false;
    for (auto& c : animator_.clips) if (c.clip.loop) { any_loop = true; break; }
    draw_button(x + 180, y, bw - 180, bh, "LOOP", button_hit(mx, my, x + 180, y, bw - 180, bh), any_loop);
    y += bh + gap;

    snprintf(buf, sizeof(buf), "Speed: %.1fx", animator_.global_speed);
    display_.draw_text(x, y + 14, buf, COL_TEXT);
    draw_button(x + bw - 50, y, 24, 24, "-", button_hit(mx, my, x + bw - 50, y, 24, 24));
    draw_button(x + bw - 22, y, 24, 24, "+", button_hit(mx, my, x + bw - 22, y, 24, 24));
    y += 28 + gap;

    draw_timeline(x, y, bw);
    y += 36;

    snprintf(buf, sizeof(buf), "Time: %.2fs / %.2fs", animator_.global_time, animator_.max_time);
    display_.draw_text(x, y + 14, buf, COL_TEXT);
    y += 22;

    display_.fill_rect(x, y, bw, 1, COL_LINE);
    y += 10;

    display_.draw_text(x, y + 14, "Target:", COL_DIM);
    y += 20;
    if (target_free_ >= 0) {
        snprintf(buf, sizeof(buf), "  Free object #%d", target_free_);
        display_.draw_text(x, y + 14, buf, COL_TEXT);
    } else if (target_i_ >= 0) {
        snprintf(buf, sizeof(buf), "  Cell [%d,%d,%d]", target_i_, target_j_, target_k_);
        display_.draw_text(x, y + 14, buf, COL_TEXT);
    } else {
        display_.draw_text(x, y + 14, "  (none)", COL_DIM);
    }
    y += 22;

    draw_button(x, y, bw, bh, "ADD CLIP", button_hit(mx, my, x, y, bw, bh));
    y += bh + gap;

    if (!animator_.clips.empty()) {
        draw_button(x, y, bw / 2 - 2, bh, "+KEY", button_hit(mx, my, x, y, bw / 2 - 2, bh));
        draw_button(x + bw / 2 + 2, y, bw / 2 - 2, bh, "DEL CLIP", button_hit(mx, my, x + bw / 2 + 2, y, bw / 2 - 2, bh));
        y += bh + gap;
    }

    static const char* props[] = { "Position", "Rotation", "Scale", "Color" };
    display_.draw_text(x, y + 14, "Property:", COL_DIM);
    int px = x + 80;
    for (int i = 0; i < 4; i++) {
        int pw = (bw - 80) / 4;
        bool hov = button_hit(mx, my, px + i * pw, y, pw - 2, 22);
        draw_button(px + i * pw, y, pw - 2, 22, props[i], hov, selected_property_ == i);
    }
    y += 26;

    static const char* interps[] = { "Linear", "EaseIn", "EaseOut", "InOut", "Step" };
    display_.draw_text(x, y + 14, "Interp:", COL_DIM);
    int ix = x + 70;
    for (int i = 0; i < 5; i++) {
        int iw = (bw - 70) / 5;
        bool hov = button_hit(mx, my, ix + i * iw, y, iw - 1, 20);
        draw_button(ix + i * iw, y, iw - 1, 20, interps[i], hov, selected_interp_ == i);
    }
    y += 26;

    y += 4;
    display_.fill_rect(x, y, bw, 1, COL_LINE);
    y += 10;

    draw_button(x, y, bw, bh, "PROC: SINE BOB", button_hit(mx, my, x, y, bw, bh));
    y += bh + gap;
    draw_button(x, y, bw, bh, "PROC: SPIN", button_hit(mx, my, x, y, bw, bh));
    y += bh + gap;
    draw_button(x, y, bw, bh, "PROC: NOISE", button_hit(mx, my, x, y, bw, bh));
    y += bh + gap;

    draw_button(x, y, bw, bh, "CLEAR ALL", button_hit(mx, my, x, y, bw, bh));

    if (key_dialog_active_) draw_key_dialog();
}

void AnimUI::draw_timeline(int x, int y, int w) {
    display_.fill_rect(x, y + 8, w, 16, COL_LINE);
    if (animator_.max_time > 0) {
        int fill = (int)((double)animator_.global_time / animator_.max_time * w);
        if (fill > w) fill = w;
        display_.fill_rect(x, y + 8, fill, 16, COL_ACC);
    }
    for (size_t ci = 0; ci < animator_.clips.size(); ci++) {
        auto& clip = animator_.clips[ci].clip;
        for (auto& tr : clip.tracks) {
            for (auto& kf : tr.keyframes) {
                if (animator_.max_time > 0) {
                    int kx = x + (int)((kf.time / animator_.max_time) * w);
                    display_.fill_rect(kx - 1, y + 4, 3, 24, 0xffffff);
                }
            }
        }
    }
}

void AnimUI::draw_key_dialog() {
    int dw = 320, dh = 200;
    int dx = (display_.width() - dw) / 2;
    int dy = (display_.height() - dh) / 2;
    display_.fill_rect(dx, dy, dw, dh, COL_PANEL);
    display_.fill_rect(dx, dy, dw, 1, COL_ACC);
    display_.draw_text(dx + 12, dy + 20, "ADD KEYFRAME", COL_ACC);

    int mx = display_.mouse_x();
    int my = display_.mouse_y();

    static const char* props[] = { "Position", "Rotation", "Scale", "Color" };
    display_.draw_text(dx + 12, dy + 48, "Property:", COL_DIM);
    for (int i = 0; i < 4; i++) {
        int bx = dx + 90 + i * 52;
        bool hov = button_hit(mx, my, bx, dy + 36, 48, 20);
        draw_button(bx, dy + 36, 48, 20, props[i], hov, selected_property_ == i);
    }

    display_.draw_text(dx + 12, dy + 78, "Time:", COL_DIM);
    char buf[64];
    snprintf(buf, sizeof(buf), "%.2f", key_time_);
    unsigned long field_bg = (key_field_ == 0) ? COL_BTN_H : COL_BTN;
    display_.fill_rect(dx + 90, dy + 66, 80, 22, field_bg);
    display_.draw_text(dx + 94, dy + 81, buf, COL_TEXT);

    static const char* comps[] = { "X", "Y", "Z" };
    for (int i = 0; i < 3; i++) {
        display_.draw_text(dx + 12, dy + 106 + i * 24, comps[i], COL_DIM);
        snprintf(buf, sizeof(buf), "%.3f", key_val_[i]);
        field_bg = (key_field_ == 1 + i) ? COL_BTN_H : COL_BTN;
        display_.fill_rect(dx + 90, dy + 94 + i * 24, 100, 22, field_bg);
        display_.draw_text(dx + 94, dy + 109 + i * 24, buf, COL_TEXT);
    }

    draw_button(dx + 12, dy + dh - 36, 80, 26, "OK", button_hit(mx, my, dx + 12, dy + dh - 36, 80, 26));
    draw_button(dx + 100, dy + dh - 36, 80, 26, "CANCEL", button_hit(mx, my, dx + 100, dy + dh - 36, 80, 26));
}

bool AnimUI::handle_click(int mx, int my) {
    if (!visible) return false;
    int sx = panel_x();
    if (mx < sx) return false;

    int x = sx + 12;
    int y = 20;
    int bw = PANEL_W - 24;
    int bh = 28;
    int gap = 4;

    y += 18 + 10;
    y += 20;
    if (!animator_.clips.empty() && selected_clip_ < (int)animator_.clips.size()) y += 20;

    if (button_hit(mx, my, x, y, 56, bh)) { animator_.play(); return true; }
    if (button_hit(mx, my, x + 60, y, 56, bh)) { animator_.pause(); return true; }
    if (button_hit(mx, my, x + 120, y, 56, bh)) { animator_.stop(); return true; }
    if (button_hit(mx, my, x + 180, y, bw - 180, bh)) {
        for (auto& c : animator_.clips) c.clip.loop = !c.clip.loop;
        return true;
    }
    y += bh + gap;

    if (button_hit(mx, my, x + bw - 50, y, 24, 24)) {
        animator_.set_speed(animator_.global_speed - 0.1f);
        if (animator_.global_speed < 0.1f) animator_.set_speed(0.1f);
        return true;
    }
    if (button_hit(mx, my, x + bw - 22, y, 24, 24)) {
        animator_.set_speed(animator_.global_speed + 0.1f);
        return true;
    }
    y += 28 + gap;

    if (button_hit(mx, my, x, y + 8, bw, 16)) {
        float t = (float)(mx - x) / bw * animator_.max_time;
        animator_.seek(t);
        return true;
    }
    y += 36 + 22;

    y += 10 + 20 + 22;

    if (button_hit(mx, my, x, y, bw, bh)) {
        AnimationClip clip;
        AnimTarget target;
        if (target_free_ >= 0) {
            target.type = AnimTarget::FREE_OBJECT;
            target.free_idx = target_free_;
        }
        clip.name = "Clip " + std::to_string(animator_.clips.size() + 1);
        animator_.add_clip(target, clip);
        selected_clip_ = (int)animator_.clips.size() - 1;
        return true;
    }
    y += bh + gap;

    if (!animator_.clips.empty()) {
        if (button_hit(mx, my, x, y, bw / 2 - 2, bh)) {
            key_dialog_active_ = true;
            key_time_ = animator_.global_time;
            key_val_[0] = key_val_[1] = key_val_[2] = 0;
            key_field_ = 0;
            return true;
        }
        if (button_hit(mx, my, x + bw / 2 + 2, y, bw / 2 - 2, bh)) {
            animator_.remove_clip(selected_clip_);
            if (selected_clip_ >= (int)animator_.clips.size())
                selected_clip_ = (int)animator_.clips.size() - 1;
            return true;
        }
        y += bh + gap;
    }

    y += 26;
    y += 26;

    if (button_hit(mx, my, x, y, bw, bh)) {
        if (selected_clip_ < (int)animator_.clips.size()) {
            auto& clip = animator_.clips[selected_clip_].clip;
            AnimationClip new_clip;
            new_clip.name = clip.name;
            new_clip.duration = clip.duration;
            new_clip.loop = clip.loop;
            for (auto& tr : clip.tracks) {
                AnimationTrack* nt = new_clip.create_track(tr.property);
                nt->keyframes = tr.keyframes;
            }
            AnimTarget t;
            if (target_free_ >= 0) { t.type = AnimTarget::FREE_OBJECT; t.free_idx = target_free_; }
            animator_.add_clip(t, new_clip);
        }
        return true;
    }
    y += bh + gap;
    if (button_hit(mx, my, x, y, bw, bh)) {
        animator_.clear();
        selected_clip_ = 0;
        return true;
    }

    if (key_dialog_active_) {
        int dw = 320, dh = 200;
        int dx = (display_.width() - dw) / 2;
        int dy = (display_.height() - dh) / 2;

        for (int i = 0; i < 4; i++) {
            int bx = dx + 90 + i * 52;
            if (button_hit(mx, my, bx, dy + 36, 48, 20)) {
                selected_property_ = i;
                return true;
            }
        }
        if (button_hit(mx, my, dx + 90, dy + 66, 80, 22)) { key_field_ = 0; return true; }
        for (int i = 0; i < 3; i++) {
            if (button_hit(mx, my, dx + 90, dy + 94 + i * 24, 100, 22)) {
                key_field_ = 1 + i;
                return true;
            }
        }
        if (button_hit(mx, my, dx + 12, dy + dh - 36, 80, 26)) {
            if (selected_clip_ < (int)animator_.clips.size()) {
                AnimProperty prop = (AnimProperty)selected_property_;
                InterpolationType it = (InterpolationType)selected_interp_;
                animator_.clips[selected_clip_].clip.add_keyframe(
                    prop, key_time_, Vec3(key_val_[0], key_val_[1], key_val_[2]), it);
                animator_.update_max_time();
            }
            key_dialog_active_ = false;
            return true;
        }
        if (button_hit(mx, my, dx + 100, dy + dh - 36, 80, 26)) {
            key_dialog_active_ = false;
            return true;
        }
    }

    return false;
}

void AnimUI::handle_key(int keysym) {
    if (!key_dialog_active_) return;
    if (keysym == XK_Escape) { key_dialog_active_ = false; return; }
    if (keysym == XK_Return || keysym == XK_KP_Enter) {
        if (selected_clip_ < (int)animator_.clips.size()) {
            AnimProperty prop = (AnimProperty)selected_property_;
            InterpolationType it = (InterpolationType)selected_interp_;
            animator_.clips[selected_clip_].clip.add_keyframe(
                prop, key_time_, Vec3(key_val_[0], key_val_[1], key_val_[2]), it);
            animator_.update_max_time();
        }
        key_dialog_active_ = false;
        return;
    }
    if (keysym == XK_Tab) {
        key_field_ = (key_field_ + 1) % 4;
        return;
    }
    if (keysym == XK_Left) {
        if (key_field_ == 0) key_time_ -= 0.1f;
        else key_val_[key_field_ - 1] -= 0.1f;
        return;
    }
    if (keysym == XK_Right) {
        if (key_field_ == 0) key_time_ += 0.1f;
        else key_val_[key_field_ - 1] += 0.1f;
        return;
    }
    if (keysym == XK_Up) {
        if (key_field_ == 0) key_time_ += 1.0f;
        else key_val_[key_field_ - 1] += 1.0f;
        return;
    }
    if (keysym == XK_Down) {
        if (key_field_ == 0) key_time_ -= 1.0f;
        else key_val_[key_field_ - 1] -= 1.0f;
        return;
    }
    if (keysym >= XK_0 && keysym <= XK_9) {
        float digit = (float)(keysym - XK_0);
        if (key_field_ == 0) key_time_ = key_time_ * 10 + digit;
        else key_val_[key_field_ - 1] = key_val_[key_field_ - 1] * 10 + digit;
        return;
    }
    if (keysym == XK_period || keysym == XK_KP_Decimal) return;
    if (keysym == XK_minus || keysym == XK_KP_Subtract) {
        if (key_field_ == 0) key_time_ = -key_time_;
        else key_val_[key_field_ - 1] = -key_val_[key_field_ - 1];
        return;
    }
}
