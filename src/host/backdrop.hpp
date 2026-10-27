// Drawing what fills the space around the game picture (Stage I7, DECISIONS.md D-012 and T-I7-1). Called after the window is cleared and before the
// picture is drawn, and it draws nothing inside the picture's rectangle that the picture would not cover again: the picture is never touched.
#pragma once

#include <SDL3/SDL.h>

#include <cstdint>
#include <vector>

#include "background.hpp"
#include "layout.hpp"

namespace host {

// What the drawing needs to know about the frame.
struct BackdropScene {
  SDL_Renderer* renderer = nullptr;
  SDL_Texture* picture = nullptr;  // the game's texture, and the part of it that is shown (src), drawn at `where`
  SDL_FRect src{};
  layout::Rect where{};
  int windowW = 0, windowH = 0;
  const std::uint32_t* pixels = nullptr;  // the whole 160 x height picture as 0x00RRGGBB
  int pixelWidth = 0, firstRow = 0, rows = 0;  // the rows that are shown
  const std::uint8_t* ram = nullptr;           // the game's 128 bytes of memory (may be null: the panel then shows zeros)
};

class Backdrop {
 public:
  Backdrop() = default;
  ~Backdrop();
  Backdrop(const Backdrop&) = delete;
  Backdrop& operator=(const Backdrop&) = delete;

  // Black draws nothing.
  void draw(const BackdropScene& scene, background::Kind kind);

 private:
  void glow(const BackdropScene& s);
  void mirror(const BackdropScene& s);
  void gradient(const BackdropScene& s, background::Rgb colour);
  void arcade(const BackdropScene& s);
  void bezel(const BackdropScene& s);

  SDL_Texture* ambient_ = nullptr;  // the blurred picture: a few pixels, drawn large with smooth scaling
  std::vector<float> smooth_;       // its colours, eased from frame to frame so the flicker of the ghosts does not flicker the glow
  bool primed_ = false;
};

}  // namespace host
