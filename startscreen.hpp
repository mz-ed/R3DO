#ifndef STARTSCREEN_H
#define STARTSCREEN_H

#include "display.hpp"
#include "grid.hpp"
#include "settings.hpp"
#include <string>
#include <vector>

enum class StartAction { NEW_SCENE, LOAD_SCENE, QUIT };

std::vector<std::string> list_saves();
StartAction show_start_screen(DisplayWin& display, Settings& settings);
std::string show_load_screen(DisplayWin& display);
void show_settings_screen(DisplayWin& display, Settings& settings);

#endif
