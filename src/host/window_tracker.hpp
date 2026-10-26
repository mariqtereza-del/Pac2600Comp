// Keeping track of the window: its place, size and fullscreen state, and Alt+Enter.
// (Stage G4.7: moved out of main.cpp, the code is unchanged.)
#pragma once
#include "common.hpp"

namespace host {

// ---- Window state and fullscreen (Stage D7, DECISIONS.md T-D7-1) ----
// The monitors' usable rectangles (without the taskbar), the first being the primary one.
std::vector<windowstate::Area> monitorAreas();

// What the program knows about its window: the rectangle it has when not fullscreen, and whether it is fullscreen.
struct Tracker {
  windowstate::State state;  // state.known becomes true once a windowed rectangle has been seen
};

void rememberWindowedRect(SDL_Window* window, Tracker& t);

void setFullscreen(SDL_Window* window, Tracker& t, bool on);

// Events about the window itself, and Alt+Enter; shared by the game window and the checks. True if the event was used.
bool handleWindowEvent(SDL_Window* window, Tracker& t, const SDL_Event& e);

// A made-up Alt+Enter, handled by the same code as the real key.
SDL_Event altEnter();

// Lets the window system finish (fullscreen changes are asynchronous) and feeds the events to the tracker.
void settle(SDL_Window* window, Tracker& t, int milliseconds = 400);

std::string where(int w, int h, int x, int y);

}  // namespace host
