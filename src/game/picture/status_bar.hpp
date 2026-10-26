// The status line under the maze: the life markers. (The score digits are drawn by the picture code, picture/kernel_score.cpp.)
// (Stage G4.3: moved out of routines.hpp. The functions are the F21 rewrites of the original routines named in their comments.)
#pragma once
#include <cstdint>

#include "../chips.hpp"
#include "../data/cartridge.hpp"
#include "../state/state.hpp"

namespace game {

// F21.11  LIVES_MARKERS ($FC68): the three playfield values for the row of life markers of one player, for `lives` markers, stored in the scratch bytes
// TEMP_D2+offset .. TEMP_D4+offset (offset 0 for player 1, 3 for player 2): $40 if there is any life (else 0), then the cartridge bytes at $FF1B+lives
// and $FEEC+lives. Returns the last value stored (the original leaves it in A).
uint8_t livesMarkers(GameState& st, const Cartridge& cart, uint8_t lives, uint8_t offset);

}  // namespace game
