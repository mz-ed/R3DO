#ifndef SAVER_H
#define SAVER_H

#include "grid.hpp"
#include <string>

void save_scene(const Grid& grid, const std::string& filename);
void load_scene(const std::string& filename, Grid& grid);

#endif
