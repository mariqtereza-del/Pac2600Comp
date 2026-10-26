#include "kernel.hpp"

namespace game {

namespace {

// The video chip registers the drawing code writes.
constexpr uint8_t VBLANK = 0x01, NUSIZ0 = 0x04, NUSIZ1 = 0x05, COLUP0 = 0x06, COLUP1 = 0x07, COLUPF = 0x08, COLUBK = 0x09, CTRLPF = 0x0A, REFP1 = 0x0C;
constexpr uint8_t PF0 = 0x0D, PF1 = 0x0E, PF2 = 0x0F, RESP0 = 0x10, RESP1 = 0x11, RESM0 = 0x12, GRP0 = 0x1B, GRP1 = 0x1C, ENAM0 = 0x1D, ENAM1 = 0x1E;
constexpr uint8_t ENABL = 0x1F, HMP0 = 0x20, HMM0 = 0x22, VDELP0 = 0x25, VDELP1 = 0x26, HMOVE = 0x2A, HMCLR = 0x2B;

// Where the beam is: the scanline and the colour clock. The processor takes 3 colour clocks for each cycle. Only the stretches of code that make
// writes are counted cycle by cycle (the original starts every line with a wait for the end of the line, which resets the count).
struct Beam {
  TimedChip& chip;
  int line;
  int clock = 0;
  void adv(int cycles) {
    clock += 3 * cycles;
    while (clock >= 228) { clock -= 228; ++line; }
  }
  void w(uint16_t site, uint8_t reg, uint8_t value, int cycles) {
    adv(cycles);
    chip.write({line, clock, reg, value, site});
  }
  void wsync() {   // STA WSYNC: the write is the third cycle; the processor then waits for the start of the next line
    adv(2);
    ++line;
    clock = 0;
  }
};

}  // namespace

PictureEnd drawPicture(Ram& ram, const Cartridge& cart, int startLine, TimedChip& chip, int kernelStartLine) {
  Beam b{chip, startLine};
  uint8_t A = 0, X = 0, Y = 0;
  auto M = [&](uint8_t addr) -> uint8_t& { return at(ram, addr); };
  auto ptr = [&](uint8_t lo) -> uint16_t { return static_cast<uint16_t>(M(lo) | (M(static_cast<uint8_t>(lo + 1)) << 8)); };
  auto negative = [](int v) { return (v & 0x80) != 0; };

  // The next picture byte of a sprite for the line being drawn, as the original fetches it: A is 0 unless the sprite's first line has been reached
  // (X, the line counter, not below the sprite's y) and it still has lines left; the line count in Y then goes down by one.
  auto fetch0 = [&]() {
    A = 0;
    if (!negative(X - M(ram::SPRITE0_Y))) {
      Y = M(ram::SPRITE0_LINES);
      if (!negative(Y)) { A = cart.at(static_cast<uint16_t>(ptr(ram::SPRITE0_PTR_LO) + Y)); --Y; M(ram::SPRITE0_LINES) = Y; }
    }
  };
  auto fetch1 = [&]() {
    A = 0;
    if (!negative(X - M(ram::SPRITE1_Y))) {
      Y = M(ram::SPRITE1_LINES);
      if (!negative(Y)) { A = cart.at(static_cast<uint16_t>(ptr(ram::SPRITE1_PTR_LO) + Y)); --Y; M(ram::SPRITE1_LINES) = Y; }
    }
  };
  auto crossing = [&](uint8_t pointerLo, uint8_t y) { return ((ptr(pointerLo) & 0xFF) + y) > 0xFF ? 1 : 0; };
  // KERNEL_LINE_P1 ($F053), KERNEL_LINE_P0 ($F06A) and KERNEL_LINE_P1_P0 ($F067): one line for a sprite, ending with the return (6 cycles).
  auto p1Line = [&]() { fetch1(); b.wsync(); b.w(0xF064, GRP1, A, 3); b.adv(6); };
  auto p0Line = [&]() { ++X; fetch0(); b.wsync(); b.w(0xF07C, GRP0, A, 3); b.adv(6); };
  auto p1p0Line = [&]() { p1Line(); p0Line(); };

  // ---- PART 2 ($F07F): the positions of the sprites and of the power-pill missile, on the lines before the picture ----
  b.w(0xF083, NUSIZ0, 0x20, 5);                    // LDA #$20, STA NUSIZ0
  X = 0;
  Y = 0x20;
  A = M(ram::FRAME_COUNTER) & 0x08;
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
    A = static_cast<uint8_t>(1 + M(static_cast<uint8_t>(ram::SPRITE_X + X)));
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

  // ---- PART 4 ($F0CA): the picture. The timer holds the program until line 34. ----
  b.line = kernelStartLine;
  b.clock = 0;
  A = 0;
  b.w(0xF0CC, VBLANK, Y, 3);
  b.w(0xF0CE, HMCLR, A, 3);
  A = M(ram::COLOUR_BG);
  b.adv(3);
  b.w(0xF0D2, COLUBK, A, 3);
  M(ram::ROW_PARITY) = Y;
  --Y;
  M(ram::CELL_INDEX) = Y;
  M(ram::CELL_ROW) = X;
  ++X;
  A = 0x31;
  b.adv(3 + 2 + 3 + 3 + 2 + 2);
  b.w(0xF0DE, CTRLPF, A, 3);
  A = M(ram::COLOUR_MAZE);
  b.adv(3);
  b.w(0xF0E2, COLUPF, A, 3);
  p0Line();
  Y = 0xFF;
  b.adv(2);
  b.w(0xF0E9, PF0, Y, 3);
  b.w(0xF0EB, PF1, Y, 3);
  A = 0x7F;
  b.adv(2);
  b.w(0xF0EF, PF2, A, 3);
  goto L_F313;

L_F0F4:   // the line after the start of a pair of maze lines: the second sprite's picture byte, then the even or odd row
  fetch1();
  b.wsync();
  b.w(0xF105, GRP1, A, 3);
  Y = M(ram::ROW_PARITY);
  --Y;
  if (Y == 0) goto L_F176;
  Y = M(ram::CELL_INDEX);
  A = M(static_cast<uint8_t>(0x88 + Y)) & 0x0F;
  A = cart.at(static_cast<uint16_t>(0xFD47 + A));
  M(ram::TEMP_D4) = A;
  p0Line();
  b.adv(2);
  b.w(0xF11E, ENAM0, 0, 3);
  Y = M(ram::CELL_INDEX);
  A = (M(static_cast<uint8_t>(0x88 + Y)) >> 4) & 0x07;
  M(ram::TEMP_D3) = cart.at(static_cast<uint16_t>(0xFD3F + A));
  p1Line();
  Y = M(ram::CELL_INDEX);
  M(ram::TEMP_D2) = (M(static_cast<uint8_t>(0x88 + Y)) & 0x80) ? 0x50 : 0x10;
  A = M(static_cast<uint8_t>(0x8C + Y)) & 0x07;
  M(ram::TEMP_D7) = cart.at(static_cast<uint16_t>(0xFD63 + A));
  p0Line();
  Y = M(ram::CELL_INDEX);
  A = M(static_cast<uint8_t>(0x8C + Y)) >> 3;
  M(ram::TEMP_D5) = A;
  M(ram::TEMP_D6) = cart.at(static_cast<uint16_t>(0xFD5B + (A & 0x07)));
  p1Line();
  A = (M(ram::TEMP_D5) >> 3) & 0x03;
  M(ram::TEMP_D5) = cart.at(static_cast<uint16_t>(0xFD57 + A));
  goto L_F1F4;

L_F176:
  Y = M(ram::CELL_INDEX);
  A = M(static_cast<uint8_t>(0x90 + Y)) & 0x07;
  M(ram::TEMP_D4) = cart.at(static_cast<uint16_t>(0xFD7B + A));
  p0Line();
  b.adv(2);
  b.w(0xF188, ENAM0, 0, 3);
  A = M(ram::TEMP_E1);
  b.adv(3);
  b.w(0xF18C, ENABL, A, 3);
  Y = M(ram::CELL_INDEX);
  A = (M(static_cast<uint8_t>(0x90 + Y)) >> 3) & 0x0F;
  M(ram::TEMP_D3) = cart.at(static_cast<uint16_t>(0xFD6B + A));
  p1Line();
  Y = M(ram::CELL_INDEX);
  M(ram::TEMP_D2) = (M(static_cast<uint8_t>(0x90 + Y)) & 0x80) ? 0x50 : 0x10;
  A = M(static_cast<uint8_t>(0x94 + Y)) & 0x07;
  M(ram::TEMP_D7) = cart.at(static_cast<uint16_t>(0xFD63 + A));
  p0Line();
  Y = M(ram::CELL_INDEX);
  A = M(static_cast<uint8_t>(0x94 + Y)) >> 3;
  M(ram::TEMP_D5) = A;
  M(ram::TEMP_D6) = cart.at(static_cast<uint16_t>(0xFD83 + (A & 0x07)));
  p1Line();
  A = M(ram::TEMP_E1);
  b.adv(3);
  b.w(0xF1D5, ENAM1, A, 3);
  A = M(ram::TEMP_D5) >> 3;
  M(ram::TEMP_D5) = cart.at(static_cast<uint16_t>(0xFD47 + A));
  goto L_F1F4;

L_F1F4:   // the wall and wafer bit patterns of the two halves of the row are written on this and the next line
  p0Line();
  if (M(ram::CELL_ROW) == 4) {
    M(ram::TEMP_D4) |= 0x40;
    M(ram::TEMP_D5) |= 0x20;
  }
  fetch1();
  b.wsync();
  b.w(0xF21A, GRP1, A, 3);
  A = M(ram::TEMP_D2);
  b.adv(3);
  b.w(0xF21E, PF0, A, 3);
  b.adv(2);
  b.w(0xF222, CTRLPF, 0x30, 3);
  A = M(ram::TEMP_D3);
  b.adv(3);
  b.w(0xF226, PF1, A, 3);
  A = M(ram::TEMP_D4);
  b.adv(3);
  b.w(0xF22A, PF2, A, 3);
  ++X;
  b.adv(2);
  A = M(ram::TEMP_D5);
  b.adv(3);
  b.w(0xF22F, PF0, A, 3);
  b.adv(3);   // CPX SPRITE0_Y
  if (negative(X - M(ram::SPRITE0_Y))) { b.adv(4); goto L_F1E5; }
  b.adv(2);
  A = M(ram::TEMP_D6);
  b.adv(3);
  b.w(0xF237, PF1, A, 3);
  Y = M(ram::SPRITE0_LINES);
  b.adv(3);
  if (negative(Y)) { b.adv(4); goto L_F1EB; }
  b.adv(2);
  A = M(ram::TEMP_D7);
  b.adv(3);
  b.w(0xF23F, PF2, A, 3);
  b.adv(5 + crossing(ram::SPRITE0_PTR_LO, Y));
  A = cart.at(static_cast<uint16_t>(ptr(ram::SPRITE0_PTR_LO) + Y));
  --Y;
  M(ram::SPRITE0_LINES) = Y;
  b.adv(2 + 3);
  goto L_F246;

L_F1E5:
  A = M(ram::TEMP_D6);
  b.adv(3);
  b.w(0xF1E7, PF1, A, 3);
  b.adv(4);   // NOP, NOP
L_F1EB:
  A = M(ram::TEMP_D7);
  b.adv(3);
  b.w(0xF1ED, PF2, A, 3);
  A = 0;
  b.adv(2 + 3);

L_F246:
  b.wsync();
  b.w(0xF248, GRP0, A, 3);
  A = M(ram::TEMP_D2);
  b.adv(3);
  b.w(0xF24C, PF0, A, 3);
  A = M(ram::TEMP_D3);
  b.adv(3);
  b.w(0xF250, PF1, A, 3);
  A = M(ram::TEMP_D4);
  b.adv(3);
  b.w(0xF254, PF2, A, 3);
  b.adv(4);   // NOP, NOP
  A = M(ram::TEMP_D5);
  b.adv(3);
  b.w(0xF25A, PF0, A, 3);
  b.adv(3);   // CPX SPRITE1_Y
  if (negative(X - M(ram::SPRITE1_Y))) {
    b.adv(3);
    A = M(ram::TEMP_D6);
    b.adv(3);
    b.w(0xF27A, PF1, A, 3);
    b.adv(4);   // NOP, NOP
    goto L_F27E;
  }
  b.adv(2);
  A = M(ram::TEMP_D6);
  b.adv(3);
  b.w(0xF262, PF1, A, 3);
  Y = M(ram::SPRITE1_LINES);
  b.adv(3);
  if (negative(Y)) { b.adv(3); goto L_F27E; }
  b.adv(2);
  A = M(ram::TEMP_D7);
  b.adv(3);
  b.w(0xF26A, PF2, A, 3);
  b.adv(5 + crossing(ram::SPRITE1_PTR_LO, Y));
  A = cart.at(static_cast<uint16_t>(ptr(ram::SPRITE1_PTR_LO) + Y));
  --Y;
  M(ram::SPRITE1_LINES) = Y;
  b.adv(2 + 3);
  goto L_F271;

L_F27E:
  A = M(ram::TEMP_D7);
  b.adv(3);
  b.w(0xF280, PF2, A, 3);
  A = 0;
  b.adv(2 + 3);

L_F271:
  b.wsync();
  b.w(0xF273, GRP1, A, 3);
  b.adv(3);   // JMP
  Y = M(ram::ROW_PARITY);
  b.adv(3);
  A = cart.at(static_cast<uint16_t>(0xFE0A + Y));
  b.adv(4);
  b.w(0xF28C, PF0, A, 3);
  A = cart.at(static_cast<uint16_t>(0xFE0C + Y));
  b.adv(4);
  b.w(0xF291, PF1, A, 3);
  b.adv(2);
  b.w(0xF295, CTRLPF, 0x31, 3);
  A = M(ram::TEMP_E3);
  b.adv(3);
  b.w(0xF299, PF2, A, 3);
  A = static_cast<uint8_t>(M(ram::CELL_ROW) + 0xFF);
  if (negative(A)) goto L_F360;
  M(ram::CELL_ROW) = A;
  ++X;
  fetch0();
  b.wsync();
  b.w(0xF2B9, GRP0, A, 3);
  b.adv(2);
  b.w(0xF2BD, ENAM1, 0, 3);
  M(ram::TEMP_E2) = 0;
  A = M(ram::CELL_ROW);
  M(ram::ROW_PARITY) = A & 1;
  M(ram::CELL_INDEX) = static_cast<uint8_t>((A + 1) >> 1);
  p1p0Line();
  Y = 1;
  if (!(M(ram::FRAME_COUNTER) & 0x08)) Y = 3;
  A = M(ram::CELL_ROW);
  if (A == 0 || A == 6) {
    if (A == 6) --Y;
    if (M(ram::PILL_FLAGS) & cart.at(static_cast<uint16_t>(0xFE6C + Y))) M(ram::TEMP_E2) = 0x02;
  }
  p1Line();
  b.adv(2);
  b.w(0xF2F3, ENABL, 0, 3);
  p0Line();
  A = M(ram::TEMP_E2);
  b.adv(3);
  b.w(0xF2FA, ENAM0, A, 3);
  p1Line();
  p0Line();
  Y = M(ram::ROW_PARITY);
  b.adv(3);
  A = cart.at(static_cast<uint16_t>(0xFF41 + Y));
  b.adv(4);
  b.w(0xF307, PF0, A, 3);
  A = cart.at(static_cast<uint16_t>(0xFE0F + Y));
  b.adv(4);
  b.w(0xF30C, PF1, A, 3);
  A = cart.at(static_cast<uint16_t>(0xFE11 + Y));
  b.adv(4);
  b.w(0xF311, PF2, A, 3);

L_F313:   // the lines of a row between its walls: the bonus item bits and the playfield bytes for the next lines
  p1Line();
  A = M(ram::GAME_FLAGS);
  Y = M(ram::CELL_ROW);
  if (Y != 3) A = 0;
  M(ram::TEMP_E1) = A;
  p0Line();
  Y = M(ram::ROW_PARITY);
  A = cart.at(static_cast<uint16_t>(0xFE0D + Y));
  Y = M(ram::CELL_ROW);
  if (Y == 4) A = 0x40;
  M(ram::TEMP_E3) = A;
  p1Line();
  ++X;
  fetch0();
  b.wsync();
  b.w(0xF349, GRP0, A, 3);
  Y = M(ram::ROW_PARITY);
  b.adv(3);
  b.w(0xF34D, GRP0, A, 3);
  A = cart.at(static_cast<uint16_t>(0xFE0A + Y));
  b.adv(4);
  b.w(0xF352, PF0, A, 3);
  A = cart.at(static_cast<uint16_t>(0xFE0C + Y));
  b.adv(4);
  b.w(0xF357, PF1, A, 3);
  A = M(ram::TEMP_E3);
  b.adv(3);
  b.w(0xF35B, PF2, A, 3);
  goto L_F0F4;

L_F360:   // the last row is done: the bottom wall, then the score
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
  A = M(ram::ATTRACT_SHIFT);
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
  {
    // SCORE_DIGITS ($F3EE), called for player 1 (offset 4) and, in a two-player game, for player 2 (offset 5).
    auto scoreByte = [&](uint8_t y) { return M(static_cast<uint8_t>(ram::SCORE_P1_LO + y)); };
    auto scoreDigits = [&](uint8_t offset) {
      uint8_t slot = 8;
      uint8_t y = offset;
      uint8_t& seen = M(ram::TEMP_E1);
      seen = 0;
      b.adv(2 + 2 + 3 + 3);   // LDX #8, LDA #0, STA TEMP_E1, BEQ
      auto shapeFor = [&](uint8_t digit) {
        // INC TEMP_E1 for a digit that is not zero; a leading zero is the blank shape 10
        if (digit == 0) b.adv(3); else { b.adv(2); ++seen; b.adv(5); }
        b.adv(3);
        if (seen != 0) b.adv(3); else { b.adv(2); digit = 10; b.adv(2); }
        b.adv(4 + (digit >= 10 ? 1 : 0));   // the shapes from number 10 on are on the next page: one more cycle
        return cart.at(static_cast<uint16_t>(0xFEF6 + digit));
      };
      bool low = true;   // the first step is the low nibble (L_F422), then high nibble, low nibble, ...
      for (;;) {
        if (!low) {   // L_F3F6: the high nibble
          b.adv(4 + 8 + 2);
          const uint8_t shape = shapeFor(scoreByte(y) >> 4);
          b.adv(3 + 4);
          M(static_cast<uint8_t>(ram::TEMP_D5 + slot)) = shape;
          slot = static_cast<uint8_t>(slot - 2);
          b.adv(4 + 2);   // DEX, DEX, CPX #4
          if (slot == 4) { b.adv(2 + 2 + 3); b.wsync(); b.w(0xF420, COLUBK, 0, 3); }
          else {
            b.adv(3 + 2);   // BNE, TXA
            if (slot != 0) b.adv(3);
            else { b.adv(2 + 3); b.wsync(); b.w(0xF420, COLUBK, M(ram::TEMP_D2), 3); }
          }
        }
        low = false;
        b.adv(3);   // L_F422: STX TEMP_E2
        M(ram::TEMP_E2) = slot;
        b.adv(4 + 2 + 2);
        const uint8_t shape = shapeFor(scoreByte(y) & 0x0F);
        b.adv(3 + 4);
        M(static_cast<uint8_t>(ram::TEMP_D5 + slot)) = shape;
        slot = static_cast<uint8_t>(slot - 2);
        b.adv(4 + 3);   // DEX, DEX, STX TEMP_E2
        M(ram::TEMP_E2) = slot;
        y = static_cast<uint8_t>(y - 2);
        b.adv(4);   // DEY, DEY
        if (!negative(y)) { b.adv(3); continue; }
        b.adv(2);
        break;
      }
      b.adv(3);   // LDA TEMP_E1
      if (seen != 0) b.adv(3);
      else { b.adv(2 + 2 + 3); M(ram::TEMP_D5) = 0xAC; }   // a score of 0 shows one 0
      b.adv(2 + 2);   // LDA #$FE, LDX #9
      for (uint8_t i = 1; i <= 9; i += 2) M(static_cast<uint8_t>(ram::TEMP_D5 + i)) = 0xFE;
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
          return cart.at(static_cast<uint16_t>(ptr(pointerLo) + row));
        };
        A = shapeByte(ram::SPRITE0_PTR_LO);
        b.w(0xF465, GRP1, A, 3);
        A = shapeByte(ram::SPRITE0_LINES);
        b.w(0xF469, GRP0, A, 3);
        A = shapeByte(ram::SPRITE1_Y);
        M(ram::TEMP_E2) = A;
        b.adv(3);
        X = shapeByte(ram::TEMP_D7);
        b.adv(2);
        Y = shapeByte(ram::TEMP_D5);
        b.adv(2);
        A = M(ram::TEMP_E2);
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
    A = M(ram::COLOUR_SCORE_P1);
    b.adv(2 + 3 + 3 + 6);   // LDY #4, LDA, STA TEMP_D2, JSR
    M(ram::TEMP_D2) = A;
    scoreDigits(4);
    A = M(ram::PLAYER_MODE);
    b.adv(3 + 2);
    A = static_cast<uint8_t>(A << 1);
    if (!negative(A)) b.adv(3);
    else {
      b.adv(2);
      Y = 5;
      A = M(ram::COLOUR_SCORE_P2);
      M(ram::TEMP_D2) = A;
      b.adv(2 + 3 + 3 + 6);
      scoreDigits(5);
    }
  }
  b.adv(2);   // LDA #0
  b.w(0xF3E3, VDELP0, 0, 3);
  b.w(0xF3E5, VDELP1, 0, 3);
  b.w(0xF3E7, NUSIZ0, 0, 3);
  b.w(0xF3E9, NUSIZ1, 0, 3);
  b.adv(3);   // JMP

  // ---- the life markers ($F48E) ----
  {
    auto marker = [&](uint8_t lives, uint8_t offset) {   // JSR LIVES_MARKERS
      b.adv(6 + 2);
      if (lives == 0) b.adv(3); else b.adv(2 + 2);
      b.adv(5 + 4 + 5 + 4 + 5 + 6);
      livesMarkers(ram, cart, lives, offset);
    };
    A = 0;
    Y = 0;
    b.adv(2 + 3);   // TAY, LDA PLAYER_MODE
    if (M(ram::PLAYER_MODE) & 0x80) { Y = 3; b.adv(2 + 2); } else b.adv(3);
    b.adv(3);   // LDX LIVES
    marker(M(ram::LIVES), Y);
    b.adv(2);   // LDA #0
    b.wsync();
    b.w(0xF49E, COLUBK, 0, 3);
    b.adv(2 + 3);   // LDY #3, LDA PLAYER_MODE
    if (!(M(ram::PLAYER_MODE) & 0x80)) { Y = 3; b.adv(3); } else { Y = 0; b.adv(2 + 2); }
    b.adv(3);   // LDX LIVES_P2
    marker(M(ram::LIVES_P2), Y);
    b.adv(2);   // LDX #6
    X = 6;
    A = M(ram::COLOUR_SCORE_P2);
    b.adv(3);
    b.w(0xF4B1, COLUP1, A, 3);
    A = M(ram::COLOUR_SCORE_P1);
    b.adv(3);
    b.w(0xF4B5, COLUP0, A, 3);
    for (;;) {
      b.wsync();
      b.adv(2);
      b.w(0xF4BB, CTRLPF, 0x02, 3);
      b.adv(3);
      b.w(0xF4BF, PF0, M(ram::TEMP_D2), 3);
      b.adv(3);
      b.w(0xF4C3, PF1, M(ram::TEMP_D3), 3);
      b.adv(3);
      b.w(0xF4C7, PF2, M(ram::TEMP_D4), 3);
      b.adv(6);   // NOP x3
      b.adv(3);
      b.w(0xF4CE, PF0, M(ram::TEMP_D5), 3);
      b.adv(3);
      b.w(0xF4D2, PF1, M(ram::TEMP_D6), 3);
      b.adv(3);
      b.w(0xF4D6, PF2, M(ram::TEMP_D7), 3);
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
  return {b.line, b.clock};
}

}  // namespace game
