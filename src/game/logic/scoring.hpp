// Adding to the score of the player in turn, in decimal as the original does.
// (Stage G4.2: moved out of routines.hpp. The functions are the F21 rewrites of the original routines named in their comments.)
#pragma once
#include <cstdint>

#include "../data/cartridge.hpp"
#include "../state/state.hpp"

namespace game {

// F21.10  ADD_SCORE ($FC45): adds the score amount number y (0-3: ghosts 20/40/80/160, 4: wafer 1, 5: power pill 5, 6: bonus item 100) to the score of the
// player in turn (PLAYER_MODE bit 7), in decimal: low byte + TBL_FF06[y], middle byte + TBL_FF0C[y] (cartridge tables), high byte + carry.
// The original keeps the caller's X in the scratch byte TEMP_E1 and gives it back; callerX is that X (the byte stays in the memory).
// Returns the last decimal addition (the high byte) for the adapter's flags.
struct Bcd {
  uint8_t value;
  bool carry;
  bool overflow;
};
Bcd addScore(GameState& st, const Cartridge& cart, uint8_t amount, uint8_t callerX);

}  // namespace game
