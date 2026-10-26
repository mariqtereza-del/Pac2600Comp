// The fullscreen check and the state report: the real window put through its states.
// (Stage G4.7: moved out of main.cpp, the code is unchanged.)
#pragma once
#include "common.hpp"
#include "options.hpp"

namespace host {

// --fullscreen-check: the real window goes to an odd size and place, into fullscreen (as Alt+Enter does), is checked there,
// comes back, and goes into fullscreen again; the state is saved as the program does when it closes.
bool fullscreenCheck(SDL_Window* window, SDL_Renderer* renderer, const Options& o, const std::string& romPath);

// --state-report: what the window looks like right after start-up from the saved state, then after leaving fullscreen.
bool stateReport(SDL_Window* window, const Options& o, const windowstate::State& wanted);

}  // namespace host
