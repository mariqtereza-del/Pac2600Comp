// (Stage G4.6: cut out of kernel.cpp; the code is the F23.2 translation of the original's drawing code, text unchanged.)
#include "kernel_internal.hpp"
#include "../data/tables.hpp"
#include "status_bar.hpp"

namespace game::kernel {

// L_F360: the last row is done: the bottom wall, then the score line.
void Kernel::bottomWall() {
  ++X;
  fetch0();
  b.wsync();
  b.w(0xF372, GRP0, A, 3);
  p1p0Line();
  p1p0Line();
  p1p0Line();
  A = 0xFF;
  b.adv(2);
  b.w(0xF37F, PF0, A, 3);
  b.w(0xF381, PF1, A, 3);
  A = 0x7F;
  b.adv(2);
  b.w(0xF385, PF2, A, 3);
  p1Line();
  p0Line();
  p1Line();
  b.wsync();
  b.w(0xF39B, PF0, 0, 3);
  b.w(0xF39D, PF1, 0, 3);
  b.w(0xF39F, PF2, 0, 3);
  b.w(0xF3A1, REFP1, 0, 3);
  b.w(0xF3A3, NUSIZ0, 3, 3);
  b.w(0xF3A5, NUSIZ1, 3, 3);
  b.w(0xF3A7, VDELP0, 3, 3);
  b.w(0xF3A9, VDELP1, 3, 3);
  b.adv(6 + 2);   // JSR CLEAR_GRAPHICS, LDA #0
  b.w(0xF485, GRP0, 0, 3);
  b.w(0xF487, GRP1, 0, 3);
  b.w(0xF489, GRP0, 0, 3);
  b.w(0xF48B, GRP1, 0, 3);
  b.adv(6);
  A = st.attractShift;
  b.adv(3);
  b.w(0xF3B0, COLUP1, A, 3);
  b.w(0xF3B2, COLUP0, A, 3);
  X = 7;
  b.adv(2);
  b.wsync();
  for (;;) {   // the delay for the position of the score
    --X;
    b.adv(2);
    if (X != 0) b.adv(3); else { b.adv(2); break; }
  }
  b.adv(2);   // NOP
  b.w(0xF3BC, RESP0, A, 3);
  b.w(0xF3BE, RESP1, A, 3);
  b.adv(2);
  b.w(0xF3C2, HMP0, 0xF0, 3);
  b.wsync();
  b.w(0xF3C6, HMOVE, 0xF0, 3);
  b.w(0xF3C8, COLUBK, X, 3);
}

void Kernel::scoreLine() {
  {
    // SCORE_DIGITS ($F3EE), called for player 1 (offset 4) and, in a two-player game, for player 2 (offset 5).
    auto scoreByte = [&](uint8_t y) { return st.score[y]; };
    auto scoreDigits = [&](uint8_t offset) {
      // ORIGINAL: only five digits are shown (slots 8 to 0); the hundred-thousands digit is never read. Evidence: DECISIONS.md (T-F21-21). Do not fix.
      uint8_t slot = 8;
      uint8_t y = offset;
      uint8_t& seen = st.scratch[kTempE1];
      seen = 0;
      b.adv(2 + 2 + 3 + 3);   // LDX #8, LDA #0, STA TEMP_E1, BEQ
      auto shapeFor = [&](uint8_t digit) {
        // INC TEMP_E1 for a digit that is not zero; a leading zero is the blank shape 10
        if (digit == 0) b.adv(3); else { b.adv(2); ++seen; b.adv(5); }
        b.adv(3);
        if (seen != 0) b.adv(3); else { b.adv(2); digit = 10; b.adv(2); }
        b.adv(4 + (digit >= 10 ? 1 : 0));   // the shapes from number 10 on are on the next page: one more cycle
        return data::kDigitPointerLow[digit];
      };
      bool low = true;   // the first step is the low nibble (L_F422), then high nibble, low nibble, ...
      for (;;) {
        if (!low) {   // L_F3F6: the high nibble
          b.adv(4 + 8 + 2);
          const uint8_t shape = shapeFor(scoreByte(y) >> 4);
          b.adv(3 + 4);
          st.scratch[kTempD5 + slot] = shape;
          slot = static_cast<uint8_t>(slot - 2);
          b.adv(4 + 2);   // DEX, DEX, CPX #4
          if (slot == 4) { b.adv(2 + 2 + 3); b.wsync(); b.w(0xF420, COLUBK, 0, 3); }
          else {
            b.adv(3 + 2);   // BNE, TXA
            if (slot != 0) b.adv(3);
            else { b.adv(2 + 3); b.wsync(); b.w(0xF420, COLUBK, st.scratch[kTempD2], 3); }
          }
        }
        low = false;
        b.adv(3);   // L_F422: STX TEMP_E2
        st.scratch[kTempE2] = slot;
        b.adv(4 + 2 + 2);
        const uint8_t shape = shapeFor(scoreByte(y) & 0x0F);
        b.adv(3 + 4);
        st.scratch[kTempD5 + slot] = shape;
        slot = static_cast<uint8_t>(slot - 2);
        b.adv(4 + 3);   // DEX, DEX, STX TEMP_E2
        st.scratch[kTempE2] = slot;
        y = static_cast<uint8_t>(y - 2);
        b.adv(4);   // DEY, DEY
        if (!negative(y)) { b.adv(3); continue; }
        b.adv(2);
        break;
      }
      b.adv(3);   // LDA TEMP_E1
      if (seen != 0) b.adv(3);
      else { b.adv(2 + 2 + 3); st.scratch[kTempD5] = 0xAC; }   // a score of 0 shows one 0
      b.adv(2 + 2);   // LDA #$FE, LDX #9
      for (uint8_t i = 1; i <= 9; i += 2) st.scratch[kTempD5 + i] = 0xFE;
      b.adv(4 * 11 + 10);
      b.adv(2 + 3);   // LDA #6, STA TEMP_E1
      seen = 6;
      for (;;) {
        const uint8_t row = seen;
        b.adv(3 + 2);   // LDY TEMP_E1, LDA #0
        b.w(0xF45D, GRP0, 0, 3);
        b.w(0xF45F, GRP0, 0, 3);
        b.wsync();
        auto shapeByte = [&](uint8_t pointerLo) {
          b.adv(5 + crossing(pointerLo, row));
          return data::byAddress(static_cast<uint16_t>(ptr(pointerLo) + row));
        };
        A = shapeByte(kSprite0PtrLo);
        b.w(0xF465, GRP1, A, 3);
        A = shapeByte(kSprite0Lines);
        b.w(0xF469, GRP0, A, 3);
        A = shapeByte(kSprite1Y);
        st.scratch[kTempE2] = A;
        b.adv(3);
        X = shapeByte(kTempD7);
        b.adv(2);
        Y = shapeByte(kTempD5);
        b.adv(2);
        A = st.scratch[kTempE2];
        b.adv(3);
        b.w(0xF477, GRP1, A, 3);
        b.w(0xF479, GRP0, X, 3);
        b.w(0xF47B, GRP1, Y, 3);
        b.w(0xF47D, GRP0, Y, 3);
        seen = static_cast<uint8_t>(seen - 1);
        b.adv(5);
        if (!negative(seen)) { b.adv(3); continue; }
        b.adv(2);
        break;
      }
      b.adv(2);   // CLEAR_GRAPHICS
      b.w(0xF485, GRP0, 0, 3);
      b.w(0xF487, GRP1, 0, 3);
      b.w(0xF489, GRP0, 0, 3);
      b.w(0xF48B, GRP1, 0, 3);
      b.adv(6);
    };
    Y = 4;
    A = st.colour[kColourScoreP1];
    b.adv(2 + 3 + 3 + 6);   // LDY #4, LDA, STA TEMP_D2, JSR
    st.scratch[kTempD2] = A;
    scoreDigits(4);   // 4 = the index in score[] of the high byte of player 1's score; the digits are read downwards from there (score[4], [2], [0])
    A = st.playerMode;   // bit 6 = two players: shifted left it becomes the sign bit, so player 2's score is drawn only in a two-player game
    b.adv(3 + 2);
    A = static_cast<uint8_t>(A << 1);
    if (!negative(A)) b.adv(3);
    else {
      b.adv(2);
      Y = 5;
      A = st.colour[kColourScoreP2];
      st.scratch[kTempD2] = A;
      b.adv(2 + 3 + 3 + 6);
      scoreDigits(5);   // 5 = the high byte of player 2's score (score[5], [3], [1])
    }
  }
  b.adv(2);   // LDA #0
  b.w(0xF3E3, VDELP0, 0, 3);
  b.w(0xF3E5, VDELP1, 0, 3);
  b.w(0xF3E7, NUSIZ0, 0, 3);
  b.w(0xF3E9, NUSIZ1, 0, 3);
  b.adv(3);   // JMP
}

void Kernel::lifeMarkers() {
  {
    auto marker = [&](uint8_t lives, uint8_t offset) {   // JSR LIVES_MARKERS
      b.adv(6 + 2);
      if (lives == 0) b.adv(3); else b.adv(2 + 2);
      b.adv(5 + 4 + 5 + 4 + 5 + 6);
      livesMarkers(st, lives, offset);
    };
    A = 0;
    Y = 0;
    b.adv(2 + 3);   // TAY, LDA PLAYER_MODE
    if (st.playerMode & 0x80) { Y = 3; b.adv(2 + 2); } else b.adv(3);
    b.adv(3);   // LDX LIVES
    marker(st.player[0].lives, Y);
    b.adv(2);   // LDA #0
    b.wsync();
    b.w(0xF49E, COLUBK, 0, 3);
    b.adv(2 + 3);   // LDY #3, LDA PLAYER_MODE
    if (!(st.playerMode & 0x80)) { Y = 3; b.adv(3); } else { Y = 0; b.adv(2 + 2); }
    b.adv(3);   // LDX LIVES_P2
    marker(st.player[1].lives, Y);
    b.adv(2);   // LDX #6
    X = 6;
    A = st.colour[kColourScoreP2];
    b.adv(3);
    b.w(0xF4B1, COLUP1, A, 3);
    A = st.colour[kColourScoreP1];
    b.adv(3);
    b.w(0xF4B5, COLUP0, A, 3);
    for (;;) {
      b.wsync();
      b.adv(2);
      b.w(0xF4BB, CTRLPF, 0x02, 3);
      b.adv(3);
      b.w(0xF4BF, PF0, st.scratch[kTempD2], 3);
      b.adv(3);
      b.w(0xF4C3, PF1, st.scratch[kTempD3], 3);
      b.adv(3);
      b.w(0xF4C7, PF2, st.scratch[kTempD4], 3);
      b.adv(6);   // NOP x3
      b.adv(3);
      b.w(0xF4CE, PF0, st.scratch[kTempD5], 3);
      b.adv(3);
      b.w(0xF4D2, PF1, st.scratch[kTempD6], 3);
      b.adv(3);
      b.w(0xF4D6, PF2, st.scratch[kTempD7], 3);
      --X;
      b.adv(2);
      if (X != 0) { b.adv(3); continue; }
      b.adv(2);
      break;
    }
    b.wsync();
    b.w(0xF4DD, CTRLPF, X, 3);
    b.w(0xF4DF, COLUPF, X, 3);
    b.w(0xF4E1, PF0, X, 3);
    b.w(0xF4E3, PF1, X, 3);
    b.w(0xF4E5, PF2, X, 3);
  }
}

}  // namespace game::kernel
