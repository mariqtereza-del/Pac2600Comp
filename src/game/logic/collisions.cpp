#include "collisions.hpp"
#include "../data/rom_tables.hpp"
#include "../state/cycles.hpp"
#include "pacman.hpp"
#include "scoring.hpp"
#include "../sound/sound_logic.hpp"

namespace game {

// L_F573: the bonus item, then a power pill. Returns true when the pill took over the rest of the frame (Pac-Man moved, frame finished).
bool bonusOrPill(BottomContext& b) {
  GameState& st = b.st;
  const Cartridge& cart = b.cart;
  const FrameInput& in = b.in;
  // L_F573: the bonus item
  cy(3 + 3 + 2);
  if (in.bonusTouched) {
    cy(2 + 2 + 6);
    addScore(st, cart, 6, 0);
    cy(2 + 6);
    soundStart(st, cart, 1);
    cy(3 + 2 + 3 + 2 + 3 + 3);
    st.gameFlags ^= 0x02;
    st.bonusTimer = 0x12;
  } else {
    cy(3);
    // L_F591: a power pill (only on the four places, with the pill still there)
    cy(2 + 3 + 2);
    uint8_t x = 0;
    bool onPill = false;
    const uint8_t py = st.objY[0], px = st.objX[0];
    bool toGhost = false;
    if (py == 0x05) cy(3);
    else {
      cy(2 + 2);
      if (py != 0x41) { cy(3); toGhost = true; }
      else { cy(2 + 2); x = 1; }
    }
    if (!toGhost) {
      cy(3 + 2);
      if (px == 0x04) cy(3);
      else {
        cy(2 + 2);
        if (px != 0x94) { cy(3); toGhost = true; }
        else { cy(2 + 2 + 2); x = static_cast<uint8_t>(x + 2); }
      }
      if (!toGhost) {
        cy(4 + 2 + 3);
        onPill = true;
        const uint8_t mask = cart.at(rom::kPillMask + x);
        if (!(st.player[0].pillFlags & mask)) { cy(3); onPill = false; }
        else {
          cy(2 + 2 + 2 + 3 + 2 + 3 + 2 + 3 + 2);
          st.player[0].pillFlags = static_cast<uint8_t>((st.player[0].pillFlags & ~mask) | 0xC0);
          const bool difficulty = (st.gameFlags & 0x20) != 0;
          if (!difficulty) cy(3); else cy(2 + 2);
          st.eventTimer = difficulty ? 0x50 : 0xA0;
          cy(3 + 2 + 6);
          addScore(st, cart, 5, x);
          cy(2 + 6);
          soundStart(st, cart, 2);
          cy(3);
          pacmanMove(b);
          return true;
        }
      }
    }
    (void)onPill;
  }
  return false;
}

// L_F5D3: a ghost touched Pac-Man (checked in four frames out of eight): he eats it when the ghosts are scared, else he is caught. Returns true
// when he is caught (frame finished).
bool ghostTouch(BottomContext& b) {
  GameState& st = b.st;
  const Cartridge& cart = b.cart;
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
      const uint8_t x = st.frameCounter & 3;
      uint8_t& ghost = st.objState[x + 1];
      if (st.player[0].pillFlags & 0x80) {
        cy(2 + 4 + 2);
        if (ghost & 0x08) cy(3 + 1);
        else {
          cy(2 + 4 + 2 + 4 + 3 + 2 + 2 + 6);
          ghost |= 0x08;
          const uint8_t y = st.player[0].pillFlags & 3;
          addScore(st, cart, y, x);
          cy(2);
          if (y == 3) cy(3 + 1);
          else { cy(2 + 5); st.player[0].pillFlags = static_cast<uint8_t>(st.player[0].pillFlags + 1); }
          cy(2 + 6);
          soundStart(st, cart, 3);
          cy(2 + 3 + 3);
          st.eatPause = 0x9F;
        }
      } else {
        cy(3 + 1 + 4 + 2);
        if (ghost & 0x08) cy(3);
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
