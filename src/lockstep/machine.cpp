#include "machine.hpp"

#include <cstring>

namespace lockstep {

namespace {
constexpr uint8_t kC = 0x01, kZ = 0x02, kI = 0x04, kD = 0x08, kB = 0x10, kU = 0x20, kV = 0x40, kN = 0x80;
}

Machine::Machine(const uint8_t* rom4k) : rom_(rom4k) {
  writerPc_.fill(0);
  writerNative_.fill(nullptr);
  pc = static_cast<uint16_t>(rom_[0xFFC] | (rom_[0xFFD] << 8));
}

void Machine::registerNative(uint16_t entry, const std::string& name, NativeFn fn) { natives_[entry] = Native{name, std::move(fn)}; }

const char* Machine::regionName(uint16_t pc) {
  for (const auto& r : kRegions)
    if (pc >= r.start && pc < r.start + r.size) return r.name;
  return "(outside the listed routines)";
}

const char* Machine::ramName(uint8_t addr) {
  for (const auto& r : kRamNames)
    if (r.addr == addr) return r.name;
  return "?";
}

const char* Machine::lastWriterName(int i) const {
  if (writerNative_[i]) return writerNative_[i];
  if (writerPc_[i] == 0) return "(never written)";
  return regionName(writerPc_[i]);
}

// ---- bus ----
uint8_t Machine::tiaRead(uint16_t addr) {
  switch (addr & 0x0F) {
    case 0x01: return oracleBonus_ ? 0x40 : 0x00;          // CXM1P: the game keeps bit 6 (missile 1 touched player 1) ...
    case 0x03: return oracleBonus_ ? 0x40 : 0x00;          // ... and CXP1FB bit 6 (player 1 touched the ball): both come from one oracle bit
    case 0x07: return oracleGhost_ ? 0x80 : 0x00;          // CXPPMM bit 7: player 0 touched player 1
    case 0x0C: return (inputs_ & 16) ? 0x00 : 0x80;        // INPT4: the fire button of the left joystick, bit 7 = 0 when pressed
    default: return 0x00;
  }
}

uint8_t Machine::riotRead(uint16_t addr) {
  switch (addr & 0x07) {
    case 0: {                                              // SWCHA: left joystick in the upper nibble, 0 = pressed
      uint8_t v = 0xFF;
      if (inputs_ & 1) v &= ~0x10;
      if (inputs_ & 2) v &= ~0x20;
      if (inputs_ & 4) v &= ~0x40;
      if (inputs_ & 8) v &= ~0x80;
      return v;
    }
    case 2: {                                              // SWCHB
      uint8_t v = 0x3F;
      if (inputs_ & 64) v &= ~0x01;                        // Reset
      if (inputs_ & 32) v &= ~0x02;                        // Select
      if (inputs_ & 512) v &= ~0x08;                       // black and white
      if (inputs_ & 128) v |= 0x40;                        // left difficulty A
      if (inputs_ & 256) v |= 0x80;                        // right difficulty A
      return v;
    }
    case 4: return 0x00;                                   // INTIM: timing is not modelled, every wait ends at once
    default: return 0x00;
  }
}

void Machine::tiaWrite(uint16_t addr, uint8_t v) {
  const uint8_t reg = addr & 0x3F;
  if (reg == 0x00) {                                       // VSYNC: switching it off ends the frame
    if (vsyncOn_ && !(v & 2)) frameDone_ = true;
    vsyncOn_ = (v & 2) != 0;
  }
}

uint8_t Machine::read(uint16_t addr) {
  const uint16_t a = addr & 0x1FFF;
  if (a & 0x1000) return rom_[a & 0x0FFF];
  if (a & 0x80) return (a & 0x200) ? riotRead(a) : ram_[a & 0x7F];
  return tiaRead(a);
}

void Machine::write(uint16_t addr, uint8_t v) {
  const uint16_t a = addr & 0x1FFF;
  if (a & 0x1000) return;
  if (a & 0x80) {
    if (a & 0x200) return;                                 // RIOT timer set-ups: not modelled
    const int i = a & 0x7F;
    ram_[i] = v;
    writerPc_[i] = currentNative_ ? 0 : currentPc_;
    writerNative_[i] = currentNative_;
    return;
  }
  tiaWrite(a, v);
}

void Machine::push(uint8_t v) { write(0x100 + sp, v); --sp; }
uint8_t Machine::pull() { ++sp; return read(0x100 + sp); }
void Machine::rts() {
  const uint8_t lo = pull();
  const uint8_t hi = pull();
  pc = static_cast<uint16_t>(((hi << 8) | lo) + 1);
}
void Machine::setZN(uint8_t v) { p = (p & ~(kZ | kN)) | (v == 0 ? kZ : 0) | (v & kN); }

void Machine::compare(uint8_t r, uint8_t v) {
  p = (p & ~kC) | (r >= v ? kC : 0);
  setZN(static_cast<uint8_t>(r - v));
}

void Machine::adc(uint8_t v) {
  const unsigned c = p & kC;
  const unsigned bin = a + v + c;
  if (!(p & kD)) {
    p = (p & ~(kC | kV)) | (bin > 0xFF ? kC : 0) | ((~(a ^ v) & (a ^ bin) & 0x80) ? kV : 0);
    a = static_cast<uint8_t>(bin);
    setZN(a);
    return;
  }
  // decimal mode (NMOS 6502)
  unsigned lo = (a & 0x0F) + (v & 0x0F) + c;
  if (lo > 9) lo += 6;
  unsigned hi = (a >> 4) + (v >> 4) + (lo > 0x0F ? 1 : 0);
  p = (p & ~(kZ | kN | kV | kC)) | ((bin & 0xFF) == 0 ? kZ : 0) | ((hi & 8) ? kN : 0) |
      ((~(a ^ v) & (a ^ (hi << 4)) & 0x80) ? kV : 0);
  if (hi > 9) hi += 6;
  p |= (hi > 0x0F) ? kC : 0;
  a = static_cast<uint8_t>(((hi << 4) | (lo & 0x0F)) & 0xFF);
}

void Machine::sbc(uint8_t v) {
  const unsigned borrow = (p & kC) ? 0 : 1;
  const int bin = static_cast<int>(a) - v - static_cast<int>(borrow);
  const uint8_t binR = static_cast<uint8_t>(bin);
  p = (p & ~(kC | kV)) | (bin >= 0 ? kC : 0) | (((a ^ v) & (a ^ binR) & 0x80) ? kV : 0);
  if (!(p & kD)) {
    a = binR;
    setZN(a);
    return;
  }
  int al = (a & 0x0F) - (v & 0x0F) - static_cast<int>(borrow);
  if (al < 0) al = ((al - 6) & 0x0F) - 0x10;
  int r = (a & 0xF0) - (v & 0xF0) + al;
  if (r < 0) r -= 0x60;
  setZN(binR);
  a = static_cast<uint8_t>(r);
}

uint16_t Machine::ea(Mode m, bool& cross) {
  cross = false;
  switch (m) {
    case MODE_ZERO_PAGE: { const uint8_t b = read(pc + 1); return b; }
    case MODE_ZERO_PAGE_X: { const uint8_t b = read(pc + 1); return static_cast<uint8_t>(b + x); }
    case MODE_ZERO_PAGE_Y: { const uint8_t b = read(pc + 1); return static_cast<uint8_t>(b + y); }
    case MODE_ABSOLUTE: return static_cast<uint16_t>(read(pc + 1) | (read(pc + 2) << 8));
    case MODE_ABSOLUTE_X: { const uint16_t base = static_cast<uint16_t>(read(pc + 1) | (read(pc + 2) << 8)); const uint16_t e = base + x; cross = (base & 0xFF00) != (e & 0xFF00); return e; }
    case MODE_ABSOLUTE_Y: { const uint16_t base = static_cast<uint16_t>(read(pc + 1) | (read(pc + 2) << 8)); const uint16_t e = base + y; cross = (base & 0xFF00) != (e & 0xFF00); return e; }
    case MODE_INDIRECT_X: { const uint8_t z = static_cast<uint8_t>(read(pc + 1) + x); return static_cast<uint16_t>(read(z) | (read(static_cast<uint8_t>(z + 1)) << 8)); }
    case MODE_INDIRECT_Y: { const uint8_t z = read(pc + 1); const uint16_t base = static_cast<uint16_t>(read(z) | (read(static_cast<uint8_t>(z + 1)) << 8)); const uint16_t e = base + y; cross = (base & 0xFF00) != (e & 0xFF00); return e; }
    default: return 0;
  }
}

void Machine::step() {
  currentPc_ = pc;
  auto nat = natives_.find(pc);
  if (nat != natives_.end()) {
    currentNative_ = nat->second.name.c_str();
    nat->second.fn(*this);
    currentNative_ = nullptr;
    ++instructions_;
    return;
  }
  const uint8_t op = read(pc);
  const OpInfo& info = kOps[op];
  bool cross = false;
  const Mode mode = info.mode;
  uint16_t addr = 0;
  uint8_t imm = 0;
  if (mode == MODE_IMMEDIATE) imm = read(pc + 1);
  else if (mode != MODE_IMPLIED && mode != MODE_ACCUMULATOR && mode != MODE_RELATIVE && mode != MODE_ABS_INDIRECT) addr = ea(mode, cross);
  auto val = [&]() -> uint8_t { return mode == MODE_IMMEDIATE ? imm : read(addr); };
  uint16_t next = static_cast<uint16_t>(pc + info.length);
  ++instructions_;
  switch (info.mn) {
    case MN_LDA: a = val(); setZN(a); break;
    case MN_LDX: x = val(); setZN(x); break;
    case MN_LDY: y = val(); setZN(y); break;
    case MN_STA: write(addr, a); break;
    case MN_STX: write(addr, x); break;
    case MN_STY: write(addr, y); break;
    case MN_TAX: x = a; setZN(x); break;
    case MN_TAY: y = a; setZN(y); break;
    case MN_TXA: a = x; setZN(a); break;
    case MN_TYA: a = y; setZN(a); break;
    case MN_TSX: x = sp; setZN(x); break;
    case MN_TXS: sp = x; break;
    case MN_INX: ++x; setZN(x); break;
    case MN_INY: ++y; setZN(y); break;
    case MN_DEX: --x; setZN(x); break;
    case MN_DEY: --y; setZN(y); break;
    case MN_INC: { const uint8_t v = read(addr) + 1; write(addr, v); setZN(v); break; }
    case MN_DEC: { const uint8_t v = read(addr) - 1; write(addr, v); setZN(v); break; }
    case MN_AND: a &= val(); setZN(a); break;
    case MN_ORA: a |= val(); setZN(a); break;
    case MN_EOR: a ^= val(); setZN(a); break;
    case MN_BIT: { const uint8_t v = val(); p = (p & ~(kZ | kN | kV)) | ((a & v) == 0 ? kZ : 0) | (v & (kN | kV)); break; }
    case MN_CMP: compare(a, val()); break;
    case MN_CPX: compare(x, val()); break;
    case MN_CPY: compare(y, val()); break;
    case MN_ADC: adc(val()); break;
    case MN_SBC: sbc(val()); break;
    case MN_ASL: case MN_LSR: case MN_ROL: case MN_ROR: {
      const uint8_t v = (mode == MODE_ACCUMULATOR) ? a : read(addr);
      uint8_t r = 0;
      uint8_t c = 0;
      if (info.mn == MN_ASL) { c = v >> 7; r = static_cast<uint8_t>(v << 1); }
      else if (info.mn == MN_LSR) { c = v & 1; r = v >> 1; }
      else if (info.mn == MN_ROL) { c = v >> 7; r = static_cast<uint8_t>((v << 1) | (p & kC)); }
      else { c = v & 1; r = static_cast<uint8_t>((v >> 1) | ((p & kC) << 7)); }
      p = (p & ~kC) | c;
      setZN(r);
      if (mode == MODE_ACCUMULATOR) a = r; else write(addr, r);
      break;
    }
    case MN_CLC: p &= ~kC; break;
    case MN_SEC: p |= kC; break;
    case MN_CLD: p &= ~kD; break;
    case MN_SED: p |= kD; break;
    case MN_CLI: p &= ~kI; break;
    case MN_SEI: p |= kI; break;
    case MN_CLV: p &= ~kV; break;
    case MN_NOP: break;
    case MN_PHA: push(a); break;
    case MN_PLA: a = pull(); setZN(a); break;
    case MN_PHP: push(p | kB | kU); break;
    case MN_PLP: p = (pull() & ~kB) | kU; break;
    case MN_JMP:
      if (mode == MODE_ABSOLUTE) next = addr;
      else {
        const uint16_t ptr = static_cast<uint16_t>(read(pc + 1) | (read(pc + 2) << 8));
        next = static_cast<uint16_t>(read(ptr) | (read((ptr & 0xFF00) | ((ptr + 1) & 0x00FF)) << 8));
      }
      break;
    case MN_JSR: {
      const uint16_t ret = static_cast<uint16_t>(pc + 2);
      push(ret >> 8);
      push(ret & 0xFF);
      next = addr;
      break;
    }
    case MN_RTS: { rts(); return; }
    case MN_BCC: case MN_BCS: case MN_BEQ: case MN_BMI: case MN_BNE: case MN_BPL: case MN_BVC: case MN_BVS: {
      bool take = false;
      switch (info.mn) {
        case MN_BCC: take = !(p & kC); break;
        case MN_BCS: take = (p & kC) != 0; break;
        case MN_BEQ: take = (p & kZ) != 0; break;
        case MN_BNE: take = !(p & kZ); break;
        case MN_BMI: take = (p & kN) != 0; break;
        case MN_BPL: take = !(p & kN); break;
        case MN_BVC: take = !(p & kV); break;
        default: take = (p & kV) != 0; break;
      }
      if (take) next = static_cast<uint16_t>(pc + 2 + static_cast<int8_t>(read(pc + 1)));
      break;
    }
    default:
      // BRK, RTI and unofficial opcodes never run in this program; stop the frame
      frameDone_ = true;
      failed_ = true;
      return;
  }
  pc = next;
}

bool Machine::runFrame() {
  frameDone_ = false;
  failed_ = false;
  for (int guard = 0; guard < 400000; ++guard) {
    step();
    if (failed_) return false;
    if (frameDone_) return true;
  }
  return false;
}

}  // namespace lockstep
