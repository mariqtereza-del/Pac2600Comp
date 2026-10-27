// The description of the moving objects (Stage I9.2, D-015): test_objects_view <inputs file> <fingerprint file> [frames]
// Runs a recorded game with both layers (game::Layers) and, every frame, draws the objects the game describes (game::viewObjects) on the picture without
// Pac-Man and the ghosts. For the objects that are in the frame (Pac-Man and the ghost of the frame) the result must be the game's own picture, pixel for
// pixel: the position, the lines, the colour and the mirroring of the description are then the ones the picture shows. The ghosts that are not in the frame
// are described too (where they will be drawn when their turn comes): when a ghost's turn comes, the description of it made a few frames before must be the
// same in place and colour as the one of its turn, unless it moved in between (the ghosts move only on their turn) or the colours changed.
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

namespace {

using Picture = std::array<uint8_t, game::TiaVideo::kWidth * game::TiaVideo::kHeight>;

// Draws one sprite on a picture: each line spans its own rows, 8 pixels wide, mirrored if asked; set bits take the sprite's colour.
void drawSprite(Picture& p, const game::SpriteImage& s, const Picture* above = nullptr) {
  if (!s.visible) return;
  for (int line = 0; line < s.lines && line < 8; ++line)
    for (int bit = 0; bit < 8; ++bit) {
      const int source = s.mirrored ? bit : 7 - bit;   // bit 7 is the left pixel; mirrored, bit 0 is
      if (!((s.bits[static_cast<std::size_t>(line)] >> source) & 1)) continue;
      for (int row = s.rowStart[static_cast<std::size_t>(line)]; row < s.rowStart[static_cast<std::size_t>(line) + 1]; ++row) {
        const int col = s.column + bit;
        if (row < 0 || row >= game::TiaVideo::kHeight || col < 0 || col >= game::TiaVideo::kWidth) continue;
        if (above && (*above)[static_cast<std::size_t>(row * game::TiaVideo::kWidth + col)]) continue;   // something drawn above this sprite
        p[static_cast<std::size_t>(row * game::TiaVideo::kWidth + col)] = s.colour;
      }
    }
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 3) { std::fprintf(stderr, "usage: test_objects_view <inputs> <fingerprints> [frames]\n"); return 2; }
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
  int total = 0;
  {
    std::ifstream in(argv[2]);
    if (!in) { std::fprintf(stderr, "cannot read %s\n", argv[2]); return 2; }
    for (std::string line; std::getline(in, line);) if (!line.empty() && line[0] != '#') ++total;
  }
  if (argc > 3) total = std::min(total, std::atoi(argv[3]));

  game::Game g;
  game::Layers layers;
  layers.attach(g);
  g.trackObjects(true);
  unsigned bits = 0;
  std::size_t next = 0;
  int compared = 0, equal = 0, different = 0, firstDifferent = -1, pacmanSeen = 0;
  // the last description of each ghost made while it was not in the frame, to be compared with the one of its turn
  uint8_t stateBefore = 0, stateAfter = 0;   // Pac-Man's state byte at the start and at the end of the frame, for a look at a difference
  std::array<game::SpriteImage, 5> last{};
  std::array<int, 5> lastFrame{};
  std::array<bool, 5> haveLast{};
  int turnsCompared = 0, turnsSame = 0, coloursSame = 0, shapesSame = 0;
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
    stateBefore = g.state().objState[0];
    if (!g.runFrame(in, layers)) { std::printf("frame %d: the game met the one path of the original that no recording runs\n", f); return 1; }
    stateAfter = g.state().objState[0];
    const Picture full = layers.full.finishPicture(g.lastFrameLines());
    const Picture back = layers.background.finishPicture(g.lastFrameLines());
    if (f == 0) continue;   // the first frame is the power-on set-up
    const game::ObjectsView& view = g.objects();

