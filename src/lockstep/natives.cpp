// Registering the native routines by name for the lockstep machine (the routine rewrites are in natives_frame.cpp and natives_objects.cpp).
#include "natives.hpp"

#include "natives_internal.hpp"

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
  if (name == "picture-skip") { m.registerNative(0xF07F, "PICTURE (skipped for the experiment)", [](Machine& mm) { pictureSkipNative(mm); }); return true; }
  if (name == "start-up") {
    Machine::Contract c;
    c.deadStack = true;
    c.regs = 16 | 32;
    c.stopAt = 0xF7D6;
    m.registerNative(0xF000, "START_UP (native)", [](Machine& mm) { startUpNative(mm); }, c);
    return true;
  }
  if (name == "frame-start") {
    Machine::Contract c;
    c.regs = 2 | 16 | 32;
    c.stopAt = 0xF87C;
    m.registerNative(0xF7D6, "FRAME_START (native)", [](Machine& mm) { frameStartNative(mm); }, c);
    return true;
  }
  if (name == "bottom-logic") {
    Machine::Contract c;
    c.deadStack = true;
    c.regs = 16 | 32;
    c.stopAt = 0xF7D1;
    m.registerNative(0xF4E7, "BOTTOM_LOGIC (native)", [](Machine& mm) { bottomLogicNative(mm); }, c);
    return true;
  }
  if (name == "top-logic") {
    Machine::Contract c;
    c.deadStack = true;
    c.regs = 16 | 32;
    c.stopAt = 0xF07F;
    m.registerNative(0xF87E, "TOP_LOGIC (native)", [](Machine& mm) { topLogicNative(mm); }, c);
    return true;
  }
  if (name == "keep-going") {
    Machine::Contract c;
    c.deadStack = true;
    c.regs = 2 | 16 | 32;
    c.stopAt = 0xF7BA;
    m.registerNative(0xF793, "PACMAN_KEEP_GOING (native)", [](Machine& mm) { keepGoingNative(mm); }, c);
    return true;
  }
  if (name == "sound-music") {
    Machine::Contract c;
    c.regs = 16 | 32;
    m.registerNative(0xFF94, "SOUND_MUSIC (native)", [](Machine& mm) { soundMusicNative(mm); }, c);
    return true;
  }
  if (name == "sound-effects" || name == "sound-effects-wrong") {
    Machine::Contract c;
    c.regs = 2 | 16 | 32;
    const bool wrong = (name == "sound-effects-wrong");
    m.registerNative(0xFF55, wrong ? "SOUND_EFFECTS (native, wrong on purpose)" : "SOUND_EFFECTS (native)", [wrong](Machine& mm) { soundEffectsNative(mm, wrong); }, c);
    return true;
  }
  if (name == "sound-start") { m.registerNative(0xFF43, "SOUND_START (native)", [](Machine& mm) { soundStartNative(mm); }); return true; }
  if (name == "ghost-update") {
    Machine::Contract c;
    c.deadStack = true;
    c.regs = 2 | 16 | 32;
    m.registerNative(0xF9AA, "GHOST_UPDATE (native)", [](Machine& mm) { ghostUpdateNative(mm); }, c);
    return true;
  }
  if (name == "ghost-decide") {
    Machine::Contract c;
    c.deadStack = true;
    c.regs = 2 | 16 | 32;
    m.registerNative(0xFA26, "GHOST_DECIDE (native)", [](Machine& mm) { ghostDecideNative(mm); }, c);
    return true;
  }
  if (name == "ghost-decide-wrong") {
    Machine::Contract c;
    c.deadStack = true;
    c.regs = 2 | 16 | 32;
    m.registerNative(0xFA26, "GHOST_DECIDE (native, wrong on purpose)", [](Machine& mm) { ghostDecideNative(mm, true); }, c);
    return true;
  }
  if (name == "animate") { m.registerNative(0xFC8B, "ANIMATE_OBJECT (native)", [](Machine& mm) { animateNative(mm); }); return true; }
  if (name == "try-pair") {
    m.registerNative(0xFAFC, "TRY_PAIR_1 (native)", [](Machine& mm) { pairNative(mm, true); });
    m.registerNative(0xFB19, "TRY_PAIR_2 (native)", [](Machine& mm) { pairNative(mm, false); });
    return true;
  }
  if (name == "try") {
    m.registerNative(0xFB36, "TRY_UP (native)", [](Machine& mm) { tryNative(mm, 3); });
    m.registerNative(0xFB5C, "TRY_DOWN (native)", [](Machine& mm) { tryNative(mm, 2); });
    m.registerNative(0xFB70, "TRY_LEFT (native)", [](Machine& mm) { tryNative(mm, 1); });
    m.registerNative(0xFB87, "TRY_RIGHT (native)", [](Machine& mm) { tryNative(mm, 0); });
    return true;
  }
  if (name == "lives-markers") { m.registerNative(0xFC68, "LIVES_MARKERS (native)", [](Machine& mm) { livesMarkersNative(mm); }); return true; }
  if (name == "add-score") { m.registerNative(0xFC45, "ADD_SCORE (native)", [](Machine& mm) { addScoreNative(mm); }); return true; }
  if (name == "set-direction") { m.registerNative(0xF9E5, "SET_DIRECTION (native)", [](Machine& mm) { setDirectionNative(mm); }); return true; }
  if (name == "move") { m.registerNative(0xF9EE, "MOVE_OBJECT (native)", [](Machine& mm) { moveNative(mm); }); return true; }
  if (name == "locate") { m.registerNative(0xFBAD, "LOCATE_OBJECT (native)", [](Machine& mm) { locateNative(mm); }); return true; }
  if (name == "new-maze") { m.registerNative(0xFC0C, "NEW_MAZE (native)", [](Machine& mm) { placeNative(mm, 1); }); return true; }
  if (name == "place-objects") { m.registerNative(0xFC21, "PLACE_OBJECTS (native)", [](Machine& mm) { placeNative(mm, 0); }); return true; }
  return false;
}

}  // namespace lockstep
