// Test of the video-chip model (Stage F23.1):
//   test_tia_video <reference dir (reference\f23)> <baseline dir (reference\baseline)> [--game BL-nnn]... [--show N] [--pf n --grp n --en n --misc n --pp n --mp n --bp n]
// For every sampled frame the emulator's list of chip writes (t_BL-nnn.csv, made by tools\f23_dump.py: the frame and the one before it) is fed to the
// model, after the power-on writes of the original (frame 0). The finished picture's fingerprint must equal the golden trace's. For the first few
// frames with a saved emulator picture (p_BL-nnn.bin) the differing pixels are listed. The options change the timing constants (experiments only).
// Exit code 0 = every sampled frame equal.
#include <array>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "../../src/game/tia_video.hpp"

using game::TiaVideo;

static uint64_t fnv(const uint8_t* d, size_t n) {
  uint64_t h = 1469598103934665603ULL;  // the start value of every fingerprint of the project (a digit short of textbook FNV-1a, on purpose)
  for (size_t i = 0; i < n; ++i) { h ^= d[i]; h *= 1099511628211ULL; }
  return h;
}

int main(int argc, char** argv) {
  if (argc < 3) { std::fprintf(stderr, "usage: test_tia_video <f23 dir> <baseline dir> [options]\n"); return 2; }
  const std::string refDir = argv[1], baseDir = argv[2];
  game::TiaTiming timing;
  std::vector<std::string> games;
  int show = 3;
  for (int i = 3; i < argc; ++i) {
    const std::string a = argv[i];
    auto num = [&]() { return i + 1 < argc ? std::atoi(argv[++i]) : 0; };
    if (a == "--game" && i + 1 < argc) games.push_back(argv[++i]);
    else if (a == "--show") show = num();
    else if (a == "--pf") timing.playfield = num();
    else if (a == "--grp") timing.player = num();
    else if (a == "--en") timing.enable = num();
    else if (a == "--misc") timing.other = num();
    else if (a == "--pp") timing.playerPos = num();
    else if (a == "--mp") timing.missilePos = num();
    else if (a == "--bp") timing.ballPos = num();
  }
  if (games.empty())
    for (int n = 1; n <= 70; ++n) { char b[16]; std::snprintf(b, sizeof b, "BL-%03d", n); games.push_back(b); }

  long total = 0, equal = 0, shown = 0;
  for (const auto& g : games) {
    std::ifstream tr(baseDir + "\\" + g + ".trace");
    std::ifstream tf(refDir + "\\t_" + g + ".csv");
    if (!tr || !tf) { std::fprintf(stderr, "missing files for %s\n", g.c_str()); return 2; }
    std::vector<std::string> golden;
    std::string line;
    std::getline(tr, line); std::getline(tr, line); std::getline(tr, line);
    while (std::getline(tr, line)) {
      std::vector<std::string> cells;
      std::istringstream ls(line);
      for (std::string c; std::getline(ls, c, ',');) cells.push_back(c);
      golden.push_back(cells.size() > 3 ? cells[3] : "");
    }
    std::map<int, std::vector<uint8_t>> pics;   // the saved emulator pictures
    {
      std::ifstream pf(refDir + "\\p_" + g + ".bin", std::ios::binary);
      int32_t fr;
      while (pf.read(reinterpret_cast<char*>(&fr), 4)) {
        std::vector<uint8_t> px(TiaVideo::kWidth * TiaVideo::kHeight);
        if (!pf.read(reinterpret_cast<char*>(px.data()), static_cast<std::streamsize>(px.size()))) break;
        pics[fr] = px;
      }
    }
    std::vector<std::array<int, 6>> entries;   // frame label, line, clock, pc, register, value
    for (std::string row; std::getline(tf, row);) {
      std::array<int, 6> e{};
      if (std::sscanf(row.c_str(), "%d,%d,%d,%x,%x,%x", &e[0], &e[1], &e[2], &e[3], &e[4], &e[5]) == 6) entries.push_back(e);
    }

    TiaVideo video(timing);
    // the writes of the power-on frame of the original that stay in the chip (the places of missile 1 and the ball): line, clock, register, value
    static const int power[][4] = {{39, 15, 0x23, 0xF0}, {39, 30, 0x24, 0x80}, {39, 132, 0x14, 0x80}, {39, 141, 0x13, 0x80}, {40, 9, 0x2A, 0}, {41, 9, 0x2B, 0}};
    for (const auto& p : power) video.write(p[0], p[1], static_cast<uint8_t>(p[2]), static_cast<uint8_t>(p[3]));
    long gameEqual = 0, gameTotal = 0;
    for (size_t i = 0; i < entries.size(); ++i) {
      const auto& e = entries[i];
      const uint64_t before = video.pictureCount();
      video.write(e[1], e[2], static_cast<uint8_t>(e[4]), static_cast<uint8_t>(e[5]));
      if (video.pictureCount() == before || i == 0) continue;
      // this write (line 0, the end of the vertical sync) finished the picture of the frame of the entry before it; the sampled frames are the
      // multiples of 53 (the frames before them are only there to bring the chip's settings up to date)
      const int label = entries[i - 1][0];
      if (label % 53 != 0 || label >= static_cast<int>(golden.size())) continue;
      char hex[20];
      std::snprintf(hex, sizeof hex, "%016llx", static_cast<unsigned long long>(fnv(video.picture().data(), video.picture().size())));
      ++gameTotal;
      if (golden[label] == hex) { ++gameEqual; continue; }
      if (shown++ >= show) continue;
      std::printf("%s frame %d: picture differs (ours %s, emulator %s)\n", g.c_str(), label, hex, golden[label].c_str());
      auto it = pics.find(label);
      if (it == pics.end()) continue;
      int listed = 0;
      long diffs = 0;
      for (int p = 0; p < TiaVideo::kWidth * TiaVideo::kHeight; ++p)
        if (it->second[p] != video.picture()[p]) {
          ++diffs;
          if (listed++ < 14) std::printf("   row %d line %d x %d: emulator $%02X ours $%02X\n", p / 160, p / 160 + 23, p % 160, it->second[p], video.picture()[p]);
        }
      std::printf("   %ld pixels differ\n", diffs);
    }
    total += gameTotal;
    equal += gameEqual;
    if (gameEqual != gameTotal) std::printf("%s: %ld of %ld sampled frames equal\n", g.c_str(), gameEqual, gameTotal);
  }
  std::printf("sampled frames equal: %ld of %ld\n", equal, total);
  std::printf("%s\n", equal == total ? "result: OK" : "result: DIFFERENT");
  return equal == total ? 0 : 1;
}
