// The drawing code of the original ($F07F-$F4E6) as one small class (Stage G4.6). The original keeps the processor registers A, X and Y across its
// whole picture routine and jumps between its parts; the class holds them as members so that the parts can be separate functions in separate files
// (kernel_positions.cpp, kernel_maze.cpp, kernel_score.cpp) and still be the same code. Only drawPicture (kernel.hpp) is used by the rest of the game.
#pragma once
#include <cstdint>

#include "../chips.hpp"
#include "../data/tables.hpp"
#include "../kernel.hpp"
#include "../state/state.hpp"

namespace game::kernel {

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
  bool hideSprites = false;   // Stage I9.1: the picture without Pac-Man and the ghosts (GRP0 and GRP1 are written as 0 while it is set)
  void adv(int cycles) {
    clock += 3 * cycles;
    while (clock >= 228) { clock -= 228; ++line; }
  }
  void w(uint16_t site, uint8_t reg, uint8_t value, int cycles) {
    adv(cycles);
    if (hideSprites && (reg == GRP0 || reg == GRP1)) value = 0;
    chip.write({line, clock, reg, value, site});
  }
  void wsync() {   // STA WSYNC: the write is the third cycle; the processor then waits for the start of the next line
    adv(2);
    ++line;
    clock = 0;
  }
};


class Kernel {
 public:
  Kernel(GameState& state, TimedChip& chip, int startLine, int kernelStart)
      : st(state), b{chip, startLine}, kernelStartLine(kernelStart) {}

  // The parts, in the order the original runs them.
  void positions();     // PART 2 ($F07F): the positions of the sprites and of the power-pill missile, on the lines before the picture
  void mazeRows();      // PART 4 ($F0CA): the picture, row by row, down to the last maze row
  void bottomWall();    // the bottom wall of the maze
  void scoreLine();     // the score digits (SCORE_DIGITS, $F3EE) and the clearing of the sprite registers
  void lifeMarkers();   // the life markers ($F48E)
  PictureEnd end() const { return {b.line, b.clock}; }
  // Stage I9.1: while set, the maze part of the picture is made without Pac-Man and the ghosts (the two players of the chip, which the maze rows use
  // for nothing else); the score and the life markers need the same registers and are never hidden.
  void hideMovingObjects(bool hide) { b.hideSprites = hide; }

 private:
  GameState& st;
  Beam b;
  int kernelStartLine;
  uint8_t A = 0, X = 0, Y = 0;

  static bool negative(int v) { return (v & 0x80) != 0; }
  // The sprite and digit pictures are found through 16-bit pointers kept in the scratch bytes: `lo` is the place (a Scratch name) of the low byte.
  uint16_t ptr(uint8_t lo) const { return static_cast<uint16_t>(st.scratch[lo] | (st.scratch[lo + 1] << 8)); }
  int crossing(uint8_t pointerLo, uint8_t y) const { return ((ptr(pointerLo) & 0xFF) + y) > 0xFF ? 1 : 0; }

  // The next picture byte of a sprite for the line being drawn, as the original fetches it: A is 0 unless the sprite's first line has been reached
  // (X, the line counter, not below the sprite's y) and it still has lines left; the line count in Y then goes down by one.
  void fetch0() {
    A = 0;
    if (!negative(X - st.scratch[kSprite0Y])) {
      Y = st.scratch[kSprite0Lines];
      if (!negative(Y)) { A = data::byAddress(static_cast<uint16_t>(ptr(kSprite0PtrLo) + Y)); --Y; st.scratch[kSprite0Lines] = Y; }
    }
  }
  void fetch1() {
    A = 0;
    if (!negative(X - st.scratch[kSprite1Y])) {
      Y = st.scratch[kSprite1Lines];
      if (!negative(Y)) { A = data::byAddress(static_cast<uint16_t>(ptr(kSprite1PtrLo) + Y)); --Y; st.scratch[kSprite1Lines] = Y; }
    }
  }
  // KERNEL_LINE_P1 ($F053), KERNEL_LINE_P0 ($F06A) and KERNEL_LINE_P1_P0 ($F067): one line for a sprite, ending with the return (6 cycles).
  void p1Line() { fetch1(); b.wsync(); b.w(0xF064, GRP1, A, 3); b.adv(6); }
  void p0Line() { ++X; fetch0(); b.wsync(); b.w(0xF07C, GRP0, A, 3); b.adv(6); }
  void p1p0Line() { p1Line(); p0Line(); }
};

}  // namespace game::kernel
