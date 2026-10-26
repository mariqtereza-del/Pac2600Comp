#include "status_bar.hpp"
#include "../data/rom_tables.hpp"
#include "../state/cycles.hpp"

namespace game {

// LIVES_MARKERS ($FC68).
uint8_t livesMarkers(GameState& st, const Cartridge& cart, uint8_t lives, uint8_t offset) {
  st.scratch[kTempD2 + offset] = lives == 0 ? 0 : 0x40;
  st.scratch[kTempD3 + offset] = cart.at(rom::kLivesMarkerPf1 + lives);
  const uint8_t last = cart.at(rom::kLivesMarkerPf2 + lives);
  st.scratch[kTempD4 + offset] = last;
  return last;
}

}  // namespace game
