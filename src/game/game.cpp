#include "game.hpp"

namespace game {

bool Game::runFrame(const FrameInputs& in, TiaChip& chip) {
  if (!started_) {
    startUp(ram_, chip);
    frameStart(ram_, cart_, in.console, chip);
    started_ = true;
    return true;
  }
  if (!topLogic(ram_, cart_, in.console, chip)) return false;
  // (the picture is drawn here: lines 0-33 are blank, the maze from line 35, the score and the life markers below it: F18; the renderer of F23)
  if (!bottomLogic(ram_, cart_, in.frame, chip)) return false;
  frameStart(ram_, cart_, in.console, chip);
  return true;
}

}  // namespace game
