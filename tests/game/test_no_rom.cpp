// The game without a ROM (Stage G5c, D-009): test_no_rom <inputs file> <fingerprint file> [frames]
// Runs the whole game from power-on on a recorded input file with the picture and sound models and compares the fingerprint of every frame's picture and
// sound with the file. This program contains no code that reads a ROM or any cartridge file: it cannot cheat. The fingerprint files (tests/game/data) come
// from the reference emulator's runs (tools/g5c_make_fingerprints.py); they are hashes of the output, not cartridge data. The ROM-free public tests use this.
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "../../src/game/game.hpp"
#include "../../src/game/renderer.hpp"

class NullChip : public game::TiaChip {
 public:
  void write(uint8_t, uint8_t) override {}
};

int main(int argc, char** argv) {
  if (argc < 3) { std::fprintf(stderr, "usage: test_no_rom <inputs> <fingerprints> [frames]\n"); return 2; }
  std::vector<std::pair<int, unsigned>> changes;
  {
    std::ifstream in(argv[1]);
    if (!in) { std::fprintf(stderr, "cannot read %s\n", argv[1]); return 2; }
    for (std::string line; std::getline(in, line);) {
      if (!line.empty() && line.back() == '\r') line.pop_back();
      if (line.empty() || line[0] == '#' || line.rfind("PacMan2600", 0) == 0 || line.rfind("rom_sha1=", 0) == 0 || line.rfind("frames=", 0) == 0) continue;
      std::istringstream ls(line);
      int f; unsigned b;
      if (ls >> f >> b) changes.emplace_back(f, b);
    }
  }
  std::vector<std::string> picture, sound;
  {
    std::ifstream in(argv[2]);
    if (!in) { std::fprintf(stderr, "cannot read %s\n", argv[2]); return 2; }
    for (std::string line; std::getline(in, line);) {
      if (line.empty() || line[0] == '#') continue;
      std::istringstream ls(line);
      int f; std::string p, s;
      if (ls >> f >> p >> s) { picture.push_back(p); sound.push_back(s); }
    }
  }
  int total = static_cast<int>(picture.size());
  if (argc > 3) total = std::min(total, std::atoi(argv[3]));

  game::Game g;
  game::Renderer renderer;
  g.setPicture(&renderer);
  unsigned bits = 0;
  std::size_t next = 0;
  int pictureDifferent = 0, soundDifferent = 0;
  for (int f = 0; f < total; ++f) {
    while (next < changes.size() && changes[next].first <= f) bits = changes[next++].second;
    game::FrameInputs in;
    in.console.fire = (bits & 16) != 0;
    in.console.select = (bits & 32) != 0;
    in.console.reset = (bits & 64) != 0;
    in.console.leftDifficultyA = (bits & 128) != 0;
    in.console.rightDifficultyA = (bits & 256) != 0;
    in.console.colour = !(bits & 512);
    unsigned dirs = bits & 15;   // the reference emulator drops up when up and down are pressed together, and left when left and right are
    if ((dirs & 3) == 3) dirs &= ~1u;
    if ((dirs & 12) == 12) dirs &= ~4u;
    uint8_t swcha = 0xFF;
    if (dirs & 1) swcha &= ~0x10;
    if (dirs & 2) swcha &= ~0x20;
    if (dirs & 4) swcha &= ~0x40;
    if (dirs & 8) swcha &= ~0x80;
    in.frame.swcha = swcha;
    if (!g.runFrame(in, renderer)) { std::printf("frame %d: the game met the one path of the original that no recording runs\n", f); return 1; }
    const auto& px = renderer.finishPicture(g.lastFrameLines());
    uint64_t h = 1469598103934665603ULL;   // the fingerprint of the whole project
    for (uint8_t v : px) { h ^= v; h *= 1099511628211ULL; }
    char hex[20];
    std::snprintf(hex, sizeof hex, "%016llx", static_cast<unsigned long long>(h));
    uint64_t sh = 1469598103934665603ULL;
    for (int16_t v : renderer.sound())
      for (int rep = 0; rep < 2; ++rep)   // mono: left and right are the same
        for (int b = 0; b < 2; ++b) { sh ^= static_cast<uint8_t>((static_cast<uint16_t>(v) >> (8 * b)) & 0xFF); sh *= 1099511628211ULL; }
    char shex[20];
    std::snprintf(shex, sizeof shex, "%016llx", static_cast<unsigned long long>(sh));
    if (f > 0 && picture[f] != hex && pictureDifferent++ == 0) std::printf("first picture difference: frame %d, expected %s, ours %s\n", f, picture[f].c_str(), hex);
    if (sound[f] != shex && soundDifferent++ == 0) std::printf("first sound difference: frame %d, expected %s, ours %s\n", f, sound[f].c_str(), shex);
  }
  std::printf("pictures: %s (%d of %d frames differ); sound: %s (%d of %d frames differ)\n", pictureDifferent ? "DIFFERENT" : "IDENTICAL", pictureDifferent, total - 1,
              soundDifferent ? "DIFFERENT" : "IDENTICAL", soundDifferent, total);
  if (pictureDifferent == 0 && soundDifferent == 0) { std::printf("result: IDENTICAL (%d frames)\n", total); return 0; }
  std::printf("result: DIFFERENT\n");
  return 1;
}
