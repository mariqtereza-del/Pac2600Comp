// stella_timing: runs a ROM in the Stella 7.0 core and records timing events at CPU-instruction precision:
// VSYNC/VBLANK changes, RIOT timer writes and timer reads (wait loops). Stage C4, DECISIONS.md T-C4-1.
// It repeats StellaLIBRETRO::runFrame() step for step; updateVideo()'s TIA::updateScanline() loop is unrolled
// into the very same sequence of M6502::execute(1) calls, and only public, side-effect-free values are read
// between instructions. It only reads the ROM file and writes into its output folder.
//
// Usage: stella_timing <rom> <out_dir> <frames> <reset_frame> [detail_frame ...]
//   detail_frame  frames whose full event list is written to events_<frame>.csv

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <set>
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
#include "EmulationTiming.hxx"
#include "Event.hxx"

namespace {

constexpr int kWidth = 160;  // TIAConstants::H_PIXEL
constexpr int kResetFrames = 10;
constexpr uInt8 kVSYNC = 0x00, kVBLANK = 0x01;

// RIOT addresses (any mirror): timer writes TIM1T..T1024T = $294-$297, timer read INTIM = $284/$286.
bool isTimerWrite(uInt16 a) { return (a & 0x1294) == 0x0294; }
bool isTimerRead(uInt16 a)  { return (a & 0x1285) == 0x0284; }

struct Ev {
  uInt32 cycle;  // CPU cycles since the start of this frame (after the instruction)
  uInt32 line;   // Stella's scanline number within the frame
  uInt32 clock;  // colour clock within the line (0-227)
  std::string what;
};

class TimedStella : public StellaLIBRETRO {
  public:
    // Same steps as StellaLIBRETRO::runFrame(); updateVideo() unrolled to single instructions.
    void runFrameTimed(std::vector<Ev>& ev, uInt64 frameStart)
    {
      Console& console = osystem().console();
      M6532& riot = console.system().m6532();
      for(int lcv = 0; lcv <= 127; lcv++)
        riot.poke(lcv | 0x80, getRAM()[lcv]);

      updateInput();

      TIA& tia = console.tia();
      M6502& cpu = console.system().m6502();
      ev.clear();
      bool vs = (tia.registerValue(kVSYNC) & 0x02) != 0, vb = (tia.registerValue(kVBLANK) & 0x02) != 0;
      uInt16 lastWrite = cpu.lastWriteAddress();
      auto add = [&](const std::string& what) {
        ev.push_back({static_cast<uInt32>(tia.cycles() - frameStart), tia.scanlines(), tia.clocksThisLine(), what});
      };
      // updateVideo(): while(true) { tia.updateScanline(); if(tia.scanlines() == 0) break; }
      // with TIA::updateScanline(): line = scanlines(); while(line == scanlines() && execute(1));
      while(true) {
        const uInt32 line = tia.scanlines();
        while(line == tia.scanlines()) {
          const bool ok = cpu.execute(1);
          const bool nvs = (tia.registerValue(kVSYNC) & 0x02) != 0, nvb = (tia.registerValue(kVBLANK) & 0x02) != 0;
          if(nvs != vs) add(nvs ? "VSYNC on" : "VSYNC off");
          if(nvb != vb) add(nvb ? "VBLANK on" : "VBLANK off");
          vs = nvs; vb = nvb;
          const uInt16 w = cpu.lastWriteAddress();
          if(w != lastWrite && isTimerWrite(w)) {
            static constexpr int kInterval[4] = {1, 8, 64, 1024};  // TIM1T, TIM8T, TIM64T, T1024T
            char b[24]; std::snprintf(b, sizeof b, "timer set T%d", kInterval[w & 3]);
            add(b);
          }
          lastWrite = w;
          if(isTimerRead(cpu.lastReadAddress())) add("timer read");
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

TimedStella stella;

uint64_t fnv1a(const uint8_t* data, size_t size)
{
  uint64_t h = 1469598103934665603ULL;
  for(size_t i = 0; i < size; ++i) { h ^= data[i]; h *= 1099511628211ULL; }
  return h;
}

}  // namespace

// Stella's FSNodeLIBRETRO reads the ROM through this function; same definition as in libretro.cxx.
uInt32 libretro_read_rom(void* data)
{
  std::memcpy(data, stella.getROM(), stella.getROMSize());
  return stella.getROMSize();
}

int main(int argc, char* argv[])
{
  if(argc < 5) {
    std::fprintf(stderr, "usage: stella_timing <rom> <out_dir> <frames> <reset_frame> [detail_frame ...]\n");
    return 2;
  }
  const std::string romPath = argv[1], outDir = argv[2];
  const int frames = std::stoi(argv[3]), resetFrame = std::stoi(argv[4]);
  std::set<int> detail;
  for(int i = 5; i < argc; ++i) detail.insert(std::stoi(argv[i]));

  std::ifstream romFile(romPath, std::ios::binary);
  const std::vector<char> rom((std::istreambuf_iterator<char>(romFile)), std::istreambuf_iterator<char>());
  if(rom.empty()) { std::fprintf(stderr, "cannot read ROM %s\n", romPath.c_str()); return 3; }

  stella.setROM(romPath.c_str(), rom.data(), rom.size());
  if(!stella.create(false)) { std::fprintf(stderr, "Stella could not create the console\n"); return 4; }

  Console& console = stella.osystem().console();
  TIA& tia = console.tia();
  EmulationTiming& timing = console.emulationTiming();

  std::ofstream info(outDir + "/info.txt");
  info << "display_format=" << console.about().DisplayFormat << "\nbankswitch=" << console.about().BankSwitch
       << "\nstella_lines_per_frame=" << timing.linesPerFrame()
       << "\nstella_cycles_per_frame=" << timing.cyclesPerFrame()
       << "\nstella_cycles_per_second=" << timing.cyclesPerSecond()
       << "\nstella_audio_sample_rate=" << timing.audioSampleRate()
       << "\nframes=" << frames << "\nreset_frame=" << resetFrame << "\n";
  info.close();

  // Per frame: total cycles, Stella's line count, and the event sequence with timer reads
  // collapsed into "waiting from line A to line B (n reads)".
  std::ofstream csv(outDir + "/frames.csv");
  csv << "frame,cycles,lines,events,hash\n";

  std::vector<Ev> ev;
  uInt64 frameStart = tia.cycles();
  for(int f = 0; f < frames; ++f) {
    const bool reset = resetFrame >= 0 && f >= resetFrame && f < resetFrame + kResetFrames;
    stella.setInputEvent(Event::ConsoleReset, reset ? 1 : 0);
    stella.runFrameTimed(ev, frameStart);

    std::string summary;
    for(size_t i = 0; i < ev.size(); ++i) {
      char b[96];
      if(ev[i].what == "timer read") {
        size_t j = i;
        while(j + 1 < ev.size() && ev[j + 1].what == "timer read") ++j;
        std::snprintf(b, sizeof b, "%swait L%u-L%u (%zu reads)", summary.empty() ? "" : "; ",
                      ev[i].line, ev[j].line, j - i + 1);
        i = j;
      }
      else
        std::snprintf(b, sizeof b, "%s%s L%u", summary.empty() ? "" : "; ", ev[i].what.c_str(), ev[i].line);
      summary += b;
    }

    char hash[17];
    std::snprintf(hash, sizeof hash, "%016llx", static_cast<unsigned long long>(
                  fnv1a(tia.frameBuffer(), static_cast<size_t>(tia.height()) * kWidth)));
    const uInt64 now = tia.cycles();
    csv << f << ',' << (now - frameStart) << ',' << tia.scanlinesLastFrame() << ",\"" << summary << "\"," << hash << '\n';

    if(detail.count(f)) {
      std::ofstream d(outDir + "/events_" + std::to_string(f) + ".csv");
      d << "cycle_in_frame,line,clock_in_line,event\n";
      for(const Ev& e : ev) d << e.cycle << ',' << e.line << ',' << e.clock << ',' << e.what << '\n';
    }
    frameStart = now;
  }

  csv.close();
  std::printf("done: %d frames\n", frames);
  stella.destroy();
  return 0;
}
