// The picture in the window, and the checks that look at it (self check, layout check, key list).
// (Stage G4.7: moved out of main.cpp, the code is unchanged.)
#pragma once
#include "backdrop.hpp"
#include "common.hpp"
#include "options.hpp"

namespace host {

// Writes the window's title, size and the colour of its centre pixel; returns true if it is a black 960x720 window.
bool selfCheck(SDL_Window* window, SDL_Renderer* renderer, const char* path);

// The game picture, enlarged to its correct shape and centred in the window (Stage D6, T-D6-1).
struct Screen {
  SDL_Renderer* renderer = nullptr;
  SDL_Texture* texture = nullptr;
  layout::Rect where{};  // where the picture was drawn in the window, in window pixels
  int shownTop = 0, shownHeight = 0;  // which rows of the 228-row picture are shown (all of them, or the game area)
  View view;
  background::Kind backgroundKind = background::Kind::Black;  // what fills the space around the picture (Stage I7); Black = nothing is drawn
  int solidChoice = 0;                                         // for the Solid colour background
  const std::uint8_t* ram = nullptr;                           // the game's 128 bytes, for the arcade panel; may stay null
  Backdrop backdrop;
  ~Screen() { if(texture) SDL_DestroyTexture(texture); }

  bool show(const std::vector<std::uint32_t>& pixels, int height)
  {
    if(!texture) {
      texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_STREAMING, Emulator::kWidth, height);
      if(!texture) return false;
      SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_PIXELART);  // sharp pixels, only their edges are blended when the scale is not whole
    }
    int windowW = 0, windowH = 0;
    SDL_GetRenderOutputSize(renderer, &windowW, &windowH);
    // The pixel shape always comes from the whole picture (Stella's); Game area only leaves out the always-black rows.
    const layout::PixelShape shape = view.shape.num > 0 ? view.shape : layout::stellaShape(Emulator::kWidth, height);
    const bool crop = (view.mode == layout::Mode::GameArea || view.mode == layout::Mode::Framed) && layout::kGameBottom <= height;
    const int top = crop ? layout::kGameTop : 0, shownRows = crop ? layout::kGameBottom - layout::kGameTop : height;
    where = layout::pictureRect(windowW, windowH, Emulator::kWidth, shownRows, shape, view.mode);
    const SDL_FRect dst{static_cast<float>(where.x), static_cast<float>(where.y), static_cast<float>(where.w), static_cast<float>(where.h)};
    const SDL_FRect src{0.0f, static_cast<float>(top), static_cast<float>(Emulator::kWidth), static_cast<float>(shownRows)};
    SDL_UpdateTexture(texture, nullptr, pixels.data(), Emulator::kWidth * 4);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    if(backgroundKind != background::Kind::Black) {
      BackdropScene scene;
      scene.renderer = renderer; scene.picture = texture; scene.src = src; scene.where = where; scene.windowW = windowW; scene.windowH = windowH;
      scene.pixels = pixels.data(); scene.pixelWidth = Emulator::kWidth; scene.firstRow = top; scene.rows = shownRows; scene.ram = ram;
      backdrop.draw(scene, backgroundKind, solidChoice);
    }
    SDL_RenderTexture(renderer, texture, &src, &dst);
    shownTop = top;
    shownHeight = shownRows;
    return true;
  }
};

// --layout-check: run 300 frames in the real window, read its pixels back and check that the picture is where the
// layout says: nothing outside its rectangle, and the outline of what is drawn inside it where the arithmetic predicts.
bool layoutCheck(SDL_Renderer* renderer, const std::string& pngPath, const std::string& romPath, const View& view, int resetAt = -1);

// The F1 list, drawn over the picture with SDL's built-in text.
void drawHelp(SDL_Renderer* renderer, const controls::Input& current, const std::string& controller, bool softBlend = false);

// --help-check: the F1 list in the real window. 300 frames, then the list is drawn, the window's pixels are read back and
// saved, and the check counts the near-white pixels (the text), which must exist, and that the picture is still there.
bool helpCheck(SDL_Renderer* renderer, const std::string& pngPath, const std::string& romPath);

}  // namespace host
