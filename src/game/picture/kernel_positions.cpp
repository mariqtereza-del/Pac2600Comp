// (Stage G4.6: cut out of kernel.cpp; the code is the F23.2 translation of the original's drawing code, text unchanged.)
#include "kernel_internal.hpp"

namespace game::kernel {

void Kernel::positions() {
  b.w(0xF083, NUSIZ0, 0x20, 5);                    // LDA #$20, STA NUSIZ0
  X = 0;
  Y = 0x20;
  A = st.frameCounter & 0x08;
  b.adv(4 + 5);                                    // LDX, TAY, LDA, AND
  if (A != 0) b.adv(3);
  else { b.adv(2); X = 9; b.adv(4); }              // BNE not taken, LDX #9, NOP
  for (;;) {                                       // the delay loop: DEX, BPL
    --X;
    b.adv(2);
    if (!negative(X)) b.adv(3); else { b.adv(2); break; }
  }
  b.w(0xF094, RESM0, A, 3);
  b.w(0xF096, HMM0, Y, 3);
  b.w(0xF098, NUSIZ1, Y, 3);
  X = 1;
  b.adv(2);
  for (;;) {                                       // L_F09C: object X (1 = Pac-Man, 0 = the ghost)
    b.adv(2 + 2 + 4);                              // LDA #1, CLC, ADC SPRITE_X,X
    A = static_cast<uint8_t>(1 + (X == 0 ? st.spriteX : st.objX[X - 1]));
    Y = 2;
    b.adv(2 + 2);                                  // LDY #2, SEC
    bool carry = true;
    do {
      ++Y;
      const int r = A - 15 - (carry ? 0 : 1);
      carry = r >= 0;
      A = static_cast<uint8_t>(r);
      b.adv(2 + 2);                                // INY, SBC #15
      b.adv(carry ? 3 : 2);                        // BCS
    } while (carry);
    A = static_cast<uint8_t>(A ^ 0xFF);
    A = static_cast<uint8_t>(A - 6 - 1);           // SBC #6 with the carry clear
    A = static_cast<uint8_t>(A << 4);
    b.adv(2 + 2 + 8);                              // EOR, SBC, ASL x4
    b.wsync();
    for (;;) {                                     // the delay loop for the coarse position
      --Y;
      b.adv(2);
      if (!negative(Y)) b.adv(3); else { b.adv(2); break; }
    }
    b.w(0xF0B6, static_cast<uint8_t>(RESP0 + X), A, 4);
    b.w(0xF0B8, static_cast<uint8_t>(HMP0 + X), A, 4);
    --X;
    b.adv(2);
    if (!negative(X)) { b.adv(3); continue; }
    b.adv(2);
    break;
  }
  b.wsync();
  b.w(0xF0BF, HMOVE, A, 3);
  Y = 1;
  X = 7;
}

}  // namespace game::kernel
