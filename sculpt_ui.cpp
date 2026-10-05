#include "sculpt_ui.hpp"

void SculptUIState::reset() {
    active = false;
    selected = nullptr;
    is_sculpting = false;
    brush.radius = 0.3f;
    brush.strength = 0.2f;
    brush.type = BrushType::Draw;
    brush.mirror_x = brush.mirror_y = brush.mirror_z = false;
    base_shape = 0;
    base_radius = 0.4f;
    base_size = 0.6f;
    base_segs = 24;
    base_rings = 16;
}
