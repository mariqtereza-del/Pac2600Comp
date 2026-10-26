#include "game.hpp"

namespace game {

namespace {
class NullSink : public TimedChip {
 public:
  void write(const ChipWrite&) override {}
};
NullSink nullSink;
}  // namespace

// The frame start of the original ($F83D-$F878) takes 75 processor cycles from the wait for the end of a line to the strobe of the next wait; each of the two
// branches that load the colour mask (black and white; title screen) takes one cycle more. With both the strobe falls on the first cycle of the next line:
// the wait then lasts a whole extra line, the vertical sync ends one line later (the frame has 263 lines) and the timer of the next frame ends one line
// earlier in the numbering from the sync (its picture starts on line 33, its frame has 261 lines). (Found with a random game, Stage F25.)
static bool frameStartRanLongImpl(const Ram& ram, const ConsoleInput& console) {
  return at(ram, ram::ATTRACT_SHIFT) != 0 && !console.colour;
}

bool Game::frameStartRanLong(const ConsoleInput& console) const { return frameStartRanLongImpl(ram_, console); }

bool Game::runFrame(const FrameInputs& in, TiaChip& chip) {
  if (!started_) {
    chip.position(kPowerOn);
    startUp(ram_, chip);
    chip.position(kFrameStartLine);
    frameStart(ram_, cart_, in.console, chip);
    shift_ = frameStartRanLong(in.console) ? 1 : 0;
    lastLines_ = 43;   // from power-on to the first vertical sync
    started_ = true;
    return true;
  }
  chip.position(kTopLogicLine);
  if (!topLogic(ram_, cart_, in.console, chip)) return false;
  const PictureEnd end = drawPicture(ram_, cart_, 16 - shift_, picture_ ? *picture_ : nullSink, 34 - shift_);   // lines 0-33 are blank, the maze from line 35, the score and the life markers below it (F18)
  chip.timeBase(end.line, end.clock);
  FrameInput flags = in.frame;
  if (picture_) picture_->collisions(flags.ghostTouched, flags.bonusTouched);   // the picture tells; otherwise the platform's flags are used
  if (!bottomLogic(ram_, cart_, flags, chip)) return false;
  chip.position(kFrameStartLine - shift_);
  frameStart(ram_, cart_, in.console, chip);
  const int extra = frameStartRanLong(in.console) ? 1 : 0;
  lastLines_ = 262 + extra - shift_;
  shift_ = extra;
  return true;
}

}  // namespace game
