// What the program remembers about its window between runs (Stage D7, DECISIONS.md T-D7-1). No window code.
#pragma once

#include <string>
#include <vector>

namespace windowstate {

struct Area {
  int x = 0, y = 0, w = 0, h = 0;
};

// The windowed rectangle (the client area, as SDL reports it) and whether the game was in fullscreen.
struct State {
  bool known = false;  // false = nothing was remembered: use the program's defaults
  Area area;
  bool fullscreen = false;
};

constexpr int kMinW = 320, kMinH = 240;  // smaller than the picture's 160 x 228 doubled would only be a curiosity

// Reads the state from settings.ini (keys window_x, window_y, window_w, window_h, fullscreen).
// Anything missing, not a number or not sensible gives known == false.
State load(const std::string& settingsFile);
bool save(const std::string& settingsFile, const State& state);

// Makes a remembered window safe to show: not larger than the biggest monitor, not tiny, and with its title bar
// on a monitor. A window that is no longer on any monitor (a screen was unplugged) is centred on the first monitor.
// `monitors` are the usable monitor rectangles; with none, the state is returned unchanged.
State fitToMonitors(State state, const std::vector<Area>& monitors);

}  // namespace windowstate
