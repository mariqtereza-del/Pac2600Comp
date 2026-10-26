// Reading tip: cy(n) counts processor cycles of the original (sound timing, state/cycles.hpp); the rules of the game are everything else.
#include "frame_start.hpp"
#include "../data/tables.hpp"
#include "../state/cycles.hpp"

namespace game {

// The start of the next frame ($F7D6-$F87A).
void frameStart(GameState& st, const ConsoleInput& in, TiaChip& chip) {
  chip.write(chip::VBLANK, 3);
  chip.write(chip::VSYNC, 3);
  chip.write(chip::CXCLR, 3);
  const uint8_t counter = st.frameCounter;
  const uint8_t eighths = counter >> 3;
  st.scratch[kSprite0PtrLo] = data::kGhostPictures[(eighths >> 1) & 3];
  uint8_t sprite1 = 0;
  bool chosen = false;
  if (st.gameFlags & 0x40) {                         // dying: the picture follows the event timer
    sprite1 = data::kPacmanDyingPictures[(st.eventTimer >> 2) & 0x0F];
    chosen = sprite1 != 0;
  }
  if (!chosen) {
    sprite1 = data::kPacmanPictures[eighths & 3];
    if (st.gameFlags & 0x80) sprite1 = data::kPacmanPictures[0];        // frozen: the first picture
  }
  st.scratch[kSprite1PtrLo] = sprite1;
  uint8_t& frame = st.frameCounter;
  frame = static_cast<uint8_t>(frame + 1);
  if (frame == 0) {
    uint8_t& tick = st.tick256;
    tick = static_cast<uint8_t>(tick + 1);
    bool attractStep = true;
    if (tick != 0) {                                             // the bonus item's timer (F15)
      uint8_t& bonus = st.bonusTimer;
      bonus = static_cast<uint8_t>(bonus - 1);
      if (bonus == 0) {
        uint8_t& flags = st.gameFlags;
        flags ^= 0x02;
        uint8_t length = 0x12;
        if (flags & 0x02) length = (flags & 0x20) ? 2 : 3;
        bonus = length;
      }
      attractStep = st.attractShift != 0;
    }
    if (attractStep) {                                           // the title-screen shift, never 0
      uint8_t& shift = st.attractShift;
      do shift = static_cast<uint8_t>(shift + 1); while (shift == 0);
    }
  }
  st.scratch[kSprite0PtrHi] = 0xFD;
  st.scratch[kSprite1PtrHi] = 0xFD;
  st.scratch[kSprite1Y] = static_cast<uint8_t>(st.objY[0] + 0x0B);
  st.scratch[kSprite0Lines] = 7;
  st.scratch[kSprite1Lines] = 6;
  uint8_t mask = in.colour ? 0xFF : 0x0F;
  if (st.attractShift != 0) mask ^= 0x08;
  st.colourMask = mask;
  st.colour[kColourBg] = 7;
  st.colour[kColourMaze] = 8;
  st.colour[kColourScoreP2] = st.player[1].scoreColour;
}

}  // namespace game
