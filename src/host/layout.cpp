#include "layout.hpp"

#include <algorithm>
#include <cmath>

namespace layout {

PixelShape stellaShape(int pictureWidth, int pictureHeight)
{
  return {4 * pictureHeight, 3 * pictureWidth};
}

Rect pictureRect(int windowW, int windowH, int pictureW, int pictureH, PixelShape shape, Mode mode)
{
  if(windowW <= 0 || windowH <= 0 || pictureW <= 0 || pictureH <= 0 || shape.num <= 0 || shape.den <= 0) return {};

  const double pixelRatio = static_cast<double>(shape.num) / shape.den;  // width of a game pixel / its height
  double availW = windowW, availH = windowH;
  if(mode == Mode::TvFrame) { availW *= 0.9; availH *= 0.9; }

  // s = screen pixels per picture row; a picture column is s * pixelRatio wide.
  double s = std::min(availH / pictureH, availW / (pictureW * pixelRatio));
  if(mode != Mode::Fill && mode != Mode::GameArea && s >= 1.0) s = std::floor(s);  // whole-number vertical scale

  Rect r;
  r.w = std::max(1, static_cast<int>(std::lround(pictureW * s * pixelRatio)));
  r.h = std::max(1, static_cast<int>(std::lround(pictureH * s)));
  r.w = std::min(r.w, windowW);
  r.h = std::min(r.h, windowH);
  r.x = (windowW - r.w) / 2;
  r.y = (windowH - r.h) / 2;
  return r;
}

}  // namespace layout
