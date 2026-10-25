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

// F21.8 MOVE_OBJECT ($F9EE): the object is in X. Registers and flags the original leaves, by path:
//  - no carry: A = the new accumulator; carry clear; zero/negative from A; overflow from the ADC; X, Y untouched;
//  - a step: Y = +1 or $FF, A = the new coordinate (the ADC result); overflow from that ADC; zero/negative/carry from the last compare that ran
//    (CMP #2, then CMP #$96 for x; for y: the ADC itself when negative, else CMP #$47).
void cmpFlags(Machine& m, uint8_t a, uint8_t v) {
  m.p = static_cast<uint8_t>((m.p & ~0x01) | (a >= v ? 0x01 : 0));
  m.setZN(static_cast<uint8_t>(a - v));
}

void finishMove(Machine& m, const game::Move& mv) {
  const uint8_t object = m.x;
  const uint8_t acc = m.read(static_cast<uint16_t>(0xC0 + object));
  if (!mv.stepped) {
    const uint8_t a0 = static_cast<uint8_t>(mv.oldAcc << 4);
    m.a = acc;
    m.p = static_cast<uint8_t>((m.p & ~0x41) | ((((a0 ^ acc) & (mv.oldAcc ^ acc)) & 0x80) ? 0x40 : 0));
    m.setZN(acc);
    m.rts();
    return;
  }
  const uint8_t r = mv.newCoord;
  m.y = mv.stepValue;
  m.a = r;
  m.p = static_cast<uint8_t>((m.p & ~0x40) | (((mv.stepValue ^ r) & (mv.oldCoord ^ r) & 0x80) ? 0x40 : 0));
  if (!mv.vertical) {
    cmpFlags(m, r, 0x02);
    if (r != 0x02) cmpFlags(m, r, 0x96);
  } else {
    const bool carry = (static_cast<unsigned>(mv.stepValue) + mv.oldCoord) > 0xFF;
    if (r & 0x80) {
      m.p = static_cast<uint8_t>((m.p & ~0x01) | (carry ? 0x01 : 0));
      m.setZN(r);
    } else {
      cmpFlags(m, r, 0x47);
    }
  }
  m.rts();
}

void moveNative(Machine& m) {
  const uint8_t object = m.x;
  game::Move mv{};
  withRam(m, [&](game::Ram& r) { mv = game::moveObject(r, object); });
  finishMove(m, mv);
}

// F21.12 TRY_UP / TRY_DOWN / TRY_LEFT / TRY_RIGHT (+ TRY_COMMIT). The object is in X. Registers and flags by path (see the listing):
//  at the edge / blocked: A, Y and the flags as the instructions before the branch leave them (CMP/CPY, or LDA + ROL for the table byte);
//  turn back refused (ghosts): A = state bits, Y = direction, flags of the CMP with TBL_FE67, normal return;
//  taken: Y = direction, A = the high byte of the return address that PLA PLA throws away, zero/negative from it, carry from the path, and the RTS
//  that follows returns to the caller's caller.
void tryNative(Machine& m, uint8_t dir) {
  const uint8_t object = m.x;
  const uint8_t cin = m.p & 1;
  const uint8_t stateBefore = m.read(static_cast<uint16_t>(0xBB + object));
  const game::Cartridge cart(m.romBytes());
  game::TryResult r{};
  withRam(m, [&](game::Ram& ram) { r = game::tryDirection(ram, cart, object, dir); });
  uint8_t carry = cin;
  const uint8_t t = r.tableByte;
  auto setFlags = [&](uint8_t av, bool c) {
    m.a = av;
    m.p = static_cast<uint8_t>((m.p & ~0x01) | (c ? 1 : 0));
    m.setZN(av);
  };
  auto cmpState = [&](uint8_t reg, uint8_t v) {  // CMP / CPY: carry, zero, negative from reg - v
    m.p = static_cast<uint8_t>((m.p & ~0x01) | (reg >= v ? 1 : 0));
    m.setZN(static_cast<uint8_t>(reg - v));
  };
  switch (dir) {
    case 3: {
      if (r.exit == game::TryExit::AtEdge) { setFlags(r.row, cin); m.rts(); return; }
      const uint8_t a1 = static_cast<uint8_t>((t << 1) | cin);
      carry = t >> 7;
      if (r.exit == game::TryExit::Wall) { setFlags(a1, carry); m.y = r.cell; m.rts(); return; }
      break;
    }
    case 2: {
      if (r.exit == game::TryExit::AtEdge) { m.a = r.row; cmpState(r.row, 7); m.rts(); return; }
      const uint8_t a1 = static_cast<uint8_t>(t << 1);   // the carry into the first ROL is the one left by CMP #7: clear for rows below 7
      const uint8_t c1 = t >> 7;
      const uint8_t a2 = static_cast<uint8_t>((a1 << 1) | c1);
      carry = a1 >> 7;
      if (r.exit == game::TryExit::Wall) { setFlags(a2, carry); m.y = r.cell; m.rts(); return; }
      break;
    }
    default: {
      const uint8_t edgeCell = (dir == 1) ? 0x3E : 0x38;
      const bool cr = (r.row == 3) ? (r.cell >= edgeCell) : (r.row >= 3);
      if (r.exit == game::TryExit::AtEdge) { m.a = 3; m.y = r.cell; cmpState(r.cell, edgeCell); m.rts(); return; }
      carry = cr;
      if (r.exit == game::TryExit::Wall) {
        setFlags(t, cr);
        m.y = static_cast<uint8_t>(dir == 1 ? r.cell - 1 : r.cell + 2);
        m.rts();
        return;
      }
      break;
    }
  }
  // TRY_COMMIT
  m.y = dir;
  m.p = static_cast<uint8_t>((m.p & ~0x01) | carry);
  if (object != 0) {
    const uint8_t rev = static_cast<uint8_t>((dir ^ 1) << 6);
    m.a = stateBefore & 0xC0;
    cmpState(stateBefore & 0xC0, rev);
    if (r.exit == game::TryExit::TurnBack) { m.rts(); return; }
  }
  // taken: PLA PLA (the caller's return is thrown away), then RTS
  ++m.sp;
  ++m.sp;
  const uint8_t high = m.read(static_cast<uint16_t>(0x100 + m.sp));
  m.a = high;
  m.setZN(high);
  m.rts();
}

