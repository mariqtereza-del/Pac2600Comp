// The sound chip of the console as a small model (Stage F24.1): it is given the writes the program makes to the six sound registers, each with the
// scanline and the colour clock at which it happened, and makes the frame's sound samples (31,440 a second: two per scanline, 524 for a frame of 262
// lines). The two channels are the chip's: a divider (AUDF), a pulse and noise pattern generator (AUDC) and a volume (AUDV); the volume is averaged over
// the clocks between two samples and the two channels are added through the chip's non-linear mixing table. It is checked against the emulator's sound
// (tools/f24_audio.py).
#pragma once
#include <array>
#include <cstdint>
#include <vector>

namespace game {

class TiaAudio {
 public:
  static constexpr int kClocks = 228;       // colour clocks in a scanline
  static constexpr int kLines = 262;        // scanlines in a steady frame
  static constexpr int kFirstFrameLines = 43;   // the partial first frame after power-on (lines 0 to 42; found by comparison, 41-42 and 44-49 fail)
  // A write changes the sound this many colour clocks after the clock at which the picture side sees it (the sound counter runs behind the picture's
  // clock). Found by comparison with the emulator over all 439,680 frames: 60 is right, 59 and 61 are not.
  static constexpr int kWriteDelay = 60;

  TiaAudio();

  // A write made at scanline `line` of the frame (0 at the end of the vertical sync), colour clock `clock` (the end of the writing instruction).
  // Only the registers AUDC0 $15, AUDC1 $16, AUDF0 $17, AUDF1 $18, AUDV0 $19 and AUDV1 $1A matter; others are ignored.
  void write(int line, int clock, uint8_t reg, uint8_t value);
  // Ends the frame, which had `lines` scanlines, and makes its samples (mono: the same value for left and right).
  void endFrame(int lines);
  // The samples made in this frame.
  const std::vector<int16_t>& samples() const { return samples_; }
  // What the emulator hands over after this frame: the sound is passed on in whole fragments of 524 samples (a steady frame's worth), so the first frame
  // (90 samples) gives nothing and every later frame gives the 524 samples that finished in it, which are the last 90 of the frame before and the first 434
  // of this one. Mono: left and right are the same.
  const std::vector<int16_t>& output() const { return output_; }
  static constexpr int kFragment = 524;

 private:
  struct Channel {
    uint8_t audc = 0, audf = 0, audv = 0;
    bool clockEnable = false, noiseFeedback = false, noiseCounterBit4 = false, pulseCounterHold = false;
    uint8_t divCounter = 0, pulseCounter = 0, noiseCounter = 0;
    int volume() const { return (pulseCounter & 1) * audv; }
    void phase0();
    void phase1();
  };
  struct Event { long when; int order; uint8_t reg; uint8_t value; };
  void apply(const Event& e);

  Channel ch_[2];
  std::vector<Event> events_;
  std::vector<int16_t> samples_, pending_, output_;
  std::array<int16_t, 0x1F> mixing_{};
  long next_ = 0;            // the next colour clock to process, counted from line 0 of the frame
  uint32_t sum0_ = 0, sum1_ = 0, sumCount_ = 0;
  int order_ = 0;
};

}  // namespace game
