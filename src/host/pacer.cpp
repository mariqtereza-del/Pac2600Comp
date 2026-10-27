// Running the game at the console's own speed, steered by the sound card, and the audio check.
// (Stage G4.7: moved out of main.cpp, the code is unchanged.)
#include <algorithm>
#include <sstream>

#include "pacer.hpp"
#include "options.hpp"
#include "rom_choice.hpp"

namespace host {

double frameRate(const Options& o) { return o.stellaSpeed ? 60.0 : kConsoleFrameHz; }

// The rate the sound device is told the emulator's samples have: the sound chip's own rate, which sets pitch and speed.
int soundRate(const Options& o, const Emulator& emulator)
{
  return o.stellaSpeed ? emulator.audioRate() : static_cast<int>(std::lround(emulator.audioRate() * kConsoleFrameHz / 60.0));
}

// Sleeps until the time given (nanoseconds from now), in small steps and ending with a precise wait, so that frames start
// within a fraction of a millisecond of when they are due.
void waitFor(Uint64 ns)
{
  if(ns > 2000000) ns = 1000000;  // look again after 1 ms: the clock may have been steered, or the program may be closing
  SDL_DelayPrecise(ns);
}

// ---- Sound (Stage D8, DECISIONS.md T-D8-1) ----

std::string audioStatsText(const AudioOutput& audio, double seconds)
{
  const AudioOutput::Stats st = audio.stats();
  char text[1024];
  std::snprintf(text, sizeof text,
                "%s\nseconds=%.1f\npushes=%lld\npushed_pairs=%lld\npushed_pairs_per_second=%.1f\npadded_pairs=%lld\ndropped_pairs=%lld\n"
                "underruns=%lld\nshortfall_bytes=%lld\nqueued_ms_target=%.1f\nqueued_ms_smallest=%.1f\nqueued_ms_largest=%.1f\n",
                audio.describe().c_str(), seconds, st.pushes, st.pushedFrames, seconds > 0 ? static_cast<double>(st.pushedFrames) / seconds : 0.0,
                st.paddedFrames, st.droppedFrames, st.underruns, st.shortfallBytes, st.targetMs, st.minQueuedMs, st.maxQueuedMs);
  return text;
}

// --audio-check: the game runs at 60 frames per second (no window) and its sound goes to the real sound device.
bool audioCheck(const Options& o)
{
  std::ofstream report(o.audioCheckFile);
  if(!report) return false;
  const auto found = rom::findQuietly(o.romArg, o.settingsFile, o.romsDir);
  std::vector<std::uint8_t> bytes;
  if(!found || found->check.status != rom::Status::Ok || !readRomBytes(found->check.path, bytes)) { report << "result=no usable ROM\n"; return false; }
  Emulator emulator;
  if(const std::string error = emulator.load(fileNameOf(found->check.path), bytes); !error.empty()) { report << "result=" << error << '\n'; return false; }

  AudioOutput audio;
  if(!audio.open(soundRate(o, emulator), o.mute)) { report << "result=FAIL " << audio.problem() << '\n'; return false; }

  Pacer pacer(audio, o.timerClock, frameRate(o));  // the same pacing as the game window
  const Uint64 started = SDL_GetTicksNS();
  long long frames = 0;
  bool stalled = false;
  while(static_cast<double>(SDL_GetTicksNS() - started) < o.audioCheckSeconds * 1e9) {
    if(o.stallMs > 0 && !stalled && static_cast<double>(SDL_GetTicksNS() - started) >= 5e9) { SDL_Delay(static_cast<Uint32>(o.stallMs)); stalled = true; }
    const Uint64 now = SDL_GetTicksNS();
    int steps = 0;
    while(steps < 5 && pacer.due(now)) {
      emulator.setConsoleReset(o.resetAt >= 0 && frames >= o.resetAt && frames < o.resetAt + 10);
      emulator.runFrame();
      audio.push(emulator.audio(), emulator.audioFrames());
      pacer.ran(now); ++steps; ++frames;
    }
    waitFor(pacer.untilDue(SDL_GetTicksNS()));
  }
  const double seconds = static_cast<double>(SDL_GetTicksNS() - started) / 1e9;
  const AudioOutput::Stats st = audio.stats();
  report << audioStatsText(audio, seconds) << "clock=" << (pacer.usesSoundClock() ? "sound card" : "PC timer") << "\naverage_clock_correction_ppm=" << pacer.averageCorrectionPpm() << "\nframes=" << frames << "\nframes_per_second=" << frames / seconds << '\n';
  const bool ok = st.underruns == 0 && st.pushedFrames > 0;
  report << "result=" << (ok ? "OK" : "CHECK underruns or no sound pushed") << '\n';
  return ok;
}

std::string frameTimingText(const std::vector<float>& computeMs, const std::vector<float>& lateMs, double firstPictureMs, double presentHz)
{
  auto percentile = [](std::vector<float> v, double p) { if(v.empty()) return 0.0f; std::sort(v.begin(), v.end()); return v[static_cast<size_t>(p * static_cast<double>(v.size() - 1))]; };
  auto mean = [](const std::vector<float>& v) { double s = 0; for(float x : v) s += x; return v.empty() ? 0.0 : s / static_cast<double>(v.size()); };
  const auto over = [](const std::vector<float>& v, float limit) { return std::count_if(v.begin(), v.end(), [limit](float x) { return x > limit; }); };
  std::ostringstream log;
  log << "frames_compute_over_16_7_ms=" << over(computeMs, 16.7f) << "\nframes_compute_over_5_ms=" << over(computeMs, 5.0f)
      << "\nframe_compute_ms_mean=" << mean(computeMs) << "\nframe_compute_ms_p99=" << percentile(computeMs, 0.99) << "\nframe_compute_ms_max=" << percentile(computeMs, 1.0)
      << "\nframe_start_late_ms_p99=" << percentile(lateMs, 0.99) << "\nframe_start_late_ms_max=" << percentile(lateMs, 1.0)
      << "\nfirst_picture_ms=" << firstPictureMs << "\npresent_hz_simulated=" << presentHz << '\n';
  return log.str();
}

}  // namespace host
