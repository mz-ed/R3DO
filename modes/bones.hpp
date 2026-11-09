#ifndef BONES_MODE_H
#define BONES_MODE_H
#include "render_modes.hpp"

// Billboard-based view with the joint overlay (the original Bones mode).
void render_bones_mode(Grid& grid, Camera& cam, DisplayWin& display,
                       const Vec3& light_dir, int samples);

// Raytraced view with the same joint overlay (Bones RT): full lighting and
// materials with the chain drawn on top, so the rig can be inspected and
// its joints grabbed with the gizmo while editing in raytrace.
void render_bones_raytrace_mode(Grid& grid, Camera& cam, DisplayWin& display,
                                const Vec3& light_dir, int samples);

// Shared overlay pass: joints, links, tags, bound-vert tints for either
// bones render path (modes/bones.cpp draws it after the base render).
void draw_bones_overlay(Grid& grid, Camera& cam, DisplayWin& display);

#endif
