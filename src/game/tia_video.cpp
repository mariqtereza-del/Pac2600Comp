#include "tia_video.hpp"

#include <algorithm>

namespace game {

namespace {

constexpr int kP0 = 0, kP1 = 1, kM0 = 2, kM1 = 3, kBL = 4;

// The first pixel columns (offsets) of the copies of a player or missile for a NUSIZ value.
int copiesOf(int nusiz, int offsets[3]) {
  switch (nusiz & 7) {
    case 1: offsets[0] = 0; offsets[1] = 16; return 2;
    case 2: offsets[0] = 0; offsets[1] = 32; return 2;
    case 3: offsets[0] = 0; offsets[1] = 16; offsets[2] = 32; return 3;
    case 4: offsets[0] = 0; offsets[1] = 64; return 2;
    case 6: offsets[0] = 0; offsets[1] = 32; offsets[2] = 64; return 3;
    default: offsets[0] = 0; return 1;
  }
}

int mod160(int v) { v %= 160; return v < 0 ? v + 160 : v; }

bool playerOn(int x, int pos, int nusiz, uint8_t graphics, bool reflected) {
  if (graphics == 0) return false;
  const int n = nusiz & 7;
  const int size = n == 5 ? 2 : n == 7 ? 4 : 1;
  int off[3];
  const int count = copiesOf(nusiz, off);
  for (int c = 0; c < count; ++c) {
    const int d = mod160(x - pos - off[c]);
    if (d < 8 * size) {
      const int bit = d / size;
      return ((graphics >> (reflected ? bit : 7 - bit)) & 1) != 0;
    }
  }
  return false;
}

bool missileOn(int x, int pos, int nusiz) {
  const int width = 1 << ((nusiz >> 4) & 3);
  int off[3];
  const int count = copiesOf(nusiz, off);
  for (int c = 0; c < count; ++c)
    if (mod160(x - pos - off[c]) < width) return true;
  return false;
}

}  // namespace

int TiaVideo::delayOf(uint8_t reg) const {
  switch (reg) {
    case 0x0D: case 0x0E: case 0x0F: return timing_.playfield;
    case 0x1B: case 0x1C: return timing_.player;
    case 0x1D: case 0x1E: case 0x1F: return timing_.enable;
    default: return timing_.other;
  }
}

void TiaVideo::write(int line, int clock, uint8_t reg, uint8_t value) {
  if (line < lastLine_) endPicture();
  lastLine_ = line;
  const long at = static_cast<long>(line) * kClocks + clock;
  const bool strobe = (reg >= 0x10 && reg <= 0x14) || reg == 0x2A || reg == 0x2B;
  events_.push_back({at + (strobe ? 0 : delayOf(reg)), order_++, reg, value, clock});
}

void TiaVideo::apply(const Event& e, State& s, bool& hmoveBlank) const {
  const uint8_t v = e.value;
  switch (e.reg) {
    case 0x01: s.vblank = v; break;
    case 0x04: s.nusiz[0] = v; break;
    case 0x05: s.nusiz[1] = v; break;
    case 0x06: s.colup0 = v & 0xFE; break;
    case 0x07: s.colup1 = v & 0xFE; break;
    case 0x08: s.colupf = v & 0xFE; break;
    case 0x09: s.colubk = v & 0xFE; break;
    case 0x0A: s.ctrlpf = v; break;
    case 0x0B: s.refp[0] = v; break;
    case 0x0C: s.refp[1] = v; break;
    case 0x0D: s.pf[0] = v; break;
    case 0x0E: s.pf[1] = v; break;
    case 0x0F: s.pf[2] = v; break;
    case 0x10: s.pos[kP0] = mod160(e.clock - 68 + timing_.playerPos); break;
    case 0x11: s.pos[kP1] = mod160(e.clock - 68 + timing_.playerPos); break;
    case 0x12: s.pos[kM0] = mod160(e.clock - 68 + timing_.missilePos); break;
    case 0x13: s.pos[kM1] = mod160(e.clock - 68 + timing_.missilePos); break;
    case 0x14: s.pos[kBL] = mod160(e.clock - 68 + timing_.ballPos); break;
    case 0x1B: s.grp[0] = v; s.grpOld[1] = s.grp[1]; break;
    case 0x1C: s.grp[1] = v; s.grpOld[0] = s.grp[0]; s.enablOld = s.enabl; break;
    case 0x1D: s.enam[0] = v; break;
    case 0x1E: s.enam[1] = v; break;
    case 0x1F: s.enabl = v; break;
    case 0x20: case 0x21: case 0x22: case 0x23: case 0x24: {
      const int n = v >> 4;
      s.hm[e.reg - 0x20] = static_cast<int8_t>(n >= 8 ? n - 16 : n);
      break;
    }
    case 0x25: s.vdelp[0] = v & 1; break;
    case 0x26: s.vdelp[1] = v & 1; break;
    case 0x27: s.vdelbl = v & 1; break;
    case 0x2A:
      for (int i = 0; i < 5; ++i) s.pos[i] = mod160(s.pos[i] - s.hm[i]);
      hmoveBlank = true;
      break;
    case 0x2B:
      for (auto& h : s.hm) h = 0;
      break;
    case 0x2C:
      s.cxP0P1 = s.cxM1P1 = s.cxP1BL = false;
      break;
    default: break;
  }
}

void TiaVideo::renderThrough(int lastRow) {
  std::stable_sort(events_.begin() + static_cast<std::ptrdiff_t>(next_), events_.end(),
                   [](const Event& a, const Event& b) { return a.when != b.when ? a.when < b.when : a.order < b.order; });
  State& s = state_;
  bool hmoveBlank = false;
  for (; row_ <= lastRow && row_ < kHeight; ++row_) {
    const int row = row_;
    const int line = kFirstLine + row;
    hmoveBlank = false;
    // The events of the lines before this one (and of this line's horizontal blank) are applied first. An HMOVE blank belongs to its own line only.
    const long lineStart = static_cast<long>(line) * kClocks;
    while (next_ < events_.size() && events_[next_].when < lineStart) {
      bool dummy = false;
      apply(events_[next_], s, dummy);
      ++next_;
    }
    uint8_t* out = &picture_[static_cast<size_t>(row) * kWidth];
    bool pfBlock = false;   // the playfield of the block of four pixels being drawn
    for (int x = 0; x < kWidth; ++x) {
      const long now = lineStart + 68 + x;
      while (next_ < events_.size() && events_[next_].when <= now) {
        apply(events_[next_], s, hmoveBlank);
        ++next_;
      }
      // The playfield is decided once for each block of four pixels, at its first pixel, as the chip and the emulator do: a write in the middle of a block
      // changes the next block, not the rest of this one (Stage I20, T-I20-1: the ninth life marker is the first picture with a write inside a block).
      if ((x & 3) == 0) {
        const int idx = x / 4;
        const int bitIdx = idx < 20 ? idx : ((s.ctrlpf & 1) ? 39 - idx : idx - 20);
        if (bitIdx < 4) pfBlock = (s.pf[0] >> (4 + bitIdx)) & 1;
        else if (bitIdx < 12) pfBlock = (s.pf[1] >> (7 - (bitIdx - 4))) & 1;
        else pfBlock = (s.pf[2] >> (bitIdx - 12)) & 1;
      }
      const bool pf = pfBlock;
      const int bw = 1 << ((s.ctrlpf >> 4) & 3);
      const uint8_t blEnabled = s.vdelbl ? s.enablOld : s.enabl;
      const bool bl = (blEnabled & 2) && mod160(x - s.pos[kBL]) < bw;
      const uint8_t g0 = s.vdelp[0] ? s.grpOld[0] : s.grp[0];
      const uint8_t g1 = s.vdelp[1] ? s.grpOld[1] : s.grp[1];
      const bool p0 = playerOn(x, s.pos[kP0], s.nusiz[0], g0, (s.refp[0] & 8) != 0);
      const bool p1 = playerOn(x, s.pos[kP1], s.nusiz[1], g1, (s.refp[1] & 8) != 0);
      const bool m0 = (s.enam[0] & 2) && missileOn(x, s.pos[kM0], s.nusiz[0]);
      const bool m1 = (s.enam[1] & 2) && missileOn(x, s.pos[kM1], s.nusiz[1]);
      // The collision latches (the ones the game reads): player 0 with player 1, missile 1 with player 1, the ball with player 1.
      if (p1) {
        if (p0) s.cxP0P1 = true;
        if (m1) s.cxM1P1 = true;
        if (bl) s.cxP1BL = true;
      }
      if ((s.vblank & 2) || (hmoveBlank && x < 8)) { out[x] = 0; continue; }
      const uint8_t pfColour = (s.ctrlpf & 2) ? (x < 80 ? s.colup0 : s.colup1) : s.colupf;
      uint8_t c = s.colubk;
      if (s.ctrlpf & 4) {
        if (pf) c = pfColour;
        else if (bl) c = s.colupf;
        else if (p0 || m0) c = s.colup0;
        else if (p1 || m1) c = s.colup1;
      } else {
        if (p0 || m0) c = s.colup0;
        else if (p1 || m1) c = s.colup1;
        else if (pf) c = pfColour;
        else if (bl) c = s.colupf;
      }
      out[x] = c;
    }
  }
}

void TiaVideo::renderDrawn() {
  // Everything the picture shows is drawn by the writes that have come so far; the rows up to the one of the latest write are done.
  const int lastRow = lastLine_ - kFirstLine;
  if (lastRow >= 0) renderThrough(lastRow < kHeight ? lastRow : kHeight - 1);
}

void TiaVideo::endPicture() {
  renderThrough(kHeight - 1);
  // The rest of the events (after the window) still change the state for the next picture.
  for (; next_ < events_.size(); ++next_) {
    bool dummy = false;
    apply(events_[next_], state_, dummy);
  }
  events_.clear();
  next_ = 0;
  row_ = 0;
  ++count_;
  order_ = 0;
  lastLine_ = 0;
}

}  // namespace game
