// Test only (Stages I5 and D10): a helper thread that presses keys in the real window's event queue, as the operating system would (--push-keys, --push-menu).
// (Stage I9.4: cut out of play_window.cpp, the code is unchanged.)
#include "key_pusher.hpp"

namespace host {

std::thread startKeyPusher(const Options& options, const std::atomic<bool>& stop)
{
  if(!(options.pushKeys || options.pushMenu)) return {};
  return std::thread([&options, &stop]() {
      auto key = [](SDL_Scancode code, bool down) {
        SDL_Event e{};
        e.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
        e.key.scancode = code;
        e.key.key = SDL_GetKeyFromScancode(code, SDL_KMOD_NONE, false);
        e.key.down = down;
        SDL_PushEvent(&e);
      };
      struct Step { double at; SDL_Scancode code; bool down; };
      std::vector<Step> plan = {
          {0.5, SDL_SCANCODE_F3, true}, {0.8, SDL_SCANCODE_F3, false},          // Game Reset: a game starts
          {4.0, SDL_SCANCODE_RIGHT, true}, {4.6, SDL_SCANCODE_RIGHT, false},    // joystick right (Pac-Man starts moving at about 2 s)
          {5.0, SDL_SCANCODE_A, true}, {5.6, SDL_SCANCODE_A, false},            // left, with the letter key
          {6.0, SDL_SCANCODE_UP, true}, {6.6, SDL_SCANCODE_UP, false},          // up
          {7.0, SDL_SCANCODE_S, true}, {7.6, SDL_SCANCODE_S, false},            // down, with the letter key
          {8.0, SDL_SCANCODE_F5, true}, {8.1, SDL_SCANCODE_F5, false},          // left difficulty to A
          {8.5, SDL_SCANCODE_F6, true}, {8.6, SDL_SCANCODE_F6, false},          // right difficulty to A
          {9.0, SDL_SCANCODE_F4, true}, {9.1, SDL_SCANCODE_F4, false},          // Colour to B&W
          {9.5, SDL_SCANCODE_F7, true}, {9.6, SDL_SCANCODE_F7, false}};         // soft glow on
      if(options.pushMenu) {  // Stage I5 (then I7): open the menu, choose game 3 and the left difficulty A, start it; open and close it again; quit from it
        plan.clear();
        auto tap = [&](double at, SDL_Scancode code) { plan.push_back({at, code, true}); plan.push_back({at + 0.05, code, false}); };
        tap(1.0, SDL_SCANCODE_ESCAPE);
        tap(1.5, SDL_SCANCODE_DOWN); tap(1.6, SDL_SCANCODE_DOWN); tap(1.65, SDL_SCANCODE_RETURN);  // open the Game options page (Stage I8), on the Game row
        tap(1.7, SDL_SCANCODE_RIGHT); tap(1.8, SDL_SCANCODE_RIGHT);  // game 3
        tap(1.85, SDL_SCANCODE_DOWN);                                // the Players row stays at one player (Stage I6)
        tap(1.9, SDL_SCANCODE_DOWN); tap(2.0, SDL_SCANCODE_RIGHT);  // left difficulty to A
        tap(2.05, SDL_SCANCODE_DOWN); tap(2.1, SDL_SCANCODE_DOWN); tap(2.15, SDL_SCANCODE_RIGHT);  // Stage I9.5: the frame rate one step on (75; 30 in the reference program)
        tap(2.2, SDL_SCANCODE_DOWN); tap(2.25, SDL_SCANCODE_RIGHT);                                // and the frame counter on
        tap(2.27, SDL_SCANCODE_ESCAPE); tap(2.29, SDL_SCANCODE_DOWN); tap(2.31, SDL_SCANCODE_RETURN);   // Stage I10: to the Graphics options page
        tap(2.33, SDL_SCANCODE_DOWN); tap(2.34, SDL_SCANCODE_DOWN); tap(2.36, SDL_SCANCODE_DOWN); tap(2.37, SDL_SCANCODE_RIGHT);   // past the TV type and the CRT filter, HDR off
        tap(2.39, SDL_SCANCODE_DOWN); tap(2.41, SDL_SCANCODE_RIGHT); tap(2.43, SDL_SCANCODE_RIGHT);    // and the highlights 10 more (60)
        tap(2.45, SDL_SCANCODE_ESCAPE); tap(2.5, SDL_SCANCODE_UP); tap(2.55, SDL_SCANCODE_UP);   // back to the main page (on Game options), up to New game
        tap(3.0, SDL_SCANCODE_RETURN);                               // go
        tap(6.0, SDL_SCANCODE_ESCAPE); tap(6.5, SDL_SCANCODE_ESCAPE);  // open and close again
        tap(6.6, SDL_SCANCODE_ESCAPE);                                // Stage I7: open the menu, go to Graphics options, choose the Mirrored edge, back, close
        for(int i = 0; i < 3; ++i) tap(6.7 + 0.1 * i, SDL_SCANCODE_DOWN);
        tap(7.4, SDL_SCANCODE_RETURN); tap(7.5, SDL_SCANCODE_RIGHT); tap(7.6, SDL_SCANCODE_RIGHT);
        tap(7.7, SDL_SCANCODE_ESCAPE); tap(7.8, SDL_SCANCODE_ESCAPE);
        tap(8.2, SDL_SCANCODE_ESCAPE);   // Stage I12: the Sound options page (4 rows down), the volume two steps down (90), Mute on, back to the main page
        for(int i = 0; i < 4; ++i) tap(8.25 + 0.02 * i, SDL_SCANCODE_DOWN);
        tap(8.33, SDL_SCANCODE_RETURN); tap(8.35, SDL_SCANCODE_LEFT); tap(8.37, SDL_SCANCODE_LEFT);
        tap(8.39, SDL_SCANCODE_DOWN); tap(8.41, SDL_SCANCODE_RIGHT); tap(8.43, SDL_SCANCODE_ESCAPE);
        tap(8.5, SDL_SCANCODE_DOWN); tap(8.6, SDL_SCANCODE_RETURN);  // Quit
      }
      const Uint64 t0 = SDL_GetTicksNS();
      for(const auto& step : plan) {
        while(!stop.load() && static_cast<double>(SDL_GetTicksNS() - t0) < step.at * 1e9) SDL_Delay(5);
        if(stop.load()) break;
        key(step.code, step.down);
      }
  });
}

}  // namespace host
