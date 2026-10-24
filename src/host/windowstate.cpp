#include "windowstate.hpp"

#include <algorithm>
#include <cstdlib>
#include <string>

#include "settings.hpp"

namespace windowstate {

namespace {

bool toInt(const std::string& text, int& out)
{
  if(text.empty()) return false;
  char* end = nullptr;
  const long v = std::strtol(text.c_str(), &end, 10);
  if(*end != '\0' || v < -100000 || v > 100000) return false;
  out = static_cast<int>(v);
  return true;
}

}  // namespace

State load(const std::string& settingsFile)
{
  State s;
  int fs = 0;
  if(!toInt(settings::get(settingsFile, "window_x"), s.area.x) || !toInt(settings::get(settingsFile, "window_y"), s.area.y)
     || !toInt(settings::get(settingsFile, "window_w"), s.area.w) || !toInt(settings::get(settingsFile, "window_h"), s.area.h))
    return {};
  if(s.area.w < kMinW || s.area.h < kMinH) return {};
  if(toInt(settings::get(settingsFile, "fullscreen"), fs)) s.fullscreen = fs != 0;
  s.known = true;
  return s;
}

bool save(const std::string& settingsFile, const State& state)
{
  return settings::set(settingsFile, "window_x", std::to_string(state.area.x))
         && settings::set(settingsFile, "window_y", std::to_string(state.area.y))
         && settings::set(settingsFile, "window_w", std::to_string(state.area.w))
         && settings::set(settingsFile, "window_h", std::to_string(state.area.h))
         && settings::set(settingsFile, "fullscreen", state.fullscreen ? "1" : "0");
}

State fitToMonitors(State state, const std::vector<Area>& monitors)
{
  if(!state.known || monitors.empty()) return state;

  int biggestW = 0, biggestH = 0;
  for(const Area& m : monitors) { biggestW = std::max(biggestW, m.w); biggestH = std::max(biggestH, m.h); }
  state.area.w = std::clamp(state.area.w, kMinW, std::max(kMinW, biggestW));
  state.area.h = std::clamp(state.area.h, kMinH, std::max(kMinH, biggestH));

  // The title bar (about 30 pixels) is just above the client area, so the middle of the top edge must lie at least
  // 24 pixels below the top of a monitor (title bar visible) and at least 40 above its bottom (room to grab it).
  const int gripX = state.area.x + state.area.w / 2, gripY = state.area.y;
  for(const Area& m : monitors)
    if(gripX >= m.x && gripX < m.x + m.w && gripY >= m.y + 24 && gripY < m.y + m.h - 40) return state;

  const Area& first = monitors.front();
  state.area.x = first.x + (first.w - state.area.w) / 2;
  state.area.y = first.y + (first.h - state.area.h) / 2;
  return state;
}

}  // namespace windowstate
