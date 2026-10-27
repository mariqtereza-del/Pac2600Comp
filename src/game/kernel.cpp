#include "kernel.hpp"
#include "picture/kernel_internal.hpp"

namespace game {

PictureEnd drawPicture(GameState& st, int startLine, TimedChip& chip, int kernelStartLine) {
  kernel::Kernel k{st, chip, startLine, kernelStartLine};
  k.positions();
  k.mazeRows();
  k.bottomWall();
  k.scoreLine();
  k.lifeMarkers();
  return k.end();
}

// Stage I9.1: the same drawing code with Pac-Man and the ghosts left out of the maze part (the picture a smooth presenter draws them on). It changes the
// state it is given (the scratch bytes), so it is always run on a copy.
PictureEnd drawBackground(GameState& st, int startLine, TimedChip& chip, int kernelStartLine) {
  kernel::Kernel k{st, chip, startLine, kernelStartLine};
  k.hideMovingObjects(true);
  k.positions();
  k.mazeRows();
  k.bottomWall();
  k.hideMovingObjects(false);
  k.scoreLine();
  k.lifeMarkers();
  return k.end();
}

}  // namespace game
