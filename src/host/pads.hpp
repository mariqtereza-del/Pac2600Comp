// The game controllers (keyboard keys and gamepads) as the console inputs, and the pad check.
// (Stage G4.7: moved out of main.cpp, the code is unchanged.)
#pragma once
#include "common.hpp"
#include "pad_feedback.hpp"
#include "pad_layout.hpp"
#include "options.hpp"

namespace host {

// ---- Keyboard (Stage D10, DECISIONS.md T-D10-1) ----
// Gives the console what the keyboard adds up to. extraReset is for tests that press Reset by frame number.
// sharedControls (Stage I6, D-011, T-I6-1): in a two-player game, while it is player 2's turn, the directions go to the console's right joystick,
// which is the one the game reads for player 2, so both players use the same keys or pad and take turns. Off, the input always goes to the left joystick.
void applyInput(Emulator& emulator, const controls::Input& in, bool extraReset, bool sharedControls = false);
// True when the game's memory says a two-player game is on ($E6 bit 6) and it is player 2's turn (bit 7).
bool playerTwoToPlay(const std::uint8_t* ram);

// The virtual pad of the tests (--pad-check, --push-pad): a DualShock 4, or with --pad-model dualsense a DualSense. Sony's vendor and product numbers make SDL take it for the
// real thing (Stage I17).
struct VirtualPadModel {
  Uint16 vendor = 0x054c, product = 0x09cc;
  const char* name = "Test DualShock 4 (virtual)";
  controls::PadFamily family = controls::PadFamily::PlayStation;
};
inline VirtualPadModel virtualPadModel(const std::string& model)
{
  VirtualPadModel m;
  if(model == "dualsense") { m.product = 0x0ce6; m.name = "Test DualSense (virtual)"; m.family = controls::PadFamily::DualSense; }
  return m;
}

// ---- Gamepads (Stage D11, DECISIONS.md T-D11-1) ----
// All the gamepads SDL knows, opened when they appear and closed when they go. What they are doing is read from them
// each time round the loop (not remembered from events), so an unplugged pad can never leave a button "held".
class Pads {
public:
  Pads() = default;
  ~Pads() { stop(); }
  Pads(const Pads&) = delete;
  Pads& operator=(const Pads&) = delete;

  bool start()
  {
    if(started_) return true;
    if(!SDL_InitSubSystem(SDL_INIT_GAMEPAD)) { problem_ = SDL_GetError(); return false; }
    started_ = true;
    int count = 0;
    SDL_JoystickID* ids = SDL_GetGamepads(&count);
    for(int i = 0; ids && i < count; ++i) open(ids[i]);
    SDL_free(ids);
    return true;
  }

  void stop()
  {
    if(lightUsed_) setLight(feedback::defaultLightColour());   // the light bar goes back to the pad's usual colour
    for(Pad& p : pads_) SDL_CloseGamepad(p.gamepad);
    pads_.clear();
    if(started_) { SDL_QuitSubSystem(SDL_INIT_GAMEPAD); started_ = false; }
  }

  void handleEvent(const SDL_Event& e)
  {
    if(e.type == SDL_EVENT_GAMEPAD_ADDED) open(e.gdevice.which);
    if(e.type == SDL_EVENT_GAMEPAD_REMOVED) close(e.gdevice.which);
  }

  // While the window is not in front, the pads do nothing (the keyboard is released the same way).
  void setActive(bool active) { active_ = active; }

  int count() const { return static_cast<int>(pads_.size()); }

  std::string names() const
  {
    if(pads_.empty()) return "none";
    std::string text;
    for(const Pad& p : pads_) text += (text.empty() ? "" : ", ") + p.name;
    return text;
  }

  // What all the open pads together are doing now.
  controls::PadState poll()
  {
    controls::PadState total;
    if(!active_) return total;
    for(Pad& p : pads_) total = controls::combine(total, controls::padState(read(p), p.stick));
    return total;
  }

  // What new presses on the open pads ask of the window (Stage I14): the menu, a move in it, a console switch. `nowMs` is any steady clock in milliseconds.
  // Called once each time round the loop, also while the window is not in front (then the answer is thrown away), so a press made meanwhile is not found later.
  controls::PadAsks asks(long long nowMs)
  {
    controls::PadAsks total;
    for(Pad& p : pads_) {
      const controls::PadAsks a = p.presses.update(read(p), nowMs);
      total.menu = total.menu || a.menu;
      total.leftDifficulty = total.leftDifficulty || a.leftDifficulty;
      total.rightDifficulty = total.rightDifficulty || a.rightDifficulty;
      total.tvType = total.tvType || a.tvType;
      total.nav.insert(total.nav.end(), a.nav.begin(), a.nav.end());
    }
    return active_ ? total : controls::PadAsks{};
  }

