// stella_pctrace: runs a ROM in the Stella 7.0 core on an input recording (a baseline `.inputs` file) one CPU instruction at a time
// and writes, for the frames asked for, one line per instruction: frame, scanline, colour clock, program counter (12-bit ROM offset).
// Stage F5, DECISIONS.md T-F5-1. Same method and same private-member access trick as stella_coverage.cpp (Stage F1); the run is checked
// against a golden trace the same way. It only reads the ROM and the inputs and writes the output file.
//
// Usage: stella_pctrace <rom> <inputs_file> <out_file> <first_frame> <last_frame> [<trace_file>]
// Output lines: <frame>,<line>,<clock>,<offset>   (line = Stella's scanline number inside the frame, 0-261)

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "StellaLIBRETRO.hxx"
#include "OSystem.hxx"
#include "Console.hxx"
#include "System.hxx"
#include "M6502.hxx"
#include "M6532.hxx"
#include "TIA.hxx"
#include "FrameBuffer.hxx"
#include "Event.hxx"

namespace {

// Access to M6502::PC (private) without changing Stella: an explicit instantiation may name a private member.
template<typename Tag, typename Tag::type Member> struct Rob { friend typename Tag::type steal(Tag) { return Member; } };
struct PcTag { using type = uInt16 M6502::*; friend type steal(PcTag); };
template struct Rob<PcTag, &M6502::PC>;

constexpr int kWidth = 160;

uint64_t fnv1a(const uint8_t* data, size_t size)
{
  uint64_t h = 1469598103934665603ULL;
  for(size_t i = 0; i < size; ++i) { h ^= data[i]; h *= 1099511628211ULL; }
  return h;
}

struct Seen { uint64_t count = 0; int first = -1; };

class CoverageStella : public StellaLIBRETRO {
  public:
    int frame = 0, firstFrame = 0, lastFrame = 0;
    std::ofstream out;

    // Same steps as StellaLIBRETRO::runFrame(), with updateVideo() unrolled to single instructions (see timing.cpp).
    void runFrameCounted()
    {
      Console& console = osystem().console();
      M6532& riot = console.system().m6532();
      for(int lcv = 0; lcv <= 127; lcv++)
        riot.poke(lcv | 0x80, getRAM()[lcv]);

      updateInput();

      TIA& tia = console.tia();
      M6502& cpu = console.system().m6502();
      while(true) {
        const uInt32 line = tia.scanlines();
        while(line == tia.scanlines()) {
          const uInt16 pc = cpu.*steal(PcTag{});
          const uInt32 clk = tia.clocksThisLine();
          const bool ok = cpu.execute(1);
          if(frame >= firstFrame && frame <= lastFrame && (pc & 0x1000))
            out << frame << ',' << line << ',' << clk << ',' << (pc & 0x0FFF) << '\n';
          if(!ok) break;
        }
        if(tia.scanlines() == 0) break;
      }
      if(tia.newFramePending()) {
        tia.renderToFrameBuffer();
        osystem().frameBuffer().updateInEmulationMode(0);
      }

      updateAudio();

      std::memcpy(getRAM(), riot.getRAM(), 128);
    }
};

CoverageStella stella;

}  // namespace

// Stella's FSNodeLIBRETRO reads the ROM through this function; same definition as in libretro.cxx.
uInt32 libretro_read_rom(void* data)
{
  std::memcpy(data, stella.getROM(), stella.getROMSize());
  return stella.getROMSize();
}

