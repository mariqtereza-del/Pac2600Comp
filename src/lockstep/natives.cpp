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
// F21.18 SOUND_EFFECTS ($FF55) + SOUND_WRITE ($FF8B). The caller has X = 0 (channel 0). The writes go to the chip through Machine::write, where the call
// check records and compares them. Contract: the one caller (the main loop) sets X afresh and does not use A, Y or the flags; the routine makes no calls,
// so the stack is compared as it is. X, the stack pointer and the return address are compared.
class MachineChip : public game::SoundChip {
 public:
  explicit MachineChip(Machine& m, bool wrong) : m_(m), wrong_(wrong) {}
  void write(uint8_t reg, uint8_t value) override { m_.write(reg, (wrong_ && reg == game::chip::AUDF0) ? static_cast<uint8_t>(value + 1) : value); }

 private:
  Machine& m_;
  bool wrong_;
};

void soundEffectsNative(Machine& m, bool wrong = false) {
  if (m.x != 0) { m.fail(); return; }
  MachineChip chip(m, wrong);
  withRam(m, [&](game::Ram& r) { game::soundEffects(r, chip); });
  m.rts();
}

// F21.24 The start of the next frame ($F7D6-$F87A): arrives after the overscan wait, goes on at $F87C with X = 0 (the next instruction writes X to the vertical
// sync). The colour / black-and-white switch is read from the machine. Contract: X, the stack pointer and the arrival address are compared; A, Y and the
// flags are reloaded by the code at $F87C.
void frameStartNative(Machine& m) {
  game::ConsoleInput in;
  in.colour = (m.read(0x282) & 0x08) != 0;
  MachineChip chip(m, false);
  const game::Cartridge cart(m.romBytes());
  withRam(m, [&](game::Ram& r) { game::frameStart(r, cart, in, chip); });
  m.x = 0;
  m.pc = 0xF87C;
}

// F21.23 The bottom logic ($F4E7-$F7CE): arrives after the picture code, goes on at $F7D1 (the wait for the overscan timer). The joystick port and the two
// collision flags are read from the machine (the flags come from the oracle until the renderer exists, F23). Contract: the registers are all reloaded at
// $F7D1, so only the stack pointer and the arrival address are compared; free stack bytes are ignored.
void bottomLogicNative(Machine& m) {
  game::FrameInput in;
  in.swcha = m.read(0x280);
  in.bonusTouched = (m.read(0x31) & m.read(0x33) & 0x40) != 0;
  in.ghostTouched = (m.read(0x37) & 0x80) != 0;
  MachineChip chip(m, false);
  const game::Cartridge cart(m.romBytes());
  bool ok = true;
  withRam(m, [&](game::Ram& r) { ok = game::bottomLogic(r, cart, in, chip); });
  if (!ok) { m.fail(); return; }
  m.pc = 0xF7D1;
}

// F21.22 The top logic ($F87E-$F9A7): the code arrives here when the vertical sync has been switched off (the frame boundary is the instruction before) and
// goes on at $F07F. The inputs come from the console registers of the machine. Contract: the registers are all reloaded at $F07F, so only the stack pointer
// and the arrival address are compared; free stack bytes are ignored (the routines called from here leave old return addresses).
void topLogicNative(Machine& m) {
  game::ConsoleInput in;
  in.fire = !(m.read(0x3C) & 0x80);                // INPT4: bit 7 low = pressed
  const uint8_t swchb = m.read(0x282);
  in.reset = !(swchb & 0x01);
  in.select = !(swchb & 0x02);
  in.leftDifficultyA = (swchb & 0x40) != 0;
  in.rightDifficultyA = (swchb & 0x80) != 0;
  MachineChip chip(m, false);
  const game::Cartridge cart(m.romBytes());
  bool ok = true;
  withRam(m, [&](game::Ram& r) { ok = game::topLogic(r, cart, in, chip); });
  if (!ok) { m.fail(); return; }
  m.pc = 0xF07F;
}

// F21.21 SCORE_DIGITS ($F3EE): the offset of the score (4 or 5) is in Y. It ends with CLEAR_GRAPHICS and an RTS. The picture-chip writes are compared in order
// (not their cycle positions: the timing of the picture is proved by the executor of F18 and the renderer of F23). Contract: the two callers load Y and
// A afresh and use neither X nor the flags; the stack pointer and the return address are compared.
void scoreDigitsNative(Machine& m) {
  MachineChip chip(m, false);
  const game::Cartridge cart(m.romBytes());
  const uint8_t offset = m.y;
  withRam(m, [&](game::Ram& r) { game::scoreDigits(r, cart, chip, offset); });
  m.rts();
}

// F21.20 PACMAN_KEEP_GOING ($F793): Pac-Man is object 0. Not on a cell, or the turn allowed: a normal return (the main loop then calls MOVE_OBJECT). A wall
// in the way: the original pops its caller's return and continues at $F7BA (no move), which the adapter does by raising the stack pointer by two and
// setting the program counter. Contract: X (kept), stack pointer, return address / arrival address; the registers A, Y and the flags are loaded afresh
// by both continuations; free stack bytes are not compared.
void keepGoingNative(Machine& m) {
  const game::Cartridge cart(m.romBytes());
  game::KeepGoing r = game::KeepGoing::NotOnCell;
  withRam(m, [&](game::Ram& ram) { r = game::pacmanKeepGoing(ram, cart); });
  if (r == game::KeepGoing::Stands) {
    m.sp = static_cast<uint8_t>(m.sp + 2);
    m.pc = 0xF7BA;
    return;
  }
  m.rts();
}

