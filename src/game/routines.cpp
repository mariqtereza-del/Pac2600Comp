#include "routines.hpp"

namespace game {

// CLEAR_SCORES ($FC7D). The original loops X from 5 down to 0 storing 0 at SCORE_LO+X, i.e. the six score bytes $CC-$D1
// (player 1: low, player 2: low, player 1: middle, player 2: middle, player 1: high, player 2: high), then stores 0 in LIVES and LIVES_P2.
void clearScores(Ram& ram) {
  for (uint8_t i = 0; i < 6; ++i) at(ram, ram::SCORE_P1_LO + i) = 0;
  at(ram, ram::LIVES) = 0;
  at(ram, ram::LIVES_P2) = 0;
}

// SWAP_PLAYERS ($FB9F). The original loops X from $13 down to 0, exchanging the bytes MAZE_00+X and MAZE_P2_00+X.
void swapPlayers(Ram& ram) {
  for (uint8_t i = 0; i < 20; ++i) {
    const uint8_t mine = at(ram, ram::MAZE_00 + i);
    at(ram, ram::MAZE_00 + i) = at(ram, ram::MAZE_P2_00 + i);
    at(ram, ram::MAZE_P2_00 + i) = mine;
  }
}

// RNG_STEP ($FBE0). With r = RNG_STATE the original forms x = (r << 1) xor r (8 bits), shifts it left twice and rotates the last bit shifted out,
// which is bit 6 of x, into the low end of r: r = (r << 1) | bit 6 of x. The value returned is TBL_FE6A[r & 7] & r, where the table of the
// original holds the bit masks $80, $40, ... $01, i.e. 0x80 >> (r & 7) (the lockstep tool checks this against the cartridge bytes).
uint8_t rngStep(Ram& ram) {
  const uint8_t r = at(ram, ram::RNG_STATE);
  const uint8_t x = static_cast<uint8_t>((r << 1) ^ r);
  const uint8_t next = static_cast<uint8_t>((r << 1) | ((x >> 6) & 1));
  at(ram, ram::RNG_STATE) = next;
  return static_cast<uint8_t>((0x80 >> (next & 7)) & next);
}

namespace {
// WRAP_SET_Y ($FD3C): the shared end of both routines.
void wrapEnd(Ram& ram, uint8_t object, uint8_t directionBits, uint8_t y) {
  uint8_t& state = at(ram, ram::PACMAN_STATE + object);
  state = static_cast<uint8_t>((state & 0x0F) | directionBits);
  at(ram, ram::PACMAN_Y + object) = y;
}
}  // namespace

void wrapToBottom(Ram& ram, uint8_t object) { wrapEnd(ram, object, 0x30, 0x51); }
void wrapToTop(Ram& ram, uint8_t object) { wrapEnd(ram, object, 0xF0, 0x0B); }

// PLACE_OBJECTS ($FC21). Objects 0-4 are Pac-Man and the four ghosts (x at $B1+n, y at $B6+n, state at $BB+n).
void placeObjects(Ram& ram) {
  at(ram, ram::GAME_FLAGS) |= 0x80;
  at(ram, ram::EVENT_TIMER) = 0x3C;
  for (uint8_t n = 0; n <= 4; ++n) {
    at(ram, ram::PACMAN_X + n) = 0x4C;
    at(ram, ram::PACMAN_Y + n) = 0x1E;
    at(ram, ram::PACMAN_STATE + n) = 0;
  }
  at(ram, ram::PACMAN_Y) = 0x3C;
  at(ram, ram::PACMAN_STATE) = 0x80;
}

// NEW_MAZE ($FC0C). The 16 wafer bytes $88-$97 are set to $FF, except $8E = $7F and $8A = $FE (the two places that start empty, F11).
void newMaze(Ram& ram) {
  for (uint8_t i = 0; i < 16; ++i) at(ram, ram::MAZE_00 + i) = 0xFF;
  at(ram, ram::MAZE_06) = 0x7F;
  at(ram, ram::MAZE_02) = 0xFE;
  at(ram, ram::PILL_FLAGS) = 0x3C;
  placeObjects(ram);
}

}  // namespace game
