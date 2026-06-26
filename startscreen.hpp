#ifndef STARTSCREEN_H
#define STARTSCREEN_H

#include "display.hpp"
#include "grid.hpp"
#include <string>
#include <vector>

enum class StartAction { NEW_SCENE, LOAD_SCENE, QUIT };

std::vector<std::string> list_saves();
StartAction show_start_screen(DisplayWin& display);
std::string show_load_screen(DisplayWin& display);

#endif