// F21.19 SOUND_MUSIC ($FF94). The caller has X = 1; the sweep writes channel 1 through it. Contract: the one caller (the main loop, which then waits for the
// timer) uses none of A, X, Y or the flags afterwards; the routine makes no calls. The stack pointer and the return address are compared.
void soundMusicNative(Machine& m) {
  const bool sweep = !(m.read(0xE7) & 0x48) && ((m.read(0x9A) << 1) & 0x80);
  if (sweep && m.x != 1) { m.fail(); return; }
  MachineChip chip(m, false);
  const game::Cartridge cart(m.romBytes());
  withRam(m, [&](game::Ram& r) { game::soundMusic(r, cart, chip); });
  m.rts();
}

// F21.17 SOUND_START ($FF43): the sound number is in Y. The original leaves A = the blocking bits (refused) or the length, zero/negative from it; X, Y,
// carry and overflow untouched.
void soundStartNative(Machine& m) {
  const game::Cartridge cart(m.romBytes());
  uint8_t a = 0;
  const uint8_t sound = m.y;
  withRam(m, [&](game::Ram& r) { a = game::soundStart(r, cart, sound); });
  m.a = a;
  m.setZN(a);
  m.rts();
}

// F21.16 GHOST_UPDATE ($F9AA): the object is in X. Contract: the callers (the top logic, three calls in a row) load A and Y afresh and use neither the
// flags nor the stack bytes below the pointer; X is kept. X, the stack pointer and the return address are compared, with all live memory.
void ghostUpdateNative(Machine& m) {
  const uint8_t object = m.x;
  const game::Cartridge cart(m.romBytes());
  bool ok = true;
  withRam(m, [&](game::Ram& r) { ok = game::ghostUpdate(r, cart, object); });
  if (!ok) { m.fail(); return; }
  m.rts();
}

// F21.15 GHOST_DECIDE ($FA26): the object is in X. Contract (see Machine::Contract): the only caller is GHOST_UPDATE, which next loads A from memory and
// does not use Y, the carry or the overflow flag; the routine keeps X. So X, the stack pointer and the return address are compared, the other registers
// are not. The routine calls other routines whose old return addresses stay below the stack pointer in the original; those free stack bytes are not
// compared. Everything else (memory above the stack pointer, the live stack) must be identical.
void ghostDecideNative(Machine& m, bool wrong = false) {
  const uint8_t object = m.x;
  const game::Cartridge cart(m.romBytes());
  bool ok = true;
  withRam(m, [&](game::Ram& r) {
    ok = game::ghostDecide(r, cart, object);
    if (wrong) r[game::ram::TEMP_D2 - 0x80] += 1;   // test fixture: a small mistake in a scratch byte
  });
  if (!ok) { m.fail(); return; }
  m.rts();
}

// F21.14 ANIMATE_OBJECT ($FC8B): the object is in X, the sprite number (0 ghost, 1 Pac-Man) in Y. Registers and flags by path (see the listing):
// the paths that end by storing $70 leave A = $70 (zero/negative clear); the step paths leave A = the new y; the path that sets the picture pointer
// leaves A = 8, X = the pointer index (0 or 2), carry and overflow of the pointer addition; carry is that of the last CMP, or unchanged while waiting.
void animateNative(Machine& m) {
  const uint8_t object = m.x, sprite = m.y;
  const game::Cartridge cart(m.romBytes());
  game::AnimResult r{};
  withRam(m, [&](game::Ram& ram) { r = game::animateObject(ram, cart, object, sprite); });
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

void tryNative(Machine& m, uint8_t dir) {
  const uint8_t object = m.x;
  const uint8_t stateBefore = m.read(static_cast<uint16_t>(0xBB + object));
  const game::Cartridge cart(m.romBytes());
  game::TryResult r{};
  withRam(m, [&](game::Ram& ram) { r = game::tryDirection(ram, cart, object, dir); });
  if (tryRegisters(m, dir, r, object, stateBefore)) dropCallerAndReturn(m);
  else m.rts();
}

// F21.13 TRY_PAIR_1 / TRY_PAIR_2 (with TRY_PAIR_UD / TRY_PAIR_LR inside). The object is in X. First the random step (its registers, as RNG_STEP leaves
// them), then up to two TRY routines in the order its zero flag picks. A taken turn throws away this routine's return too (the same two pops, then the
// RTS goes to the caller's caller). If neither turn is taken, the end of TRY_PAIR_UD/LR (PLA PLA, RTS) gives A = $FB (the high byte of the address
// after the JSR inside TRY_PAIR_1/2, thrown away), N set, Z clear, and returns to the caller; Y and the carry stay as the last TRY routine left them.
void rngRegisters(Machine& m);

void pairNative(Machine& m, bool vertical) {
  rngRegisters(m);
  const bool zero = (m.p & 0x02) != 0;
  const uint8_t object = m.x;
  const uint8_t stateBefore = m.read(static_cast<uint16_t>(0xBB + object));
  const game::Cartridge cart(m.romBytes());
  game::PairResult pr{};
  withRam(m, [&](game::Ram& ram) { pr = game::tryPair(ram, cart, object, vertical, zero); });
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
void rngRegisters(Machine& m) {
  const uint8_t old = m.read(0xCB);
  uint8_t result = 0;
  withRam(m, [&result](game::Ram& r) { result = game::rngStep(r); });
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
  if (name == "score-digits") {
    Machine::Contract c;
    c.regs = 16 | 32;
    m.registerNative(0xF3EE, "SCORE_DIGITS (native)", [](Machine& mm) { scoreDigitsNative(mm); }, c);
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
