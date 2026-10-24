// Where the game picture goes in the window (Stage D6, DECISIONS.md T-D6-1). No window code: pure arithmetic, so it can be tested alone.
#pragma once

namespace layout {

enum class Mode {
  Sharp,    // largest whole-number vertical scale; the horizontal size follows from the pixel shape
  Fill,     // largest size that fits, any scale
  TvFrame,  // like Sharp but inside 90 % of the window, leaving the black border a TV's overscan gave
  GameArea, // like Fill, but of the game area only (kGameTop..kGameBottom): the always-black rows are left out
};

// The rows of the 160 x 228 picture that hold the game. Every frame measured has been black above and below them:
// rows 11-199 over 24,000 frames (docs\DISPLAY.md) and in all eight reference pictures (docs\CAPTURES.md).
constexpr int kGameTop = 11;
constexpr int kGameBottom = 200;  // one past the last row

// The shape of one game pixel: its width is num/den times its height.
struct PixelShape {
  int num = 1;
  int den = 1;
};

// Stella with aspect correction on stretches the whole picture to a 4:3 box (emucore\TIASurface.cxx, Console.cxx),
// so a game pixel is (4/3) * pictureHeight / pictureWidth wide-to-tall: 19:10 for the 160 x 228 picture of this ROM.
PixelShape stellaShape(int pictureWidth, int pictureHeight);

struct Rect {
  int x = 0, y = 0, w = 0, h = 0;
};

// The picture's rectangle in a window of the given size. Centred; bars are whatever is left over.
Rect pictureRect(int windowW, int windowH, int pictureW, int pictureH, PixelShape shape, Mode mode);

}  // namespace layout
