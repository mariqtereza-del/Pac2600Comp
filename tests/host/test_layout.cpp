// Tests for src\host\layout (Stage D6): the picture's shape and place in five window sizes, three modes, two pixel shapes.
// Usage: test_layout
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

#include "../../src/host/layout.hpp"

static int failures = 0;
static int checks = 0;

static void expect(bool ok, const std::string& what)
{
  ++checks;
  if(!ok) { ++failures; std::printf("FAIL  %s\n", what.c_str()); }
}

int main()
{
  constexpr int pw = 160, ph = 228;  // the picture of this ROM (docs\DISPLAY.md)
  const struct { int w, h; const char* name; } windows[] = {
      {1920, 1080, "1920x1080"}, {2560, 1440, "2560x1440"}, {3840, 2160, "3840x2160"}, {3440, 1440, "3440x1440"}, {1280, 1024, "1280x1024"}};
  const struct { layout::PixelShape shape; const char* name; } shapes[] = {
      {layout::stellaShape(pw, ph), "Stella 19:10"}, {{12, 7}, "12:7"}};
  const struct { layout::Mode mode; const char* name; } modes[] = {
      {layout::Mode::Sharp, "Sharp"}, {layout::Mode::Fill, "Fill"}, {layout::Mode::TvFrame, "TV frame"}, {layout::Mode::Framed, "Framed"}};

  // The shape derived from Stella's source: 4:3 box over 160 x 228 pixels.
  const auto stella = layout::stellaShape(pw, ph);
  expect(stella.num * 10 == stella.den * 19, "Stella's pixel shape for 160 x 228 is exactly 19:10");

  // Fill in a 960 x 720 window must be exactly Stella's own rectangle (its default window, zoom 3 of 320 x 240).
  {
    const auto r = layout::pictureRect(960, 720, pw, ph, stella, layout::Mode::Fill);
    expect(r.x == 0 && r.y == 0 && r.w == 960 && r.h == 720, "Fill in 960x720 equals Stella's rectangle (0,0,960,720)");
  }

  std::printf("%-10s %-13s %-9s %5s %5s %5s %5s  pixel shape\n", "window", "pixel shape", "mode", "x", "y", "w", "h");
  for(const auto& win : windows)
    for(const auto& sh : shapes) {
      const double target = static_cast<double>(sh.shape.num) / sh.shape.den;
      layout::Rect sharp{}, fill{}, tv{};
      for(const auto& m : modes) {
        const auto r = layout::pictureRect(win.w, win.h, pw, ph, sh.shape, m.mode);
        const std::string tag = std::string(win.name) + " / " + sh.name + " / " + m.name;
        const double got = (static_cast<double>(r.w) / pw) / (static_cast<double>(r.h) / ph);
        std::printf("%-10s %-13s %-9s %5d %5d %5d %5d  %.4f (wanted %.4f)\n", win.name, sh.name, m.name, r.x, r.y, r.w, r.h, got, target);
        expect(std::fabs(got - target) / target <= 0.005, tag + ": pixel shape within 0.5 %");
        expect(r.x >= 0 && r.y >= 0 && r.x + r.w <= win.w && r.y + r.h <= win.h, tag + ": inside the window");
        expect(std::abs(r.x - (win.w - r.x - r.w)) <= 1 && std::abs(r.y - (win.h - r.y - r.h)) <= 1, tag + ": centred");
        if(m.mode == layout::Mode::Sharp) { sharp = r; expect(r.h % ph == 0, tag + ": whole-number vertical scale"); }
        if(m.mode == layout::Mode::Fill) { fill = r; expect(r.w >= win.w - 1 || r.h >= win.h - 1, tag + ": touches the window on one axis"); }
        if(m.mode == layout::Mode::TvFrame) {
          tv = r;
          expect(r.w <= win.w * 0.9 + 1 && r.h <= win.h * 0.9 + 1, tag + ": leaves a border (within 90 %)");
        }
      }
      expect(fill.w >= sharp.w && fill.h >= sharp.h, std::string(win.name) + " / " + sh.name + ": Fill is at least as big as Sharp");
      expect(tv.w <= sharp.w && tv.h <= sharp.h, std::string(win.name) + " / " + sh.name + ": TV frame is not bigger than Sharp");
    }

  // Game area: the same pixel shape, but only the rows 11-203 are shown (the always-black rows are left out).
  {
    constexpr int shownRows = layout::kGameBottom - layout::kGameTop;
    expect(layout::kGameTop >= 0 && layout::kGameBottom <= ph && shownRows == 193, "game area rows 11..203 (193 rows) lie inside the 228-row picture");
    for(const auto& win : windows) {
      const auto r = layout::pictureRect(win.w, win.h, pw, shownRows, stella, layout::Mode::GameArea);
      const double got = (static_cast<double>(r.w) / pw) / (static_cast<double>(r.h) / shownRows);
      std::printf("%-10s %-13s %-9s %5d %5d %5d %5d  %.4f (wanted 1.9000)\n", win.name, "Stella 19:10", "Game area", r.x, r.y, r.w, r.h, got);
      const std::string tag = std::string(win.name) + " / Game area";
      expect(std::fabs(got - 1.9) / 1.9 <= 0.005, tag + ": pixel shape within 0.5 %");
      expect(r.x >= 0 && r.y >= 0 && r.x + r.w <= win.w && r.y + r.h <= win.h, tag + ": inside the window");
      expect(std::abs(r.x - (win.w - r.x - r.w)) <= 1 && std::abs(r.y - (win.h - r.y - r.h)) <= 1, tag + ": centred");
      expect(r.w >= win.w - 1 || r.h >= win.h - 1, tag + ": touches the window on one axis");
      // on a screen as wide as 16:9 or wider the game area runs from the top edge to the bottom edge
      if(win.w * 9 >= win.h * 16) expect(r.h >= win.h - 1, tag + ": fills the window top to bottom");
    }
  }

  // Framed (D-006): 12:7 pixels, the game area, and a border of kFrameMargin game pixels on every side
  {
    constexpr int shownRows = layout::kGameBottom - layout::kGameTop;
    const layout::PixelShape twelveSeven{12, 7};
    for(const auto& win : windows) {
      const auto r = layout::pictureRect(win.w, win.h, pw, shownRows, twelveSeven, layout::Mode::Framed);
      const double got = (static_cast<double>(r.w) / pw) / (static_cast<double>(r.h) / shownRows);
      const std::string tag = std::string(win.name) + " / Framed";
      std::printf("%-10s %-13s %-9s %5d %5d %5d %5d  %.4f (wanted 1.7143)\n", win.name, "12:7", "Framed", r.x, r.y, r.w, r.h, got);
      expect(std::fabs(got - 12.0 / 7.0) / (12.0 / 7.0) <= 0.005, tag + ": pixel shape 12:7 within 0.5 %");
      expect(r.x >= 0 && r.y >= 0 && r.x + r.w <= win.w && r.y + r.h <= win.h, tag + ": inside the window");
      expect(std::abs(r.x - (win.w - r.x - r.w)) <= 1 && std::abs(r.y - (win.h - r.y - r.h)) <= 1, tag + ": centred");
      // the border, measured in game pixels, is at least kFrameMargin on every side and exactly that on the tight axis
      const double rowPx = static_cast<double>(r.h) / shownRows, colPx = static_cast<double>(r.w) / pw;
      const double top = r.y / rowPx, left = r.x / colPx;
      expect(top >= layout::kFrameMargin - 0.6 && left >= layout::kFrameMargin - 0.6, tag + ": border at least " + std::to_string(layout::kFrameMargin) + " game pixels");
      expect(top <= layout::kFrameMargin + 0.6 || left <= layout::kFrameMargin + 0.6, tag + ": border exactly that on the axis that limits the size");
    }
    // 16:9: the border above and below is as many game rows as the border left and right is game columns
    const auto r = layout::pictureRect(2560, 1440, pw, shownRows, twelveSeven, layout::Mode::Framed);
    expect(std::fabs(r.y / (static_cast<double>(r.h) / shownRows) - layout::kFrameMargin) <= 0.6, "2560x1440: border above is 8 game rows");
  }

  // Odd and tiny windows must not crash or distort
  for(const auto& m : modes) {
    const auto r = layout::pictureRect(100, 100, pw, ph, stella, m.mode);
    expect(r.w >= 1 && r.h >= 1 && r.w <= 100 && r.h <= 100, std::string("100x100 window / ") + m.name + ": a valid rectangle");
    const auto none = layout::pictureRect(0, 0, pw, ph, stella, m.mode);
    expect(none.w == 0 && none.h == 0, std::string("empty window / ") + m.name + ": empty rectangle");
  }

  std::printf("\n%s (%d checks, %d failure%s)\n", failures ? "TESTS FAILED" : "ALL TESTS PASSED", checks, failures, failures == 1 ? "" : "s");
  return failures ? 1 : 0;
}
