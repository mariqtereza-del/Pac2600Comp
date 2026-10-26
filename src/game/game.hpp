// The whole game as one object (Stage F21.27): the 128 bytes of state and the frame function that runs one frame of the original's logic, with no 6502
// and no original code. The platform gives it the console and joystick state of the frame and the two collision flags, and a chip to write to; the
// cartridge data is read by address through game::Cartridge. The drawing code (F23.2) runs in every frame between the two logics and hands its
// timed chip writes to the picture sink, if one is set (the video-chip model of F23.1 turns them into the picture).
#pragma once
#include "kernel.hpp"
#include "routines.hpp"

namespace game {

// The scanlines at which the writes of the logic parts are placed for the picture (the logic code has no exact timing, and none of these writes depends
// on it: the top logic ends before the drawing code starts, the bottom logic follows its last line, the frame start comes last).
inline constexpr int kPowerOn = -1, kTopLogicLine = 5, kBottomLogicLine = 240, kFrameStartLine = 259;

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

  // The number of scanlines of the frame just played (262 as a rule, 43 for the first, 263 or 261 around a frame whose frame start ran one line long).
  int lastFrameLines() const { return lastLines_; }

  const Ram& state() const { return ram_; }
  Ram& state() { return ram_; }

 private:
  bool frameStartRanLong(const ConsoleInput& console) const;
  const Cartridge& cart_;
  Ram ram_{};
  bool started_ = false;
  TimedChip* picture_ = nullptr;
  int shift_ = 0;         // 1 when the vertical timer of the last frame start ended one line early: this frame's numbering starts one line later
  int lastLines_ = 0;
};

}  // namespace game
