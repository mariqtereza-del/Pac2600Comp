// Playing the emulator's sound (Stage D8, DECISIONS.md T-D8-1). One SDL audio stream at the emulator's own sample rate;
// SDL converts it to whatever the sound device wants. The stream is kept near a small target (a few frames of sound)
// and every shortage or overflow is counted, so a run can be judged from numbers and not only by ear.
#pragma once

#include <atomic>
#include <cstdint>
#include <string>

struct SDL_AudioStream;

class AudioOutput {
public:
  struct Stats {
    long long pushedFrames = 0;     // sample pairs given to the stream (silence padding not included)
    long long droppedFrames = 0;    // pairs thrown away because the stream was already too full
    long long paddedFrames = 0;     // pairs of silence added (start-up and after a shortage)
    long long underruns = 0;        // times the sound device asked for sound the stream did not have
    long long shortfallBytes = 0;   // how much it was short, in bytes of the device's format
    double minQueuedMs = 1e9;       // smallest and largest amount waiting, measured just before each push
    double maxQueuedMs = 0;
    double targetMs = 0;
    long long pushes = 0;
  };

  AudioOutput() = default;
  ~AudioOutput();
  AudioOutput(const AudioOutput&) = delete;
  AudioOutput& operator=(const AudioOutput&) = delete;

  // Opens the default sound device. muted = the stream runs but only silence is played (for tests). False if there
  // is no usable sound device; problem() says why and the game can go on without sound.
  bool open(int sampleRate, bool muted);
  void close();
  bool isOpen() const { return stream_ != nullptr; }

  // The player's volume (Stage I12): 0 to 100 percent, and a mute button. Only the samples that are played change; 100 and not muted hands on the game's
  // sound untouched. The stream keeps running when muted (silence is played), so the pacing does not change. Callable from any thread.
  void setVolume(int percent, bool muted);
  static float gainOf(int percent);   // the factor the samples are multiplied by: the square of the slider, 1 at 100 and 0 at 0
  static void scale(const std::int16_t* in, std::int16_t* out, int frames, int percent);   // `frames` pairs, multiplied by gainOf(percent)

  // One frame's sound: 16-bit stereo, left and right interleaved, `frames` pairs.
  void push(const std::int16_t* stereo, int frames);

  // How much sound is waiting to be played, and how much there should be (Stage D9 runs a game frame whenever it is less).
  int queuedPairs() const { return queuedFrames(); }
  int targetPairs() const { return targetFrames_; }

  Stats stats() const;
  const std::string& problem() const { return problem_; }
  std::string describe() const;  // device name, device format, source format, buffer sizes

private:
  static void onNeedData(void* self, SDL_AudioStream* stream, int additional, int total);
  void pad(int frames);
  int queuedFrames() const;

  SDL_AudioStream* stream_ = nullptr;
  bool muted_ = false;
  bool started_ = false;
  bool audioStarted_ = false;  // SDL's audio part was started by us
  int rate_ = 0;
  int targetFrames_ = 0, maxFrames_ = 0;
  std::string problem_;
  Stats stats_;
  std::atomic<int> volume_{100};
  std::atomic<bool> userMuted_{false};
  std::atomic<long long> underruns_{0};
  std::atomic<long long> shortfall_{0};
  long long underrunsHandled_ = 0;
};
