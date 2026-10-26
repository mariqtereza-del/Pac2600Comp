// Shared by the adapters that let the lockstep machine run natively written routines (split from natives.cpp in Stage G5).
// (Stage G5: split out of natives.cpp; the adapters are unchanged.)
#pragma once
#include "machine.hpp"

#include "../game/logic/players.hpp"
#include "../game/logic/maze.hpp"
#include "../game/logic/objects.hpp"
#include "../game/logic/animation.hpp"
#include "../game/logic/scoring.hpp"
#include "../game/logic/ghosts.hpp"
#include "../game/logic/flow.hpp"
#include "../game/logic/collisions.hpp"
#include "../game/logic/pacman.hpp"
#include "../game/logic/bottom.hpp"
#include "../game/logic/top.hpp"
#include "../game/logic/frame_start.hpp"
#include "../game/picture/status_bar.hpp"
#include "../game/sound/sound_logic.hpp"
#include "../game/chips.hpp"
#include "../game/inputs.hpp"
#include "../game/state/cycles.hpp"
#include "../testsupport/state_bytes.hpp"

namespace lockstep {

// Hands the memory of the machine to a game routine and writes back what changed (so the writer of each byte is the native routine).
template <typename F>
void withRam(Machine& m, F&& fn) {
  const testsupport::Bytes before = m.ram();
  game::GameState state = testsupport::fromBytes(before);
  fn(state);
  const testsupport::Bytes after = testsupport::toBytes(state);
  for (int i = 0; i < 128; ++i)
    if (after[i] != before[i]) m.write(static_cast<uint16_t>(0x80 + i), after[i]);
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

void moveNative(Machine& m);
void animateNative(Machine& m);
void tryNative(Machine& m, uint8_t dir);
void pairNative(Machine& m, bool vertical);
void livesMarkersNative(Machine& m);
void addScoreNative(Machine& m);
void setDirectionNative(Machine& m);
void locateNative(Machine& m);
void placeNative(Machine& m, int which);
void wrapNative(Machine& m, bool top);
void rngRegisters(Machine& m);
void rngStepNative(Machine& m);
void clearScores(Machine& m, int variant = 0);
void swapPlayers(Machine& m);
void soundEffectsNative(Machine& m, bool wrong = false);
void pictureSkipNative(Machine& m);
void startUpNative(Machine& m);
void frameStartNative(Machine& m);
void bottomLogicNative(Machine& m);
void topLogicNative(Machine& m);
void keepGoingNative(Machine& m);
void soundMusicNative(Machine& m);
void soundStartNative(Machine& m);
void ghostUpdateNative(Machine& m);
void ghostDecideNative(Machine& m, bool wrong = false);

}  // namespace lockstep
