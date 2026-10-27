// The drawing code (Stage F23.2): everything the original program does between the top logic and the bottom logic of a frame ($F07F-$F4E6) to make the
// picture. The picture is a function of the 128 memory bytes and the cartridge (F18). This code works on the same bytes and, instead of writing to the
// video chip, hands every write to a TimedChip together with the scanline and the colour clock at which the original makes it (counted from the
// processor cycles of the original instructions). The video-chip model (tia_video.hpp, F23.1) turns these writes into the picture.
// Like the original it also leaves its scratch bytes in memory (the eleven bytes of F21.26).
#pragma once
#include <cstdint>
#include <vector>

#include "chips.hpp"
#include "state/state.hpp"

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
// maze itself starts on `kernelStartLine`, 34 as a rule: one line earlier after a frame whose frame start ran one line long, see game.cpp).
PictureEnd drawPicture(GameState& st, int startLine, TimedChip& chip, int kernelStartLine = 34);

// Stage I9.2 (D-015): when the drawing code gives each line of the two players their picture byte. The maze lines are drawn in pairs and more, so a sprite
// line is not always two rows high; where each one is shown follows from the order of the drawing code, not from the sprite. `x` is the line counter the
// drawing code compares with the sprite's y (the sprite starts at the first fetch whose x is not below its y, and each later fetch shows its next line);
// `row` is the picture row on which the fetched byte begins to show. One list for each player of the chip (0 = the ghost, 1 = Pac-Man).
struct SpriteFetch {
  uint8_t x;
  int row;
  bool clear;   // not a fetch: the write that switches the players off before the score (CLEAR_GRAPHICS); it ends whatever the sprite shows
};
struct SpriteSchedule {
  std::vector<SpriteFetch> player[2];
};
SpriteSchedule spriteSchedule(int startLine, int kernelStartLine);

// The picture column of the left edge of a sprite for each x value the drawing code is given, found by running its positioning code on the video-chip
// model and looking where an 8-pixel player appears (-1: not in the picture). The coarse position is a multiple of 15 pixels and the rest is a fine move,
// so the column is not simply x.
struct SpriteColumns {
  int player[2][256];   // [0] the ghost's player, [1] Pac-Man's
};
const SpriteColumns& spriteColumns();

// Stage I9.1 (D-015): the same picture without Pac-Man and the ghosts (maze, wafers, power pills, bonus item, score and life markers stay). Used on a copy of
// the state, for the smooth presenter; the game itself never uses it. A power pill is missile 0 of the chip, in the ghost's colour and above Pac-Man;
// with `withoutPowerPills` it is left out too, so
// that the pills can be found as the difference.
PictureEnd drawBackground(GameState& st, int startLine, TimedChip& chip, int kernelStartLine = 34, bool withoutPowerPills = false);

}  // namespace game
