// Keeping track of the window: its place, size and fullscreen state, and Alt+Enter.
// (Stage G4.7: moved out of main.cpp, the code is unchanged.)
#include "window_tracker.hpp"

namespace host {

// ---- Window state and fullscreen (Stage D7, DECISIONS.md T-D7-1) ----

// The monitors' usable rectangles (without the taskbar), the first being the primary one.
std::vector<windowstate::Area> monitorAreas()
{
  std::vector<windowstate::Area> areas;
  int count = 0;
  SDL_DisplayID* ids = SDL_GetDisplays(&count);
  const SDL_DisplayID primary = SDL_GetPrimaryDisplay();
  auto add = [&](SDL_DisplayID id) {
    SDL_Rect r{};
    if(SDL_GetDisplayUsableBounds(id, &r) || SDL_GetDisplayBounds(id, &r)) areas.push_back({r.x, r.y, r.w, r.h});
  };
  if(primary) add(primary);
  for(int i = 0; ids && i < count; ++i)
    if(ids[i] != primary) add(ids[i]);
  SDL_free(ids);
  return areas;
}

namespace {

bool windowIsFree(SDL_Window* window)  // neither fullscreen, maximized nor minimized: its rectangle is the "windowed" one
{
  const SDL_WindowFlags f = SDL_GetWindowFlags(window);
  return (f & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_MAXIMIZED | SDL_WINDOW_MINIMIZED)) == 0;
}

}  // namespace

void rememberWindowedRect(SDL_Window* window, Tracker& t)
{
  if(!windowIsFree(window)) return;
  windowstate::Area a;
  SDL_GetWindowPosition(window, &a.x, &a.y);
  SDL_GetWindowSize(window, &a.w, &a.h);
  if(a.w < windowstate::kMinW || a.h < windowstate::kMinH) return;
  t.state.area = a;
  t.state.known = true;
}

void setFullscreen(SDL_Window* window, Tracker& t, bool on)
{
  if(on) rememberWindowedRect(window, t);  // before the window jumps to the monitor's size
  t.state.fullscreen = on;
  SDL_SetWindowFullscreenMode(window, nullptr);  // no mode: borderless fullscreen at the desktop's own size
  SDL_SetWindowFullscreen(window, on);
  if(on) SDL_HideCursor(); else SDL_ShowCursor();
}

// Events about the window itself, and Alt+Enter; shared by the game window and the checks. True if the event was used.
bool handleWindowEvent(SDL_Window* window, Tracker& t, const SDL_Event& e)
{
  switch(e.type) {
    case SDL_EVENT_WINDOW_MOVED:
    case SDL_EVENT_WINDOW_RESIZED:
    case SDL_EVENT_WINDOW_RESTORED:
      rememberWindowedRect(window, t);
      return true;
    case SDL_EVENT_KEY_DOWN:
      if(!e.key.repeat && (e.key.key == SDLK_RETURN || e.key.key == SDLK_KP_ENTER) && (e.key.mod & SDL_KMOD_ALT)) {
        setFullscreen(window, t, !t.state.fullscreen);
        return true;
      }
      return false;
    default:
      return false;
  }
}

// A made-up Alt+Enter, handled by the same code as the real key.
SDL_Event altEnter()
{
  SDL_Event e{};
  e.type = SDL_EVENT_KEY_DOWN;
  e.key.key = SDLK_RETURN;
  e.key.mod = SDL_KMOD_LALT;
  return e;
}

// Lets the window system finish (fullscreen changes are asynchronous) and feeds the events to the tracker.
void settle(SDL_Window* window, Tracker& t, int milliseconds)
{
  SDL_SyncWindow(window);
  for(int waited = 0; waited < milliseconds; waited += 10) {
    for(SDL_Event e; SDL_PollEvent(&e); ) handleWindowEvent(window, t, e);
    SDL_Delay(10);
  }
  SDL_SyncWindow(window);
}

std::string where(int w, int h, int x, int y)
{
  return std::to_string(w) + "x" + std::to_string(h) + " at " + std::to_string(x) + "," + std::to_string(y);
}

}  // namespace host
