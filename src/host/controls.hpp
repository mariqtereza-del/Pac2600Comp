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

  std::uint32_t pack() const;                   // all nine switches in one number, to pass between threads
  static Input unpack(std::uint32_t bits);
  bool operator==(const Input& o) const { return pack() == o.pack(); }
};

enum class Action { Up, Down, Left, Right, Fire, Select, Reset, LeftDifficulty, RightDifficulty };

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
  void releaseAll();  // the window lost focus: no key may stay "held" (the difficulty switches stay where they are)
  Input input() const;

private:
  bool held(Action action) const;
  std::vector<SDL_Scancode> down_;
  bool leftA_ = false, rightA_ = false;
};

// The lines of the F1 list: key text and meaning (the keys that are not game controls are included).
struct HelpLine {
  std::string keys, meaning;
};
std::vector<HelpLine> helpLines(const Input& current);

}  // namespace controls
