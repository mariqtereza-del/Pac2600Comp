// The picture in the window, and the checks that look at it (self check, layout check, key list).
// (Stage G4.7: moved out of main.cpp, the code is unchanged.)
#pragma once
#include "backdrop.hpp"
#include "crt_filters.hpp"
#include "common.hpp"
#include <cmath>

#include "framerate.hpp"
#include "hdr.hpp"
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
  const std::uint8_t* ram = nullptr;                           // the game's 128 bytes, for the arcade panel; may stay null
  Backdrop backdrop;
  crt::Filter crt = crt::Filter::Off;   // the CRT filter the picture goes through (Stage I11, D-017); Off = the picture as it always was
  CrtFilter crtEngine;
  std::string crtError;               // why the filter could not be used, if it could not (the picture is then drawn without it)
  ~Screen() { dropTextures(); }
  // The renderer is about to be replaced (Stage I10.1): what it made goes with it.
  void dropTextures()
  {
    if(texture) SDL_DestroyTexture(texture);
    texture = nullptr;
    backdrop.forget();
    crtEngine.close();
  }

  // `moving`: the rectangles of Pac-Man, the pills and the ghosts (smooth motion, Stage I9.4), drawn over `pixels`, which is then the picture without them.
  // The standard (sRGB) value of a channel as linear light, and back (the back direction goes on above 1, which is what the HDR renderer reads as brighter than white).
  static float decode(std::uint32_t channel)
  {
    const float v = static_cast<float>(channel & 0xFF) / 255.0f;
    return v <= 0.04045f ? v / 12.92f : std::pow((v + 0.055f) / 1.055f, 2.4f);
  }
  static float encode(float linear) { return linear <= 0.0031308f ? 12.92f * linear : 1.055f * std::pow(linear, 1.0f / 2.4f) - 0.055f; }

  // `highlight` (Stage I10, D-016): in the HDR pipeline the moving objects are drawn this many times brighter than the same colour at normal white.
  bool show(const std::vector<std::uint32_t>& pixels, int height, const std::vector<SmoothRect>* moving = nullptr, float highlight = 1.0f)
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
      backdrop.draw(scene, backgroundKind);
    }
    SDL_Texture* filtered = nullptr;
    if(crt != crt::Filter::Off && where.w > 0 && where.h > 0) filtered = crtEngine.run(renderer, crt, pixels, height, top, shownRows, where.w, where.h, crtError);
    if(filtered) SDL_RenderTexture(renderer, filtered, nullptr, &dst);   // one texel to a pixel of the screen
    else SDL_RenderTexture(renderer, texture, &src, &dst);
    if(!filtered && moving && !moving->empty()) {
      const float scaleX = dst.w / static_cast<float>(Emulator::kWidth), scaleY = dst.h / static_cast<float>(shownRows);
      const SDL_Rect clip{where.x, where.y, where.w, where.h};
      SDL_SetRenderClipRect(renderer, &clip);   // nothing outside the picture, whatever the rows of the moving objects
      for(const SmoothRect& r : *moving) {
        if(highlight > 1.0f) {   // brighter than white: the colour in linear light, made `highlight` times as bright and kept vivid, handed over as the renderer expects it
          const hdr::Rgb c = hdr::highlightColour({decode(r.colour >> 16), decode(r.colour >> 8), decode(r.colour)}, highlight);
          SDL_SetRenderDrawColorFloat(renderer, encode(c.r), encode(c.g), encode(c.b), 1.0f);
        } else {
          SDL_SetRenderDrawColor(renderer, static_cast<Uint8>(r.colour >> 16), static_cast<Uint8>(r.colour >> 8), static_cast<Uint8>(r.colour), 255);
        }
        const SDL_FRect d{dst.x + r.x * scaleX, dst.y + (r.y - static_cast<float>(top)) * scaleY, r.w * scaleX, r.h * scaleY};
        SDL_RenderFillRect(renderer, &d);
      }
      SDL_SetRenderClipRect(renderer, nullptr);
    }
    shownTop = top;
    shownHeight = shownRows;
    return true;
  }
};

// --layout-check: run 300 frames in the real window, read its pixels back and check that the picture is where the
// layout says: nothing outside its rectangle, and the outline of what is drawn inside it where the arithmetic predicts.
bool layoutCheck(SDL_Renderer* renderer, const std::string& pngPath, const std::string& romPath, const View& view, int resetAt = -1);

// The pictures-a-second counter in the top left corner (Game options, Stage I9.5): the frame rate chosen and how many different pictures are shown a second.
void drawFrameCounter(SDL_Renderer* renderer, framerate::Rate rate, double picturesPerSecond);

// The F1 list, drawn over the picture with SDL's built-in text.
void drawHelp(SDL_Renderer* renderer, const controls::Input& current, const std::string& controller, bool softBlend = false);

// --help-check: the F1 list in the real window. 300 frames, then the list is drawn, the window's pixels are read back and
// saved, and the check counts the near-white pixels (the text), which must exist, and that the picture is still there.
bool helpCheck(SDL_Renderer* renderer, const std::string& pngPath, const std::string& romPath);

}  // namespace host
