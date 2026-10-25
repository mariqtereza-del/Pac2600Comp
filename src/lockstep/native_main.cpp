// lockstep_native: runs a recorded game with the finished native game code only (src/game: no 6502 core, no original program code) and compares the 128 memory
// bytes with the golden trace after every frame (Stage F21.27). The ROM is read only to give the game its cartridge data by address.
//
// Usage: lockstep_native <rom> <inputs_file> <trace_file> [--oracle <file>] [--poke <frame>:<hex addr>:<hex value>]... [--ignore <hex list>] [--all]
//   The stack bytes $F6-$FF are always left out (the original leaves old return addresses there; the native game has no stack) and so are the bytes of --ignore
//   (the scratch bytes of the drawing code, tools/f21_scratch_bytes.txt).
// Exit code 0 = identical, 1 = different, 2 = bad arguments or files.
#include <algorithm>
#include <array>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "../game/game.hpp"

class NullChip : public game::TiaChip {
 public:
  void write(uint8_t, uint8_t) override {}
};

int main(int argc, char** argv) {
  if (argc < 4) {
    std::fprintf(stderr, "usage: lockstep_native <rom> <inputs> <trace> [--oracle f] [--poke f:a:v]... [--ignore hex,hex] [--all]\n");
    return 2;
  }
  std::ifstream romFile(argv[1], std::ios::binary);
  std::vector<char> rom((std::istreambuf_iterator<char>(romFile)), std::istreambuf_iterator<char>());
  if (rom.size() != 4096) { std::fprintf(stderr, "the ROM must be 4096 bytes\n"); return 2; }
  int frames = 0;
  std::vector<std::pair<int, unsigned>> changes;
  {
    std::ifstream in(argv[2]);
    if (!in) { std::fprintf(stderr, "cannot read %s\n", argv[2]); return 2; }
    for (std::string line; std::getline(in, line);) {
      if (!line.empty() && line.back() == '\r') line.pop_back();
      if (line.rfind("frames=", 0) == 0) { frames = std::stoi(line.substr(7)); continue; }
      if (line.empty() || line[0] == '#' || line.rfind("PacMan2600", 0) == 0 || line.rfind("rom_sha1=", 0) == 0) continue;
      std::istringstream ls(line);
      int f; unsigned b;
      if (ls >> f >> b) changes.emplace_back(f, b);
    }
  }
  std::vector<std::string> golden;
  {
    std::ifstream in(argv[3]);
    if (!in) { std::fprintf(stderr, "cannot read %s\n", argv[3]); return 2; }
    std::string line;
    std::getline(in, line); std::getline(in, line); std::getline(in, line);
    while (std::getline(in, line)) {
      std::vector<std::string> cells;
      std::istringstream ls(line);
      for (std::string c; std::getline(ls, c, ',');) cells.push_back(c);
      if (cells.size() >= 6) golden.push_back(cells[5]);
    }
  }
  std::map<int, std::pair<bool, bool>> oracle;
  std::vector<std::array<int, 3>> pokes;
  std::vector<int> ignore;
  bool all = false;
  for (int i = 4; i < argc; ++i) {
    const std::string a = argv[i];
    if (a == "--oracle" && i + 1 < argc) {
      std::ifstream in(argv[++i]);
      int f, b, g;
      while (in >> f >> b >> g) oracle[f] = {b != 0, g != 0};
    } else if (a == "--poke" && i + 1 < argc) {
      int pf = 0, pa = 0, pv = 0;
      if (std::sscanf(argv[++i], "%d:%x:%x", &pf, &pa, &pv) == 3) pokes.push_back({pf, pa, pv});
    } else if (a == "--ignore" && i + 1 < argc) {
      std::istringstream ls(argv[++i]);
      for (std::string t; std::getline(ls, t, ',');) ignore.push_back(static_cast<int>(std::stoul(t, nullptr, 16)) - 0x80);
    } else if (a == "--all") {
      all = true;
    }
  }
  for (int a = 0xF6; a <= 0xFF; ++a) ignore.push_back(a - 0x80);

  const game::Cartridge cart(reinterpret_cast<const uint8_t*>(rom.data()));
  game::Game g(cart);
  NullChip chip;
  const int total = std::min<int>(frames > 0 ? frames : static_cast<int>(golden.size()), static_cast<int>(golden.size()));
  unsigned bits = 0;
  size_t next = 0;
  int different = 0, firstFrame = -1;
  std::array<long, 128> diffCount{};
  for (int f = 0; f < total; ++f) {
    while (next < changes.size() && changes[next].first <= f) bits = changes[next++].second;
    game::FrameInputs in;
    in.console.fire = (bits & 16) != 0;
    in.console.select = (bits & 32) != 0;
    in.console.reset = (bits & 64) != 0;
    in.console.leftDifficultyA = (bits & 128) != 0;
    in.console.rightDifficultyA = (bits & 256) != 0;
    in.console.colour = !(bits & 512);
    uint8_t swcha = 0xFF;
    if (bits & 1) swcha &= ~0x10;
    if (bits & 2) swcha &= ~0x20;
    if (bits & 4) swcha &= ~0x40;
    if (bits & 8) swcha &= ~0x80;
    in.frame.swcha = swcha;
    auto o = oracle.find(f);
    in.frame.bonusTouched = o != oracle.end() && o->second.first;
    in.frame.ghostTouched = o != oracle.end() && o->second.second;
    for (const auto& pk : pokes)
      if (pk[0] == f && pk[1] >= 0x80 && pk[1] <= 0xFF) g.state()[pk[1] - 0x80] = static_cast<uint8_t>(pk[2]);
    if (!g.runFrame(in, chip)) { std::printf("frame %d: the game met the one path of the original that no recording runs\n", f); return 1; }
    std::string ours;
    char buf[3];
    for (int i = 0; i < 128; ++i) { std::snprintf(buf, sizeof buf, "%02x", g.state()[i]); ours += buf; }
    for (int i : ignore)
      if (i >= 0 && i < 128) ours.replace(2 * i, 2, golden[f].substr(2 * i, 2));
    if (ours != golden[f]) {
      ++different;
      bool first = firstFrame < 0;
      if (first) firstFrame = f;
      for (int i = 0; i < 128; ++i) {
        if (ours.compare(2 * i, 2, golden[f], 2 * i, 2) == 0) continue;
        ++diffCount[i];
        if (first) std::printf("first difference: frame %d, byte $%02X: golden $%s, ours $%s\n", f, 0x80 + i, golden[f].substr(2 * i, 2).c_str(), ours.substr(2 * i, 2).c_str());
      }
      if (!all) break;
    }
  }
  if (all) {
    std::printf("bytes that differ in some frame:");
    for (int i = 0; i < 128; ++i)
      if (diffCount[i]) std::printf(" $%02X x%ld", 0x80 + i, diffCount[i]);
    std::printf("\n");
  }
  if (different == 0) {
    std::printf("result: IDENTICAL (%d frames)\n", total);
    return 0;
  }
  std::printf("result: DIFFERENT (%d of %d frames differ; first at frame %d)\n", different, total, firstFrame);
  return 1;
}
