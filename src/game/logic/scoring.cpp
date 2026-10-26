#include "scoring.hpp"
#include "../data/rom_tables.hpp"
#include "../state/cycles.hpp"

namespace game {

namespace {
// The decimal-mode ADC of the 6502 (NMOS): two digits at a time.
Bcd bcdAdd(uint8_t a, uint8_t b, bool carryIn) {
  unsigned lo = (a & 0x0F) + (b & 0x0F) + (carryIn ? 1 : 0);
  if (lo > 9) lo += 6;
  unsigned hi = (a >> 4) + (b >> 4) + (lo > 0x0F ? 1 : 0);
  Bcd r{};
  r.overflow = ((~(a ^ b) & (a ^ (hi << 4)) & 0x80) != 0);
  if (hi > 9) hi += 6;
  r.carry = hi > 0x0F;
  r.value = static_cast<uint8_t>(((hi << 4) | (lo & 0x0F)) & 0xFF);
  return r;
}
}  // namespace

// ADD_SCORE ($FC45).
Bcd addScore(GameState& st, const Cartridge& cart, uint8_t amount, uint8_t callerX) {
  st.scratch[kTempE1] = callerX;
  const uint8_t player = (st.playerMode & 0x80) ? 1 : 0;
  cy(3 + 2 + 3 + (player ? 2 + 2 : 3));   // STX, LDX #0, LDA, BPL (INX when player two)
  cy(2 + 2 + 4 + 4 + 4 + 4 + 4 + 4 + 4 + 2 + 4 + 2 + 3 + 6);   // SED, CLC, three load-add-store groups, CLD, LDX, RTS
  uint8_t& lo = st.score[player];
  uint8_t& mid = st.score[player + 2];
  uint8_t& hi = st.score[player + 4];
  const Bcd a = bcdAdd(lo, cart.at(rom::kScoreAmountLow + amount), false);
  lo = a.value;
  const Bcd b = bcdAdd(mid, cart.at(rom::kScoreAmountMid + amount), a.carry);
  mid = b.value;
  const Bcd c = bcdAdd(hi, 0, b.carry);
  hi = c.value;
  return c;
}

}  // namespace game