    // 1. the objects of the frame, drawn on the background, are the game's picture
    const Picture back2 = layers.bare.finishPicture(g.lastFrameLines());   // the same without the power pills
    Picture pills{};
    for (std::size_t i = 0; i < pills.size(); ++i) pills[i] = back[i] != back2[i] ? 1 : 0;
    Picture p = back;
    drawSprite(p, view[0], &pills);   // a power pill (missile 0) and the ghost (player 0) are above Pac-Man (player 1)
    int drawnGhost = 0;
    for (int i = 1; i <= 4; ++i) if (view[static_cast<std::size_t>(i)].drawn) drawnGhost = i;
    drawSprite(p, view[static_cast<std::size_t>(drawnGhost)]);
    ++compared;
    pacmanSeen += view[0].visible;
    if (p == full) ++equal;
    else {
      ++different;
      if (firstDifferent < 0) {
        firstDifferent = f;
        int n = 0, r0 = 999, r1 = -1, c0 = 999, c1 = -1;
        for (std::size_t i = 0; i < p.size(); ++i)
          if (p[i] != full[i]) { ++n; r0 = std::min(r0, static_cast<int>(i) / 160); r1 = std::max(r1, static_cast<int>(i) / 160); c0 = std::min(c0, static_cast<int>(i) % 160); c1 = std::max(c1, static_cast<int>(i) % 160); }
        std::printf("first difference: frame %d, %d pixels, rows %d-%d, columns %d-%d\n", f, n, r0, r1, c0, c1);
        std::printf("  Pac-Man state byte at the start of the frame %02x, at the end %02x\n", stateBefore, stateAfter);
        std::printf("  Pac-Man x %d, columns of the chip for x-2..x+2:", g.state().objX[0]);
        for (int d = -2; d <= 2; ++d) std::printf(" %d", game::spriteColumns().player[1][g.state().objX[0] + d]);
        std::printf("\n");
        for (int o = 0; o < 5; ++o) {
          const game::SpriteImage& s = view[static_cast<std::size_t>(o)];
          std::printf("  object %d drawn %d visible %d held %d column %d lines %d colour %02x mirrored %d rows", o, s.drawn, s.visible, s.held, s.column, s.lines, s.colour, s.mirrored);
          for (int i = 0; i <= s.lines; ++i) std::printf(" %d", s.rowStart[static_cast<std::size_t>(i)]);
          std::printf("\n");
        }
        for (int row = r0 - 3; row <= r1 + 3; ++row) {
          std::printf("  row %3d ours ", row);
          for (int col = std::max(0, c0 - 3); col <= std::min(159, c1 + 3); ++col) std::printf("%02x ", p[static_cast<std::size_t>(row * 160 + col)]);
          std::printf("| game ");
          for (int col = std::max(0, c0 - 3); col <= std::min(159, c1 + 3); ++col) std::printf("%02x ", full[static_cast<std::size_t>(row * 160 + col)]);
          std::printf("\n");
        }
      }
    }

    // 2. a ghost that is not in the frame is described as the picture showed it on its last turn (it does not move in between)
    for (int i = 1; i <= 4; ++i) {
      const game::SpriteImage& s = view[static_cast<std::size_t>(i)];
      const std::size_t k = static_cast<std::size_t>(i);
      if (s.drawn) { last[k] = s; lastFrame[k] = f; haveLast[k] = s.visible && s.lines == 8; continue; }
      if (!haveLast[k] || s.held || !s.visible || f - lastFrame[k] > 3) continue;
      ++turnsCompared;
      const game::SpriteImage& l = last[k];
      if (l.column == s.column && l.rowStart == s.rowStart && l.lines == s.lines) ++turnsSame;
      else if (turnsCompared - turnsSame <= 0) std::printf("ghost %d, frame %d: described column %d row %d, drawn on its turn (frame %d) column %d row %d\n", i, f, s.column, s.rowStart[0], lastFrame[k], l.column, l.rowStart[0]);
      if (l.colour == s.colour) ++coloursSame;
      if (l.bits == s.bits) ++shapesSame;
    }
  }
  std::printf("objects of the frame: %d frames compared, %d equal to the game's picture, %d different (Pac-Man in the picture of %d of them)\n", compared, equal, different, pacmanSeen);
  std::printf("ghosts not in the frame: %d descriptions compared with the picture of that ghost's last turn: place and rows the same in %d, colour in %d, shape in %d\n",
              turnsCompared, turnsSame, coloursSame, shapesSame);
  // The ghosts do not move between their turns, so the descriptions agree, except where the game itself puts them elsewhere (a new game, a death, an eaten ghost
  // going home); the shape changes every 16 frames, which a third of the comparisons span.
  const bool ok = different == 0 && turnsSame * 100 >= turnsCompared * 99 && coloursSame * 100 >= turnsCompared * 98 &&
                  shapesSame * 100 >= turnsCompared * 85;
  std::printf("result: %s\n", ok ? "OK" : "FAIL");
  return ok ? 0 : 1;
}
