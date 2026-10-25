// The whole game as one object (Stage F21.27): the 128 bytes of state and the frame function that runs one frame of the original's logic, with no 6502
// and no original code. The platform gives it the console and joystick state of the frame and the two collision flags, and a chip to write to; the
// cartridge data is read by address through game::Cartridge. The picture itself is the renderer's job (F23).
#pragma once
#include "routines.hpp"

namespace game {

struct FrameInputs {
  ConsoleInput console;   // fire, Reset, Select, difficulty switches, colour switch
  FrameInput frame;       // joystick port and collision flags
};

class Game {
 public:
  explicit Game(const Cartridge& cart) : cart_(cart) {}

  // One frame. The first frame is the power-on set-up and the frame start; every later frame is the top logic, the bottom logic (the picture is drawn
  // between them by the renderer) and the frame start. Returns false if the game met the one path of the original that no recording ever runs.
  bool runFrame(const FrameInputs& in, TiaChip& chip);

  const Ram& state() const { return ram_; }
  Ram& state() { return ram_; }

 private:
  const Cartridge& cart_;
  Ram ram_{};
  bool started_ = false;
};

}  // namespace game
