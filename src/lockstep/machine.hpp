// Lockstep machine (Stage F20): the original program run by a small 6502 core over the 128 bytes of console memory, with hooks that let a
// natively written routine replace the original one. It models only what the program's LOGIC can see: the inputs, the collision flags
// (taken from an oracle until the native renderer exists, F23) and the frame boundary. Timing is not modelled: the timer always reads 0,
// WSYNC does nothing (the picture kernel still runs, because it writes scratch bytes of the memory). The picture is not made.
#pragma once
#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "generated_tables.hpp"

namespace lockstep {

class Machine;
using NativeFn = std::function<void(Machine&)>;   // runs the whole routine and returns like RTS (see Machine::rts)

struct Inputs {
  // The recorded input bits of D13/E1: up 1, down 2, left 4, right 8, fire 16, select 32, reset 64, left difficulty A 128,
  // right difficulty A 256, black and white 512.
  unsigned bits = 0;
};

class Machine {
 public:
  Machine(const uint8_t* rom4k);

  // ---- running ----
  void setInputs(unsigned bits) { inputs_ = bits; }
  void setOracle(bool bonusFlag, bool ghostFlag) { oracleBonus_ = bonusFlag; oracleGhost_ = ghostFlag; }
  // Runs until the frame ends (the write that switches the vertical sync off, $F87C). Returns false if the guard stops it.
  bool runFrame();

  // ---- natives ----
  void registerNative(uint16_t entry, const std::string& name, NativeFn fn);
  // For natives: the registers, the memory with the writer recorded as the native routine, and RTS.
  uint8_t a = 0, x = 0, y = 0, sp = 0xFF, p = 0x24;
  uint16_t pc = 0;
  uint8_t read(uint16_t addr);
  void write(uint16_t addr, uint8_t value);
  void rts();
  void setZN(uint8_t v);

  // ---- state ----
  const std::array<uint8_t, 128>& ram() const { return ram_; }
  std::array<uint8_t, 128>& ramMutable() { return ram_; }
  // The routine (name) that wrote byte $80+i last, and the pc of the instruction (0 for a native routine).
  const char* lastWriterName(int i) const;
  uint16_t lastWriterPc(int i) const { return writerPc_[i]; }
  uint64_t instructions() const { return instructions_; }
  static const char* regionName(uint16_t pc);
  static const char* ramName(uint8_t addr);

 private:
  void step();
  uint16_t ea(Mode m, bool& pageCross);
  void push(uint8_t v);
  uint8_t pull();
  void adc(uint8_t v);
  void sbc(uint8_t v);
  void compare(uint8_t r, uint8_t v);
  uint8_t tiaRead(uint16_t addr);
  uint8_t riotRead(uint16_t addr);
  void tiaWrite(uint16_t addr, uint8_t v);

  const uint8_t* rom_;
  std::array<uint8_t, 128> ram_{};
  std::array<uint16_t, 128> writerPc_{};
  std::array<const char*, 128> writerNative_{};
  struct Native { std::string name; NativeFn fn; };
  std::map<uint16_t, Native> natives_;
  const char* currentNative_ = nullptr;
  uint16_t currentPc_ = 0;
  unsigned inputs_ = 0;
  bool oracleBonus_ = false, oracleGhost_ = false;
  bool frameDone_ = false;
  bool vsyncOn_ = false;
  bool failed_ = false;
  uint64_t instructions_ = 0;
};

}  // namespace lockstep
