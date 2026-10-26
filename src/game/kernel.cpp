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

}  // namespace game
