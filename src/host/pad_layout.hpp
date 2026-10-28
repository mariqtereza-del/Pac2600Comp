// The game pad's buttons as the player meets them (Stage I14, D-020): the names of the buttons for each family of pad, the PlayStation symbols, and what a press
// asks of the window (open the menu, move in it, flip a console switch). No window code, so it can be tested on its own.
#pragma once

#include <string>
#include <vector>

#include "controls.hpp"

namespace controls {

// The four PlayStation symbols are written into texts as these characters (they are never printed by themselves; drawSymbolText draws a small picture for each).
inline constexpr char kCross = '\x01', kCircle = '\x02', kSquare = '\x03', kTriangle = '\x04';
// The four lettered buttons of an Xbox pad (Stage I18): drawn as the letter in the button's colour (A green, B red, X blue, Y yellow).
inline constexpr char kButtonA = '\x05', kButtonB = '\x06', kButtonX = '\x07', kButtonY = '\x08';
bool isSymbol(char c);
std::string plainSymbols(const std::string& text);   // the same text with the words Cross, Circle, Square, Triangle (and the letters A, B, X, Y) instead of the symbols

const char* familyTitle(PadFamily family);

// The pad lines of the F1 list and of the Control options page: button names and meaning (PlayStation names carry the symbols).
std::vector<HelpLine> padLines(PadFamily family);

// What the pad asks of the menu: a move, "do" (Enter) or "back" (Escape).
enum class Nav { Up, Down, Left, Right, Enter, Back };

// What new presses on a pad ask of the window. The game's own buttons (directions, fire, Select, Reset) are not here: they are held states (PadState).
struct PadAsks {
  bool menu = false;                          // PS button or touchpad (Xbox: the Guide button or a press on the right stick): open or close the menu
  bool leftDifficulty = false, rightDifficulty = false, tvType = false;   // L1, R1, Triangle: flip the console switch
  std::vector<Nav> nav;                       // directions (with repeat while held), Cross = Enter, Circle = Back
  bool any() const { return menu || leftDifficulty || rightDifficulty || tvType || !nav.empty(); }
};

// Finds the new presses of one pad from its readings, one reading per turn round the window loop. A held direction repeats after 400 ms and then every 90 ms.
class PadPresses {
 public:
  explicit PadPresses(PadFamily family = PadFamily::PlayStation) : family_(family) {}
  PadAsks update(const PadReading& reading, long long nowMs);

 private:
  PadFamily family_;
  PadReading before_;
  StickDirection stick_;
  int heldDirection_ = -1;          // -1 none, else a Nav value
  long long nextRepeatAt_ = 0;
};

// The console switches flip when their button is pressed, as they do with F4 to F6.
void flipSwitches(Keyboard& keyboard, const PadAsks& asks);

}  // namespace controls
