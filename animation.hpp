#ifndef ANIMATION_H
#define ANIMATION_H

#include "v3.hpp"
#include <vector>
#include <string>
#include <map>
#include <cmath>

enum class InterpolationType {
    Linear,
    EaseIn,
    EaseOut,
    EaseInOut,
    Step
};

enum class AnimProperty {
    Position,
    Rotation,
    Scale,
    Color
};

struct Keyframe {
    float time = 0.0f;
    Vec3 value;
    Vec3 in_tangent;
    Vec3 out_tangent;
    InterpolationType interp = InterpolationType::EaseInOut;
};

class AnimationTrack {
public:
    AnimProperty property;
    std::vector<Keyframe> keyframes;

    void add_keyframe(float time, const Vec3& val,
                      InterpolationType interp = InterpolationType::EaseInOut);
    Vec3 evaluate(float time) const;
    void clear();
    bool empty() const { return keyframes.empty(); }
};

class AnimationClip {
public:
    std::string name;
    float duration = 0.0f;
    bool loop = false;
    std::vector<AnimationTrack> tracks;

    AnimationTrack* get_track(AnimProperty prop);
    AnimationTrack* create_track(AnimProperty prop);
    void add_keyframe(AnimProperty prop, float time, const Vec3& val,
                      InterpolationType interp = InterpolationType::EaseInOut);
    Vec3 evaluate(AnimProperty prop, float time) const;
    void update_duration();
    void clear();
};

struct AnimTarget {
    enum Type { NONE, CELL, FREE_OBJECT, CAMERA };
    Type type = NONE;
    int i = -1, j = -1, k = -1;
    int free_idx = -1;
};

enum class ProceduralType {
    ProceduralNone,
    SineBob,
    SpinRotate,
    NoiseShake
};

struct ProceduralModifier {
    ProceduralType type = ProceduralType::ProceduralNone;
    float amplitude = 0.5f;
    float frequency = 1.0f;
    float speed = 1.0f;
    Vec3 axis = Vec3(0, 1, 0);
    bool enabled = false;

    Vec3 evaluate(float time) const;
};

class Animator {
public:
    struct ClipInstance {
        AnimTarget target;
        AnimationClip clip;
        Vec3 base_pos{0, 0, 0};   // target's base position when the target was picked
        float current_time = 0.0f;
        bool playing = false;
        float speed = 1.0f;
        float weight = 1.0f;
    };

    std::vector<ClipInstance> clips;
    bool global_playing = false;
    float global_speed = 1.0f;
    float global_time = 0.0f;
    float max_time = 0.0f;

    void add_clip(const AnimTarget& target, const AnimationClip& clip);
    void remove_clip(int index);
    void play();
    void pause();
    void stop();
    void seek(float time);
    void set_speed(float s);
    void update(float dt);
    void update_max_time();
    void clear();
};

#endif
