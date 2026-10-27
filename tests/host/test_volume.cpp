// Tests for the volume (Stage I12): the factor the played samples are multiplied by, and that the sound changes only in loudness.
// Needs no ROM and no sound device. Build: tools\build_host.cmd; run: build\host\test_volume.exe
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "audio.hpp"

static int failures = 0, checks = 0;

static void expect(bool ok, const std::string& what)
{
  ++checks;
  if(!ok) { ++failures; std::printf("FAIL: %s\n", what.c_str()); }
}

int main()
{
  expect(AudioOutput::gainOf(100) == 1.0f, "100 percent is exactly the game's sound");
  expect(AudioOutput::gainOf(0) == 0.0f, "0 percent is silence");
  expect(AudioOutput::gainOf(200) == 1.0f && AudioOutput::gainOf(-5) == 0.0f, "outside 0..100 is held at the ends");
  for(int v = 5; v <= 100; v += 5) expect(AudioOutput::gainOf(v) > AudioOutput::gainOf(v - 5), "each step of the slider is louder (" + std::to_string(v) + ")");
  expect(std::fabs(AudioOutput::gainOf(50) - 0.25f) < 1e-6f, "half way is a quarter of the amplitude (the square of the slider)");

  // A test sound: a ramp that includes the extremes of 16 bits.
  std::vector<std::int16_t> in;
  for(int i = 0; i < 2000; ++i) in.push_back(static_cast<std::int16_t>(-32768 + (i * 65535) / 1999));
  std::vector<std::int16_t> out(in.size());
  AudioOutput::scale(in.data(), out.data(), static_cast<int>(in.size() / 2), 100);
  expect(out == in, "100 percent changes no sample");
  AudioOutput::scale(in.data(), out.data(), static_cast<int>(in.size() / 2), 0);
  bool silent = true;
  for(std::int16_t s : out) if(s != 0) silent = false;
  expect(silent, "0 percent gives only zeros");
  for(int v = 5; v < 100; v += 5) {
    AudioOutput::scale(in.data(), out.data(), static_cast<int>(in.size() / 2), v);
    bool shape = true, quieter = true;
    const float gain = AudioOutput::gainOf(v);
    for(std::size_t i = 0; i < in.size(); ++i) {
      if(std::abs(static_cast<int>(out[i])) > std::abs(static_cast<int>(in[i]))) quieter = false;
      if(std::fabs(static_cast<float>(out[i]) - static_cast<float>(in[i]) * gain) > 0.5f) shape = false;   // the same wave, only smaller (to the nearest whole value)
      if(in[i] != 0 && out[i] != 0 && (in[i] < 0) != (out[i] < 0)) shape = false;                         // never turned over
    }
    expect(quieter && shape, "volume " + std::to_string(v) + ": the same wave, never louder, only smaller");
  }
  std::printf("%d checks, %d failed\n%s\n", checks, failures, failures == 0 ? "ALL TESTS PASSED" : "SOME TESTS FAILED");
  return failures == 0 ? 0 : 1;
}
