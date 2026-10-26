// What Pac-Man and the ghosts share: where an object stands in the maze grid, moving it one pixel, setting its direction, asking whether it may turn, entering the openings.
// (Stage G4.2: moved out of routines.hpp. The functions are the F21 rewrites of the original routines named in their comments.)
#pragma once
#include <cstdint>

#include "../data/cartridge.hpp"
#include "../state/state.hpp"

namespace game {

// F21.4  WRAP_TO_BOTTOM ($FD26) and WRAP_TO_TOP ($FD32), with their shared end WRAP_SET_Y ($FD3C): Pac-Man or a ghost (object 0-4) enters the opening
// in the bottom or top wall of the maze. The direction bits of its state are set (bottom: $30, top: $F0; the low four bits are kept) and its y
// is set to the start of the opening animation ($51 below the maze, $0B above it).
void wrapToBottom(GameState& st, uint8_t object);
void wrapToTop(GameState& st, uint8_t object);

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
Location locateObject(GameState& st, uint8_t object);

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
Move moveObject(GameState& st, uint8_t object);

// F21.9  SET_DIRECTION ($F9E5): stores direction code d (0 right, 1 left, 2 down, 3 up) in bits 7-6 of the state of object n and sets the "step due"
// bit 4 (the original takes both from the table TBL_FE63, whose four bytes are (d << 6) or $10), then falls into MOVE_OBJECT: the object moves at once.
// Returns what moveObject returned.
Move setDirection(GameState& st, uint8_t object, uint8_t direction);

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
TryResult tryDirection(GameState& st, const Cartridge& cart, uint8_t object, uint8_t direction);

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
PairResult tryPair(GameState& st, const Cartridge& cart, uint8_t object, bool vertical, bool rngResultIsZero);

}  // namespace game
