#include "kernel.hpp"
#include "picture/kernel_internal.hpp"

namespace game {

PictureEnd drawPicture(GameState& st, const Cartridge& cart, int startLine, TimedChip& chip, int kernelStartLine) {
  kernel::Kernel k{st, cart, chip, startLine, kernelStartLine};
  k.positions();
  k.mazeRows();
  k.bottomWall();
  k.scoreLine();
  k.lifeMarkers();
  return k.end();
}

}  // namespace game
