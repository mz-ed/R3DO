#include "animation.hpp"
#include <algorithm>
#include <cmath>

static float smoothstepf(float t) {
    return t * t * (3.0f - 2.0f * t);
}

static float eval_interp(InterpolationType it, float t) {
    switch (it) {
        case InterpolationType::Linear:  return t;
        case InterpolationType::EaseIn:  return t * t * t;
        case InterpolationType::EaseOut: {
            float u = 1.0f - t;
            return 1.0f - u * u * u;
        }
        case InterpolationType::EaseInOut: return smoothstepf(t);
        case InterpolationType::Step:    return t >= 1.0f ? 1.0f : 0.0f;
    }
    return t;
}

void AnimationTrack::add_keyframe(float time, const Vec3& val, InterpolationType interp) {
    Keyframe kf;
    kf.time = time;
    kf.value = val;
    kf.interp = interp;
    keyframes.push_back(kf);
    std::sort(keyframes.begin(), keyframes.end(),
              [](const Keyframe& a, const Keyframe& b) { return a.time < b.time; });
}

Vec3 AnimationTrack::evaluate(float time) const {
    if (keyframes.empty()) return Vec3(0, 0, 0);
    if (keyframes.size() == 1) return keyframes[0].value;

    for (size_t i = 0; i < keyframes.size() - 1; i++) {
        const Keyframe& a = keyframes[i];
        const Keyframe& b = keyframes[i + 1];
        if (time <= a.time && i == 0) return a.value;
        if (time >= b.time && i == keyframes.size() - 2) return b.value;
        if (time >= a.time && time <= b.time) {
            float span = b.time - a.time;
            if (span < 1e-8f) return b.value;
            float t = (time - a.time) / span;
            float w = eval_interp(a.interp, t);
            return a.value + (b.value - a.value) * (double)w;
        }
    }
    return keyframes.back().value;
}

void AnimationTrack::clear() {
    keyframes.clear();
}

AnimationTrack* AnimationClip::get_track(AnimProperty prop) {
    for (auto& t : tracks)
        if (t.property == prop) return &t;
    return nullptr;
}

AnimationTrack* AnimationClip::create_track(AnimProperty prop) {
    AnimationTrack* existing = get_track(prop);
    if (existing) return existing;
    tracks.push_back(AnimationTrack());
    tracks.back().property = prop;
    return &tracks.back();
}

void AnimationClip::add_keyframe(AnimProperty prop, float time, const Vec3& val,
                                 InterpolationType interp) {
    AnimationTrack* t = create_track(prop);
    t->add_keyframe(time, val, interp);
    update_duration();
}

Vec3 AnimationClip::evaluate(AnimProperty prop, float time) const {
    for (const auto& t : tracks)
        if (t.property == prop) return t.evaluate(time);
    return Vec3(0, 0, 0);
}

void AnimationClip::update_duration() {
    duration = 0.0f;
    for (const auto& t : tracks) {
        if (!t.keyframes.empty()) {
            float end = t.keyframes.back().time;
            if (end > duration) duration = end;
        }
    }
}

void AnimationClip::clear() {
    tracks.clear();
    duration = 0.0f;
}

Vec3 ProceduralModifier::evaluate(float time) const {
    if (!enabled) return Vec3(0, 0, 0);
    switch (type) {
        case ProceduralType::SineBob:
            return Vec3(0, std::sin(time * frequency * 6.28318f) * amplitude, 0);
        case ProceduralType::SpinRotate: {
            float angle = time * speed * 6.28318f;
            Vec3 n = unit_vector(axis);
            return Vec3(n.x * angle, n.y * angle, n.z * angle);
        }
        case ProceduralType::NoiseShake: {
            float t = time * frequency;
            return Vec3(
                std::sin(t * 12.9898f + 78.233f) * amplitude * 0.5f,
                std::sin(t * 4.1414f + 37.719f) * amplitude * 0.5f,
                std::sin(t * 9.233f + 11.135f) * amplitude * 0.5f
            );
        }
        default:
            return Vec3(0, 0, 0);
    }
}

void Animator::add_clip(const AnimTarget& target, const AnimationClip& clip) {
    ClipInstance ci;
    ci.target = target;
    ci.clip = clip;
    clips.push_back(ci);
    update_max_time();
}

void Animator::remove_clip(int index) {
    if (index >= 0 && index < (int)clips.size())
        clips.erase(clips.begin() + index);
}

void Animator::play() {
    global_playing = true;
    for (auto& c : clips) c.playing = true;
}

void Animator::pause() {
    global_playing = false;
    for (auto& c : clips) c.playing = false;
}

void Animator::stop() {
    global_playing = false;
    global_time = 0.0f;
    for (auto& c : clips) {
        c.playing = false;
        c.current_time = 0.0f;
    }
}

void Animator::seek(float time) {
    global_time = time;
    for (auto& c : clips) c.current_time = time;
}

void Animator::set_speed(float s) {
    global_speed = s;
}

void Animator::update(float dt) {
    if (!global_playing) return;
    float step = dt * global_speed;
    global_time += step;
    if (max_time > 0.0f) {
        bool any_loop = false;
        for (auto& c : clips) if (c.clip.loop) { any_loop = true; break; }
        if (any_loop) {
            while (global_time > max_time) global_time -= max_time;
        } else if (global_time > max_time) {
            global_time = max_time;
            pause();
        }
    }
    for (auto& c : clips) {
        c.current_time = global_time;
    }
}

void Animator::update_max_time() {
    max_time = 0.0f;
    for (auto& c : clips) {
        if (c.clip.duration > max_time) max_time = c.clip.duration;
    }
}

void Animator::clear() {
    clips.clear();
    global_playing = false;
    global_time = 0.0f;
    max_time = 0.0f;
}
