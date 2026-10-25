#include "natives.hpp"

#include "../game/routines.hpp"

namespace lockstep {
namespace {

// RNG_STEP ($FBE0): steps the shift register $CB and returns TBL_FE6A[state & 7] & state in A (Stage F14).
// `broken` leaves out the bit that is rotated in (a deliberate mistake used to test the harness).
void rngStep(Machine& m, bool broken) {
  const uint8_t r = m.read(0xCB);
  const uint8_t a0 = static_cast<uint8_t>(static_cast<uint8_t>(r << 1) ^ r);
  const uint8_t carry = (static_cast<uint8_t>(a0 << 1) >> 7) & 1;
  const uint8_t n = static_cast<uint8_t>((r << 1) | (broken ? 0 : carry));
  m.write(0xCB, n);
  m.y = n & 7;
  m.a = m.read(static_cast<uint16_t>(0xFE6A + m.y)) & n;
  m.p = static_cast<uint8_t>((m.p & ~0x01) | (r >> 7));    // the carry left by ROL RNG_STATE
  m.setZN(m.a);
  m.rts();
}

// Hands the memory of the machine to a game routine and writes back what changed (so the writer of each byte is the native routine).
template <typename F>
void withRam(Machine& m, F&& fn) {
  game::Ram before = m.ram();
  game::Ram after = before;
  fn(after);
  for (int i = 0; i < 128; ++i)
    if (after[i] != before[i]) m.write(static_cast<uint16_t>(0x80 + i), after[i]);
}

// F21.5/F21.6 NEW_GAME_SETUP ($FBF4), NEW_MAZE ($FC0C) and PLACE_OBJECTS ($FC21): all three end with LDA #$80 (A = $80, negative set, zero clear), X = $FF (the loop leaves it after
// DEX of 0), Y = 0 (LDY #0 in the loop), carry untouched.
void placeNative(Machine& m, int which) {
  withRam(m, [which](game::Ram& r) { if (which == 2) game::newGameSetup(r); else if (which == 1) game::newMaze(r); else game::placeObjects(r); });
  m.a = 0x80;
  m.x = 0xFF;
  m.y = 0;
  m.setZN(m.a);
  // NEW_GAME_SETUP also ran SEC, SBC #2, CLC, ADC #4 on small numbers: carry and overflow end clear (found by the routine check).
  if (which == 2) m.p = static_cast<uint8_t>(m.p & ~0x41);
  m.rts();
}

// F21.4 WRAP_TO_BOTTOM ($FD26) / WRAP_TO_TOP ($FD32): the object is in X. The original leaves A = the new y ($51 / $0B), X and Y untouched, zero and
// negative clear (LDA of a positive number), carry untouched.
void wrapNative(Machine& m, bool top) {
  const uint8_t object = m.x;
  withRam(m, [object, top](game::Ram& r) { if (top) game::wrapToTop(r, object); else game::wrapToBottom(r, object); });
  m.a = top ? 0x0B : 0x51;
  m.setZN(m.a);
  m.rts();
}

// F21.3 RNG_STEP ($FBE0). Registers the original leaves: A = the returned value, Y = new state and 7, carry = the old bit 7 (the carry out of ROL RNG_STATE),
// zero and negative from A (the last instruction is AND).
void rngStepNative(Machine& m) {
  const uint8_t old = m.read(0xCB);
  uint8_t result = 0;
  withRam(m, [&result](game::Ram& r) { result = game::rngStep(r); });
  m.y = m.read(0xCB) & 7;
  m.a = result;
  m.p = static_cast<uint8_t>((m.p & ~0x01) | (old >> 7));
  m.setZN(m.a);
  m.rts();
}

// F21.1 CLEAR_SCORES ($FC7D). The original ends with A = 0 (LDA #0), X = $FF (the loop leaves X after DEX of 0), Y untouched, carry untouched,
// the flags of the last DEX: negative set, zero clear.
// `variant` 1 = test fixture that forgets LIVES_P2 (found by the memory comparison), 2 = test fixture that leaves X = 0 (found only by the routine check).
void clearScores(Machine& m, int variant = 0) {
  withRam(m, [variant](game::Ram& r) {
    game::clearScores(r);
    if (variant == 1) r[game::ram::LIVES_P2 - 0x80] = 1;
  });
  m.a = 0;
  m.x = (variant == 2) ? 0 : 0xFF;
  m.setZN(m.x);
  m.rts();
}

// F21.2 SWAP_PLAYERS ($FB9F). Registers the original leaves: A = the old first byte of the block of the player in turn ($88), Y = the old first byte of
// the other block ($9C) (the last pair exchanged), X = $FF, flags of the last DEX (negative set, zero clear), carry untouched.
void swapPlayers(Machine& m) {
  const uint8_t oldA = m.read(0x88);
  const uint8_t oldY = m.read(0x9C);
  withRam(m, [](game::Ram& r) { game::swapPlayers(r); });
  m.a = oldA;
  m.y = oldY;
  m.x = 0xFF;
  m.setZN(m.x);
  m.rts();
}

}  // namespace

bool registerNativeByName(Machine& m, const std::string& name) {
  if (name == "rng") { m.registerNative(0xFBE0, "RNG_STEP (native)", [](Machine& mm) { rngStepNative(mm); }); return true; }
  if (name == "rng-broken") { m.registerNative(0xFBE0, "RNG_STEP (native, broken on purpose)", [](Machine& mm) { rngStep(mm, true); }); return true; }
  if (name == "clear-scores") { m.registerNative(0xFC7D, "CLEAR_SCORES (native)", [](Machine& mm) { clearScores(mm); }); return true; }
  if (name == "clear-scores-wrong") { m.registerNative(0xFC7D, "CLEAR_SCORES (native, wrong on purpose)", [](Machine& mm) { clearScores(mm, 1); }); return true; }
  if (name == "clear-scores-badreg") { m.registerNative(0xFC7D, "CLEAR_SCORES (native, register wrong on purpose)", [](Machine& mm) { clearScores(mm, 2); }); return true; }
  if (name == "swap-players") { m.registerNative(0xFB9F, "SWAP_PLAYERS (native)", [](Machine& mm) { swapPlayers(mm); }); return true; }
  if (name == "wrap") {
    m.registerNative(0xFD26, "WRAP_TO_BOTTOM (native)", [](Machine& mm) { wrapNative(mm, false); });
    m.registerNative(0xFD32, "WRAP_TO_TOP (native)", [](Machine& mm) { wrapNative(mm, true); });
    return true;
  }
  if (name == "new-game-setup") { m.registerNative(0xFBF4, "NEW_GAME_SETUP (native)", [](Machine& mm) { placeNative(mm, 2); }); return true; }
  if (name == "new-maze") { m.registerNative(0xFC0C, "NEW_MAZE (native)", [](Machine& mm) { placeNative(mm, 1); }); return true; }
  if (name == "place-objects") { m.registerNative(0xFC21, "PLACE_OBJECTS (native)", [](Machine& mm) { placeNative(mm, 0); }); return true; }
  return false;
}

}  // namespace lockstep
