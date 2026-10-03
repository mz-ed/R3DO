#ifndef SCULPT_UI_H
#define SCULPT_UI_H

#include "brush.hpp"
#include "sculpt_mesh.hpp"
#include "sculpt_engine.hpp"
#include <string>

struct SculptUIState {
    bool active = false;
    Brush brush;
    int base_shape = 0; // 0 sphere 1 cube 2 plane 3 torus
    float base_radius = 0.4f;
    float base_size = 0.6f;
    int base_segs = 24;
    int base_rings = 16;
    bool is_sculpting = false;
    SculptMesh* selected = nullptr;
    std::string export_path = "sculpt_export.obj";
    std::string sculpt_save_path = "saves/sculpt_test.sculpt";
};

#endif