  // Stage I15 (D-021): what goes out to the pads. The light bar colour goes to every PlayStation pad (a DualShock 4) when it changed or the pad has just been plugged in.
  void setLight(const background::Rgb& colour)
  {
    lightUsed_ = true;
    for(Pad& p : pads_)
      if(controls::isPlayStation(p.family) && (!p.lightSent || !(p.light == colour))) { SDL_SetGamepadLED(p.gamepad, colour.r, colour.g, colour.b); p.light = colour; p.lightSent = true; }
  }

  // A pulse of the rumble motors (a pad without them ignores it); nothing while the window is not in front.
  void rumble(Uint16 low, Uint16 high, Uint32 ms)
  {
    if(!active_) return;
    for(Pad& p : pads_) SDL_RumbleGamepad(p.gamepad, low, high, ms);
  }

  // The buttons of the first pad open (a DualShock 4 when there is none), for the names in the F1 list and the menu.
  controls::PadFamily family() const { return pads_.empty() ? controls::PadFamily::PlayStation : pads_.front().family; }

private:
  struct Pad {
    SDL_Gamepad* gamepad = nullptr;
    SDL_JoystickID id = 0;
    std::string name;
    controls::StickDirection stick;
    controls::PadFamily family = controls::PadFamily::Other;
    controls::PadPresses presses;
    background::Rgb light;
    bool lightSent = false;
  };

  static controls::PadFamily familyOf(SDL_Gamepad* g)
  {
    switch(SDL_GetGamepadType(g)) {
      case SDL_GAMEPAD_TYPE_PS3: case SDL_GAMEPAD_TYPE_PS4: return controls::PadFamily::PlayStation;
      case SDL_GAMEPAD_TYPE_PS5: return controls::PadFamily::DualSense;
      case SDL_GAMEPAD_TYPE_XBOX360: case SDL_GAMEPAD_TYPE_XBOXONE: return controls::PadFamily::Xbox;
      default: return controls::PadFamily::Other;
    }
  }

  static controls::PadReading read(const Pad& p)
  {
    auto down = [&](SDL_GamepadButton b) { return SDL_GetGamepadButton(p.gamepad, b); };
    controls::PadReading r;
    r.dpadUp = down(SDL_GAMEPAD_BUTTON_DPAD_UP);
    r.dpadDown = down(SDL_GAMEPAD_BUTTON_DPAD_DOWN);
    r.dpadLeft = down(SDL_GAMEPAD_BUTTON_DPAD_LEFT);
    r.dpadRight = down(SDL_GAMEPAD_BUTTON_DPAD_RIGHT);
    r.cross = down(SDL_GAMEPAD_BUTTON_SOUTH);
    r.square = down(SDL_GAMEPAD_BUTTON_WEST);
    r.options = down(SDL_GAMEPAD_BUTTON_START);
    r.share = down(SDL_GAMEPAD_BUTTON_BACK);
    r.circle = down(SDL_GAMEPAD_BUTTON_EAST);
    r.triangle = down(SDL_GAMEPAD_BUTTON_NORTH);
    r.l1 = down(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER);
    r.r1 = down(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER);
    r.home = down(SDL_GAMEPAD_BUTTON_GUIDE);
    r.touchpad = down(SDL_GAMEPAD_BUTTON_TOUCHPAD);
    r.rightStick = down(SDL_GAMEPAD_BUTTON_RIGHT_STICK);
    r.stickX = SDL_GetGamepadAxis(p.gamepad, SDL_GAMEPAD_AXIS_LEFTX);
    r.stickY = SDL_GetGamepadAxis(p.gamepad, SDL_GAMEPAD_AXIS_LEFTY);
    return r;
  }

  void open(SDL_JoystickID id)
  {
    for(const Pad& p : pads_)
      if(p.id == id) return;  // already open (it was found at start-up and the "added" event came too)
    SDL_Gamepad* g = SDL_OpenGamepad(id);
    if(!g) return;
    Pad p;
    p.gamepad = g;
    p.id = id;
    const char* name = SDL_GetGamepadName(g);
    p.name = name ? name : "gamepad";
    p.family = familyOf(g);
    p.presses = controls::PadPresses(p.family);
    pads_.push_back(std::move(p));
  }

  void close(SDL_JoystickID id)
  {
    for(auto it = pads_.begin(); it != pads_.end(); ++it)
      if(it->id == id) { SDL_CloseGamepad(it->gamepad); pads_.erase(it); return; }
  }

  std::vector<Pad> pads_;
  bool started_ = false, active_ = true, lightUsed_ = false;
  std::string problem_;
};

// --pad-check: a virtual gamepad (SDL's own test device, which goes through the same gamepad code as a real pad) plays the
// real game through the same Pads code the window uses. Every button and the stick are checked against the game's own
// memory and the console switches; then the pad is unplugged while the stick is held and plugged in again.
bool padCheck(const Options& o);

}  // namespace host
