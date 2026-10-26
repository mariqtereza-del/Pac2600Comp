// The game controllers (keyboard keys and gamepads) as the console inputs, and the pad check.
// (Stage G4.7: moved out of main.cpp, the code is unchanged.)
#pragma once
#include "common.hpp"
#include "options.hpp"

namespace host {

// ---- Keyboard (Stage D10, DECISIONS.md T-D10-1) ----
// Gives the console what the keyboard adds up to. extraReset is for tests that press Reset by frame number.
void applyInput(Emulator& emulator, const controls::Input& in, bool extraReset);

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
    for(Pad& p : pads_) {
      controls::PadReading r;
      r.dpadUp = SDL_GetGamepadButton(p.gamepad, SDL_GAMEPAD_BUTTON_DPAD_UP);
      r.dpadDown = SDL_GetGamepadButton(p.gamepad, SDL_GAMEPAD_BUTTON_DPAD_DOWN);
      r.dpadLeft = SDL_GetGamepadButton(p.gamepad, SDL_GAMEPAD_BUTTON_DPAD_LEFT);
      r.dpadRight = SDL_GetGamepadButton(p.gamepad, SDL_GAMEPAD_BUTTON_DPAD_RIGHT);
      r.cross = SDL_GetGamepadButton(p.gamepad, SDL_GAMEPAD_BUTTON_SOUTH);
      r.square = SDL_GetGamepadButton(p.gamepad, SDL_GAMEPAD_BUTTON_WEST);
      r.options = SDL_GetGamepadButton(p.gamepad, SDL_GAMEPAD_BUTTON_START);
      r.share = SDL_GetGamepadButton(p.gamepad, SDL_GAMEPAD_BUTTON_BACK);
      r.stickX = SDL_GetGamepadAxis(p.gamepad, SDL_GAMEPAD_AXIS_LEFTX);
      r.stickY = SDL_GetGamepadAxis(p.gamepad, SDL_GAMEPAD_AXIS_LEFTY);
      total = controls::combine(total, controls::padState(r, p.stick));
    }
    return total;
  }

private:
  struct Pad {
    SDL_Gamepad* gamepad = nullptr;
    SDL_JoystickID id = 0;
    std::string name;
    controls::StickDirection stick;
  };

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
    pads_.push_back(std::move(p));
  }

  void close(SDL_JoystickID id)
  {
    for(auto it = pads_.begin(); it != pads_.end(); ++it)
      if(it->id == id) { SDL_CloseGamepad(it->gamepad); pads_.erase(it); return; }
  }

  std::vector<Pad> pads_;
  bool started_ = false, active_ = true;
  std::string problem_;
};

// --pad-check: a virtual gamepad (SDL's own test device, which goes through the same gamepad code as a real pad) plays the
// real game through the same Pads code the window uses. Every button and the stick are checked against the game's own
// memory and the console switches; then the pad is unplugged while the stick is held and plugged in again.
bool padCheck(const Options& o);

}  // namespace host
