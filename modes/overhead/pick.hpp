#ifndef OVERHEAD_PICK_H
#define OVERHEAD_PICK_H

#include "grid.hpp"
#include "display.hpp"
#include "hittable.hpp"

// Nearest object under a map click: free-object markers within their bound
// radius, else the top-most visible occupant of the clicked cell column.
// Returns nullptr on empty map space.
Hittable* overhead_pick(DisplayWin& display, const Grid& grid, int mx, int my);

#endif
