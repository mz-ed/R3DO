#ifndef OVERHEAD_TERRAIN_H
#define OVERHEAD_TERRAIN_H

#include "grid.hpp"
#include "display.hpp"
#include "view.hpp"

// Shaded-relief heightmap tiles for the Overhead map. Sampling is cached
// per terrain version (bumped by HeightField::paint) and re-projected each
// frame through the view transform.
void overhead_draw_terrain(DisplayWin& display, const Grid& grid,
                           const OverScreenMap& map);

#endif
