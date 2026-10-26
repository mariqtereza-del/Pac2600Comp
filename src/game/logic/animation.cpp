#include "animation.hpp"
#include "../data/tables.hpp"
#include "../state/cycles.hpp"
#include "objects.hpp"

namespace game {

// ANIMATE_OBJECT ($FC8B).
AnimResult animateObject(GameState& st, const Cartridge& cart, uint8_t object, uint8_t sprite) {
  AnimResult r{};
  uint8_t& state = st.objState[object];
  uint8_t& y = st.objY[object];
  uint8_t& spriteY = st.scratch[kSprite0Y + sprite];
  if (!(state & 0x10)) {          // waiting: y counts down; at 0 the animation starts (y = $5B if bit 7 of the state is set, else 0)
    y = static_cast<uint8_t>(y - 1);
    if (y != 0) { r.path = AnimPath::Counting; spriteY = 0x70; r.newY = y; r.stateAfter = state; return r; }
    y = (state & 0x80) ? 0x5B : 0x00;
    state |= 0x10;
    r.path = AnimPath::CountDone;
    spriteY = 0x70;
    r.newY = y;
    r.stateAfter = state;
    return r;
  }
  const uint8_t direction = state >> 6;
  auto common = [&]() {           // L_FCB2: the part shared by the two upward cases
    r.newY = y;
    if (y == 0x0B) {
      y = 0;
      spriteY = 0x0B;
      state = static_cast<uint8_t>((state & 0x0F) | 0x80);
      r.path = AnimPath::Reached0B;
    } else if (!((y - 6) & 0x80)) {
      spriteY = y;
      r.path = AnimPath::SpriteY;
    } else {
      st.scratch[kSprite0Lines + sprite] = y;
      r.tableByte = data::kScoreAmounts[7 + y];
      r.ptrIndex = data::kAnimWhichPointer[sprite];
      uint8_t& ptr = st.scratch[kSprite0PtrLo + r.ptrIndex];
      r.ptrOld = ptr;
      ptr = static_cast<uint8_t>(ptr + r.tableByte);
      spriteY = 8;
      r.path = AnimPath::SpritePtr;
    }
    r.stateAfter = state;
  };
  switch (direction) {
    case 0:                       // down the outside, towards $5B
      y = static_cast<uint8_t>(y + 1);
      r.newY = y;
      if (y != 0x5B) { spriteY = y; r.path = AnimPath::RightStep; r.stateAfter = state; return r; }
      state = static_cast<uint8_t>((state & 0x0F) | 0x60);
      y = data::kAnimNextY[sprite];
      spriteY = 0x70;
      r.path = AnimPath::RightEnd;
      r.stateAfter = state;
      return r;
    case 1:                       // up from the bottom
      y = static_cast<uint8_t>(y + 1);
      common();
      return r;
    case 2:                       // back into the maze from below, towards $51
      y = static_cast<uint8_t>(y - 1);
      r.newY = y;
      if (y != 0x51) { spriteY = y; r.path = AnimPath::DownStep; r.stateAfter = state; return r; }
      y = 0x46;
      state = static_cast<uint8_t>((state & 0x0F) | 0xC0);
      spriteY = 0x70;
      r.path = AnimPath::DownEnd;
      r.stateAfter = state;
      return r;
    default:                      // 3: up the outside; below 0 the next phase starts
      y = static_cast<uint8_t>(y - 1);
      if (y & 0x80) {
        state = static_cast<uint8_t>((state & 0x0F) | 0xA0);
        y = data::kAnimNextY[sprite];
        spriteY = 0x70;
        r.path = AnimPath::UpEnd;
        r.newY = y;
        r.stateAfter = state;
        return r;
      }
      common();
      return r;
  }
}

}  // namespace game
