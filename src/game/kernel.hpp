// The drawing code (Stage F23.2): everything the original program does between the top logic and the bottom logic of a frame ($F07F-$F4E6) to make the
// picture. The picture is a function of the 128 memory bytes and the cartridge (F18). This code works on the same bytes and, instead of writing to the
// video chip, hands every write to a TimedChip together with the scanline and the colour clock at which the original makes it (counted from the
// processor cycles of the original instructions). The video-chip model (tia_video.hpp, F23.1) turns these writes into the picture.
// Like the original it also leaves its scratch bytes in memory (the eleven bytes of F21.26).
#pragma once
#include <cstdint>

#include "routines.hpp"

namespace game {

// One write to the video chip: `line` counts from the end of the vertical sync, `clock` is the colour clock at the end of the writing instruction.
// `site` is the address of the writing instruction in the original program (for tracing; it has no other meaning).
struct ChipWrite {
  int line;
  int clock;
  uint8_t reg;
  uint8_t value;
  uint16_t site;
};

class TimedChip {
 public:
  virtual ~TimedChip() = default;
  virtual void write(const ChipWrite& w) = 0;
  // After the drawing: the collisions the picture shows, if this sink can tell (the video-chip model can): the ghost of the frame touched Pac-Man
  // (chip flag player 0 - player 1) and the bonus item touched Pac-Man (missile 1 and the ball both touched player 1). Returns false if it cannot.
  virtual bool collisions(bool& /*ghostTouched*/, bool& /*bonusTouched*/) { return false; }
};

// Where the beam is when the drawing code ends (the bottom logic of the original follows at once).
struct PictureEnd {
  int line;
  int clock;
};

// Draws one picture. `startLine` is the scanline on which the original code starts (it follows the top logic, whose length varies a little; the
// maze itself always starts on line 34, so only the set-up writes before that depend on it).
PictureEnd drawPicture(Ram& ram, const Cartridge& cart, int startLine, TimedChip& chip);

}  // namespace game
