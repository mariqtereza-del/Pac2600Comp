// Running the game at the console's own speed, steered by the sound card, and the audio check.
// (Stage G4.7: moved out of main.cpp, the code is unchanged.)
#pragma once
#include "common.hpp"
#include "options.hpp"

namespace host {

// ---- Frame pacing (Stage D9, DECISIONS.md T-D9-1 and T-D9-2; the authentic speed: D-004 and T-D14-2) ----
// A real NTSC Atari 2600 runs from the colour clock of 3,579,545.45 Hz: 228 colour clocks a line, 262 lines a frame =
// 59.9227 frames a second, and its sound chip makes 31,399.5 samples a second. Stella plays at exactly 60 frames and
// 31,440 samples (0.13 % fast). The port follows the console (the owner's rule: authentic first); --stella-speed brings
// back Stella's speed for comparison. The emulation itself is counted in CPU cycles and does not change.
inline constexpr double kConsoleFrameHz = 3579545.4545 / (228.0 * 262.0);

double frameRate(const Options& o);

// The rate the sound device is told the emulator's samples have: the sound chip's own rate, which sets pitch and speed.
int soundRate(const Options& o, const Emulator& emulator);

// Decides when the next game frame is due. Frames are started from a steady PC timer (so they are evenly spaced, which a
// monitor needs to show each one for the same number of refreshes), but the timer's speed is steered by the sound card:
// when more sound is waiting than the target the next frames are started a little later, when less, a little sooner.
// So on average the game runs exactly as fast as the sound card plays (59.92 frames a second for this game: 524 sound
// pairs per frame at 31,400 per second), and music and picture cannot drift apart. Without a sound device, or with
// --timer-clock, the steering is simply off and the PC's clock is used.
class Pacer {
public:
  Pacer(const AudioOutput& audio, bool timerOnly, double frameHz)
      : audio_(audio), useAudio_(audio.isOpen() && !timerOnly), frameNs_(1e9 / frameHz), next_(SDL_GetTicksNS()) {}

  bool due(Uint64 now) const { return now >= next_; }

  // When the frame that is due now was meant to start (Stage H6: how late frames start is measured).
  Uint64 dueAt() const { return next_; }

  // Nanoseconds until the next frame is due (0 if it is due now).
  Uint64 untilDue(Uint64 now) const { return now >= next_ ? 0 : next_ - now; }

  void ran(Uint64 now)
  {
    double correction = 0;  // fraction by which this frame's period is lengthened (waiting sound above target) or shortened
    if(useAudio_) {
      const double queued = audio_.queuedPairs();
      smoothed_ = frames_ == 0 ? queued : smoothed_ + 0.02 * (queued - smoothed_);  // the device takes sound in chunks: average them out
      correction = std::clamp(kGain * (smoothed_ - audio_.targetPairs()), -kMaxCorrection, kMaxCorrection);
      correctionSum_ += correction;
    }
    ++frames_;
    next_ += static_cast<Uint64>(frameNs_ * (1.0 + correction));
    if(now > next_ + static_cast<Uint64>(5 * frameNs_)) next_ = now;  // far behind (the PC was busy): do not try to catch up
  }

  bool usesSoundClock() const { return useAudio_; }
  double averageCorrectionPpm() const { return frames_ ? 1e6 * correctionSum_ / static_cast<double>(frames_) : 0.0; }

private:
  static constexpr double kGain = 4e-6;           // period change per sound pair above target (a time constant of about 8 s)
  static constexpr double kMaxCorrection = 1e-3;  // never more than 0.1 %
  const AudioOutput& audio_;
  bool useAudio_;
  double frameNs_;  // one frame of the console's clock, in nanoseconds
  Uint64 next_;
  double smoothed_ = 0, correctionSum_ = 0;
  long long frames_ = 0;
};

// The lines of the run log about how long each game frame took to compute and how late it started (Stage H6): milliseconds, in the order the frames ran.
std::string frameTimingText(const std::vector<float>& computeMs, const std::vector<float>& lateMs, double firstPictureMs, double presentHz);

// Sleeps until the time given (nanoseconds from now), in small steps and ending with a precise wait, so that frames start
// within a fraction of a millisecond of when they are due.
void waitFor(Uint64 ns);

// ---- Sound (Stage D8, DECISIONS.md T-D8-1) ----
std::string audioStatsText(const AudioOutput& audio, double seconds);

// --audio-check: the game runs at 60 frames per second (no window) and its sound goes to the real sound device.
bool audioCheck(const Options& o);

}  // namespace host
