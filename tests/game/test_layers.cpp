// The two layers of the picture (Stage I9.1, D-015): test_layers <inputs file> <fingerprint file> [frames]
// Runs a recorded game with the picture without Pac-Man and the ghosts made alongside (game::Layers) and checks:
//  1. the game's own picture and sound are still the recorded ones, frame for frame (the second layer changes nothing in the game);
//  2. the two pictures differ only inside the maze part of the picture (never in the score or the life markers), and in few pixels;
//  3. the picture without the moving objects does not depend on where they are: the drawing code is run on copies of the game's state with Pac-Man and the
//     ghost moved about, and the picture stays the same, while the picture with them changes (so the check is not an empty one).
// No ROM and no cartridge file is read; the fingerprint files come from the reference emulator's runs (tests/game/data).
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

namespace {

using Picture = std::array<uint8_t, game::TiaVideo::kWidth * game::TiaVideo::kHeight>;

// A chip that only collects the drawing code's writes into a picture.
class Sink : public game::TimedChip {
 public:
  void write(const game::ChipWrite& w) override { video.write(w.line, w.clock, w.reg, w.value); }
  game::TiaVideo video;
};

// The drawing code run on a copy of a state, with or without Pac-Man and the ghosts, into a picture.
Picture drawWith(game::GameState state, bool moving) {
  Sink sink;
  if (moving) game::drawPicture(state, 16, sink, 34);
  else game::drawBackground(state, 16, sink, 34);
  sink.video.endPicture();
  return sink.video.picture();
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 3) { std::fprintf(stderr, "usage: test_layers <inputs> <fingerprints> [frames]\n"); return 2; }
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
  game::Layers layers;
  layers.attach(g);
  unsigned bits = 0;
  std::size_t next = 0;
  int pictureDifferent = 0, soundDifferent = 0, outsideMaze = 0, tooMany = 0, framesWithObjects = 0, maxDiff = 0;
  int firstRow = 999, lastRow = -1;
  int checked = 0, independentFailures = 0, objectsDiffer = 0, objectsMove = 0;
  // The rows the drawing code gives to the maze: it starts on line 35 (picture row 12) and has 8 rows of 20 lines (F18), so ends on row 171. The score and
  // the life markers lie below. ORIGINAL: Pac-Man in the opening of the bottom wall is still drawn for a few lines after the wall (recording BL-070, frame
  // 3536: rows 172 to 175), so the sprites may reach 8 rows (the height of a sprite) past the maze; the score digits are never hidden.
  constexpr int kMazeFirstRow = 12, kMazeLastRow = 171 + 8;
  for (int f = 0; f < total; ++f) {
    while (next < changes.size() && changes[next].first <= f) bits = changes[next++].second;
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
    const game::GameState before = g.state();   // the state this frame starts with (after the last frame start)
    if (!g.runFrame(in, layers)) { std::printf("frame %d: the game met the one path of the original that no recording runs\n", f); return 1; }
    const Picture full = layers.full.finishPicture(g.lastFrameLines());
    const Picture back = layers.background.finishPicture(g.lastFrameLines());

    uint64_t h = 1469598103934665603ULL;
    for (uint8_t v : full) { h ^= v; h *= 1099511628211ULL; }
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

    // 2. where and how much the two layers differ
    int diff = 0;
    bool outside = false;
    for (int row = 0; row < game::TiaVideo::kHeight; ++row)
      for (int col = 0; col < game::TiaVideo::kWidth; ++col) {
        const std::size_t i = static_cast<std::size_t>(row * game::TiaVideo::kWidth + col);
        if (full[i] != back[i]) {
          ++diff;
          firstRow = std::min(firstRow, row);
          lastRow = std::max(lastRow, row);
          if (row < kMazeFirstRow || row > kMazeLastRow) outside = true;
        }
      }
    if (outside) ++outsideMaze;
    if (diff > 400) ++tooMany;
    if (diff > 0) ++framesWithObjects;
    maxDiff = std::max(maxDiff, diff);

    // 3. the picture without the moving objects does not depend on where they are (every 7th frame, from the state this frame started with)
    if (f % 7 == 0 && f > 0) {
      ++checked;
      game::GameState shifted = before;
      shifted.spriteX = static_cast<uint8_t>(before.spriteX + 37);
      for (int i = 0; i < 5; ++i) {
        shifted.objX[i] = static_cast<uint8_t>(before.objX[i] + 11 * (i + 1));
        shifted.objY[i] = static_cast<uint8_t>(before.objY[i] + 5 * (i + 1));
      }
      const Picture still = drawWith(before, false), stillShifted = drawWith(shifted, false);
      if (still != stillShifted) ++independentFailures;
      if (still != drawWith(before, true)) ++objectsDiffer;                  // the objects are in the picture with them
      if (drawWith(before, true) != drawWith(shifted, true)) ++objectsMove;  // and move when the state says so
    }
  }
  std::printf("game: pictures %s (%d differ), sound %s (%d differ)\n", pictureDifferent ? "DIFFERENT" : "IDENTICAL", pictureDifferent,
              soundDifferent ? "DIFFERENT" : "IDENTICAL", soundDifferent);
  std::printf("layers: %d of %d frames have moving objects in them, largest difference %d pixels; %d frames differ outside the maze rows; %d frames differ in more than 400 pixels\n",
              framesWithObjects, total, maxDiff, outsideMaze, tooMany);
  std::printf("rows with differences: %d to %d\n", firstRow, lastRow);
  std::printf("independence: %d frames checked; the picture without the moving objects changed in %d; the picture with them differs from it in %d and changes when they are moved in %d (so the check is not an empty one)\n",
              checked, independentFailures, objectsDiffer, objectsMove);
  const bool ok = pictureDifferent == 0 && soundDifferent == 0 && outsideMaze == 0 && tooMany == 0 && independentFailures == 0 && framesWithObjects > total / 2 &&
                  objectsDiffer > checked / 2 && objectsMove > checked / 4;
  std::printf("result: %s\n", ok ? "OK" : "FAIL");
  return ok ? 0 : 1;
}
