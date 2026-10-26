// (Stage G4.6: cut out of kernel.cpp; the code is the F23.2 translation of the original's drawing code, text unchanged.)
#include "kernel_internal.hpp"
#include "../data/tables.hpp"

namespace game::kernel {

void Kernel::mazeRows() {
  // ---- PART 4 ($F0CA): the picture. The timer holds the program until line 34. ----
  b.line = kernelStartLine;
  b.clock = 0;
  A = 0;
  b.w(0xF0CC, VBLANK, Y, 3);
  b.w(0xF0CE, HMCLR, A, 3);
  A = st.colour[kColourBg];
  b.adv(3);
  b.w(0xF0D2, COLUBK, A, 3);
  st.rowParity = Y;
  --Y;
  st.cellIndex = Y;
  st.cellRow = X;
  ++X;
  A = 0x31;
  b.adv(3 + 2 + 3 + 3 + 2 + 2);
  b.w(0xF0DE, CTRLPF, A, 3);
  A = st.colour[kColourMaze];
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
  Y = st.rowParity;
  --Y;
  if (Y == 0) goto L_F176;
  Y = st.cellIndex;
  A = st.player[0].maze[Y] & 0x0F;
  A = data::kMazeRowFD47[A];
  st.scratch[kTempD4] = A;
  p0Line();
  b.adv(2);
  b.w(0xF11E, ENAM0, 0, 3);
  Y = st.cellIndex;
  A = (st.player[0].maze[Y] >> 4) & 0x07;
  st.scratch[kTempD3] = data::kMazeRowFD3F[A];
  p1Line();
  Y = st.cellIndex;
  st.scratch[kTempD2] = (st.player[0].maze[Y] & 0x80) ? 0x50 : 0x10;
  A = st.player[0].maze[Y + 4] & 0x07;
  st.scratch[kTempD7] = data::kMazeRowFD63[A];
  p0Line();
  Y = st.cellIndex;
  A = st.player[0].maze[Y + 4] >> 3;
  st.scratch[kTempD5] = A;
  st.scratch[kTempD6] = data::kMazeRowFD5B[A & 0x07];
  p1Line();
  A = (st.scratch[kTempD5] >> 3) & 0x03;
  st.scratch[kTempD5] = data::kMazeRowFD57[A];
  goto L_F1F4;

L_F176:
  Y = st.cellIndex;
  A = st.player[0].maze[Y + 8] & 0x07;
  st.scratch[kTempD4] = data::kMazeRowFD7B[A];
  p0Line();
  b.adv(2);
  b.w(0xF188, ENAM0, 0, 3);
  A = st.scratch[kTempE1];
  b.adv(3);
  b.w(0xF18C, ENABL, A, 3);
  Y = st.cellIndex;
  A = (st.player[0].maze[Y + 8] >> 3) & 0x0F;
  st.scratch[kTempD3] = data::kMazeRowFD6B[A];
  p1Line();
  Y = st.cellIndex;
  st.scratch[kTempD2] = (st.player[0].maze[Y + 8] & 0x80) ? 0x50 : 0x10;
  A = st.player[0].maze[Y + 12] & 0x07;
  st.scratch[kTempD7] = data::kMazeRowFD63[A];
  p0Line();
  Y = st.cellIndex;
  A = st.player[0].maze[Y + 12] >> 3;
  st.scratch[kTempD5] = A;
  st.scratch[kTempD6] = data::kMazeRowFD83[A & 0x07];
  p1Line();
  A = st.scratch[kTempE1];
  b.adv(3);
  b.w(0xF1D5, ENAM1, A, 3);
  A = st.scratch[kTempD5] >> 3;
  st.scratch[kTempD5] = data::kMazeRowFD47[A];
  goto L_F1F4;

L_F1F4:   // the wall and wafer bit patterns of the two halves of the row are written on this and the next line
  p0Line();
  if (st.cellRow == 4) {
    st.scratch[kTempD4] |= 0x40;
    st.scratch[kTempD5] |= 0x20;
  }
  fetch1();
  b.wsync();
  b.w(0xF21A, GRP1, A, 3);
  A = st.scratch[kTempD2];
  b.adv(3);
  b.w(0xF21E, PF0, A, 3);
  b.adv(2);
  b.w(0xF222, CTRLPF, 0x30, 3);
  A = st.scratch[kTempD3];
  b.adv(3);
  b.w(0xF226, PF1, A, 3);
  A = st.scratch[kTempD4];
  b.adv(3);
  b.w(0xF22A, PF2, A, 3);
  ++X;
  b.adv(2);
  A = st.scratch[kTempD5];
  b.adv(3);
  b.w(0xF22F, PF0, A, 3);
  b.adv(3);   // CPX SPRITE0_Y
  if (negative(X - st.scratch[kSprite0Y])) { b.adv(4); goto L_F1E5; }
  b.adv(2);
  A = st.scratch[kTempD6];
  b.adv(3);
  b.w(0xF237, PF1, A, 3);
  Y = st.scratch[kSprite0Lines];
  b.adv(3);
  if (negative(Y)) { b.adv(4); goto L_F1EB; }
  b.adv(2);
  A = st.scratch[kTempD7];
  b.adv(3);
  b.w(0xF23F, PF2, A, 3);
  b.adv(5 + crossing(kSprite0PtrLo, Y));
  A = data::byAddress(static_cast<uint16_t>(ptr(kSprite0PtrLo) + Y));
  --Y;
  st.scratch[kSprite0Lines] = Y;
  b.adv(2 + 3);
  goto L_F246;

L_F1E5:
  A = st.scratch[kTempD6];
  b.adv(3);
  b.w(0xF1E7, PF1, A, 3);
  b.adv(4);   // NOP, NOP
L_F1EB:
  A = st.scratch[kTempD7];
  b.adv(3);
  b.w(0xF1ED, PF2, A, 3);
  A = 0;
  b.adv(2 + 3);

L_F246:
  b.wsync();
  b.w(0xF248, GRP0, A, 3);
  A = st.scratch[kTempD2];
  b.adv(3);
  b.w(0xF24C, PF0, A, 3);
  A = st.scratch[kTempD3];
  b.adv(3);
  b.w(0xF250, PF1, A, 3);
  A = st.scratch[kTempD4];
  b.adv(3);
  b.w(0xF254, PF2, A, 3);
  b.adv(4);   // NOP, NOP
  A = st.scratch[kTempD5];
  b.adv(3);
  b.w(0xF25A, PF0, A, 3);
  b.adv(3);   // CPX SPRITE1_Y
  if (negative(X - st.scratch[kSprite1Y])) {
    b.adv(3);
    A = st.scratch[kTempD6];
    b.adv(3);
    b.w(0xF27A, PF1, A, 3);
    b.adv(4);   // NOP, NOP
    goto L_F27E;
  }
  b.adv(2);
  A = st.scratch[kTempD6];
  b.adv(3);
  b.w(0xF262, PF1, A, 3);
  Y = st.scratch[kSprite1Lines];
  b.adv(3);
  if (negative(Y)) { b.adv(3); goto L_F27E; }
  b.adv(2);
  A = st.scratch[kTempD7];
  b.adv(3);
  b.w(0xF26A, PF2, A, 3);
  b.adv(5 + crossing(kSprite1PtrLo, Y));
  A = data::byAddress(static_cast<uint16_t>(ptr(kSprite1PtrLo) + Y));
  --Y;
  st.scratch[kSprite1Lines] = Y;
  b.adv(2 + 3);
  goto L_F271;

L_F27E:
  A = st.scratch[kTempD7];
  b.adv(3);
  b.w(0xF280, PF2, A, 3);
  A = 0;
  b.adv(2 + 3);

L_F271:
  b.wsync();
  b.w(0xF273, GRP1, A, 3);
  b.adv(3);   // JMP
  Y = st.rowParity;
  b.adv(3);
  A = data::kPlayfieldFE0A[Y];
  b.adv(4);
  b.w(0xF28C, PF0, A, 3);
  A = data::kPlayfieldFE0C[Y];
  b.adv(4);
  b.w(0xF291, PF1, A, 3);
  b.adv(2);
  b.w(0xF295, CTRLPF, 0x31, 3);
  A = st.scratch[kTempE3];
  b.adv(3);
  b.w(0xF299, PF2, A, 3);
  A = static_cast<uint8_t>(st.cellRow + 0xFF);
  if (negative(A)) return;
  st.cellRow = A;
  ++X;
  fetch0();
  b.wsync();
  b.w(0xF2B9, GRP0, A, 3);
  b.adv(2);
  b.w(0xF2BD, ENAM1, 0, 3);
  st.scratch[kTempE2] = 0;
  A = st.cellRow;
  st.rowParity = A & 1;
  st.cellIndex = static_cast<uint8_t>((A + 1) >> 1);
  p1p0Line();
  Y = 1;
  if (!(st.frameCounter & 0x08)) Y = 3;
  A = st.cellRow;
  if (A == 0 || A == 6) {
    if (A == 6) --Y;
    if (st.player[0].pillFlags & data::kBitMasksAndReloads[data::kPillMaskAt + Y]) st.scratch[kTempE2] = 0x02;
  }
  p1Line();
  b.adv(2);
  b.w(0xF2F3, ENABL, 0, 3);
  p0Line();
  A = st.scratch[kTempE2];
  b.adv(3);
  b.w(0xF2FA, ENAM0, A, 3);
  p1Line();
  p0Line();
  Y = st.rowParity;
  b.adv(3);
  A = data::kPlayfieldFF41[Y];
  b.adv(4);
  b.w(0xF307, PF0, A, 3);
  A = data::kPlayfieldFE0F[Y];
  b.adv(4);
  b.w(0xF30C, PF1, A, 3);
  A = data::kPlayfieldFE11[Y];
  b.adv(4);
  b.w(0xF311, PF2, A, 3);

L_F313:   // the lines of a row between its walls: the bonus item bits and the playfield bytes for the next lines
  p1Line();
  A = st.gameFlags;
  Y = st.cellRow;
  if (Y != 3) A = 0;
  st.scratch[kTempE1] = A;
  p0Line();
  Y = st.rowParity;
  A = data::kPlayfieldFE0C[data::kPlayfieldFE0DAt + Y];
  Y = st.cellRow;
  if (Y == 4) A = 0x40;
  st.scratch[kTempE3] = A;
  p1Line();
  ++X;
  fetch0();
  b.wsync();
  b.w(0xF349, GRP0, A, 3);
  Y = st.rowParity;
  b.adv(3);
  b.w(0xF34D, GRP0, A, 3);
  A = data::kPlayfieldFE0A[Y];
  b.adv(4);
  b.w(0xF352, PF0, A, 3);
  A = data::kPlayfieldFE0C[Y];
  b.adv(4);
  b.w(0xF357, PF1, A, 3);
  A = st.scratch[kTempE3];
  b.adv(3);
  b.w(0xF35B, PF2, A, 3);
  goto L_F0F4;
}

}  // namespace game::kernel
