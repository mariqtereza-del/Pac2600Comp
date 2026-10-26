#include "renderer.hpp"
#include "state/cycles.hpp"

#include <cstdio>

namespace game {

void Renderer::write(uint8_t reg, uint8_t value) {
  if (powerOn_) return;   // the power-on clearing changes nothing (the chip starts cleared); the places of the ball and missile 1 come with position()
  int line = line_, clock = 0;
  if (timed_) {   // the bottom logic: its time is the processor cycles counted since the end of the drawing code
    const long t = baseClock_ + 3 * (gCycles + skewCycles);
    line = baseLine_ + static_cast<int>(t / TiaVideo::kClocks);
    clock = static_cast<int>(t % TiaVideo::kClocks);
  }
  if (trace) std::printf("  logic write line %d clock %d reg $%02X value $%02X\n", line, clock, reg, value);
  video_.write(line, clock, reg, value);
  audio_.write(line, clock, reg, value);
}

void Renderer::write(const ChipWrite& w) {
  if (trace) std::printf("  drawing write line %d clock %d reg $%02X value $%02X\n", w.line, w.clock, w.reg, w.value);
  video_.write(w.line, w.clock, w.reg, w.value);
}

void Renderer::writeAtCycle(long cycles, uint8_t reg, uint8_t value) {
  const long t = 15 + 3 * cycles;   // the vertical sync is switched off by a write that ends at colour clock 15 of line 0
  const int line = static_cast<int>(t / TiaVideo::kClocks), clock = static_cast<int>(t % TiaVideo::kClocks);
  if (trace) std::printf("  logic write line %d clock %d reg $%02X value $%02X\n", line, clock, reg, value);
  video_.write(line, clock, reg, value);
  audio_.write(line, clock, reg, value);
}

void Renderer::timeBase(int line, int clock) {
  timed_ = true;
  baseLine_ = line;
  baseClock_ = clock;
}

void Renderer::position(int line) {
  timed_ = false;
  if (line >= 0) {
    powerOn_ = false;
    line_ = line;
    return;
  }
  // Power-on set-up: the only writes of it that stay in the chip are the places of missile 1 and the ball, made by strobes at an exact colour clock
  // (lines 39-41 of the first frame of the original: fine move values, the two strobes, the move, the clearing of the fine moves).
  powerOn_ = true;
  video_.write(39, 15, 0x23, 0xF0);    // HMM1
  video_.write(39, 30, 0x24, 0x80);    // HMBL
  video_.write(39, 132, 0x14, 0x80);   // RESBL
  video_.write(39, 141, 0x13, 0x80);   // RESM1
  video_.write(40, 9, 0x2A, 0x80);     // HMOVE
  video_.write(41, 9, 0x2B, 0x80);     // HMCLR
}

}  // namespace game
