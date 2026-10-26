// Reading tip: cy(n) counts processor cycles of the original (sound timing, state/cycles.hpp); the rules of the game are everything else.
#include "bottom.hpp"
#include "collisions.hpp"
#include "flow.hpp"
#include "pacman.hpp"
#include "objects.hpp"
#include "../sound/sound_logic.hpp"
#include "../state/cycles.hpp"

namespace game {

// The bottom logic of the frame ($F4E7-$F7CE). It is written along the listing of the original, with the processor cycles of every instruction counted
// along the path taken (Stage F24.2): the sound registers are written at the end of it, and the exact moment decides which sound samples they change.
// The numbers in the cy() calls are the cycles of the original instructions (2 for a load of a constant, 3 for a zero-page access, 4 for an indexed or
// absolute one, a taken branch 3, a branch not taken 2, one more when a taken branch crosses a page, JSR and RTS 6) as listed by tools/f24_cycles.py.

// L_F7C3: the volumes are silenced, then the sound effects (channel 0) and the long sounds (channel 1 for the sweep)
void playSounds(BottomContext& b) {
  GameState& st = b.st;
  TiaChip& chip = b.chip;
  cy(2 + 3);
  chip.write(chip::AUDV0, 0);
  cy(3);
  chip.write(chip::AUDV1, 0);
  cy(6);
  soundEffects(st, chip);
  cy(2 + 6);
  soundMusic(st, chip);
}

// L_F7BA: the mirror bit of Pac-Man's picture, then the sounds
void finishFrame(BottomContext& b) {
  GameState& st = b.st;
  TiaChip& chip = b.chip;
  cy(3 + 2);
  if (!(st.objState[0] & kObjMirrored)) cy(3);
  else { cy(2 + 2 + 3); chip.write(chip::REFP1, 0x08); }
  playSounds(b);
}

bool bottomLogic(GameState& st, const FrameInput& input, TiaChip& chip) {
  gCycles = 0;   // the writes to the chip are timed by the cycles counted from here (the end of the drawing code)
  BottomContext b{st, input, chip};
  cy(3);                                                 // LDX ATTRACT_SHIFT
  if (st.attractShift != 0) { cy(3 + 3); playSounds(b); return true; }   // the title screen: sound only
  cy(2 + 6);
  locateObject(st, 0);
  cy(3);
  if (st.eatPause & 0x80) {                  // the pause after a ghost was eaten
    cy(2 + 5 + 3);
    st.eatPause = static_cast<uint8_t>(st.eatPause - 1);
    playSounds(b); return true;
  }
  if (eventTimerStep(b)) { playSounds(b); return true; }
  // L_F55B
  cy(3);
  if (st.gameFlags & 0x80) { frozenSpeedSetup(b); playSounds(b); return true; }
  cy(3);
  if (bonusOrPill(b)) return true;
  if (ghostTouch(b)) return true;
  pacmanBottom(b);
  return true;
}

}  // namespace game
