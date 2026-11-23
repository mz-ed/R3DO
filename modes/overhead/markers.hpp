#ifndef OVERHEAD_MARKERS_H
#define OVERHEAD_MARKERS_H

#include "grid.hpp"
#include "display.hpp"
#include "view.hpp"

// Cell-occupant squares, free-object circles sized by their real XZ bounds
// (with facing arrow), and the camera marker for the Overhead map.
void overhead_draw_markers(DisplayWin& display, const Grid& grid,
                           const OverScreenMap& map);

#endif
