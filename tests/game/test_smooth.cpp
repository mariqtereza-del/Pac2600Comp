// Smooth motion (Stage I9.3, D-015): test_smooth <inputs file> <fingerprint file> [frames]
// Plays a recorded game with the three picture layers and, after every frame, gives game::Smoother the objects of this frame and (look-ahead, the controls
// held) of the next four. Checks:
//  1. with gliding off and only the objects the original draws, the smoother's rectangles on the picture without the moving objects are the game's own
//     picture, pixel for pixel, in every frame (the composing, the order of Pac-Man, the pills and the ghost);
//  2. with gliding on, no object is ever shown behind the original: along each axis it is between the place the original shows now and the place the
//     original shows when the object next changes (as the look-ahead says), and it does not move backwards within a step;
//  3. no delay: when the controls stay as they were, the place an object has at the end of a step is the place it starts the next step with, so the
//     motion is continuous, and an object that changes by one step arrives at the new place exactly when the original shows it.
// No ROM is read.
#include <algorithm>
#include <array>
#include <cmath>
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
#include "../../src/game/smooth.hpp"

namespace {

using Picture = std::array<uint8_t, game::TiaVideo::kWidth * game::TiaVideo::kHeight>;

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

// Fills the rectangles (whole pixels here) into a picture.
void paint(Picture& p, const std::vector<game::Rect>& rects) {
  for (const game::Rect& r : rects)
    for (int row = static_cast<int>(std::lround(r.y)); row < static_cast<int>(std::lround(r.y + r.h)); ++row)
      for (int col = static_cast<int>(std::lround(r.x)); col < static_cast<int>(std::lround(r.x + r.w)); ++col)
        if (row >= 0 && row < game::TiaVideo::kHeight && col >= 0 && col < game::TiaVideo::kWidth) p[static_cast<std::size_t>(row * game::TiaVideo::kWidth + col)] = r.colour;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 3) { std::fprintf(stderr, "usage: test_smooth <inputs> <fingerprints> [frames]\n"); return 2; }
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
  std::vector<unsigned> bitsOf(static_cast<std::size_t>(total));
  {
    unsigned bits = 0;
    std::size_t next = 0;
    for (int f = 0; f < total; ++f) {
      while (next < changes.size() && changes[next].first <= f) bits = changes[next++].second;
      bitsOf[static_cast<std::size_t>(f)] = bits;
    }
  }

