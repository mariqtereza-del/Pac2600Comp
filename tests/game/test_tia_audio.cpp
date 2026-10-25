// Test of the sound-chip model (Stage F24.1):
//   test_tia_audio <reference dir (reference\f24)> <baseline dir (reference\baseline)> [--game BL-nnn]... [--show N] [--delta N]
// For every frame of every recorded game the emulator's list of writes to the six sound registers (a_BL-nnn.csv, made by tools\f24_dump.py) is fed to the
// model; the samples of the frame (mono, left = right, 16 bits little endian, as the emulator hands them over) must have the same fingerprint as the golden
// trace's sound column. --delta N moves the effect time of every write by N colour clocks (experiments and controls). Exit code 0 = every frame equal.
#include <array>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "../../src/game/tia_audio.hpp"

static uint64_t fnvPairs(const std::vector<int16_t>& s) {
  uint64_t h = 1469598103934665603ULL;   // the start value of every fingerprint of the project
  for (int16_t v : s)
    for (int side = 0; side < 2; ++side)
      for (int b = 0; b < 2; ++b) { h ^= static_cast<uint8_t>((static_cast<uint16_t>(v) >> (8 * b)) & 0xFF); h *= 1099511628211ULL; }
  return h;
}

int main(int argc, char** argv) {
  if (argc < 3) { std::fprintf(stderr, "usage: test_tia_audio <f24 dir> <baseline dir> [options]\n"); return 2; }
  const std::string refDir = argv[1], baseDir = argv[2];
  std::vector<std::string> games;
  int show = 4, delta = 0, dumpFrame = -1;
  std::string dumpFile, streamFile;
  for (int i = 3; i < argc; ++i) {
    const std::string a = argv[i];
    if (a == "--game" && i + 1 < argc) games.push_back(argv[++i]);
    else if (a == "--show" && i + 1 < argc) show = std::atoi(argv[++i]);
    else if (a == "--delta" && i + 1 < argc) delta = std::atoi(argv[++i]);
    else if (a == "--stream" && i + 1 < argc) streamFile = argv[++i];   // appends the output of every frame of the first game to a file (for looking)
    else if (a == "--dump" && i + 1 < argc) { const std::string v = argv[++i]; dumpFrame = std::atoi(v.c_str()); dumpFile = v.substr(v.find(':') + 1); }   // frame:file, the output of that frame as 16-bit values
  }
  if (games.empty())
    for (int n = 1; n <= 70; ++n) { char b[16]; std::snprintf(b, sizeof b, "BL-%03d", n); games.push_back(b); }

  long total = 0, equal = 0, shown = 0, withSound = 0;
  for (const auto& g : games) {
    std::ifstream tr(baseDir + "\\" + g + ".trace");
    std::ifstream af(refDir + "\\a_" + g + ".csv");
    if (!tr || !af) { std::fprintf(stderr, "missing files for %s\n", g.c_str()); return 2; }
    std::vector<std::string> golden;
    std::string line;
    std::getline(tr, line); std::getline(tr, line); std::getline(tr, line);
    while (std::getline(tr, line)) {
      std::vector<std::string> cells;
      std::istringstream ls(line);
      for (std::string c; std::getline(ls, c, ',');) cells.push_back(c);
      golden.push_back(cells.size() > 4 ? cells[4] : "");
    }
    struct W { int frame, line, clock, reg, value; };
    std::vector<W> writes;
    for (std::string row; std::getline(af, row);) {
      int f, l, c, pc, reg, val;
      if (std::sscanf(row.c_str(), "%d,%d,%d,%x,%x,%x", &f, &l, &c, &pc, &reg, &val) == 6) writes.push_back({f, l, c, reg, val});
    }
    game::TiaAudio audio;
    size_t next = 0;
    long gameEqual = 0;
    for (size_t f = 0; f < golden.size(); ++f) {
      while (next < writes.size() && writes[next].frame == static_cast<int>(f)) {
        const W& w = writes[next++];
        audio.write(w.line, w.clock + delta, static_cast<uint8_t>(w.reg), static_cast<uint8_t>(w.value));
      }
      audio.endFrame(f == 0 ? game::TiaAudio::kFirstFrameLines : game::TiaAudio::kLines);
      if (static_cast<int>(f) == dumpFrame && g == games.front()) { std::ofstream df(dumpFile, std::ios::binary); df.write(reinterpret_cast<const char*>(audio.output().data()), static_cast<std::streamsize>(audio.output().size() * 2)); }
      if (!streamFile.empty() && g == games.front()) { std::ofstream sf(streamFile, std::ios::binary | std::ios::app); sf.write(reinterpret_cast<const char*>(audio.output().data()), static_cast<std::streamsize>(audio.output().size() * 2)); }
      char hex[20];
      std::snprintf(hex, sizeof hex, "%016llx", static_cast<unsigned long long>(fnvPairs(audio.output())));
      ++total;
      if (golden[f] == hex) { ++equal; ++gameEqual; continue; }
      if (shown++ < show) std::printf("%s frame %zu: sound differs (%zu samples; ours %s, emulator %s)\n", g.c_str(), f, audio.output().size(), hex, golden[f].c_str());
    }
    (void)withSound;
    if (gameEqual != static_cast<long>(golden.size())) std::printf("%s: %ld of %zu frames equal\n", g.c_str(), gameEqual, golden.size());
  }
  std::printf("frames equal: %ld of %ld\n", equal, total);
  std::printf("%s\n", equal == total ? "result: OK" : "result: DIFFERENT");
  return equal == total ? 0 : 1;
}