// F21.11 LIVES_MARKERS ($FC68): the number of lives is in X, the offset (0 or 3) in Y. The original leaves A = the last cartridge byte (zero/negative from it),
// X and Y untouched, carry untouched.
void livesMarkersNative(Machine& m) {
  const game::Cartridge cart(m.romBytes());
  uint8_t last = 0;
  const uint8_t lives = m.x, offset = m.y;
  withRam(m, [&](game::Ram& r) { last = game::livesMarkers(r, cart, lives, offset); });
  m.a = last;
  m.setZN(last);
  m.rts();
}

// F21.10 ADD_SCORE ($FC45): the amount number is in Y, the caller's X is kept. The original leaves A = the new high byte, X as it was (reloaded from the
// scratch byte, which sets zero/negative from X), Y untouched, decimal flag clear, carry and overflow from the last decimal ADC #0.
void addScoreNative(Machine& m) {
  const uint8_t callerX = m.x;
  const game::Cartridge cart(m.romBytes());
  game::Bcd last{};
  withRam(m, [&](game::Ram& r) { last = game::addScore(r, cart, m.y, callerX); });
  m.a = last.value;
  m.p = static_cast<uint8_t>((m.p & ~(0x08 | 0x41)) | (last.carry ? 0x01 : 0) | (last.overflow ? 0x40 : 0));
  m.setZN(callerX);
  m.rts();
}

// F21.9 SET_DIRECTION ($F9E5): the object is in X, the direction code in Y. The original sets the state and then runs the MOVE_OBJECT code, so the
// registers and flags it leaves are MOVE_OBJECT's (shared adapter; the direction code must be 0-3, as at every call of the recordings).
void setDirectionNative(Machine& m) {
  const uint8_t object = m.x;
  const uint8_t direction = m.y & 3;
  game::Move mv{};
  withRam(m, [&](game::Ram& r) { mv = game::setDirection(r, object, direction); });
  finishMove(m, mv);
}

// F21.7 LOCATE_OBJECT ($FBAD): the object is in X. The original leaves A = POS_CODE, Y = the row, X untouched, carry = bit 6 of the cell index,
// zero and negative from the row (its last instruction is ROL CELL_ROW). It also leaves, in the stack byte where its PHP wrote, the flags it saved:
// the flags after LSR of the row (carry = row bit 0, zero = row < 2, negative clear) with the B and U bits set; that byte is part of the memory
// the lockstep tool compares, so the adapter writes it too.
void locateNative(Machine& m) {
  const uint8_t object = m.x;
  game::Location loc{};
  withRam(m, [&](game::Ram& r) { loc = game::locateObject(r, object); });
  // The row loop ends with an SBC #10: it leaves the overflow flag as that subtraction sets it (the flag is kept by the PHP and stays to the end).
  uint8_t flags = m.p;
  if (loc.row != 0) {
    const uint8_t before = static_cast<uint8_t>(loc.yOffset + 10);
    flags = static_cast<uint8_t>((flags & ~0x40) | ((((before ^ 10) & (before ^ loc.yOffset)) & 0x80) ? 0x40 : 0));
  }
  const uint8_t saved = static_cast<uint8_t>(((flags & ~0x83) | (loc.row & 1) | ((loc.row >> 1) == 0 ? 0x02 : 0)) | 0x30);
  m.write(static_cast<uint16_t>(0x100 + m.sp), saved);
  m.a = loc.posCode;
  m.y = loc.row;
  m.p = static_cast<uint8_t>((flags & ~0x01) | loc.cellBit6);
  m.setZN(loc.row);
  m.rts();
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