int main(int argc, char* argv[])
{
  if(argc < 6) {
    std::fprintf(stderr, "usage: stella_pctrace <rom> <inputs_file> <out_file> <first_frame> <last_frame> [<trace_file>]\n");
    return 2;
  }
  const std::string romPath = argv[1];

  std::ifstream romFile(romPath, std::ios::binary);
  const std::vector<char> rom((std::istreambuf_iterator<char>(romFile)), std::istreambuf_iterator<char>());
  if(rom.empty()) { std::fprintf(stderr, "cannot read ROM %s\n", romPath.c_str()); return 3; }

  // The recording: header, then "frame bits" at every change (docs\BASELINE.md; bits: up 1, down 2, left 4, right 8, fire 16,
  // select 32, reset 64, left difficulty A 128, right difficulty A 256, black and white 512).
  std::ifstream inputs(argv[2]);
  if(!inputs) { std::fprintf(stderr, "cannot read inputs %s\n", argv[2]); return 3; }
  int frames = 0;
  std::vector<std::pair<int, unsigned>> changes;
  for(std::string line; std::getline(inputs, line); ) {
    if(!line.empty() && line.back() == '\r') line.pop_back();
    if(line.rfind("frames=", 0) == 0) { frames = std::stoi(line.substr(7)); continue; }
    if(line.empty() || line[0] == '#' || line.rfind("PacMan2600", 0) == 0 || line.rfind("rom_sha1=", 0) == 0) continue;
    std::istringstream in(line);
    int f; unsigned b;
    if(in >> f >> b) changes.emplace_back(f, b);
  }

  // The trace to check against (frame, input, switches, picture, audio, ram).
  std::vector<std::pair<std::string, std::string>> expect;
  if(argc > 6) {
    std::ifstream tr(argv[6]);
    std::string line;
    std::getline(tr, line); std::getline(tr, line); std::getline(tr, line);
    while(std::getline(tr, line)) {
      std::vector<std::string> p;
      std::istringstream in(line);
      for(std::string cell; std::getline(in, cell, ','); ) p.push_back(cell);
      if(p.size() >= 6) expect.emplace_back(p[3], p[5]);
    }
  }

  stella.setROM(romPath.c_str(), rom.data(), rom.size());
  if(!stella.create(false)) { std::fprintf(stderr, "Stella could not create the console\n"); return 4; }
  Console& console = stella.osystem().console();
  TIA& tia = console.tia();
  M6532& riot = console.system().m6532();

  stella.firstFrame = std::stoi(argv[4]);
  stella.lastFrame = std::stoi(argv[5]);
  stella.out.open(argv[3]);
  unsigned bits = 0;
  size_t next = 0;
  int mismatches = 0, checked = 0;
  for(int f = 0; f < frames; ++f) {
    while(next < changes.size() && changes[next].first <= f) bits = changes[next++].second;
    stella.frame = f;
    stella.setInputEvent(Event::LeftJoystickUp, bits & 1);
    stella.setInputEvent(Event::LeftJoystickDown, bits & 2);
    stella.setInputEvent(Event::LeftJoystickLeft, bits & 4);
    stella.setInputEvent(Event::LeftJoystickRight, bits & 8);
    stella.setInputEvent(Event::LeftJoystickFire, bits & 16);
    stella.setInputEvent(Event::ConsoleSelect, bits & 32);
    stella.setInputEvent(Event::ConsoleReset, bits & 64);
    stella.setInputEvent(Event::ConsoleLeftDiffA, (bits & 128) ? 1 : 0);
    stella.setInputEvent(Event::ConsoleLeftDiffB, (bits & 128) ? 0 : 1);
    stella.setInputEvent(Event::ConsoleRightDiffA, (bits & 256) ? 1 : 0);
    stella.setInputEvent(Event::ConsoleRightDiffB, (bits & 256) ? 0 : 1);
    stella.setInputEvent(Event::ConsoleColor, (bits & 512) ? 0 : 1);
    stella.setInputEvent(Event::ConsoleBlackWhite, (bits & 512) ? 1 : 0);
    stella.runFrameCounted();

    if(f < static_cast<int>(expect.size())) {
      char hash[17];
      std::snprintf(hash, sizeof hash, "%016llx", static_cast<unsigned long long>(
                    fnv1a(tia.frameBuffer(), static_cast<size_t>(tia.height()) * kWidth)));
      std::string ram;
      for(int i = 0; i < 128; ++i) { char b[3]; std::snprintf(b, sizeof b, "%02x", riot.getRAM()[i]); ram += b; }
      ++checked;
      if(expect[f].first != hash || expect[f].second != ram) ++mismatches;
    }
  }

  stella.out.close();
  if(argc > 6)
    std::printf("done: %d frames, instructions traced for frames %d-%d, checked %d frames against the trace: %d differences\n", frames, stella.firstFrame, stella.lastFrame, checked, mismatches);
  else
    std::printf("done: %d frames\n", frames);
  stella.destroy();
  return mismatches ? 5 : 0;
}
