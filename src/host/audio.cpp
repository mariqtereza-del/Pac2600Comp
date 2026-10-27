#include "audio.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

namespace {

constexpr int kBytesPerPair = 4;  // 16-bit stereo
constexpr int kTargetFramesOfPicture = 4;  // the stream is kept at about 4 TV frames of sound (about 67 ms)
constexpr int kMaxFramesOfPicture = 8;     // more than this waiting: new sound is dropped instead of adding delay

}  // namespace

AudioOutput::~AudioOutput() { close(); }

void AudioOutput::onNeedData(void* self, SDL_AudioStream*, int additional, int)
{
  // Runs on the sound device's thread. additional > 0 means the stream did not have all the sound the device wanted.
  if(additional <= 0) return;
  auto* a = static_cast<AudioOutput*>(self);
  a->underruns_.fetch_add(1);
  a->shortfall_.fetch_add(additional);
}

bool AudioOutput::open(int sampleRate, bool muted)
{
  close();
  problem_.clear();
  stats_ = Stats{};
  underruns_ = 0;
  shortfall_ = 0;
  underrunsHandled_ = 0;
  muted_ = muted;
  started_ = false;
  rate_ = sampleRate;

  if(!SDL_InitSubSystem(SDL_INIT_AUDIO)) { problem_ = std::string("Sound could not start: ") + SDL_GetError(); return false; }
  audioStarted_ = true;

  const SDL_AudioSpec spec{SDL_AUDIO_S16, 2, sampleRate};
  stream_ = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);  // opened paused
  if(!stream_) { problem_ = std::string("No sound device could be opened: ") + SDL_GetError(); close(); return false; }
  SDL_SetAudioStreamGetCallback(stream_, onNeedData, this);

  const int frameLength = sampleRate / 60 + 1;  // about one TV frame of sound
  targetFrames_ = kTargetFramesOfPicture * frameLength;
  maxFrames_ = kMaxFramesOfPicture * frameLength;
  stats_.targetMs = 1000.0 * targetFrames_ / sampleRate;
  return true;
}

void AudioOutput::close()
{
  if(stream_) { SDL_DestroyAudioStream(stream_); stream_ = nullptr; }
  if(audioStarted_) { SDL_QuitSubSystem(SDL_INIT_AUDIO); audioStarted_ = false; }
}

int AudioOutput::queuedFrames() const
{
  return stream_ ? SDL_GetAudioStreamQueued(stream_) / kBytesPerPair : 0;
}

void AudioOutput::pad(int frames)
{
  if(frames <= 0) return;
  const std::vector<std::int16_t> silence(static_cast<std::size_t>(frames) * 2, 0);
  SDL_PutAudioStreamData(stream_, silence.data(), frames * kBytesPerPair);
  stats_.paddedFrames += frames;
}

void AudioOutput::push(const std::int16_t* stereo, int frames)
{
  if(!stream_ || frames <= 0) return;

  const int queued = queuedFrames();
  const double queuedMs = 1000.0 * queued / rate_;
  if(started_) {  // the first push finds an empty stream: not a measurement
    stats_.minQueuedMs = std::min(stats_.minQueuedMs, queuedMs);
    stats_.maxQueuedMs = std::max(stats_.maxQueuedMs, queuedMs);
  }
  ++stats_.pushes;

  if(!started_) {  // a cushion of silence first, then let the device start playing
    pad(targetFrames_);
    SDL_ResumeAudioStreamDevice(stream_);
    started_ = true;
  } else if(const long long now = underruns_.load(); now != underrunsHandled_) {
    underrunsHandled_ = now;  // it ran dry: rebuild the cushion so it does not run dry again at once
    pad(targetFrames_ - queued);
  }

  if(queued + frames > maxFrames_) {  // too much waiting already (the clocks drifted apart): do not add delay
    stats_.droppedFrames += frames;
    return;
  }
  const int volume = volume_.load();
  if(muted_ || userMuted_.load() || volume <= 0) {
    const std::vector<std::int16_t> silence(static_cast<std::size_t>(frames) * 2, 0);
    SDL_PutAudioStreamData(stream_, silence.data(), frames * kBytesPerPair);
  } else if(volume >= 100) {
    SDL_PutAudioStreamData(stream_, stereo, frames * kBytesPerPair);
  } else {
    std::vector<std::int16_t> quieter(static_cast<std::size_t>(frames) * 2);
    scale(stereo, quieter.data(), frames, volume);
    SDL_PutAudioStreamData(stream_, quieter.data(), frames * kBytesPerPair);
  }
  stats_.pushedFrames += frames;
}

void AudioOutput::setVolume(int percent, bool muted)
{
  volume_ = std::clamp(percent, 0, 100);
  userMuted_ = muted;
}

void AudioOutput::scale(const std::int16_t* in, std::int16_t* out, int frames, int percent)
{
  const float gain = gainOf(percent);
  for(int i = 0; i < frames * 2; ++i) out[i] = static_cast<std::int16_t>(std::lround(static_cast<float>(in[i]) * gain));
}

float AudioOutput::gainOf(int percent)
{
  const float slider = static_cast<float>(std::clamp(percent, 0, 100)) / 100.0f;
  return slider * slider;
}

AudioOutput::Stats AudioOutput::stats() const
{
  Stats s = stats_;
  s.underruns = underruns_.load();
  s.shortfallBytes = shortfall_.load();
  if(s.minQueuedMs > 1e8) s.minQueuedMs = 0;
  return s;
}

std::string AudioOutput::describe() const
{
  if(!stream_) return "no sound device";
  SDL_AudioSpec source{}, device{};
  SDL_GetAudioStreamFormat(stream_, &source, &device);
  const SDL_AudioDeviceID id = SDL_GetAudioStreamDevice(stream_);
  const char* name = SDL_GetAudioDeviceName(id);
  int deviceFrames = 0;
  SDL_AudioSpec deviceNative{};
  SDL_GetAudioDeviceFormat(id, &deviceNative, &deviceFrames);
  char text[512];
  std::snprintf(text, sizeof text, "device=%s\nsource_format=%d Hz, %d channels, 16-bit\ndevice_format=%d Hz, %d channels, %s\ndevice_buffer_pairs=%d\nmuted=%s",
                name ? name : "?", source.freq, source.channels, device.freq, device.channels, SDL_GetAudioFormatName(device.format), deviceFrames,
                muted_ ? "yes" : "no");
  return text;
}
