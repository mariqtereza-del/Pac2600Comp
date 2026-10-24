// stella_script: runs a ROM in the Stella 7.0 core (StellaLIBRETRO::runFrame, as in main.cpp) with inputs
// read from a script file, and records per frame the picture and status-bar fingerprints, the colours,
// the console switch positions and the 128 bytes of RAM. Stage C6, DECISIONS.md T-C6-1.
// It only reads the ROM and the script, and writes into its output folder.
//
// Usage: stella_script <rom> <out_dir> <frames> <script_file>
// Script lines ('#' starts a comment):
//   <from> <to> <INPUT>     hold INPUT on frames from..to (inclusive). INPUT is one of
//                           UP DOWN LEFT RIGHT FIRE SELECT RESET (held while active) or
//                           LDIFF_A LDIFF_B RDIFF_A RDIFF_B COLOR BW (switches: stay where set)
//   dump <from> <to> <step> save the picture of frames from, from+step, ... to as frame_NNNNNN_160xH.raw
//   region <y0> <y1> <x0> <x1>  also record the colours inside this picture area (rows y0..y1, columns x0..x1)

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
#include "M6532.hxx"
#include "Switches.hxx"
#include "TIA.hxx"
#include "Event.hxx"

namespace {

constexpr int kWidth = 160;  // TIAConstants::H_PIXEL

StellaLIBRETRO stella;

uint64_t fnv1a(const uint8_t* data, size_t size)
{
  uint64_t h = 1469598103934665603ULL;
  for(size_t i = 0; i < size; ++i) { h ^= data[i]; h *= 1099511628211ULL; }
  return h;
}

struct Hold { int from, to; std::string input; };

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
    std::fprintf(stderr, "usage: stella_script <rom> <out_dir> <frames> <script_file>\n");
    return 2;
  }
  const std::string romPath = argv[1], outDir = argv[2];
  const int frames = std::stoi(argv[3]);

  const std::map<std::string, Event::Type> events = {
    {"UP", Event::LeftJoystickUp}, {"DOWN", Event::LeftJoystickDown}, {"LEFT", Event::LeftJoystickLeft},
    {"RIGHT", Event::LeftJoystickRight}, {"FIRE", Event::LeftJoystickFire}, {"SELECT", Event::ConsoleSelect},
    {"RESET", Event::ConsoleReset}, {"LDIFF_A", Event::ConsoleLeftDiffA}, {"LDIFF_B", Event::ConsoleLeftDiffB},
    {"RDIFF_A", Event::ConsoleRightDiffA}, {"RDIFF_B", Event::ConsoleRightDiffB},
    {"COLOR", Event::ConsoleColor}, {"BW", Event::ConsoleBlackWhite}};

  std::vector<Hold> holds;
  std::set<int> dump;
  int region[4] = {-1, -1, -1, -1};
  std::ifstream script(argv[4]);
  if(!script) { std::fprintf(stderr, "cannot read script %s\n", argv[4]); return 3; }
  for(std::string line; std::getline(script, line); ) {
    line = line.substr(0, line.find('#'));
    std::istringstream in(line);
    std::string first;
    if(!(in >> first)) continue;
    if(first == "dump") {
      int a, b, s;
      if(!(in >> a >> b >> s) || s <= 0) { std::fprintf(stderr, "bad dump line: %s\n", line.c_str()); return 3; }
      for(int f = a; f <= b; f += s) dump.insert(f);
      continue;
    }
    if(first == "region") {
      if(!(in >> region[0] >> region[1] >> region[2] >> region[3]) || region[0] > region[1] || region[2] > region[3]
         || region[0] < 0 || region[3] >= kWidth) { std::fprintf(stderr, "bad region line: %s\n", line.c_str()); return 3; }
      continue;
    }
    Hold h{std::stoi(first), 0, ""};
    if(!(in >> h.to >> h.input) || !events.count(h.input)) { std::fprintf(stderr, "bad line: %s\n", line.c_str()); return 3; }
    holds.push_back(h);
  }

  std::ifstream romFile(romPath, std::ios::binary);
  const std::vector<char> rom((std::istreambuf_iterator<char>(romFile)), std::istreambuf_iterator<char>());
  if(rom.empty()) { std::fprintf(stderr, "cannot read ROM %s\n", romPath.c_str()); return 3; }

  stella.setROM(romPath.c_str(), rom.data(), rom.size());
  if(!stella.create(false)) { std::fprintf(stderr, "Stella could not create the console\n"); return 4; }

  Console& console = stella.osystem().console();
  TIA& tia = console.tia();
  const Switches& sw = console.switches();

  std::ofstream info(outDir + "/info.txt");
  info << "display_format=" << console.about().DisplayFormat << "\nbankswitch=" << console.about().BankSwitch
       << "\nframes=" << frames << "\nscript=" << argv[4] << "\nholds=" << holds.size() << "\n";
  info.close();

  std::ofstream csv(outDir + "/frames.csv");
  csv << "frame,inputs,left_diff,right_diff,tv,hash,status_hash,colors,ram" << (region[0] >= 0 ? ",region_colors" : "") << '\n';

  for(int f = 0; f < frames; ++f) {
    std::set<std::string> active;
    for(const Hold& h : holds)
      if(f >= h.from && f <= h.to) active.insert(h.input);
    for(const auto& [name, ev] : events) stella.setInputEvent(ev, active.count(name) ? 1 : 0);

    stella.runFrame();

    const uInt8* fb = tia.frameBuffer();
    const size_t fbSize = static_cast<size_t>(tia.height()) * kWidth;
    char hash[17], statusHash[17] = "-";
    std::snprintf(hash, sizeof hash, "%016llx", static_cast<unsigned long long>(fnv1a(fb, fbSize)));
    if(tia.height() >= 200)  // status bar: rows 180-199 (C3: drawn rows end at 199)
      std::snprintf(statusHash, sizeof statusHash, "%016llx",
                    static_cast<unsigned long long>(fnv1a(fb + 180 * kWidth, 20 * kWidth)));
    bool used[256] = {};
    for(size_t i = 0; i < fbSize; ++i) used[fb[i]] = true;
    std::string colors, inputs, ram;
    for(int c = 0; c < 256; ++c)
      if(used[c]) { char b[4]; std::snprintf(b, sizeof b, "%02x", c); colors += (colors.empty() ? "" : ":"); colors += b; }
    for(const std::string& a : active) inputs += (inputs.empty() ? "" : "+") + a;
    const uInt8* r = console.system().m6532().getRAM();
    for(int i = 0; i < 128; ++i) { char b[3]; std::snprintf(b, sizeof b, "%02x", r[i]); ram += b; }

    csv << f << ',' << (inputs.empty() ? "-" : inputs) << ',' << (sw.leftDifficultyA() ? 'A' : 'B') << ','
        << (sw.rightDifficultyA() ? 'A' : 'B') << ',' << (sw.tvColor() ? "color" : "bw") << ','
        << hash << ',' << statusHash << ',' << colors << ',' << ram;
    if(region[0] >= 0) {
      bool inRegion[256] = {};
      for(int y = region[0]; y <= region[1] && y < static_cast<int>(tia.height()); ++y)
        for(int x = region[2]; x <= region[3]; ++x) inRegion[fb[y * kWidth + x]] = true;
      std::string rc;
      for(int c = 0; c < 256; ++c)
        if(inRegion[c]) { char b[4]; std::snprintf(b, sizeof b, "%02x", c); rc += (rc.empty() ? "" : ":"); rc += b; }
      csv << ',' << rc;
    }
    csv << '\n';

    if(dump.count(f)) {
      char name[64];
      std::snprintf(name, sizeof name, "/frame_%06d_%dx%u.raw", f, kWidth, tia.height());
      std::ofstream raw(outDir + name, std::ios::binary);
      raw.write(reinterpret_cast<const char*>(fb), static_cast<std::streamsize>(fbSize));
    }
  }

  csv.close();
  std::printf("done: %d frames, %zu input holds, %zu pictures\n", frames, holds.size(), dump.size());
  stella.destroy();
  return 0;
}
