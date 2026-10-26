// What the platform gives the game in one frame (Stage G4.4, moved out of routines.hpp): the console switches, and the joystick port with the
// two collision flags of the chip.
#pragma once
#include <cstdint>

namespace game {

// The console as the game logic sees it in one frame: the buttons and switches the top logic reads (F9).
struct ConsoleInput {
  bool fire = false;          // the fire button of the left joystick
  bool reset = false;         // the Reset switch pressed
  bool select = false;        // the Select switch pressed
  bool leftDifficultyA = false;   // the left difficulty switch in position A (SWCHB bit 6 set)
  bool rightDifficultyA = false;  // the right difficulty switch in position A (SWCHB bit 7 set)
  bool colour = true;             // the colour / black-and-white switch in position colour (SWCHB bit 3 set)
};

// What the chip tells the game in one frame, besides the console switches: the joysticks and the two collision flags (F12, F18).
struct FrameInput {
  uint8_t swcha = 0xFF;       // the joystick port: left joystick in the upper nibble, right joystick in the lower; 0 = pressed
  bool bonusTouched = false;  // missile 1 and the ball both overlapped Pac-Man (the bonus item)
  bool ghostTouched = false;  // player 0 (the ghost of the frame) overlapped player 1 (Pac-Man)
};

}  // namespace game
