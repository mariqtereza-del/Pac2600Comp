// The opening and pen animations of an object (a ghost leaving the pen or crossing an opening, Pac-Man crossing an opening or dying).
// (Stage G4.2: moved out of routines.hpp. The functions are the F21 rewrites of the original routines named in their comments.)
#pragma once
#include <cstdint>

#include "../data/cartridge.hpp"
#include "../state/state.hpp"

namespace game {

// F21.14  ANIMATE_OBJECT ($FC8B): the animation of an object (n = 0 Pac-Man, 1-4 ghosts) whose state has bit 5 set: a ghost leaving the pen or going
// through an opening, Pac-Man going through an opening or dying (F10, F13). `sprite` is 0 for a ghost (player 0) and 1 for Pac-Man (player 1): it picks the
// sprite scratch bytes (SPRITE0_Y / SPRITE1_Y, SPRITE0_LINES / SPRITE1_LINES, the picture pointer) that the animation also sets. Depending on the
// "step due" bit 4 the object waits (its y counts down, then the animation starts) or moves along the opening path (direction bits 7-6 of the state:
// 0 down the outside to y $5B, 1 and 3 up the outside from the bottom or top, 2 back into the maze). The cartridge tables used: $FF0D (picture pointer
// steps), $FF19 (which pointer), $FF25 (start y of the next phase).
enum class AnimPath { Counting, CountDone, RightStep, RightEnd, DownStep, DownEnd, UpEnd, Reached0B, SpriteY, SpritePtr };
struct AnimResult {
  AnimPath path;
  uint8_t newY;        // the y after the step (before a table value replaces it)
  uint8_t stateAfter;  // the state after
  uint8_t tableByte;   // $FF0D byte added to the picture pointer (SpritePtr only)
  uint8_t ptrOld;      // the picture pointer byte before (SpritePtr only)
  uint8_t ptrIndex;    // which pointer (0 or 2: SPRITE_PTR + index) (SpritePtr only)
};
AnimResult animateObject(GameState& st, const Cartridge& cart, uint8_t object, uint8_t sprite);

}  // namespace game
