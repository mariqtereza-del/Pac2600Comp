// Adapters of the frame-level routines (start-up, frame start, top and bottom logic, ghosts, sound, score line).
// (Stage G5: split out of natives.cpp; the adapters are unchanged.)
#include "natives_internal.hpp"

namespace lockstep {

void soundEffectsNative(Machine& m, bool wrong) {
  if (m.x != 0) { m.fail(); return; }
  MachineChip chip(m, wrong);
  withRam(m, [&](game::GameState& r) { game::soundEffects(r, chip); });
  m.rts();
}

// F21.26 The picture code ($F07F-$F4E6) replaced by nothing, for an experiment only: it jumps straight to the bottom logic at $F4E7 and draws nothing. It is
// not a rewrite and has no call check; the experiment shows which memory bytes the picture code writes and that nothing else depends on them.
void pictureSkipNative(Machine& m) { m.pc = 0xF4E7; }

// F21.25 The start-up ($F000-$F052): the machine starts here; the native piece clears the memory and the chip, sets the first values and goes on at the frame
// start ($F7D6) with the stack pointer at $FF. Contract: stack pointer and arrival address; free stack bytes are ignored.
void startUpNative(Machine& m) {
  MachineChip chip(m, false);
  withRam(m, [&](game::GameState& r) { game::startUp(r, chip); });
  m.sp = 0xFF;
  m.p = static_cast<uint8_t>((m.p | 0x04) & ~0x08);   // SEI, CLD
  m.pc = 0xF7D6;
}

// F21.24 The start of the next frame ($F7D6-$F87A): arrives after the overscan wait, goes on at $F87C with X = 0 (the next instruction writes X to the vertical
// sync). The colour / black-and-white switch is read from the machine. Contract: X, the stack pointer and the arrival address are compared; A, Y and the
// flags are reloaded by the code at $F87C.
void frameStartNative(Machine& m) {
  game::ConsoleInput in;
  in.colour = (m.read(0x282) & 0x08) != 0;
  MachineChip chip(m, false);
  const game::Cartridge cart(m.romBytes());
  withRam(m, [&](game::GameState& r) { game::frameStart(r, cart, in, chip); });
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
  withRam(m, [&](game::GameState& r) { ok = game::bottomLogic(r, cart, in, chip); });
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
  withRam(m, [&](game::GameState& r) { ok = game::topLogic(r, cart, in, chip); });
  if (!ok) { m.fail(); return; }
  m.pc = 0xF07F;
}

// F21.20 PACMAN_KEEP_GOING ($F793): Pac-Man is object 0. Not on a cell, or the turn allowed: a normal return (the main loop then calls MOVE_OBJECT). A wall
// in the way: the original pops its caller's return and continues at $F7BA (no move), which the adapter does by raising the stack pointer by two and
// setting the program counter. Contract: X (kept), stack pointer, return address / arrival address; the registers A, Y and the flags are loaded afresh
// by both continuations; free stack bytes are not compared.
void keepGoingNative(Machine& m) {
  const game::Cartridge cart(m.romBytes());
  game::KeepGoing r = game::KeepGoing::NotOnCell;
  withRam(m, [&](game::GameState& ram) { r = game::pacmanKeepGoing(ram, cart); });
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
  withRam(m, [&](game::GameState& r) { game::soundMusic(r, cart, chip); });
  m.rts();
}

// F21.17 SOUND_START ($FF43): the sound number is in Y. The original leaves A = the blocking bits (refused) or the length, zero/negative from it; X, Y,
// carry and overflow untouched.
void soundStartNative(Machine& m) {
  const game::Cartridge cart(m.romBytes());
  uint8_t a = 0;
  const uint8_t sound = m.y;
  withRam(m, [&](game::GameState& r) { a = game::soundStart(r, cart, sound); });
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
  withRam(m, [&](game::GameState& r) { ok = game::ghostUpdate(r, cart, object); });
  if (!ok) { m.fail(); return; }
  m.rts();
}

// F21.15 GHOST_DECIDE ($FA26): the object is in X. Contract (see Machine::Contract): the only caller is GHOST_UPDATE, which next loads A from memory and
// does not use Y, the carry or the overflow flag; the routine keeps X. So X, the stack pointer and the return address are compared, the other registers
// are not. The routine calls other routines whose old return addresses stay below the stack pointer in the original; those free stack bytes are not
// compared. Everything else (memory above the stack pointer, the live stack) must be identical.
void ghostDecideNative(Machine& m, bool wrong) {
  const uint8_t object = m.x;
  const game::Cartridge cart(m.romBytes());
  bool ok = true;
  withRam(m, [&](game::GameState& r) {
    ok = game::ghostDecide(r, cart, object);
    if (wrong) r.scratch[game::kTempD2] += 1;   // test fixture: a small mistake in a scratch byte
  });
  if (!ok) { m.fail(); return; }
  m.rts();
}

}  // namespace lockstep
