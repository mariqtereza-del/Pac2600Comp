#include "kernel.hpp"
#include "data/tables.hpp"
#include "picture/kernel_internal.hpp"
#include "tia_video.hpp"

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

namespace {
class NullSink : public TimedChip {
 public:
  void write(const ChipWrite&) override {}
};
}  // namespace

// Stage I9.2: the schedule is made by running the drawing code on a state with both sprites out of sight; the order of the lines does not depend on the
// sprites (only the cycle counts inside a line do), so it is the same for every frame with the same start lines.
SpriteSchedule spriteSchedule(int startLine, int kernelStartLine) {
  GameState st{};
  st.scratch[kSprite0Y] = st.scratch[kSprite1Y] = 0x70;
  st.scratch[kSprite0Lines] = 7;
  st.scratch[kSprite1Lines] = 6;
  st.scratch[kSprite0PtrLo] = data::kGhostPictures[0];
  st.scratch[kSprite1PtrLo] = data::kPacmanPictures[0];
  st.scratch[kSprite0PtrHi] = st.scratch[kSprite1PtrHi] = 0xFD;
  NullSink sink;
  SpriteSchedule schedule;
  kernel::Kernel k{st, sink, startLine, kernelStartLine};
  k.logSprites(&schedule.player[0], &schedule.player[1]);
  k.positions();
  k.mazeRows();
  k.bottomWall();
  return schedule;
}

namespace {
class VideoSink : public TimedChip {
 public:
  void write(const ChipWrite& w) override { video.write(w.line, w.clock, w.reg, w.value); }
  TiaVideo video;
};
}  // namespace

const SpriteColumns& spriteColumns() {
  static const SpriteColumns table = [] {
    SpriteColumns t{};
    for (int player = 0; player < 2; ++player)
      for (int x = 0; x < 256; ++x) {
        GameState st{};
        st.spriteX = st.objX[0] = static_cast<uint8_t>(x);
        VideoSink sink;
        kernel::Kernel k{st, sink, 16, 34};
        k.positions();
        // one picture line with the player on: colour on, the blank off, the picture byte all ones; the other player stays off
        const int line = 40;
        sink.video.write(line - 3, 0, 0x09, 0x00);                           // COLUBK black
        sink.video.write(line - 3, 10, player == 0 ? 0x06 : 0x07, 0x0E);     // COLUP0 or COLUP1
        sink.video.write(line - 3, 20, 0x01, 0x00);                          // VBLANK off
        sink.video.write(line - 1, 10, player == 0 ? 0x1B : 0x1C, 0xFF);     // GRP0 or GRP1
        sink.video.write(line + 2, 10, player == 0 ? 0x1B : 0x1C, 0x00);
        sink.video.endPicture();
        int first = -1;
        const auto& picture = sink.video.picture();
        for (int col = 0; col < TiaVideo::kWidth && first < 0; ++col)
          if (picture[static_cast<std::size_t>((line - TiaVideo::kFirstLine) * TiaVideo::kWidth + col)] != 0) first = col;
        t.player[player][x] = first;
      }
    return t;
  }();
  return table;
}

// Stage I9.1: the same drawing code with Pac-Man and the ghosts left out of the maze part (the picture a smooth presenter draws them on). It changes the
// state it is given (the scratch bytes), so it is always run on a copy.
PictureEnd drawBackground(GameState& st, int startLine, TimedChip& chip, int kernelStartLine, bool withoutPowerPills) {
  kernel::Kernel k{st, chip, startLine, kernelStartLine};
  k.hideMovingObjects(true);
  k.hidePowerPills(withoutPowerPills);
  k.positions();
  k.mazeRows();
  k.bottomWall();
  k.hideMovingObjects(false);
  k.scoreLine();
  k.lifeMarkers();
  return k.end();
}

}  // namespace game
