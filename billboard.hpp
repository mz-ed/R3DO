#ifndef BILLBOARD_H
#define BILLBOARD_H

#include "grid.hpp"
#include "camera.hpp"
#include "display.hpp"

void render_billboard(Grid& grid, Camera& cam, DisplayWin& display, const Vec3& light_dir);

#endif
