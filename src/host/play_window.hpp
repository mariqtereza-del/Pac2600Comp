// Playing the game in the window.
// (Stage G4.7: moved out of main.cpp, the code is unchanged.)
#pragma once
#include "common.hpp"
#include "options.hpp"
#include "window_tracker.hpp"

namespace host {

// The game window: one emulator frame per 1/60 s from a simple clock (temporary; real pacing is Stage D9).
// With maxSeconds > 0 it stops by itself after that long and returns the frames per second it managed (for tests).
// Keys 1, 2, 3, 4 choose Sharp, Fill, TV frame, Game area; Alt+Enter toggles fullscreen (temporary; Stage D10 gives the keys their final home).
double playWindow(SDL_Window* window, SDL_Renderer*& renderer, const std::string& romPath, double maxSeconds, View view, Tracker& tracker, const Options& options);

}  // namespace host
