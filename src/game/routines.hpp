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

}  // namespace game
