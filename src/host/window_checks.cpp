// The fullscreen check and the state report: the real window put through its states.
// (Stage G4.7: moved out of main.cpp, the code is unchanged.)
#include "window_checks.hpp"
#include "options.hpp"
#include "screen.hpp"
#include "window_tracker.hpp"

namespace host {

// --fullscreen-check: the real window goes to an odd size and place, into fullscreen (as Alt+Enter does), is checked there,
// comes back, and goes into fullscreen again; the state is saved as the program does when it closes.
bool fullscreenCheck(SDL_Window* window, SDL_Renderer* renderer, const Options& o, const std::string& romPath)
{
  std::ofstream report(o.fullscreenCheckFile);
  if(!report) return false;
  bool ok = true;
  auto line = [&](const char* name, bool pass, const std::string& detail) {
    report << name << (pass ? ": ok  " : ": FAIL  ") << detail << '\n';
    ok = ok && pass;
  };
  Tracker t;

  constexpr int oddX = 123, oddY = 87, oddW = 1011, oddH = 757;
  SDL_SetWindowSize(window, oddW, oddH);
  SDL_SetWindowPosition(window, oddX, oddY);
  settle(window, t);
  int x = 0, y = 0, w = 0, h = 0;
  SDL_GetWindowPosition(window, &x, &y);
  SDL_GetWindowSize(window, &w, &h);
  const int winX = x, winY = y, winW = w, winH = h;  // what Windows really gave (it may adjust a little)
  line("odd windowed size and place", w == oddW && h == oddH && std::abs(x - oddX) <= 8 && std::abs(y - oddY) <= 40, where(w, h, x, y));
  rememberWindowedRect(window, t);

  const SDL_Event first = altEnter();
  handleWindowEvent(window, t, first);
  settle(window, t);
  SDL_Rect bounds{};
  SDL_GetDisplayBounds(SDL_GetDisplayForWindow(window), &bounds);
  int pw = 0, ph = 0;
  SDL_GetWindowSizeInPixels(window, &pw, &ph);
  SDL_GetWindowPosition(window, &x, &y);
  const bool fullFlag = (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) != 0;
  line("Alt+Enter enters fullscreen", fullFlag && t.state.fullscreen, fullFlag ? "window flag set" : "window flag not set");
  line("borderless (no video mode change)", SDL_GetWindowFullscreenMode(window) == nullptr, "fullscreen mode = desktop");
  line("covers the whole monitor", x == bounds.x && y == bounds.y && pw == bounds.w && ph == bounds.h,
       where(pw, ph, x, y) + "; monitor " + where(bounds.w, bounds.h, bounds.x, bounds.y));
  line("windowed place remembered while fullscreen", t.state.area.w == winW && t.state.area.h == winH && t.state.area.x == winX && t.state.area.y == winY,
       where(t.state.area.w, t.state.area.h, t.state.area.x, t.state.area.y));

  View view;
  view.mode = o.mode;
  view.shape = o.pixelShape;
  const bool pictureOk = layoutCheck(renderer, o.fullscreenCheckFile + ".fullscreen.png", romPath, view, o.resetAt);
  line("picture in fullscreen (D6 check, details in .fullscreen.png.txt)", pictureOk, "");

  const SDL_Event second = altEnter();
  handleWindowEvent(window, t, second);
  settle(window, t);
  SDL_GetWindowPosition(window, &x, &y);
  SDL_GetWindowSize(window, &w, &h);
  line("Alt+Enter leaves fullscreen", (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) == 0 && !t.state.fullscreen, "");
  line("the odd size and place came back", w == winW && h == winH && x == winX && y == winY, where(w, h, x, y));

  const SDL_Event third = altEnter();
  handleWindowEvent(window, t, third);
  settle(window, t);
  line("fullscreen again", t.state.fullscreen && (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) != 0, "");
  line("state saved", windowstate::save(o.settingsFile, t.state),
       "window " + where(t.state.area.w, t.state.area.h, t.state.area.x, t.state.area.y) + ", fullscreen " + (t.state.fullscreen ? "on" : "off"));
  report << "result=" << (ok ? "OK" : "FAIL") << '\n';
  return ok;
}

// --state-report: what the window looks like right after start-up from the saved state, then after leaving fullscreen.
bool stateReport(SDL_Window* window, const Options& o, const windowstate::State& wanted)
{
  std::ofstream report(o.stateReportFile);
  if(!report) return false;
  Tracker t;
  t.state = wanted;
  settle(window, t);
  int x = 0, y = 0, w = 0, h = 0;
  SDL_GetWindowPosition(window, &x, &y);
  SDL_GetWindowSize(window, &w, &h);
  const bool full = (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) != 0;
  report << "saved_state_found=" << (wanted.known ? "yes" : "no") << "\nsaved=" << where(wanted.area.w, wanted.area.h, wanted.area.x, wanted.area.y)
         << " fullscreen " << (wanted.fullscreen ? "on" : "off") << "\nstarted_fullscreen=" << (full ? "yes" : "no")
         << "\nstarted_window=" << where(w, h, x, y) << '\n';
  bool ok = full == wanted.fullscreen;
  if(full) {
    const SDL_Event leave = altEnter();
    handleWindowEvent(window, t, leave);
    settle(window, t);
    SDL_GetWindowPosition(window, &x, &y);
    SDL_GetWindowSize(window, &w, &h);
    report << "after_leaving_fullscreen=" << where(w, h, x, y) << '\n';
  }
  if(wanted.known) ok = ok && w == wanted.area.w && h == wanted.area.h && x == wanted.area.x && y == wanted.area.y;
  else ok = !full && w == kWindowW && h == kWindowH;  // nothing remembered: the program's own first window
  report << "result=" << (ok ? "OK" : "FAIL") << '\n';
  return ok;
}

}  // namespace host
