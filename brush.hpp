#ifndef BRUSH_H
#define BRUSH_H

#include "v3.hpp"
#include <cmath>
#include <string>

enum class BrushFalloff {
    Linear,
    Smoothstep,
    Sphere,
    Root,
    Constant
};

enum class BrushType {
    Grab,
    Draw,
    PushPull,
    Inflate,
    Deflate,
    Smooth,
    Flatten,
    Clay,
    Pinch,
    Crease,
    Blob
};

struct Brush {
    BrushType type = BrushType::Draw;
    BrushFalloff falloff = BrushFalloff::Smoothstep;
    float radius = 0.5f;
    float strength = 0.25f;
    float spacing = 0.05f;  // world units between stroke samples
    float autosmooth = 0.0f;
    bool mirror_x = false;
    bool mirror_y = false;
    bool mirror_z = false;
    bool invert = false;    // RMB behavior

    // Falloff weight at distance d from center (0 <= d <= radius)
    inline float weight(float d) const {
        if (d >= radius) return 0.0f;
        float t = d / radius; // 0..1
        float w = 0.0f;
        switch (falloff) {
        case BrushFalloff::Constant:
            w = 1.0f;
            break;
        case BrushFalloff::Linear:
            w = 1.0f - t;
            break;
        case BrushFalloff::Smoothstep:
            w = (1.0f - t * t * (3.0f - 2.0f * t));
            break;
        case BrushFalloff::Sphere:
            w = std::sqrt(std::max(0.0f, 1.0f - t * t));
            break;
        case BrushFalloff::Root:
            w = std::pow(1.0f - t, 0.5f);
            break;
        }
        return w;
    }

    static const char* type_name(BrushType t) {
        switch (t) {
        case BrushType::Grab: return "Grab";
        case BrushType::Draw: return "Draw";
        case BrushType::PushPull: return "Push/Pull";
        case BrushType::Inflate: return "Inflate";
        case BrushType::Deflate: return "Deflate";
        case BrushType::Smooth: return "Smooth";
        case BrushType::Flatten: return "Flatten";
        case BrushType::Clay: return "Clay";
        case BrushType::Pinch: return "Pinch";
        case BrushType::Crease: return "Crease";
        case BrushType::Blob: return "Blob";
        default: return "Draw";
        }
    }

    static const char* falloff_name(BrushFalloff f) {
        switch (f) {
        case BrushFalloff::Linear: return "Linear";
        case BrushFalloff::Smoothstep: return "Smoothstep";
        case BrushFalloff::Sphere: return "Sphere";
        case BrushFalloff::Root: return "Root";
        case BrushFalloff::Constant: return "Constant";
        default: return "Smoothstep";
        }
    }
};

#endif
