// The look-ahead (Stage I9.2, D-015): test_lookahead <inputs file> <fingerprint file> [frames]
// Plays a recorded game and, after every frame, looks 4 frames ahead with the controls of that frame held (game::lookAhead). Checks:
//  1. the game itself is untouched: its picture and sound are still the recorded ones, frame for frame, with the look-ahead made after every frame;
//  2. the look-ahead is right: when the recorded controls really stay as they were for the next k frames, the objects of frame f + k are exactly the ones
//     predicted (place, rows, lines, colour, shape, mirroring; all five objects), in every case; and how often the controls changed is counted.
// No ROM is read.
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "../../src/game/layers.hpp"
#include "../../src/game/lookahead.hpp"

namespace {

bool same(const game::SpriteImage& a, const game::SpriteImage& b) {
  return a.drawn == b.drawn && a.visible == b.visible && a.held == b.held && a.column == b.column && a.lines == b.lines && a.rowStart == b.rowStart && a.bits == b.bits &&
         a.colour == b.colour && a.mirrored == b.mirrored;
}

game::FrameInputs inputsOf(unsigned bits) {
  game::FrameInputs in;
  in.console.fire = (bits & 16) != 0;
  in.console.select = (bits & 32) != 0;
  in.console.reset = (bits & 64) != 0;
  in.console.leftDifficultyA = (bits & 128) != 0;
  in.console.rightDifficultyA = (bits & 256) != 0;
  in.console.colour = !(bits & 512);
  unsigned dirs = bits & 15;
  if ((dirs & 3) == 3) dirs &= ~1u;
  if ((dirs & 12) == 12) dirs &= ~4u;
  uint8_t swcha = 0xFF;
  if (dirs & 1) swcha &= ~0x10;
  if (dirs & 2) swcha &= ~0x20;
  if (dirs & 4) swcha &= ~0x40;
  if (dirs & 8) swcha &= ~0x80;
  in.frame.swcha = swcha;
  return in;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 3) { std::fprintf(stderr, "usage: test_lookahead <inputs> <fingerprints> [frames]\n"); return 2; }
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

  // the controls of every frame
  std::vector<unsigned> bitsOf(static_cast<std::size_t>(total));
  {
    unsigned bits = 0;
    std::size_t next = 0;
    for (int f = 0; f < total; ++f) {
      while (next < changes.size() && changes[next].first <= f) bits = changes[next++].second;
      bitsOf[static_cast<std::size_t>(f)] = bits;
    }
  }

  constexpr int kSteps = 4;
  game::Game g;
  game::Layers layers;
  g.setPicture(&layers.full);   // only the game's own picture: the look-ahead needs nothing more
  std::vector<std::vector<game::ObjectsView>> predicted(static_cast<std::size_t>(total));
  std::vector<game::ObjectsView> actual(static_cast<std::size_t>(total));
  int pictureDifferent = 0, soundDifferent = 0;
  g.trackObjects(true);
  for (int f = 0; f < total; ++f) {
    if (!g.runFrame(inputsOf(bitsOf[static_cast<std::size_t>(f)]), layers.full)) { std::printf("frame %d: the game met the one path of the original that no recording runs\n", f); return 1; }
    const auto& px = layers.full.finishPicture(g.lastFrameLines());
    uint64_t h = 1469598103934665603ULL;
    for (uint8_t v : px) { h ^= v; h *= 1099511628211ULL; }
    char hex[20];
    std::snprintf(hex, sizeof hex, "%016llx", static_cast<unsigned long long>(h));
    uint64_t sh = 1469598103934665603ULL;
    for (int16_t v : layers.full.sound())
      for (int rep = 0; rep < 2; ++rep)
        for (int b = 0; b < 2; ++b) { sh ^= static_cast<uint8_t>((static_cast<uint16_t>(v) >> (8 * b)) & 0xFF); sh *= 1099511628211ULL; }
    char shex[20];
    std::snprintf(shex, sizeof shex, "%016llx", static_cast<unsigned long long>(sh));
    if (f > 0 && picture[static_cast<std::size_t>(f)] != hex && pictureDifferent++ == 0) std::printf("first picture difference: frame %d\n", f);
    if (sound[static_cast<std::size_t>(f)] != shex && soundDifferent++ == 0) std::printf("first sound difference: frame %d\n", f);
    actual[static_cast<std::size_t>(f)] = g.objects();
    if (f > 0) predicted[static_cast<std::size_t>(f)] = game::lookAhead(g, layers.full, inputsOf(bitsOf[static_cast<std::size_t>(f)]), kSteps);
  }

  int heldCompared = 0, heldRight = 0, changedCompared = 0, changedRight = 0, shown = 0;
  for (int f = 1; f + kSteps < total; ++f)
    for (int k = 1; k <= kSteps; ++k) {
      bool held = true;
      for (int j = 1; j <= k; ++j) if (bitsOf[static_cast<std::size_t>(f + j)] != bitsOf[static_cast<std::size_t>(f)]) held = false;
      const auto& p = predicted[static_cast<std::size_t>(f)];
      if (static_cast<int>(p.size()) < k) continue;
      bool ok = true;
      for (std::size_t o = 0; o < 5; ++o) if (!same(p[static_cast<std::size_t>(k - 1)][o], actual[static_cast<std::size_t>(f + k)][o])) ok = false;
      if (held) {
        ++heldCompared;
        heldRight += ok;
        if (!ok && shown++ < 3) std::printf("wrong prediction: made after frame %d, %d frames ahead, controls held\n", f, k);
      } else {
        ++changedCompared;
        changedRight += ok;
      }
    }
  std::printf("game: pictures %s (%d differ), sound %s (%d differ), with a look-ahead of %d frames after every frame\n", pictureDifferent ? "DIFFERENT" : "IDENTICAL", pictureDifferent,
              soundDifferent ? "DIFFERENT" : "IDENTICAL", soundDifferent, kSteps);
  std::printf("controls held: %d predictions compared with what happened, %d exactly right\n", heldCompared, heldRight);
  std::printf("controls changed in between: %d predictions, %d still right\n", changedCompared, changedRight);
  const bool ok = pictureDifferent == 0 && soundDifferent == 0 && heldCompared > 0 && heldRight == heldCompared;
  std::printf("result: %s\n", ok ? "OK" : "FAIL");
  return ok ? 0 : 1;
}
