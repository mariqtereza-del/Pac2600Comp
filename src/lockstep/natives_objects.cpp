// Adapters of the object-level routines (movement, turning, animation, score, players, random number).
// (Stage G5: split out of natives.cpp; the adapters are unchanged.)
#include "natives_internal.hpp"

namespace lockstep {

namespace {

// F21.8 MOVE_OBJECT ($F9EE): the object is in X. Registers and flags the original leaves, by path:
//  - no carry: A = the new accumulator; carry clear; zero/negative from A; overflow from the ADC; X, Y untouched;
//  - a step: Y = +1 or $FF, A = the new coordinate (the ADC result); overflow from that ADC; zero/negative/carry from the last compare that ran
//    (CMP #2, then CMP #$96 for x; for y: the ADC itself when negative, else CMP #$47).
void cmpFlags(Machine& m, uint8_t a, uint8_t v) {
  m.p = static_cast<uint8_t>((m.p & ~0x01) | (a >= v ? 0x01 : 0));
  m.setZN(static_cast<uint8_t>(a - v));
}

}  // namespace

namespace {

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

}  // namespace

void moveNative(Machine& m) {
  const uint8_t object = m.x;
  game::Move mv{};
  withRam(m, [&](game::GameState& r) { mv = game::moveObject(r, object); });
  finishMove(m, mv);
}

// F21.14 ANIMATE_OBJECT ($FC8B): the object is in X, the sprite number (0 ghost, 1 Pac-Man) in Y. Registers and flags by path (see the listing):
// the paths that end by storing $70 leave A = $70 (zero/negative clear); the step paths leave A = the new y; the path that sets the picture pointer
// leaves A = 8, X = the pointer index (0 or 2), carry and overflow of the pointer addition; carry is that of the last CMP, or unchanged while waiting.
void animateNative(Machine& m) {
  const uint8_t object = m.x, sprite = m.y;
  const game::Cartridge cart(m.romBytes());
  game::AnimResult r{};
  withRam(m, [&](game::GameState& ram) { r = game::animateObject(ram, cart, object, sprite); });
  auto carry = [&](bool c) { m.p = static_cast<uint8_t>((m.p & ~0x01) | (c ? 1 : 0)); };
  auto finish = [&](uint8_t a) { m.a = a; m.setZN(a); };
  switch (r.path) {
    case game::AnimPath::Counting:
    case game::AnimPath::CountDone: finish(0x70); break;
    case game::AnimPath::RightStep: finish(r.newY); carry(0x5B >= r.newY); break;
    case game::AnimPath::RightEnd: finish(0x70); carry(true); break;
    case game::AnimPath::DownStep: finish(r.newY); carry(0x51 >= r.newY); break;
    case game::AnimPath::DownEnd: finish(0x70); carry(true); break;
    case game::AnimPath::UpEnd: finish(0x70); carry(true); break;
    case game::AnimPath::Reached0B: finish(r.stateAfter); carry(true); break;
    case game::AnimPath::SpriteY: finish(r.newY); carry(true); break;
    case game::AnimPath::SpritePtr: {
      const unsigned sum = static_cast<unsigned>(r.tableByte) + r.ptrOld;
      const uint8_t res = static_cast<uint8_t>(sum);
      finish(8);
      m.x = r.ptrIndex;
      carry(sum > 0xFF);
      m.p = static_cast<uint8_t>((m.p & ~0x40) | ((((r.tableByte ^ res) & (r.ptrOld ^ res)) & 0x80) ? 0x40 : 0));
      break;
    }
  }
  m.rts();
}

namespace {

// The registers and flags of one TRY routine up to the point where it returns or (taken) is about to throw its caller's return away. Returns true if
// the turn is taken (then A, N and Z are still to be set by the two pops); false means a normal return is next.
bool tryRegisters(Machine& m, uint8_t dir, const game::TryResult& r, uint8_t object, uint8_t stateBefore) {
  const uint8_t cin = m.p & 1;
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
      if (r.exit == game::TryExit::AtEdge) { setFlags(r.row, cin); return false; }
      const uint8_t a1 = static_cast<uint8_t>((t << 1) | cin);
      carry = t >> 7;
      if (r.exit == game::TryExit::Wall) { setFlags(a1, carry); m.y = r.cell; return false; }
      break;
    }
    case 2: {
      if (r.exit == game::TryExit::AtEdge) { m.a = r.row; cmpState(r.row, 7); return false; }
      const uint8_t a1 = static_cast<uint8_t>(t << 1);   // the carry into the first ROL is the one left by CMP #7: clear for rows below 7
      const uint8_t c1 = t >> 7;
      const uint8_t a2 = static_cast<uint8_t>((a1 << 1) | c1);
      carry = a1 >> 7;
      if (r.exit == game::TryExit::Wall) { setFlags(a2, carry); m.y = r.cell; return false; }
      break;
    }
    default: {
      const uint8_t edgeCell = (dir == 1) ? 0x3E : 0x38;
      const bool cr = (r.row == 3) ? (r.cell >= edgeCell) : (r.row >= 3);
      if (r.exit == game::TryExit::AtEdge) { m.a = 3; m.y = r.cell; cmpState(r.cell, edgeCell); return false; }
      carry = cr;
      if (r.exit == game::TryExit::Wall) {
        setFlags(t, cr);
        m.y = static_cast<uint8_t>(dir == 1 ? r.cell - 1 : r.cell + 2);
        return false;
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
    if (r.exit == game::TryExit::TurnBack) return false;
  }
  return true;
}

}  // namespace

namespace {

// The two pops (PLA PLA) of a taken turn: the return address on top of the stack is thrown away, A gets its high byte; the RTS that follows goes to
// the caller's caller.
void dropCallerAndReturn(Machine& m) {
  ++m.sp;
  ++m.sp;
  const uint8_t high = m.read(static_cast<uint16_t>(0x100 + m.sp));
  m.a = high;
  m.setZN(high);
  m.rts();
}

}  // namespace

void tryNative(Machine& m, uint8_t dir) {
  const uint8_t object = m.x;
  const uint8_t stateBefore = m.read(static_cast<uint16_t>(0xBB + object));
  const game::Cartridge cart(m.romBytes());
  game::TryResult r{};
  withRam(m, [&](game::GameState& ram) { r = game::tryDirection(ram, cart, object, dir); });
  if (tryRegisters(m, dir, r, object, stateBefore)) dropCallerAndReturn(m);
  else m.rts();
}

// F21.13 TRY_PAIR_1 / TRY_PAIR_2 (with TRY_PAIR_UD / TRY_PAIR_LR inside). The object is in X. First the random step (its registers, as RNG_STEP leaves
// them), then up to two TRY routines in the order its zero flag picks. A taken turn throws away this routine's return too (the same two pops, then the
// RTS goes to the caller's caller). If neither turn is taken, the end of TRY_PAIR_UD/LR (PLA PLA, RTS) gives A = $FB (the high byte of the address
// after the JSR inside TRY_PAIR_1/2, thrown away), N set, Z clear, and returns to the caller; Y and the carry stay as the last TRY routine left them.

void pairNative(Machine& m, bool vertical) {
  rngRegisters(m);
  const bool zero = (m.p & 0x02) != 0;
  const uint8_t object = m.x;
  const uint8_t stateBefore = m.read(static_cast<uint16_t>(0xBB + object));
  const game::Cartridge cart(m.romBytes());
  game::PairResult pr{};
  withRam(m, [&](game::GameState& ram) { pr = game::tryPair(ram, cart, object, vertical, zero); });
  // The original pushes return addresses with its JSRs and leaves them in the stack bytes (part of the compared memory): the address after the JSR
  // into TRY_PAIR_UD / TRY_PAIR_LR at the top, and under it the address after the JSR into each TRY routine that was run (the last one stays).
  const uint16_t sp0 = m.sp;
  auto pushed = [&](int slot, uint16_t ret) {
    const uint16_t base = static_cast<uint16_t>(0x100 + sp0 - 2 * slot);
    m.write(base, static_cast<uint8_t>(ret >> 8));
    m.write(static_cast<uint16_t>(base - 1), static_cast<uint8_t>(ret & 0xFF));
  };
  pushed(0, vertical ? 0xFB01 : 0xFB1E);
  static const uint16_t kRet[2][2][2] = {{{0xFB26, 0xFB29}, {0xFB2F, 0xFB32}}, {{0xFB09, 0xFB0C}, {0xFB12, 0xFB15}}};
  for (int i = 0; i < pr.count; ++i) {
    pushed(1, kRet[vertical ? 1 : 0][zero ? 0 : 1][i]);
    if (tryRegisters(m, pr.dir[i], pr.res[i], object, stateBefore)) { dropCallerAndReturn(m); return; }
  }
  m.a = 0xFB;
  m.setZN(m.a);
  m.rts();
}

// F21.11 LIVES_MARKERS ($FC68): the number of lives is in X, the offset (0 or 3) in Y. The original leaves A = the last cartridge byte (zero/negative from it),
// X and Y untouched, carry untouched.
void livesMarkersNative(Machine& m) {
  const game::Cartridge cart(m.romBytes());
  uint8_t last = 0;
  const uint8_t lives = m.x, offset = m.y;
  withRam(m, [&](game::GameState& r) { last = game::livesMarkers(r, cart, lives, offset); });
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
  withRam(m, [&](game::GameState& r) { last = game::addScore(r, cart, m.y, callerX); });
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
  withRam(m, [&](game::GameState& r) { mv = game::setDirection(r, object, direction); });
  finishMove(m, mv);
}

// F21.7 LOCATE_OBJECT ($FBAD): the object is in X. The original leaves A = POS_CODE, Y = the row, X untouched, carry = bit 6 of the cell index,
// zero and negative from the row (its last instruction is ROL CELL_ROW). It also leaves, in the stack byte where its PHP wrote, the flags it saved:
// the flags after LSR of the row (carry = row bit 0, zero = row < 2, negative clear) with the B and U bits set; that byte is part of the memory
// the lockstep tool compares, so the adapter writes it too.
void locateNative(Machine& m) {
  const uint8_t object = m.x;
  game::Location loc{};
  withRam(m, [&](game::GameState& r) { loc = game::locateObject(r, object); });
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
  withRam(m, [which](game::GameState& r) { if (which == 2) game::newGameSetup(r); else if (which == 1) game::newMaze(r); else game::placeObjects(r); });
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
  withRam(m, [object, top](game::GameState& r) { if (top) game::wrapToTop(r, object); else game::wrapToBottom(r, object); });
  m.a = top ? 0x0B : 0x51;
  m.setZN(m.a);
  m.rts();
}

// F21.3 RNG_STEP ($FBE0). Registers the original leaves: A = the returned value, Y = new state and 7, carry = the old bit 7 (the carry out of ROL RNG_STATE),
// zero and negative from A (the last instruction is AND).
void rngRegisters(Machine& m) {
  const uint8_t old = m.read(0xCB);
  uint8_t result = 0;
  withRam(m, [&result](game::GameState& r) { result = game::rngStep(r); });
  m.y = m.read(0xCB) & 7;
  m.a = result;
  m.p = static_cast<uint8_t>((m.p & ~0x01) | (old >> 7));
  m.setZN(m.a);
}

void rngStepNative(Machine& m) {
  rngRegisters(m);
  m.rts();
}

// F21.1 CLEAR_SCORES ($FC7D). The original ends with A = 0 (LDA #0), X = $FF (the loop leaves X after DEX of 0), Y untouched, carry untouched,
// the flags of the last DEX: negative set, zero clear.
// `variant` 1 = test fixture that forgets LIVES_P2 (found by the memory comparison), 2 = test fixture that leaves X = 0 (found only by the routine check).
void clearScores(Machine& m, int variant) {
  withRam(m, [variant](game::GameState& r) {
    game::clearScores(r);
    if (variant == 1) r.player[1].lives = 1;
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
  withRam(m, [](game::GameState& r) { game::swapPlayers(r); });
  m.a = oldA;
  m.y = oldY;
  m.x = 0xFF;
  m.setZN(m.x);
  m.rts();
}

}  // namespace lockstep
