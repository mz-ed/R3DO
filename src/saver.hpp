#ifndef SAVER_H
#define SAVER_H

#include "grid.hpp"
#include "camera.hpp"
#include <string>

class Animator;

void save_scene(const Grid& grid, const Camera& cam, const std::string& filename,
                int render_mode = 1, int palette_idx = 0,
                const Animator* anim = nullptr);
bool load_scene(const std::string& filename, Grid& grid, Camera* cam = nullptr,
                int* render_mode = nullptr, int* palette_idx = nullptr,
                Animator* anim = nullptr);

// In-memory variants power undo/redo snapshots. These never carry animation
// clips: undoing a move should not wipe animation work, so pass no animator.
std::string serialize_scene(const Grid& grid, const Camera& cam,
                            int render_mode, int palette_idx,
                            const Animator* anim = nullptr,
                            const std::string& filename = "");
bool deserialize_scene(const std::string& data, Grid& grid, Camera* cam,
                       int* render_mode = nullptr, int* palette_idx = nullptr,
                       Animator* anim = nullptr);

#endif
