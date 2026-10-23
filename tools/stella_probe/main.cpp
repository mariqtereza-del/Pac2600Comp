// stella_probe: runs a ROM in the Stella 7.0 core (the same code as the reference app) and records,
// for every frame, what the TIA video chip produced. Stage C3, DECISIONS.md T-C3-1.
// It only reads the ROM file and writes into the output folder it is given.
//
// Usage: stella_probe <rom> <out_dir> <frames> <reset_frame> [dump_every] [log]
//   frames       number of frames to run
//   reset_frame  frame at which Game Reset is held for 10 frames (-1 = never)
//   dump_every   also save the raw picture every N frames (0 = never)
//   log          the word "log" turns on Stella's own logging to the console

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "StellaLIBRETRO.hxx"
#include "OSystem.hxx"
#include "Console.hxx"
#include "TIA.hxx"
#include "Event.hxx"

// After Stella's headers, so Windows' macros cannot change them.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

namespace {

constexpr int kWidth = 160;  // TIAConstants::H_PIXEL
constexpr int kResetFrames = 10;

StellaLIBRETRO stella;
int currentFrame = -1;

// On a crash, print where it happened (offset inside stella_probe.exe; look it up in the
// linker map build\stella_probe\stella_probe.map) and which frame was running.
LONG WINAPI reportCrash(EXCEPTION_POINTERS* e)
{
  const auto base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
  const auto addr = reinterpret_cast<uintptr_t>(e->ExceptionRecord->ExceptionAddress);
  std::fprintf(stderr, "CRASH: code 0x%08lx at exe offset 0x%llx (frame %d)\n",
               e->ExceptionRecord->ExceptionCode, static_cast<unsigned long long>(addr - base), currentFrame);
  if(e->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION)
    std::fprintf(stderr, "       %s address 0x%llx\n", e->ExceptionRecord->ExceptionInformation[0] ? "writing" : "reading",
                 static_cast<unsigned long long>(e->ExceptionRecord->ExceptionInformation[1]));
  std::fflush(stderr);
  return EXCEPTION_EXECUTE_HANDLER;
}

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
    std::fprintf(stderr, "usage: stella_probe <rom> <out_dir> <frames> <reset_frame> [dump_every]\n");
    return 2;
  }
  SetUnhandledExceptionFilter(reportCrash);
  const std::string romPath = argv[1], outDir = argv[2];
  const int frames = std::stoi(argv[3]), resetFrame = std::stoi(argv[4]);
  const int dumpEvery = argc > 5 ? std::stoi(argv[5]) : 0;
  const bool logging = argc > 6 && std::string(argv[6]) == "log";

  std::ifstream romFile(romPath, std::ios::binary);
  const std::vector<char> rom((std::istreambuf_iterator<char>(romFile)), std::istreambuf_iterator<char>());
  if(rom.empty()) { std::fprintf(stderr, "cannot read ROM %s\n", romPath.c_str()); return 3; }

  // Same sequence as Stella's libretro.cxx: setROM, then create (default settings).
  stella.setROM(romPath.c_str(), rom.data(), rom.size());
  if(!stella.create(logging)) { std::fprintf(stderr, "Stella could not create the console\n"); return 4; }

  Console& console = stella.osystem().console();
  TIA& tia = console.tia();
  const ConsoleInfo& about = console.about();

  std::ofstream info(outDir + "/info.txt");
  info << "rom=" << romPath << "\nrom_bytes=" << rom.size()
       << "\ncart_name=" << about.CartName << "\ncart_md5=" << about.CartMD5
       << "\nbankswitch=" << about.BankSwitch << "\ndisplay_format=" << about.DisplayFormat
       << "\ncontrol0=" << about.Control0 << "\ncontrol1=" << about.Control1
       << "\nconsole_ntsc=" << stella.getConsoleNTSC() << "\nframes=" << frames
       << "\nreset_frame=" << resetFrame << "\nreset_frames=" << kResetFrames << "\n";

  std::ofstream csv(outDir + "/frames.csv");
  csv << "frame,ready,scanlines,start_line,height,first_row,last_row,first_col,last_col,colors,hash\n";

  for(int f = 0; f < frames; ++f) {
    currentFrame = f;
    const bool reset = resetFrame >= 0 && f >= resetFrame && f < resetFrame + kResetFrames;
    stella.setInputEvent(Event::ConsoleReset, reset ? 1 : 0);
    stella.runFrame();

    const bool ready = stella.getVideoReady();
    const int height = static_cast<int>(tia.height());
    const uint8_t* fb = tia.frameBuffer();

    // Drawn area = rows/columns holding any pixel that is not colour 0x00 (black).
    int firstRow = -1, lastRow = -1, firstCol = kWidth, lastCol = -1;
    bool used[256] = {};
    for(int y = 0; y < height; ++y)
      for(int x = 0; x < kWidth; ++x) {
        const uint8_t c = fb[y * kWidth + x];
        used[c] = true;
        if(c != 0) {
          if(firstRow < 0) firstRow = y;
          lastRow = y;
          if(x < firstCol) firstCol = x;
          if(x > lastCol) lastCol = x;
        }
      }
    if(lastCol < 0) firstCol = -1;

    std::string colors;
    for(int c = 0; c < 256; ++c)
      if(used[c]) { char b[4]; std::snprintf(b, sizeof b, "%02x", c); colors += (colors.empty() ? "" : ":"); colors += b; }

    char hash[17];
    std::snprintf(hash, sizeof hash, "%016llx",
                  static_cast<unsigned long long>(fnv1a(fb, static_cast<size_t>(height) * kWidth)));
    csv << f << ',' << ready << ',' << tia.scanlinesLastFrame() << ',' << tia.startLine() << ','
        << height << ',' << firstRow << ',' << lastRow << ',' << firstCol << ',' << lastCol << ','
        << colors << ',' << hash << '\n';

    if(dumpEvery > 0 && f % dumpEvery == 0) {
      char name[64];
      std::snprintf(name, sizeof name, "/frame_%06d_%dx%d.raw", f, kWidth, height);
      std::ofstream raw(outDir + name, std::ios::binary);
      raw.write(reinterpret_cast<const char*>(fb), static_cast<std::streamsize>(height) * kWidth);
    }
  }

  std::printf("done: %d frames, %s, %s\n", frames, about.BankSwitch.c_str(), about.DisplayFormat.c_str());
  info.close();
  csv.close();
  stella.destroy();
  return 0;
}
