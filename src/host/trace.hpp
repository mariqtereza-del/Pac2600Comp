// Frame traces: what the original game did on every frame of a replay (Stage E1, DECISIONS.md T-E1-1). No emulator code.
//
// A trace is a text file:
//     PacMan2600-trace 1
//     rom_sha1=<40 hex digits>
//     columns=frame,input,switches,picture,audio,ram
//     <frame>,<input>,<switches>,<picture>,<audio>,<ram>      one line per frame, frame 0 first, no gaps
// frame    the number of the frame, counted from power-on
// input    the console input given on that frame (controls::Input::pack, decimal)
// switches the console switch register (SWCHB) as the game could read it on that frame, 2 hex digits
// picture  FNV-1a of the 160 x H palette numbers the chip drew (the same fingerprint as Phase C), 16 hex digits
// audio    FNV-1a of the frame's sound bytes (16-bit stereo, little endian, exactly as the emulator made them), 16 hex digits
// ram      the 128 bytes of RIOT memory after the frame, 256 hex digits
// The lines carry Atari's memory contents, so traces are kept local (reference\baseline\), never in Git (docs\BASELINE.md).
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace trace {

constexpr const char* kHeaderLine = "PacMan2600-trace 1";
constexpr const char* kColumnsLine = "columns=frame,input,switches,picture,audio,ram";

std::uint64_t fnv1a(const void* data, std::size_t size);  // 64-bit FNV-1a, the fingerprint of Phase C

struct Row {
  long long frame = 0;
  std::uint32_t input = 0;
  std::uint8_t switches = 0;
  std::uint64_t picture = 0, audio = 0;
  std::array<std::uint8_t, 128> ram{};
  bool operator==(const Row& o) const { return frame == o.frame && input == o.input && switches == o.switches && picture == o.picture && audio == o.audio && ram == o.ram; }
};

std::string header(const std::string& romSha1);  // the three header lines, each ending in a newline
std::string line(const Row& row);                // one row, ending in a newline
bool parseLine(const std::string& text, Row& out, std::string& error);

}  // namespace trace
