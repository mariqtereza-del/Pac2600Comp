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

}  // namespace game
