// The keyboard: which keys do what, and the state they add up to (Stage D10, DECISIONS.md T-D10-1).
// No window code; the only SDL part is the names of the key codes, so it can be tested on its own.
#pragma once

#include <SDL3/SDL_scancode.h>

#include <cstdint>
#include <string>
#include <vector>

namespace controls {

// What the console sees. Directions, fire, Select and Reset are "held while the key is down"; the two difficulty
// switches are real switches (they flip when their key is pressed and stay where they are).
struct Input {
  bool up = false, down = false, left = false, right = false, fire = false;
  bool select = false, reset = false;
  bool leftDifficultyA = false, rightDifficultyA = false;  // false = B, which is the default for this game (docs\CONTROLS.md)
  bool blackWhite = false;                                 // the Colour / B&W switch: false = Colour (the default)

  std::uint32_t pack() const;                   // all ten switches in one number, to pass between threads
  static Input unpack(std::uint32_t bits);
  bool operator==(const Input& o) const { return pack() == o.pack(); }
};

enum class Action { Up, Down, Left, Right, Fire, Select, Reset, LeftDifficulty, RightDifficulty, TvType };

// Every key that does something in the game window, for the tests and for the F1 list.
struct Binding {
  Action action;
  std::vector<SDL_Scancode> keys;  // positions on the keyboard, so WASD is right on any layout
  const char* keysText;            // as shown in the F1 list
  const char* meaning;
};
const std::vector<Binding>& bindings();

// The first key of an action (used by the replay, which presses real keys).
SDL_Scancode primaryKey(Action action);

class Keyboard {
public:
  void keyDown(SDL_Scancode key, bool repeat);  // repeat = the key is only being held down
  void keyUp(SDL_Scancode key);
  void releaseAll();  // the window lost focus: no key may stay "held" (the three real switches stay where they are)
  void setSwitches(bool leftA, bool rightA, bool blackWhite);  // where the switches start (remembered from last time)
  Input input() const;

private:
  bool held(Action action) const;
  std::vector<SDL_Scancode> down_;
  bool leftA_ = false, rightA_ = false, bw_ = false;
};

// The lines of the F1 list: key text and meaning (the keys that are not game controls are included).
struct HelpLine {
  std::string keys, meaning;
};
std::vector<HelpLine> helpLines(const Input& current, const std::string& controller = "none");

// ---- Gamepad (Stage D11, DECISIONS.md T-D11-1) ----

// The raw readings of one pad: the D-pad and four buttons as pressed or not, the left stick from -32768 to 32767
// (up and left are negative, as in SDL). Buttons are named by SDL's layout: Cross = south, Square = west,
// Options = start, Share = back (SDL's PS4 driver delivers them that way).
struct PadReading {
  bool dpadUp = false, dpadDown = false, dpadLeft = false, dpadRight = false;
  bool cross = false, square = false, options = false, share = false;
  int stickX = 0, stickY = 0;
};

// What a pad does to the console.
struct PadState {
  bool up = false, down = false, left = false, right = false, fire = false, select = false, reset = false;
};

// The left stick as one of four directions. A push counts once it passes 50 % of the way and stays counted until it
// falls under 30 %; if both axes are pushed the stronger one wins, and a direction already held is kept unless the
// other axis is clearly stronger (25 %), so a stick resting near 45 degrees does not flicker.
class StickDirection {
public:
  enum class Dir { None, Up, Down, Left, Right };
  Dir update(int x, int y);

private:
  Dir previous_ = Dir::None;
};

PadState padState(const PadReading& reading, StickDirection& stick);
PadState combine(const PadState& a, const PadState& b);  // two pads at once: either one counts
Input merge(const Input& keyboard, const PadState& pad);  // keyboard and pad at once; opposite directions cancel

}  // namespace controls
