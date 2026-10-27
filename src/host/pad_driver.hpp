// What the window does with the pad's new presses (Stage I14, D-020): the menu button opens and closes the Escape menu, the D-pad, stick, Cross and Circle work it, and
// L1, R1 and Triangle flip the console switches. While the menu is open, and until its buttons are let go after it closes, the pad does not play the game.
#pragma once

#include <functional>
#include <string>

#include "controls.hpp"
#include "menu.hpp"
#include "pad_feedback.hpp"
#include "pads.hpp"

namespace host {

struct PadMenuHooks {
  std::function<bool()> isOpen;               // the menu is open
  std::function<void()> open;                 // open it (the window collects the choices it starts from)
  std::function<void(menu::Key)> press;       // a key for the open menu
};

// What goes out to the pad (Stage I15, D-021): the light bar colour and a rumble pulse for the game's events. The choices come from settings.ini (light_bar=, rumble=)
// and from the menu; both default to Off, so nothing is sent unless asked for.
class FeedbackDriver {
 public:
  explicit FeedbackDriver(const std::string& settingsFile);
  feedback::Light light() const { return light_; }
  feedback::Rumble rumble() const { return rumble_; }
  void set(feedback::Light light, feedback::Rumble rumble) { light_ = light; rumble_ = rumble; }
  // Once each time round the window loop; `pixels` is the latest picture of the game (for the maze colour), `rows` its height.
  void update(Pads& pads, feedback::Hub& hub, const std::uint32_t* pixels, int rows);

 private:
  feedback::Light light_ = feedback::Light::Off;
  feedback::Rumble rumble_ = feedback::Rumble::Off;
  background::Rgb maze_ = background::defaultMazeColour();
  bool lightSent_ = false;
  int turns_ = 0;
};

class PadDriver {
 public:
  // Once each time round the window loop. Returns what the pad is doing to the game now (nothing while it is held back).
  controls::PadState update(Pads& pads, controls::Keyboard& keyboard, const PadMenuHooks& menu, long long nowMs);

 private:
  bool held_ = false;
};

}  // namespace host
