// stella_coverage: runs a ROM in the Stella 7.0 core on an input recording (a baseline `.inputs` file) one CPU instruction at a time
// and records which ROM addresses the CPU executes (instruction starts) and which ROM addresses an instruction reads as data.
// Stage F1, DECISIONS.md T-F1-2. The frame loop repeats StellaLIBRETRO::runFrame() step for step, as stella_timing does (Stage C4);
// the program counter, which Stella keeps private, is read through a standard-conforming access trick (an explicit template
// instantiation names the private member); no Stella file is changed. It only reads the ROM and the inputs and writes the output file.
//
// Usage: stella_coverage <rom> <inputs_file> <out_file> [<trace_file>]
//   trace_file  a baseline trace (docs\BASELINE.md) of the same inputs: the run is checked against it frame by frame
//               (picture fingerprint and all 128 memory bytes) and the result is printed; this proves the instruction-by-instruction
//               loop plays exactly like the normal one.
// Output lines (addresses are the 12-bit ROM offsets, 0x000-0xFFF; $F000 + offset):
//   x,<offset>,<times executed as an instruction start>,<first frame>
//   r,<instruction offset>,<offset read as data>,<times>      (the last read of an instruction that lies in the ROM and outside
//                                                              the bytes the instruction itself is made of: filtered by the reader)
//   m,<offset>,<times>                                         (instruction starts that were not in the ROM: counted, not listed)

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <map>
#include <set>
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
struct ATag { using type = uInt8 M6502::*; friend type steal(ATag); };
template struct Rob<ATag, &M6502::A>;
struct XTag { using type = uInt8 M6502::*; friend type steal(XTag); };
template struct Rob<XTag, &M6502::X>;
struct YTag { using type = uInt8 M6502::*; friend type steal(YTag); };
template struct Rob<YTag, &M6502::Y>;

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
    std::map<int, Seen> executed;                         // offset -> seen
    std::map<std::pair<int, int>, uint64_t> dataReads;    // (instruction offset, read offset) -> times
    std::map<int, uint64_t> outside;                      // instruction starts outside the ROM
    int frame = 0;
    std::vector<uint16_t> watch;                          // Stage F12: ROM addresses whose instruction result (A) is logged
    std::vector<std::array<int, 3>> watched;              // frame, address, A after the instruction
    int tiaFrom = -1, tiaTo = -2, tiaStride = 1;
    uint64_t tiaRegMask = ~0ULL;                          // tiaregs= : only these TIA registers (bit n = register n) are logged
    bool tiaOn = false;                                   // log TIA writes (tiaw=) ; ramdump= alone does not
    std::set<int> onlyFrames;                             // frames= : if not empty, only these frames are logged
    bool logFrame() const { return frame >= tiaFrom && frame <= tiaTo && frame % tiaStride == 0 && (onlyFrames.empty() || onlyFrames.count(frame)); }                         // Stage F18: frames whose writes to the TIA are logged
    int dumpPc = -1;                                      // Stage F18: memory dump when the instruction at this address is about to run
    std::vector<std::pair<int, std::string>> dumps;
    std::vector<std::array<int, 6>> tiaWrites;            // frame, scanline after, clock after, pc, register, value

    // The value a store instruction wrote: STA / STX / STY by opcode (the data bus holds the next opcode after the instruction).
    static int storedValue(uint8_t op, M6502& cpu)
    {
      if(op == 0x85 || op == 0x8D || op == 0x95 || op == 0x99 || op == 0x9D || op == 0x81 || op == 0x91) return cpu.*steal(ATag{});
      if(op == 0x86 || op == 0x8E || op == 0x96) return cpu.*steal(XTag{});
      if(op == 0x84 || op == 0x8C || op == 0x94) return cpu.*steal(YTag{});
      return -1;
    }

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
          if((pc & 0x1000) && static_cast<int>(pc & 0x0FFF) == dumpPc && logFrame()) {
            std::string h;
            for(int i = 0; i < 128; ++i) { char b[3]; std::snprintf(b, sizeof b, "%02x", riot.getRAM()[i]); h += b; }
            dumps.emplace_back(frame, h);
          }
          const bool ok = cpu.execute(1);
          if(tiaOn && logFrame()) {
            const uInt16 w = cpu.lastWriteAddress();
            const int sv = (pc & 0x1000) ? storedValue(static_cast<const uint8_t*>(getROM())[pc & 0x0FFF], cpu) : -1;
            if(sv >= 0 && !(w & 0x1080) && (w & 0x3F) < 0x2D && (w & 0x3F) != 0x02 && ((tiaRegMask >> (w & 0x3F)) & 1))
              tiaWrites.push_back({frame, static_cast<int>(tia.scanlines()), static_cast<int>(tia.clocksThisLine()), pc & 0x0FFF, w & 0x3F, sv});
          }
          if(pc & 0x1000) {
            for(uint16_t w : watch)
              if(w == pc) watched.push_back({frame, pc, cpu.*steal(ATag{})});
            const int off = pc & 0x0FFF;
            Seen& s = executed[off];
            if(s.count++ == 0) s.first = frame;
            const uInt16 r = cpu.lastReadAddress();
            if(r & 0x1000) ++dataReads[{off, r & 0x0FFF}];
          }
          else
            ++outside[pc];
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
  if(argc < 4) {
    std::fprintf(stderr, "usage: stella_coverage <rom> <inputs_file> <out_file> [<trace_file>]\n");
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
  if(argc > 4) {
    std::ifstream tr(argv[4]);
    std::string line;
    std::getline(tr, line); std::getline(tr, line); std::getline(tr, line);
    while(std::getline(tr, line)) {
      std::vector<std::string> p;
      std::istringstream in(line);
      for(std::string cell; std::getline(in, cell, ','); ) p.push_back(cell);
      if(p.size() >= 6) expect.emplace_back(p[3], p[5]);
    }
  }

  // TEST-ONLY memory edits (Stage F6, entries BL-065/066): <frame>:<hex address>:<hex value> from argument 6 on, applied before that frame runs.
  std::vector<std::array<int, 3>> pokes;
  for(int i = 5; i < argc; ++i) {
    int pf = 0, pa = 0, pv = 0;
    if(std::sscanf(argv[i], "%d:%x:%x", &pf, &pa, &pv) == 3) pokes.push_back({pf, pa, pv});
  }
  // Stage F12: `watch=<file>:<hex pc>,<hex pc>..` writes `frame,pc,A` after each run of those instructions.
  std::string watchFile, tiaFile, dumpFile;
  for(int i = 5; i < argc; ++i) {
    if(std::strncmp(argv[i], "tiaregs=", 8) == 0) {
      stella.tiaRegMask = 0;
      std::istringstream in(argv[i] + 8);
      for(std::string t; std::getline(in, t, ','); ) stella.tiaRegMask |= 1ULL << (std::stoul(t, nullptr, 16) & 0x3F);
    }
    if(std::strncmp(argv[i], "tiastride=", 10) == 0) stella.tiaStride = std::max(1, std::atoi(argv[i] + 10));
    if(std::strncmp(argv[i], "frames=", 7) == 0) {
      std::ifstream fl(argv[i] + 7);
      for(int fnum; fl >> fnum; ) stella.onlyFrames.insert(fnum);
      if(stella.tiaFrom < 0) { stella.tiaFrom = 0; stella.tiaTo = 1 << 30; }
    }
    if(std::strncmp(argv[i], "ramdump=", 8) == 0) {
      std::string a = argv[i] + 8;
      if(stella.tiaFrom < 0) { stella.tiaFrom = 0; stella.tiaTo = 1 << 30; }
      const size_t c = a.rfind(':');
      dumpFile = a.substr(0, c);
      stella.dumpPc = static_cast<int>(std::stoul(a.substr(c + 1), nullptr, 16)) & 0x0FFF;
    }
    if(std::strncmp(argv[i], "tiaw=", 5) == 0) {
      std::string a = argv[i] + 5;
      const size_t c1 = a.rfind(':'), c0 = a.rfind(':', c1 - 1);
      tiaFile = a.substr(0, c0);
      stella.tiaOn = true;
      stella.tiaFrom = std::stoi(a.substr(c0 + 1, c1 - c0 - 1));
      stella.tiaTo = std::stoi(a.substr(c1 + 1));
    }
    if(std::strncmp(argv[i], "watch=", 6) == 0) {
      std::string a = argv[i] + 6;
      const size_t c = a.rfind(':');
      watchFile = a.substr(0, c);
      std::istringstream in(a.substr(c + 1));
      for(std::string t; std::getline(in, t, ','); ) stella.watch.push_back(static_cast<uint16_t>(std::stoul(t, nullptr, 16)));
    }
  }

  stella.setROM(romPath.c_str(), rom.data(), rom.size());
  if(!stella.create(false)) { std::fprintf(stderr, "Stella could not create the console\n"); return 4; }
  Console& console = stella.osystem().console();
  TIA& tia = console.tia();
  M6532& riot = console.system().m6532();

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
    for(const auto& pk : pokes)
      if(pk[0] == f && pk[1] >= 0x80 && pk[1] <= 0xFF) stella.getRAM()[pk[1] - 0x80] = static_cast<uInt8>(pk[2]);
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

  std::ofstream out(argv[3]);
  out << "# stella_coverage: " << frames << " frames of " << argv[2] << "\n";
  for(const auto& [off, s] : stella.executed) out << "x," << off << ',' << s.count << ',' << s.first << '\n';
  for(const auto& [k, n] : stella.dataReads) out << "r," << k.first << ',' << k.second << ',' << n << '\n';
  for(const auto& [pc, n] : stella.outside) out << "m," << pc << ',' << n << '\n';
  out.close();
  if(!dumpFile.empty()) {
    std::ofstream w(dumpFile);
    for(const auto& e : stella.dumps) w << e.first << ',' << e.second << '\n';
  }
  if(!tiaFile.empty()) {
    std::ofstream w(tiaFile);
    for(const auto& e : stella.tiaWrites) w << e[0] << ',' << e[1] << ',' << e[2] << ',' << std::hex << e[3] << ',' << e[4] << ',' << e[5] << std::dec << '\n';
  }
  if(!watchFile.empty()) {
    std::ofstream w(watchFile);
    for(const auto& e : stella.watched) w << e[0] << ',' << std::hex << e[1] << std::dec << ',' << e[2] << '\n';
  }
  if(argc > 4)
    std::printf("done: %d frames, %zu instruction starts, checked %d frames against the trace: %d differences\n", frames, stella.executed.size(), checked, mismatches);
  else
    std::printf("done: %d frames, %zu instruction starts\n", frames, stella.executed.size());
  stella.destroy();
  return mismatches ? 5 : 0;
}
