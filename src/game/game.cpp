#include "game.hpp"

namespace game {

namespace {
class NullSink : public TimedChip {
 public:
  void write(const ChipWrite&) override {}
};
NullSink nullSink;
}  // namespace

bool Game::runFrame(const FrameInputs& in, TiaChip& chip) {
  if (!started_) {
    chip.position(kPowerOn);
    startUp(ram_, chip);
    chip.position(kFrameStartLine);
    frameStart(ram_, cart_, in.console, chip);
    started_ = true;
    return true;
  }
  chip.position(kTopLogicLine);
  if (!topLogic(ram_, cart_, in.console, chip)) return false;
  drawPicture(ram_, cart_, 16, picture_ ? *picture_ : nullSink);   // lines 0-33 are blank, the maze from line 35, the score and the life markers below it (F18)
  chip.position(kBottomLogicLine);
  FrameInput flags = in.frame;
  if (picture_) picture_->collisions(flags.ghostTouched, flags.bonusTouched);   // the picture tells; otherwise the platform's flags are used
  if (!bottomLogic(ram_, cart_, flags, chip)) return false;
  chip.position(kFrameStartLine);
  frameStart(ram_, cart_, in.console, chip);
  return true;
}

}  // namespace game
