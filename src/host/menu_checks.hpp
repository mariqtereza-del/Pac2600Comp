// Drawing the Escape menu and the two test modes that go with it (Stage I5, T-I5-1).
#pragma once
#include "common.hpp"
#include "menu.hpp"
#include "options.hpp"
#include "screen.hpp"

namespace host {

// The menu, drawn over whatever is in the window: the picture dimmed, a box with the rows and the explanation of the marked row.
void drawMenu(SDL_Renderer* renderer, const menu::Menu& menu);

// --menu-shot: the real window shows the game for 300 frames, the menu is opened on the "Game" row, drawn, and the window's pixels are saved as a PNG.
bool menuShot(SDL_Renderer* renderer, const std::string& pngPath, const std::string& romPath);

// --backdrop-check (Stage I7): every background in the real window, at the window's size; saves one PNG per background (<prefix>_<name>.png) and a report.
bool backdropCheck(SDL_Renderer* renderer, const std::string& pngPrefix, const std::string& romPath);

// --smooth-shot (Stage I9.4): the smooth picture at three moments of three steps in the real window; saves nine PNGs (<prefix>_s<step>_p<moment>.png) and a report.
bool smoothShot(SDL_Renderer* renderer, const std::string& pngPrefix, const std::string& romPath, const View& view);

// --menu-check: no window; plays the menu and the game together for three starting states and all eight games (menu_check.cpp). Exit 0 = all right.
int menuCheck(const Options& o);

}  // namespace host
