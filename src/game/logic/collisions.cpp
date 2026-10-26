// Reading tip: cy(n) counts processor cycles of the original (sound timing, state/cycles.hpp); the rules of the game are everything else.
#include "collisions.hpp"
#include "../data/tables.hpp"
#include "../state/cycles.hpp"
#include "pacman.hpp"
#include "scoring.hpp"
#include "../sound/sound_logic.hpp"

namespace game {

// L_F573: the bonus item, then a power pill. Returns true when the pill took over the rest of the frame (Pac-Man moved, frame finished).
bool bonusOrPill(BottomContext& b) {
  GameState& st = b.st;
  const FrameInput& in = b.in;
  // L_F573: the bonus item
  cy(3 + 3 + 2);
  if (in.bonusTouched) {
    cy(2 + 2 + 6);
    addScore(st, 6, 0);   // amount number 6: the bonus item, 100 points
    cy(2 + 6);
    soundStart(st, 1);   // sound 1: the bonus item
    cy(3 + 2 + 3 + 2 + 3 + 3);
    st.gameFlags ^= 0x02;
    st.bonusTimer = 0x12;
  } else {
    cy(3);
    // L_F591: a power pill (only on the four places, with the pill still there)
    cy(2 + 3 + 2);
    uint8_t pillPlace = 0;   // which of the four power pill places Pac-Man stands on (0-3)
    const uint8_t py = st.objY[0], px = st.objX[0];
    bool toGhost = false;
    if (py == 0x05) cy(3);
    else {
      cy(2 + 2);
      if (py != 0x41) { cy(3); toGhost = true; }
      else { cy(2 + 2); pillPlace = 1; }
    }
    if (!toGhost) {
      cy(3 + 2);
      if (px == 0x04) cy(3);
      else {
        cy(2 + 2);
        if (px != 0x94) { cy(3); toGhost = true; }
        else { cy(2 + 2 + 2); pillPlace = static_cast<uint8_t>(pillPlace + 2); }
      }
      if (!toGhost) {
        cy(4 + 2 + 3);
        const uint8_t mask = data::kBitMasksAndReloads[data::kPillMaskAt + pillPlace];   // this pill's bit in the pill flags (bits 2-5)
        if (!(st.player[0].pillFlags & mask)) cy(3);   // the pill was eaten before
        else {
          cy(2 + 2 + 2 + 3 + 2 + 3 + 2 + 3 + 2);
          st.player[0].pillFlags = static_cast<uint8_t>((st.player[0].pillFlags & ~mask) | (kPillScared | kPillScareFirstPart));
          const bool difficulty = (st.gameFlags & 0x20) != 0;
          if (!difficulty) cy(3); else cy(2 + 2);
          st.eventTimer = difficulty ? 0x50 : 0xA0;
          cy(3 + 2 + 6);
          addScore(st, 5, pillPlace);   // amount number 5: a power pill, 5 points (the last argument only keeps the original's register X)
          cy(2 + 6);
          soundStart(st, 2);   // sound 2: a power pill
          cy(3);
          pacmanMove(b);
          return true;
        }
      }
    }
  }
  return false;
}

// L_F5D3: a ghost touched Pac-Man (checked in four frames out of eight): he eats it when the ghosts are scared, else he is caught. Returns true
// when he is caught (frame finished).
bool ghostTouch(BottomContext& b) {
  GameState& st = b.st;
  const FrameInput& in = b.in;
  // L_F5D3: a ghost touched Pac-Man (checked in four frames out of eight)
  cy(3 + 2);
  // ORIGINAL: a ghost touch counts in 4 frames out of 8, one ghost per frame. Evidence: docs\F12_COLLISIONS.md (F12). Do not fix.
  if (st.frameCounter & 4) cy(3 + 1);
  else {
    cy(2 + 3);
    if (!in.ghostTouched) cy(3 + 1);
    else {
      cy(2 + 3 + 2 + 2 + 3);
      const uint8_t ghostIndex = st.frameCounter & 3;   // the ghost of this frame (0-3), the only one that can touch Pac-Man now
      uint8_t& ghost = st.objState[ghostIndex + 1];
      if (st.player[0].pillFlags & kPillScared) {
        cy(2 + 4 + 2);
        if (ghost & kObjEyes) cy(3 + 1);
        else {
          cy(2 + 4 + 2 + 4 + 3 + 2 + 2 + 6);
          ghost |= kObjEyes;
          // The amount number 0-3 (20, 40, 80, 160 points) is the count of ghosts eaten under this pill: it lives in the pill flags (bits 0-1),
          // goes up by one after each ghost (not beyond 3) and is cleared when the scare ends (flow.cpp).
          const uint8_t ghostAmount = st.player[0].pillFlags & kPillGhostsEaten;
          addScore(st, ghostAmount, ghostIndex);
          cy(2);
          if (ghostAmount == 3) cy(3 + 1);
          else { cy(2 + 5); st.player[0].pillFlags = static_cast<uint8_t>(st.player[0].pillFlags + 1); }
          cy(2 + 6);
          soundStart(st, 3);   // sound 3: a ghost eaten
          cy(2 + 3 + 3);
          st.eatPause = 0x9F;
        }
      } else {
        cy(3 + 1 + 4 + 2);
        if (ghost & kObjEyes) cy(3);
        else {                                              // Pac-Man is caught
          cy(2 + 2 + 3 + 3 + 2 + 3 + 3);
          st.gameFlags |= 0xC0;
          st.eventTimer = 0x3F;
          finishFrame(b); return true;
        }
      }
    }
  }
  return false;
}

}  // namespace game
