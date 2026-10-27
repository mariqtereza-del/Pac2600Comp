#include "smooth.hpp"

#include <algorithm>
#include <cstdlib>

namespace game {

namespace {

// A change bigger than this is not an ordinary step of an object (a ghost moves a column at a time, a sprite line is two rows) and is not glided across.
constexpr int kMaxGlideColumns = 3;
constexpr int kMaxGlideRows = 6;

}  // namespace

void Smoother::step(const ObjectsView& now, const std::vector<ObjectsView>& ahead, const std::array<uint8_t, TiaVideo::kWidth * TiaVideo::kHeight>& background,
                    const std::array<uint8_t, TiaVideo::kWidth * TiaVideo::kHeight>& bare) {
  horizon_ = static_cast<int>(ahead.size());
  for (std::size_t o = 0; o < 5; ++o) {
    Track& t = track_[o];
    SpriteImage used = now[o];
    if (used.held && t.known) { used = t.image; used.drawn = false; }   // inside an opening: the last picture stays
    const bool first = !t.known || used.visible != t.image.visible;
    const bool columnChanged = first || used.column != t.image.column;
    const bool rowChanged = first || used.rowStart[0] != t.image.rowStart[0] || used.lines != t.image.lines;
    t.column.since = columnChanged ? 0 : t.column.since + 1;
    t.row.since = rowChanged ? 0 : t.row.since + 1;
    t.image = used;
    t.known = true;
    t.column.ahead = t.row.ahead = 0;
    if (!used.visible) continue;
    for (std::size_t k = 0; k < ahead.size(); ++k) {
      const SpriteImage& a = ahead[k][o];
      if (a.held || !a.visible || a.lines != used.lines) break;   // not known, or not the same picture shifted: no glide
      if (t.column.ahead == 0 && a.column != used.column) {
        t.column.ahead = static_cast<int>(k) + 1;
        t.column.delta = a.column - used.column;
      }
      if (t.row.ahead == 0 && a.rowStart[0] != used.rowStart[0]) {
        t.row.ahead = static_cast<int>(k) + 1;
        t.row.delta = a.rowStart[0] - used.rowStart[0];
      }
      if (t.column.ahead != 0 && t.row.ahead != 0) break;
    }
    if (t.column.ahead > 0 && std::abs(t.column.delta) > kMaxGlideColumns) t.column.ahead = 0;
    if (t.row.ahead > 0 && std::abs(t.row.delta) > kMaxGlideRows) t.row.ahead = 0;
  }
  // the power pills: what the picture has and the one without them does not (a pill is above Pac-Man, so it is drawn again after him)
  pills_.clear();
  for (int row = 0; row < TiaVideo::kHeight; ++row) {
    auto at = [&](int col) { return static_cast<std::size_t>(row * TiaVideo::kWidth + col); };
    int col = 0;
    while (col < TiaVideo::kWidth) {
      if (background[at(col)] == bare[at(col)]) { ++col; continue; }
      const int start = col;
      const uint8_t colour = background[at(col)];
      while (col < TiaVideo::kWidth && background[at(col)] != bare[at(col)] && background[at(col)] == colour) ++col;
      Rect* above = pills_.empty() ? nullptr : &pills_.back();   // the same run in the row above: one taller rectangle
      if (above && above->x == static_cast<float>(start) && above->w == static_cast<float>(col - start) && above->y + above->h == static_cast<float>(row) &&
          above->colour == colour)
        above->h += 1.0f;
      else
        pills_.push_back({static_cast<float>(start), static_cast<float>(row), static_cast<float>(col - start), 1.0f, colour});
    }
  }
}

void Smoother::offset(int object, double tau, double& columns, double& rows) const {
  const Track& t = track_[static_cast<std::size_t>(object)];
  // The move spreads over the steps since the last change and the steps until the next, but never starts earlier than the look-ahead first saw it coming:
  // an object that has been still for a long time does not begin to creep, and the move is the same from one step to the next.
  auto share = [&](const Axis& a) {
    if (a.ahead <= 0) return 0.0;
    const int since = std::min(a.since, std::max(0, horizon_ - a.ahead));
    return (since + tau) / (since + a.ahead);
  };
  columns = t.column.delta * share(t.column);
  rows = t.row.delta * share(t.row);
}

void Smoother::compose(double tau, const Options& options, std::vector<Rect>& out) const {
  out.clear();
  auto sprite = [&](std::size_t o) {
    const SpriteImage& s = track_[o].image;
    if (!track_[o].known || !s.visible) return;
    if (o > 0 && !options.allGhosts && !s.drawn) return;
    double dc = 0, dr = 0;
    if (options.glide) offset(static_cast<int>(o), tau, dc, dr);
    for (int line = 0; line < s.lines; ++line) {
      const std::size_t i = static_cast<std::size_t>(line);
      const float y0 = static_cast<float>(s.rowStart[i] + dr), y1 = static_cast<float>(s.rowStart[i + 1] + dr);
      int pixel = 0;
      while (pixel < 8) {
        auto set = [&](int p) { return ((s.bits[static_cast<std::size_t>(line)] >> (s.mirrored ? p : 7 - p)) & 1) != 0; };
        if (!set(pixel)) { ++pixel; continue; }
        const int start = pixel;
        while (pixel < 8 && set(pixel)) ++pixel;
        out.push_back({static_cast<float>(s.column + dc + start), y0, static_cast<float>(pixel - start), y1 - y0, s.colour});
      }
    }
  };
  sprite(0);                                   // Pac-Man, player 1 of the chip
  for (const Rect& pill : pills_) out.push_back(pill);   // a power pill is above Pac-Man
  for (std::size_t o = 1; o < 5; ++o) sprite(o);          // the ghosts, player 0
}

}  // namespace game