  game::Game g;
  game::Layers layers;
  layers.attach(g);
  g.trackObjects(true);
  game::Smoother smoother;
  const game::Smoother::Options faithful{false, false}, smooth{true, true};
  std::vector<game::Rect> rects;
  const double taus[] = {0.0, 0.25, 0.5, 0.75, 0.999};
  int exact = 0, inexact = 0, firstInexact = -1;
  int axesChecked = 0, behind = 0, backwards = 0, glidingAxes = 0;
  int continuity = 0, continuityBroken = 0, arrivals = 0, arrivalsLate = 0, maxRects = 0;
  // the position of each object at the end of the last step, and the controls of that step, for the continuity check
  std::array<double, 5> endColumn{}, endRow{};
  std::array<bool, 5> haveEnd{};
  bool lastHeld = false;
  for (int f = 0; f < total; ++f) {
    if (!g.runFrame(inputsOf(bitsOf[static_cast<std::size_t>(f)]), layers)) { std::printf("frame %d: the game met the one path of the original that no recording runs\n", f); return 1; }
    const Picture full = layers.full.finishPicture(g.lastFrameLines());
    const Picture back = layers.background.finishPicture(g.lastFrameLines());
    const Picture bare = layers.bare.finishPicture(g.lastFrameLines());
    if (f == 0) continue;
    const std::vector<game::ObjectsView> ahead = game::lookAhead(g, layers.full, inputsOf(bitsOf[static_cast<std::size_t>(f)]), 4);
    const game::ObjectsView now = g.objects();
    smoother.step(now, ahead, back, bare);

    // 1. the original's picture
    smoother.compose(0.0, faithful, rects);
    Picture p = back;
    paint(p, rects);
    if (p == full) ++exact; else { ++inexact; if (firstInexact < 0) firstInexact = f; }

    // 2. and 3. gliding
    smoother.compose(0.5, smooth, rects);
    maxRects = std::max(maxRects, static_cast<int>(rects.size()));
    for (int o = 0; o < 5; ++o) {
      const game::SpriteImage& s = now[static_cast<std::size_t>(o)];
      if (!s.visible || s.held) { haveEnd[static_cast<std::size_t>(o)] = false; continue; }
      // where the original shows the object next time it changes, according to the look-ahead (along each axis)
      int targetColumn = s.column, targetRow = s.rowStart[0], whenColumn = 0, whenRow = 0;
      for (std::size_t k = 0; k < ahead.size(); ++k) {
        const game::SpriteImage& a = ahead[k][static_cast<std::size_t>(o)];
        if (a.held || !a.visible || a.lines != s.lines) break;
        if (whenColumn == 0 && a.column != s.column) { targetColumn = a.column; whenColumn = static_cast<int>(k) + 1; }
        if (whenRow == 0 && a.rowStart[0] != s.rowStart[0]) { targetRow = a.rowStart[0]; whenRow = static_cast<int>(k) + 1; }
      }
      const bool column = std::abs(targetColumn - s.column) <= 3, row = std::abs(targetRow - s.rowStart[0]) <= 6;
      double previousC = 0, previousR = 0;
      for (double tau : taus) {
        double dc, dr;
        smoother.offset(o, tau, dc, dr);
        const double wantC = column ? targetColumn - s.column : 0, wantR = row ? targetRow - s.rowStart[0] : 0;
        ++axesChecked;
        // between the place the original shows now and the place it shows next (never behind)
        auto between = [](double d, double want) { return want >= 0 ? d >= -1e-9 && d <= want + 1e-9 : d <= 1e-9 && d >= want - 1e-9; };
        if (!between(dc, wantC) || !between(dr, wantR)) ++behind;
        if (std::abs(dc) + 1e-9 < std::abs(previousC) || std::abs(dr) + 1e-9 < std::abs(previousR)) ++backwards;
        previousC = dc; previousR = dr;
      }
      glidingAxes += (column && whenColumn > 0) + (row && whenRow > 0);
      double dcEnd, drEnd;
      smoother.offset(o, 0.999999, dcEnd, drEnd);
      const std::size_t i = static_cast<std::size_t>(o);
      // 3a. an object that changes at the very next step arrives there at the end of this one
      if (column && whenColumn == 1) { ++arrivals; if (std::abs(s.column + dcEnd - targetColumn) > 0.01) ++arrivalsLate; }
      if (row && whenRow == 1) { ++arrivals; if (std::abs(s.rowStart[0] + drEnd - targetRow) > 0.01) ++arrivalsLate; }
      // 3b. continuity with the step before, when the controls did not change
      double dcStart, drStart;
      smoother.offset(o, 0.0, dcStart, drStart);
      if (haveEnd[i] && lastHeld) {
        const bool smallC = std::abs(s.column + dcStart - endColumn[i]) < 4.5, smallR = std::abs(s.rowStart[0] + drStart - endRow[i]) < 8.5;   // not a jump of the game itself
        if (smallC && smallR) {
          ++continuity;
          if (std::abs(s.column + dcStart - endColumn[i]) > 1e-3 || std::abs(s.rowStart[0] + drStart - endRow[i]) > 1e-3) {
            ++continuityBroken;
            if (continuityBroken <= 6)
              std::printf("broken: frame %d object %d: column now %d offset %.3f, ended %.3f; row now %d offset %.3f, ended %.3f; next change column %d (%d), row %d (%d)\n", f, o, s.column, dcStart,
                          endColumn[i], s.rowStart[0], drStart, endRow[i], targetColumn, whenColumn, targetRow, whenRow);
          }
        }
      }
      endColumn[i] = s.column + dcEnd;
      endRow[i] = s.rowStart[0] + drEnd;
      haveEnd[i] = true;
    }
    lastHeld = f + 1 < total && bitsOf[static_cast<std::size_t>(f + 1)] == bitsOf[static_cast<std::size_t>(f)];
  }
  std::printf("the original's picture from the smoother (gliding off): %d frames equal, %d different (first: frame %d)\n", exact, inexact, firstInexact);
  std::printf("gliding on: %d object positions checked at 5 moments of a step (%d axes glide): %d behind the original, %d moving backwards; at most %d rectangles\n", axesChecked,
              glidingAxes, behind, backwards, maxRects);
  std::printf("no delay: %d objects that change at the next step, %d not at their place at the end of this one; continuity from step to step: %d compared, %d broken\n", arrivals,
              arrivalsLate, continuity, continuityBroken);
  const bool ok = inexact == 0 && behind == 0 && backwards == 0 && arrivalsLate == 0 && continuity > 0 && continuityBroken * 100 <= continuity;
  std::printf("result: %s\n", ok ? "OK" : "FAIL");
  return ok ? 0 : 1;
}
