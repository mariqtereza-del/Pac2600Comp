#include "tia_audio.hpp"

#include <algorithm>
#include <cmath>

namespace game {

namespace {
// The chip's mixing of the two volumes: a resistor network, so two volumes added are less than twice as loud.
int16_t mixingEntry(int v, int vMax) {
  const double rMax = 30.0, r = 1.0;
  return static_cast<int16_t>(std::floor(0x7fff * static_cast<double>(v) / static_cast<double>(vMax) * (rMax + r * vMax) / (rMax + r * v)));
}
}  // namespace

TiaAudio::TiaAudio() {
  for (int i = 0; i <= 0x1e; ++i) mixing_[i] = mixingEntry(i, 0x1e);
}

void TiaAudio::Channel::phase0() {
  if (clockEnable) {
    noiseCounterBit4 = noiseCounter & 0x01;
    switch (audc & 0x03) {
      case 0x00:
      case 0x01: pulseCounterHold = false; break;
      case 0x02: pulseCounterHold = (noiseCounter & 0x1e) != 0x02; break;
      default: pulseCounterHold = !noiseCounterBit4; break;
    }
    if ((audc & 0x03) == 0) {
      noiseFeedback = ((pulseCounter ^ noiseCounter) & 0x01) || !(noiseCounter || (pulseCounter != 0x0a)) || !(audc & 0x0c);
    } else {
      noiseFeedback = (((noiseCounter & 0x04) ? 1 : 0) ^ (noiseCounter & 0x01)) || noiseCounter == 0;
    }
  }
  clockEnable = divCounter == audf;
  if (divCounter == audf || divCounter == 0x1f) divCounter = 0; else ++divCounter;
}

void TiaAudio::Channel::phase1() {
  if (clockEnable) {
    bool pulseFeedback = false;
    switch (audc >> 2) {
      case 0x00: pulseFeedback = (((pulseCounter & 0x02) ? 1 : 0) ^ (pulseCounter & 0x01)) && (pulseCounter != 0x0a) && (audc & 0x03); break;
      case 0x01: pulseFeedback = !(pulseCounter & 0x08); break;
      case 0x02: pulseFeedback = !noiseCounterBit4; break;
      default: pulseFeedback = !((pulseCounter & 0x02) || !(pulseCounter & 0x0e)); break;
    }
    noiseCounter >>= 1;
    if (noiseFeedback) noiseCounter |= 0x10;
    if (!pulseCounterHold) {
      pulseCounter = ~(pulseCounter >> 1) & 0x07;
      if (pulseFeedback) pulseCounter |= 0x08;
    }
  }
}

void TiaAudio::write(int line, int clock, uint8_t reg, uint8_t value) {
  if (reg < 0x15 || reg > 0x1A) return;
  events_.push_back({static_cast<long>(line) * kClocks + clock + kWriteDelay, order_++, reg, value});
}

void TiaAudio::apply(const Event& e) {
  switch (e.reg) {
    case 0x15: ch_[0].audc = e.value & 0x0F; break;
    case 0x16: ch_[1].audc = e.value & 0x0F; break;
    case 0x17: ch_[0].audf = e.value & 0x1F; break;
    case 0x18: ch_[1].audf = e.value & 0x1F; break;
    case 0x19: ch_[0].audv = e.value & 0x0F; break;
    case 0x1A: ch_[1].audv = e.value & 0x0F; break;
    default: break;
  }
}

void TiaAudio::endFrame(int lines) {
  samples_.clear();
  std::stable_sort(events_.begin(), events_.end(), [](const Event& a, const Event& b) { return a.when != b.when ? a.when < b.when : a.order < b.order; });
  const long end = static_cast<long>(lines) * kClocks + 15;   // the last clock of the frame: the end of the instruction that switches the vertical sync off
  size_t ev = 0;
  long i = next_;
  while (i <= end) {
    while (ev < events_.size() && events_[ev].when <= i) apply(events_[ev++]);
    // one clock: the volumes are added up, and at four places of the line the generators step and the samples are made
    sum0_ += ch_[0].volume();
    sum1_ += ch_[1].volume();
    ++sumCount_;
    const int c = static_cast<int>(i % kClocks);
    if (c == 9 || c == 81) { ch_[0].phase0(); ch_[1].phase0(); }
    else if (c == 37 || c == 149) {
      ch_[0].phase1();
      ch_[1].phase1();
      const uint8_t s0 = static_cast<uint8_t>(sum0_ / sumCount_);
      const uint8_t s1 = static_cast<uint8_t>(sum1_ / sumCount_);
      sum0_ = sum1_ = sumCount_ = 0;
      samples_.push_back(mixing_[s0 + s1]);
    }
    // the clocks up to the next event or special clock change nothing but the sums
    long j = i + 1;
    long stop = end + 1;
    if (ev < events_.size()) stop = std::min(stop, std::max(events_[ev].when, j));
    for (const int sp : {9, 37, 81, 149}) {
      long cand = i - c + sp;
      if (cand <= i) cand += kClocks;
      stop = std::min(stop, cand);
    }
    if (stop > j) {
      const long n = stop - j;
      sum0_ += static_cast<uint32_t>(ch_[0].volume() * n);
      sum1_ += static_cast<uint32_t>(ch_[1].volume() * n);
      sumCount_ += static_cast<uint32_t>(n);
      j = stop;
    }
    i = j;
  }
  while (ev < events_.size()) apply(events_[ev++]);
  pending_.insert(pending_.end(), samples_.begin(), samples_.end());
  const size_t whole = pending_.size() / kFragment * kFragment;
  output_.assign(pending_.begin(), pending_.begin() + static_cast<std::ptrdiff_t>(whole));
  pending_.erase(pending_.begin(), pending_.begin() + static_cast<std::ptrdiff_t>(whole));
  events_.clear();
  order_ = 0;
  next_ = end + 1 - static_cast<long>(lines) * kClocks;
}

}  // namespace game
