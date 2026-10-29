#include "nkui.hpp"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include "vendor/nuklear/nuklear.h"
#pragma GCC diagnostic pop
#include "animation.hpp"
#include "camera.hpp"
#include "ui.hpp"
#include "hittable.hpp"
#include "v3.hpp"
#include <cstdio>
#include <cstring>

void NkUI::draw_anim_panel(Animator& anim, Camera& cam,
                           const std::vector<std::string>& target_names,
                           const std::vector<Hittable*>& target_objs, int wh) {
    if (!visible_) return;
    bool open = nk_begin(ctx_, "Animation", nk_rect(0, 0, (float)PANEL_W, (float)wh),
                         NK_WINDOW_BORDER | NK_WINDOW_TITLE);
    if (!open) { nk_end(ctx_); return; }
    ctx_->current->layout->flags &= ~(nk_flags)NK_WINDOW_ROM;
    ctx_->current->flags &= ~(nk_flags)NK_WINDOW_ROM;

    // ---- playback ----
    nk_layout_row_dynamic(ctx_, 24, 4);
    if (nk_button_label(ctx_, "PLAY")) anim.play();
    if (nk_button_label(ctx_, "PAUSE")) anim.pause();
    if (nk_button_label(ctx_, "STOP")) anim.stop();
    {
        bool any_loop = false;
        for (auto& c : anim.clips) if (c.clip.loop) { any_loop = true; break; }
        if (nk_button_label(ctx_, any_loop ? "LOOP+" : "LOOP-"))
            for (auto& c : anim.clips) c.clip.loop = !any_loop;
    }

    nk_layout_row_dynamic(ctx_, 20, 1);
    nk_property_float(ctx_, "speed", 0.1f, &anim.global_speed, 5.0f, 0.1f, 0.05f);

    char tl[80];
    snprintf(tl, sizeof(tl), "%.2f / %.2f", anim.global_time, anim.max_time);
    nk_layout_row_begin(ctx_, NK_DYNAMIC, 22, 2);
    nk_layout_row_push(ctx_, 0.60f);
    if (anim.max_time > 0.01f) {
        float pct = anim.global_time / anim.max_time;
        float old = pct;
        nk_slider_float(ctx_, 0.0f, &pct, 1.0f, 0.002f);
        if (pct != old) anim.seek(pct * anim.max_time);
    } else {
        nk_label(ctx_, "(no keys)", NK_TEXT_CENTERED);
    }
    nk_layout_row_push(ctx_, 0.40f);
    nk_label(ctx_, tl, NK_TEXT_RIGHT);
    nk_layout_row_end(ctx_);

    nk_spacing(ctx_, 1);

    if (!anim.clips.empty()) {
        if (sel_clip_ >= (int)anim.clips.size()) sel_clip_ = (int)anim.clips.size() - 1;
        if (sel_clip_ < 0) sel_clip_ = 0;
    }
    auto sel_cli = [&]() -> Animator::ClipInstance& {
        static Animator::ClipInstance dummy; // only used when no clips exist
        auto* p = anim.clips.empty() ? nullptr : &anim.clips[(size_t)sel_clip_];
        return p ? *p : dummy;
    };

    // helper: object bound to the selected clip's target
    auto tgt_obj = [&]() -> Hittable* {
        if (anim.clips.empty()) return nullptr;
        const auto& t = sel_cli().target;
        if (t.type != AnimTarget::FREE_OBJECT) return nullptr;
        if (t.free_idx < 0 || t.free_idx >= (int)target_objs.size()) return nullptr;
        return target_objs[(size_t)t.free_idx];
    };
    // snapshot a camera clip's pose base; object clips use the live base so
    // their keys stay linked to drag / button / UI position edits.
    auto snap_base = [&](int ci) {
        if (ci < 0 || ci >= (int)anim.clips.size()) return;
        auto& c = anim.clips[(size_t)ci];
        if (c.target.type != AnimTarget::CAMERA) return;
        c.base_pos = cam.pos;
        c.base_yaw = (float)cam.yaw;
        c.base_pitch = (float)cam.pitch;
        c.base_snapped = true;
    };

    if (!anim.clips.empty()) {
        nk_layout_row_dynamic(ctx_, 24, 2);
        if (nk_button_label(ctx_, "NEW CLIP")) {
            AnimationClip nc;
            nc.name = "Clip " + std::to_string(anim.clips.size() + 1);
            AnimTarget t;
            if (!target_names.empty()) { t.type = AnimTarget::FREE_OBJECT; t.free_idx = 0; }
            anim.add_clip(t, nc);
            sel_clip_ = (int)anim.clips.size() - 1;
            snap_base(sel_clip_);
        }
        if (nk_button_label(ctx_, "DEL CLIP")) {
            anim.remove_clip(sel_clip_);
            if (sel_clip_ >= (int)anim.clips.size())
                sel_clip_ = (int)anim.clips.size() - 1;
            anim.update_max_time();
        }

        // ---- target combo (free objects + camera) ----
        nk_layout_row_dynamic(ctx_, 20, 1);
        const int free_count = (int)target_names.size() - 1; // last entry = camera
        AnimTarget& tg = sel_cli().target;
        const char* tcur;
        if (tg.type == AnimTarget::CAMERA) tcur = "camera";
        else if (tg.type == AnimTarget::CELL) {
            static char cellbuf[64];
            snprintf(cellbuf, sizeof(cellbuf), "cell %d,%d,%d", tg.i, tg.j, tg.k);
            tcur = cellbuf;
        } else if (tg.type == AnimTarget::FREE_OBJECT &&
                   tg.free_idx >= 0 && tg.free_idx < free_count)
            tcur = target_names[(size_t)tg.free_idx].c_str();
        else
            tcur = "^ pick target";

        if (nk_combo_begin_label(ctx_, tcur, nk_vec2((float)PANEL_W - 30, 170))) {
            nk_layout_row_dynamic(ctx_, 18, 1);
            for (int i = 0; i < (int)target_names.size(); i++)
                if (nk_combo_item_label(ctx_, target_names[(size_t)i].c_str(), NK_TEXT_LEFT)) {
                    auto& t = sel_cli().target;
                    if (i >= free_count) {
                        t.type = AnimTarget::CAMERA;
                        t.free_idx = -1;
                        t.i = t.j = t.k = -1;
                        sel_cli().base_snapped = false; // snap pose on next apply
                    } else {
                        t.type = AnimTarget::FREE_OBJECT;
                        t.free_idx = i;
                        snap_base(sel_clip_);
                    }
                }
            nk_combo_end(ctx_);
        }

        // dead-target warning: a clip bound to an index that no longer exists
        if (tg.type == AnimTarget::FREE_OBJECT &&
            (tg.free_idx < 0 || tg.free_idx >= free_count)) {
            nk_layout_row_dynamic(ctx_, 16, 1);
            nk_label_colored(ctx_, "dead target - not animating, re-pick",
                             NK_TEXT_LEFT, nk_rgba(233, 69, 96, 255));
        }
        if (tg.type == AnimTarget::CELL) {
            nk_layout_row_dynamic(ctx_, 16, 1);
            char cb[96];
            snprintf(cb, sizeof(cb), "animating cell %d,%d,%d (empty cells skip)",
                     tg.i, tg.j, tg.k);
            nk_label_colored(ctx_, cb, NK_TEXT_LEFT, nk_rgba(97, 175, 239, 255));
        }

        // ---- cell target picker ----
        nk_bool cell_mode = (tg.type == AnimTarget::CELL) ? nk_true : nk_false;
        nk_layout_row_dynamic(ctx_, 18, 1);
        if (nk_checkbox_label(ctx_, "animate a grid cell (i,j,k)", &cell_mode)) {
            tg.type = cell_mode ? AnimTarget::CELL : AnimTarget::FREE_OBJECT;
            if (cell_mode) { tg.free_idx = -1; sel_cli().base_snapped = false; }
            else if (!target_names.empty()) tg.free_idx = 0;
        }
        if (cell_mode) {
            nk_layout_row_dynamic(ctx_, 18, 3);
            nk_property_int(ctx_, "i", 0, &cell_i_, 32, 1, 1);
            nk_property_int(ctx_, "j", 0, &cell_j_, 32, 1, 1);
            nk_property_int(ctx_, "k", 0, &cell_k_, 32, 1, 1);
        }
        if (cell_mode && nk_button_label(ctx_, "USE THIS CELL")) {
            tg.type = AnimTarget::CELL;
            tg.free_idx = -1;
            tg.i = cell_i_; tg.j = cell_j_; tg.k = cell_k_;
            sel_cli().base_snapped = false;
        }
    } else {
        nk_layout_row_dynamic(ctx_, 24, 1);
        if (nk_button_label(ctx_, "+ NEW CLIP")) {
            AnimationClip nc;
            nc.name = "Clip 1";
            AnimTarget t;
            if (!target_names.empty()) { t.type = AnimTarget::FREE_OBJECT; t.free_idx = 0; }
            anim.add_clip(t, nc);
            sel_clip_ = 0;
            snap_base(0);
        }
    }

    if (!anim.clips.empty()) {
        nk_spacing(ctx_, 1);
        static const char* kprops[] = { "Position", "Rotation", "Scale", "Color" };
        static const char* kinterps[] = { "Linear", "EaseIn", "EaseOut", "EaseInOut", "Step" };
        nk_layout_row_begin(ctx_, NK_DYNAMIC, 20, 2);
        nk_layout_row_push(ctx_, 0.5f);
        if (nk_combo_begin_label(ctx_, kprops[sel_prop_], nk_vec2(140, 130))) {
            nk_layout_row_dynamic(ctx_, 18, 1);
            for (int i = 0; i < 4; i++)
                if (nk_combo_item_label(ctx_, kprops[i], NK_TEXT_LEFT)) sel_prop_ = i;
            nk_combo_end(ctx_);
        }
        nk_layout_row_push(ctx_, 0.5f);
        if (nk_combo_begin_label(ctx_, kinterps[sel_interp_], nk_vec2(140, 150))) {
            nk_layout_row_dynamic(ctx_, 18, 1);
            for (int i = 0; i < 5; i++)
                if (nk_combo_item_label(ctx_, kinterps[i], NK_TEXT_LEFT)) sel_interp_ = i;
            nk_combo_end(ctx_);
        }
        nk_layout_row_end(ctx_);

        nk_layout_row_begin(ctx_, NK_DYNAMIC, 22, 2);
        nk_layout_row_push(ctx_, 0.62f);
        nk_slider_float(ctx_, 0.0f, &key_time_, 120.0f, 0.05f);
        nk_layout_row_push(ctx_, 0.38f);
        nk_property_float(ctx_, "time", 0.0f, &key_time_, 120.0f, 0.1f, 0.05f);
        nk_layout_row_end(ctx_);

        // capture keys from the target's live pose
        auto& cli = sel_cli();
        AnimTarget& tg2 = cli.target;
        auto sync_key = [&]() {
            if (tg2.type == AnimTarget::CAMERA) {
                if (sel_prop_ == 1) { // Rotation: deltas vs pose base
                    key_x_ = (float)(cam.yaw - (double)cli.base_yaw);
                    key_y_ = (float)(cam.pitch - (double)cli.base_pitch);
                    key_z_ = 0.0f;
                } else {              // Position: world offsets vs base pos
                    Vec3 v = cam.pos - cli.base_pos;
                    key_x_ = (float)v.x; key_y_ = (float)v.y; key_z_ = (float)v.z;
                    sel_prop_ = 0;
                }
                return;
            }
            Hittable* o = tgt_obj();
            if (!o) return;
            // Live base: the object's current offset IS its animation value
            // (drag/buttons/base edits move the base, not the offset).
            Vec3 v = o->anim_offset();
            key_x_ = (float)v.x; key_y_ = (float)v.y; key_z_ = (float)v.z;
            sel_prop_ = 0; // Position
        };
        auto key_from_target = [&]() {
            sync_key();
            Vec3 v(key_x_, key_y_, key_z_);
            AnimationClip& c = cli.clip;
            c.add_keyframe((AnimProperty)sel_prop_, key_time_, v,
                           (InterpolationType)sel_interp_);
            anim.update_max_time();
        };

        nk_layout_row_dynamic(ctx_, 22, 3);
        if (nk_button_label(ctx_, "KEY @ CUR")) {
            key_time_ = anim.global_time;
            if (key_time_ < 0.0f) key_time_ = 0.0f;
        }
        if (nk_button_label(ctx_, "SYNC FROM TGT")) sync_key();
        if (nk_button_label(ctx_, "KEY @ TGT")) key_from_target();

        nk_property_float(ctx_, "key x", -50.0f, &key_x_, 50.0f, 0.5f, 0.05f);
        nk_property_float(ctx_, "key y", -50.0f, &key_y_, 50.0f, 0.5f, 0.05f);
        nk_property_float(ctx_, "key z", -50.0f, &key_z_, 50.0f, 0.5f, 0.05f);
        {
            const char* src;
            Vec3 ref(0, 0, 0);
            if (tg2.type == AnimTarget::CAMERA) {
                if (sel_prop_ == 1) ref = Vec3((float)(cam.yaw - (double)cli.base_yaw),
                                               (float)(cam.pitch - (double)cli.base_pitch), 0);
                else ref = cam.pos - cli.base_pos;
                src = "cam offset now";
            } else if (Hittable* o = tgt_obj()) {
                ref = o->anim_offset();
                src = "offset now (live base)";
            } else src = nullptr;
            if (src) {
                nk_layout_row_dynamic(ctx_, 16, 1);
                char ob[128];
                snprintf(ob, sizeof(ob), "%s: %.2f %.2f %.2f",
                         src, (double)ref.x, (double)ref.y, (double)ref.z);
                nk_label_colored(ctx_, ob, NK_TEXT_LEFT, nk_rgba(150, 150, 150, 255));
            }
            // Shared world position of the bound object: updates live from
            // drag, buttons and the sidebar position editor.
            if (Hittable* o = tgt_obj()) {
                nk_layout_row_dynamic(ctx_, 16, 1);
                char wb[112];
                Vec3 wp = o->get_center();
                snprintf(wb, sizeof(wb), "world pos: %.2f %.2f %.2f",
                         (double)wp.x, (double)wp.y, (double)wp.z);
                nk_label_colored(ctx_, wb, NK_TEXT_LEFT, nk_rgba(97, 175, 239, 255));
            }
        }

        nk_layout_row_dynamic(ctx_, 22, 2);
        if (nk_button_label(ctx_, "+ KEY")) {
            AnimationClip& c = cli.clip;
            c.add_keyframe((AnimProperty)sel_prop_, key_time_,
                           Vec3(key_x_, key_y_, key_z_), (InterpolationType)sel_interp_);
            anim.update_max_time();
        }
        if (nk_button_label(ctx_, "CLEAR")) {
            cli.clip.clear();
            anim.update_max_time();
        }

        // live preview of the selected property at the playhead (paused=new pose)
        {
            float tp = anim.clip_time(cli);
            Vec3 pv = cli.clip.evaluate((AnimProperty)sel_prop_, tp);
            nk_layout_row_dynamic(ctx_, 16, 1);
            char pvb[96];
            snprintf(pvb, sizeof(pvb), "t=%.2f -> %s(%.2f, %.2f, %.2f)",
                     (double)tp, kprops[sel_prop_], (double)pv.x, (double)pv.y, (double)pv.z);
            nk_label_colored(ctx_, pvb, NK_TEXT_LEFT, nk_rgba(97, 175, 239, 255));
        }

        auto& tr = cli.clip.tracks;
        if (!tr.empty()) {
            nk_layout_row_dynamic(ctx_, 96, 1);
            if (nk_group_begin(ctx_, "tracks", NK_WINDOW_BORDER)) {
                for (int i = 0; i < (int)tr.size(); i++) {
                    char row[96];
                    snprintf(row, sizeof(row), "%s  (%d keys)",
                             kprops[(int)tr[(size_t)i].property],
                             (int)tr[(size_t)i].keyframes.size());
                    nk_layout_row_begin(ctx_, NK_DYNAMIC, 18, 2);
                    nk_layout_row_push(ctx_, 0.88f);
                    nk_label(ctx_, row, NK_TEXT_LEFT);
                    nk_layout_row_push(ctx_, 0.12f);
                    if (nk_button_label(ctx_, "x")) {
                        tr.erase(tr.begin() + i);
                        anim.update_max_time();
                        break;
                    }
                    nk_layout_row_end(ctx_);
                }
                nk_group_end(ctx_);
            }
        }

        // ---- procedural modifiers (attach to the selected clip) ----
        nk_layout_row_dynamic(ctx_, 18, 1);
        nk_label_colored(ctx_, "-- procedural modifiers --", NK_TEXT_LEFT, nk_rgba(150,150,150,255));
        nk_layout_row_dynamic(ctx_, 22, 3);
        nk_slider_float(ctx_, 0.01f, &proc_amp_, 3.0f, 0.05f);
        nk_slider_float(ctx_, 0.05f, &proc_freq_, 5.0f, 0.05f);
        nk_slider_float(ctx_, 0.05f, &proc_speed_, 5.0f, 0.05f);
        nk_layout_row_dynamic(ctx_, 22, 3);
        if (nk_button_label(ctx_, "+ SINE")) {
            ProceduralModifier pm;
            pm.type = ProceduralType::SineBob;
            pm.amplitude = proc_amp_; pm.frequency = proc_freq_; pm.speed = proc_speed_;
            pm.enabled = true;
            cli.procs.push_back(pm);
        }
        if (nk_button_label(ctx_, "+ SPIN")) {
            ProceduralModifier pm;
            pm.type = ProceduralType::SpinRotate;
            pm.amplitude = proc_amp_; pm.frequency = proc_freq_; pm.speed = proc_speed_;
            pm.enabled = true;
            cli.procs.push_back(pm);
        }
        if (nk_button_label(ctx_, "+ SHAKE")) {
            ProceduralModifier pm;
            pm.type = ProceduralType::NoiseShake;
            pm.amplitude = proc_amp_; pm.frequency = proc_freq_; pm.speed = proc_speed_;
            pm.enabled = true;
            cli.procs.push_back(pm);
        }
        for (size_t pi = 0; pi < cli.procs.size(); pi++) {
            auto& pm = cli.procs[pi];
            nk_layout_row_begin(ctx_, NK_DYNAMIC, 18, 2);
            nk_layout_row_push(ctx_, 0.90f);
            char pmrow[96];
            snprintf(pmrow, sizeof(pmrow), "%s amp=%.2f freq=%.2f spd=%.2f",
                     pm.type == ProceduralType::SineBob   ? "sine bob" :
                     pm.type == ProceduralType::SpinRotate? "spin" : "noise shake",
                     (double)pm.amplitude, (double)pm.frequency, (double)pm.speed);
            nk_label(ctx_, pmrow, NK_TEXT_LEFT);
            nk_layout_row_push(ctx_, 0.10f);
            if (nk_button_label(ctx_, "x")) {
                cli.procs.erase(cli.procs.begin() + (long)pi);
                break;
            }
            nk_layout_row_end(ctx_);
        }
    }

    nk_end(ctx_);
}
