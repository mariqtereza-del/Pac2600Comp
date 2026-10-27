// The whole game as one object (Stage F21.27): the 128 bytes of state and the frame function that runs one frame of the original's logic, with no 6502
// and no original code. The platform gives it the console and joystick state of the frame and the two collision flags, and a chip to write to; the
// data tables are part of the program (data/tables.hpp); the game needs no ROM at all. The drawing code (F23.2) runs in every frame between the two
// logics and hands its timed chip writes to the picture sink, if one is set (the video-chip model of F23.1 turns them into the picture).
#pragma once
#include "kernel.hpp"
#include "chips.hpp"
#include "inputs.hpp"
#include "logic/bottom.hpp"
#include "logic/flow.hpp"
#include "logic/frame_start.hpp"
#include "logic/top.hpp"
#include "objects_view.hpp"
#include "state/cycles.hpp"
#include "state/state.hpp"

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
  Game() = default;

  // One frame. The first frame is the power-on set-up and the frame start; every later frame is the top logic, the bottom logic (the picture is drawn
  // between them by the renderer) and the frame start. Returns false if the game met the one path of the original that no recording ever runs.
  bool runFrame(const FrameInputs& in, TiaChip& chip);

  // Where the drawing code's writes to the video chip go (not owned). Without one the drawing still runs, so the game's memory is the same.
  void setPicture(TimedChip* sink) { picture_ = sink; }

  // Stage I9.1: a second sink that is given the picture without Pac-Man and the ghosts, drawn from a copy of the state (so the game, its memory and its sound
  // are the same with or without it). The logic's own writes to the chip must reach this sink too (see Layers, layers.hpp).
  void setBackgroundPicture(TimedChip* sink) { background_ = sink; }
  // Stage I9.2: a third sink, given the picture without the power pills either (so that the pills can be found as the difference to the background: a
  // pill is drawn above Pac-Man, the ghosts and Pac-Man are not above it).
  void setBarePicture(TimedChip* sink) { bare_ = sink; }

  // Stage I9.2: when switched on, every frame also describes the five moving objects as the picture of that frame shows them (objects_view.hpp). Read only: the
  // game does not use it.
  void trackObjects(bool on) { trackObjects_ = on; }
  const ObjectsView& objects() const { return objects_; }

  // The number of scanlines of the frame just played (262 as a rule, 43 for the first, 263 or 261 around a frame whose frame start ran one line long).
  int lastFrameLines() const { return lastLines_; }

  const GameState& state() const { return state_; }
  GameState& state() { return state_; }

 private:
  bool frameStartRanLong(const ConsoleInput& console) const;
  GameState state_{};
  bool started_ = false;
  TimedChip* picture_ = nullptr;
  TimedChip* background_ = nullptr;
  TimedChip* bare_ = nullptr;
  bool trackObjects_ = false;
  // the bottom logic of the last frame wrote the reflect bit of Pac-Man's player (the chip keeps it until the picture code clears it)
  bool mirrorSet_ = false;
  SpriteSchedule schedule_[2];       // for the frames that start the drawing code on line 16 and 15 (shift_ 0 and 1), made when first needed
  bool haveSchedule_[2] = {false, false};
  ObjectsView objects_{};
  int shift_ = 0;         // 1 when the vertical timer of the last frame start ended one line early: this frame's numbering starts one line later
  int lastLines_ = 0;
};

}  // namespace game
