// Drawing text with the PlayStation button symbols in it (Stage I14, D-020). SDL's built-in text has no such shapes, so each symbol character
// (pad_layout.hpp) is drawn as a small picture in its own 8 x 8 cell, in the colours the buttons have on a DualShock 4.
#pragma once

#include <SDL3/SDL.h>

#include <string>

namespace host {

// Draws `text` at (x, y) in the renderer's current draw colour, as SDL_RenderDebugText does, with the symbols as pictures. The draw colour is the same when it returns.
void drawSymbolText(SDL_Renderer* renderer, float x, float y, const std::string& text);

}  // namespace host
