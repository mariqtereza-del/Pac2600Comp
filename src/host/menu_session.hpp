// The menu as the window thread and the game thread share it (Stage I5, T-I5-1). The window thread opens it, sends it keys and draws it; the game
// thread stops while it is open, tells it the game number after every frame and gives it the chance to add the Select and Reset presses of a
// "new game". The menu itself is touched by the window thread only; what crosses over are three atomic numbers. No SDL window code.
#pragma once

#include <atomic>
#include <cstdint>

#include "controls.hpp"
#include "menu.hpp"

namespace menu {

class Session {
 public:
  // allowStart = false while a recording is being replayed: the replay decides what the console is given, so no game may be started.
  explicit Session(bool allowStart) : allowStart_(allowStart) {}

  // ---- window thread ----
  bool isOpen() const { return open_.load(); }
  // What the menu cannot read from the game: the choices of the window (Stage I7 and I9.5).
  struct Window {
    background::Kind background = background::Kind::Black;
    framerate::Rate frameRate = framerate::Rate::Fps60;
    bool frameCounter = false;
    bool smoothAvailable = true;
    crt::Filter crt = crt::Filter::Off;
    bool hdr = true;
    int hdrStrength = 50;
    bool hdrWindows = false;
    int volume = 100;       // Stage I12
    bool muted = false;
    controls::Scheme keyboard = controls::Scheme::Both;   // Stage I13
    controls::PadFamily pad = controls::PadFamily::PlayStation;   // Stage I14
    bool padConnected = false;
    feedback::Rumble rumble = feedback::Rumble::Off;   // Stage I15
    achievements::Progress progress;                   // Stage I20
    stats::Totals totals;                              // Stage I21
  };
  void open(const controls::Input& switches, const Window& window = Window());  // the choices start where the game, the switches and the window are now
  Result key(Key key);                           // sends a key to the open menu; StartGame has been queued for the game thread when it returns
  const Menu& menu() const { return menu_; }

  // ---- game thread ----
  bool paused() const { return open_.load(); }
  void publish(const std::uint8_t* ram);         // after every frame: the 128 bytes of the game's memory
  void apply(controls::Input& in);               // before every frame: adds the presses of a new game, if one is under way

 private:
  Menu menu_;
  bool allowStart_;
  std::atomic<bool> open_{false};
  std::atomic<int> gameSelect_{6}, pendingPresses_{-1};
  GameStarter starter_;
};

}  // namespace menu
