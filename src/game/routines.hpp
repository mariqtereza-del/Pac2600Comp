// The reconstructed game logic, routine by routine (Stage F21). Each function is the C++ form of one routine of the original (the F8 list)
// and works on the 128 bytes of the console memory exactly as the original does (the names are in ram_map.hpp). The lockstep tool
// (src/lockstep) proves each function against the original: every recorded game stays identical, and a direct comparison of the routine's
// results (memory and registers) is made at every call.
// Rules for this file: no ROM data and no emulator in here; tables that the original reads from the cartridge come later through the
// data loader (F22). Names of the routines are the original's F8 names in camelCase.
#pragma once
#include <array>
#include <cstdint>

#include "ram_map.hpp"

namespace game {

using Ram = std::array<uint8_t, 128>;

// The data of the cartridge (maze, amounts, shapes, sounds) is never part of this source: the routines read it by its address in the original
// program ($F000-$FFFF) through this view, which the data loader (F22) fills from the player's own ROM after checking its fingerprint.
class Cartridge {
 public:
  explicit Cartridge(const uint8_t* bytes4k) : bytes_(bytes4k) {}
  uint8_t at(uint16_t address) const { return bytes_[address & 0x0FFF]; }

 private:
  const uint8_t* bytes_;
};

inline uint8_t& at(Ram& ram, uint8_t addr) { return ram[addr - 0x80]; }
inline const uint8_t& at(const Ram& ram, uint8_t addr) { return ram[addr - 0x80]; }

// F21.1  CLEAR_SCORES ($FC7D): both scores to 0 and both lives bytes to 0.
void clearScores(Ram& ram);

// F21.2  SWAP_PLAYERS ($FB9F): exchanges the 20-byte block of the player in turn ($88-$9B: wafer map, lives, level, pill flags, score colour)
// with the block of the other player ($9C-$AF).
void swapPlayers(Ram& ram);

// F21.3  RNG_STEP ($FBE0): steps the shift register RNG_STATE (the game's only random number, F14) and returns the value the original leaves in A:
// one bit of the new state, picked by its low three bits (zero or not is all the callers ask).
uint8_t rngStep(Ram& ram);

// F21.4  WRAP_TO_BOTTOM ($FD26) and WRAP_TO_TOP ($FD32), with their shared end WRAP_SET_Y ($FD3C): Pac-Man or a ghost (object 0-4) enters the opening
// in the bottom or top wall of the maze. The direction bits of its state are set (bottom: $30, top: $F0; the low four bits are kept) and its y
// is set to the start of the opening animation ($51 below the maze, $0B above it).
void wrapToBottom(Ram& ram, uint8_t object);
void wrapToTop(Ram& ram, uint8_t object);

// F21.5  PLACE_OBJECTS ($FC21): freezes the game (GAME_FLAGS bit 7), sets the event timer to $3C (the freeze lasts 60 steps) and puts the four ghosts
// at x = $4C, y = $1E (state 0) and Pac-Man at x = $4C, y = $3C with the state $80.
void placeObjects(Ram& ram);
// F21.5  NEW_MAZE ($FC0C): fills the wafer map of the player in turn (all 126 wafers and the 4 power pills; two places stay empty), sets the pill
// flags to $3C, and then does PLACE_OBJECTS (the original falls into it).
void newMaze(Ram& ram);

// F21.6  NEW_GAME_SETUP ($FBF4): the start of a game for the player in turn: score colours ($0A and $09), three lives, the start level from the game
// number ((GAME_SELECT - 2) and 6) + 4 (F16), then NEW_MAZE (the original falls into it).
void newGameSetup(Ram& ram);

// F21.7  LOCATE_OBJECT ($FBAD): works out where object n (0 = Pac-Man, 1-4 = ghosts) stands in the maze grid (F10): the row, the distance from the
// row and cell column, and the cell index. Writes CELL_ROW, Y_OFFSET, X_OFFSET, CELL_INDEX and POS_CODE and returns them (the adapter needs them
// for the registers the original leaves).
struct Location {
  uint8_t row;        // CELL_ROW: y / 10 rounded up (0 when y = 0)
  uint8_t yOffset;    // Y_OFFSET: y - 10 * row (0 or negative, 8-bit)
  uint8_t xOffset;    // X_OFFSET: x and 3
  uint8_t cellIndex;  // CELL_INDEX: x / 4, plus 40 on odd rows
  uint8_t posCode;    // POS_CODE: (cell index << 2) or (bit 6 of the cell index << 7) or (row >> 1)
  uint8_t cellBit6;   // bit 6 of the cell index (the carry the original leaves)
};
Location locateObject(Ram& ram, uint8_t object);

// F21.8  MOVE_OBJECT ($F9EE): the speed accumulator of object n (0 = Pac-Man, 1-4 = ghosts). It becomes (acc << 4) + acc (8 bits); a carry out is one
// step of one pixel in the direction in bits 7-6 of the state (0 right, 1 left, 2 down, 3 up), after clearing the "step due" bit 4 of the state.
// A step to x = 2 or x = $96, to a negative y or to y = $47 is not made (F10). The result describes what happened for the adapter.
struct Move {
  bool stepped;       // the accumulator carried
  bool vertical;      // the direction is down or up
  uint8_t oldAcc;     // the accumulator before
  uint8_t stepValue;  // 1 or $FF
  uint8_t oldCoord;   // x or y before the step
  uint8_t newCoord;   // x or y the step would give
  bool stored;        // the new coordinate was stored (not stopped by a border)
};
Move moveObject(Ram& ram, uint8_t object);

// F21.9  SET_DIRECTION ($F9E5): stores direction code d (0 right, 1 left, 2 down, 3 up) in bits 7-6 of the state of object n and sets the "step due"
// bit 4 (the original takes both from the table TBL_FE63, whose four bytes are (d << 6) or $10), then falls into MOVE_OBJECT: the object moves at once.
// Returns what moveObject returned.
Move setDirection(Ram& ram, uint8_t object, uint8_t direction);

// F21.10  ADD_SCORE ($FC45): adds the score amount number y (0-3: ghosts 20/40/80/160, 4: wafer 1, 5: power pill 5, 6: bonus item 100) to the score of the
// player in turn (PLAYER_MODE bit 7), in decimal: low byte + TBL_FF06[y], middle byte + TBL_FF0C[y] (cartridge tables), high byte + carry.
// The original keeps the caller's X in the scratch byte TEMP_E1 and gives it back; callerX is that X (the byte stays in the memory).
// Returns the last decimal addition (the high byte) for the adapter's flags.
struct Bcd {
  uint8_t value;
  bool carry;
  bool overflow;
};
Bcd addScore(Ram& ram, const Cartridge& cart, uint8_t amount, uint8_t callerX);

// F21.11  LIVES_MARKERS ($FC68): the three playfield values for the row of life markers of one player, for `lives` markers, stored in the scratch bytes
// TEMP_D2+offset .. TEMP_D4+offset (offset 0 for player 1, 3 for player 2): $40 if there is any life (else 0), then the cartridge bytes at $FF1B+lives
// and $FEEC+lives. Returns the last value stored (the original leaves it in A).
uint8_t livesMarkers(Ram& ram, const Cartridge& cart, uint8_t lives, uint8_t offset);

// F21.12  TRY_UP ($FB36), TRY_DOWN ($FB5C), TRY_LEFT ($FB70), TRY_RIGHT ($FB87) and their shared end TRY_COMMIT ($FB44): can object n turn into direction d
// (0 right, 1 left, 2 down, 3 up) from the cell it was located in (CELL_ROW, CELL_INDEX, F21.7)? The maze wall table of the cartridge (at $FE13, one
// byte per cell: bit 7 wall, bit 6 up allowed, bit 5 down allowed, F10) decides; the first and last row, and two cells of row 3 (the tunnel ends),
// are closed. A ghost (n > 0) may not turn straight back. If the turn is allowed the state is set (direction bits and the "step due" bit) and the
// ORIGINAL ROUTINE DROPS ITS CALLER'S RETURN (the caller is left, too: the caller's caller continues); otherwise it returns normally.
enum class TryExit { AtEdge, Wall, TurnBack, Taken };
struct TryResult {
  TryExit exit;
  uint8_t row, cell;
  uint8_t tableByte;     // the wall table byte that was read (not for AtEdge)
  uint8_t stateBits;     // bits 7-6 of the state before (for the turn-back test of ghosts)
};
TryResult tryDirection(Ram& ram, const Cartridge& cart, uint8_t object, uint8_t direction);

// F21.13  TRY_PAIR_1 ($FAFC) with TRY_PAIR_UD ($FB05), and TRY_PAIR_2 ($FB19) with TRY_PAIR_LR ($FB22): after a random step, try two directions of one
// axis in an order the random number picks (vertical pair: up then down when the result is 0, else down then up; horizontal pair: left then right when
// 0, else right then left). The first turn that is allowed is taken (and, as in F21.12, ends the caller too); if neither is, the routine returns.
// `rngResultIsZero` is the Z flag the random step left (A = 0).
struct PairResult {
  uint8_t count;        // how many directions were tried (1 or 2)
  uint8_t dir[2];       // the directions, in the order tried
  TryResult res[2];     // their results
  bool taken;           // the last one tried was taken
};
PairResult tryPair(Ram& ram, const Cartridge& cart, uint8_t object, bool vertical, bool rngResultIsZero);

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
AnimResult animateObject(Ram& ram, const Cartridge& cart, uint8_t object, uint8_t sprite);

}  // namespace game
