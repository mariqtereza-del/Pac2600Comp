// The whole game as one object (Stage F21.27): the 128 bytes of state and the frame function that runs one frame of the original's logic, with no 6502
// and no original code. The platform gives it the console and joystick state of the frame and the two collision flags, and a chip to write to; the
// cartridge data is read by address through game::Cartridge. The drawing code (F23.2) runs in every frame between the two logics and hands its
// timed chip writes to the picture sink, if one is set (the video-chip model of F23.1 turns them into the picture).
#pragma once
#include "kernel.hpp"
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

  // Where the drawing code's writes to the video chip go (not owned). Without one the drawing still runs, so the game's memory is the same.
  void setPicture(TimedChip* sink) { picture_ = sink; }

  const Ram& state() const { return ram_; }
  Ram& state() { return ram_; }

 private:
  const Cartridge& cart_;
  Ram ram_{};
  bool started_ = false;
  TimedChip* picture_ = nullptr;
};

}  // namespace game
