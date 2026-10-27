// What the window does with the pad's new presses (Stage I14, D-020): the menu button opens and closes the Escape menu, the D-pad, stick, Cross and Circle work it, and
// L1, R1 and Triangle flip the console switches. While the menu is open, and until its buttons are let go after it closes, the pad does not play the game.
#pragma once

#include <functional>

#include "controls.hpp"
#include "menu.hpp"
#include "pads.hpp"

namespace host {

struct PadMenuHooks {
  std::function<bool()> isOpen;               // the menu is open
  std::function<void()> open;                 // open it (the window collects the choices it starts from)
  std::function<void(menu::Key)> press;       // a key for the open menu
};

class PadDriver {
 public:
  // Once each time round the window loop. Returns what the pad is doing to the game now (nothing while it is held back).
  controls::PadState update(Pads& pads, controls::Keyboard& keyboard, const PadMenuHooks& menu, long long nowMs);

 private:
  bool held_ = false;
};

}  // namespace host
