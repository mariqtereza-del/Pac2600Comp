#include "symbols.hpp"

#include <cmath>

#include "pad_layout.hpp"

namespace host {

namespace {

// One symbol in the 8 x 8 cell whose top left corner is (x, y). Lines are used (not filled shapes) so that the picture is the same at every scale.
void drawSymbol(SDL_Renderer* renderer, float x, float y, char symbol)
{
  const float cx = x + 4.0f, cy = y + 4.0f;
  switch(symbol) {
    case controls::kCross:      // blue
      SDL_SetRenderDrawColor(renderer, 110, 170, 255, 255);
      SDL_RenderLine(renderer, x + 1.0f, y + 1.0f, x + 7.0f, y + 7.0f);
      SDL_RenderLine(renderer, x + 7.0f, y + 1.0f, x + 1.0f, y + 7.0f);
      break;
    case controls::kCircle: {   // red
      SDL_SetRenderDrawColor(renderer, 255, 90, 90, 255);
      const int sides = 16;
      for(int i = 0; i < sides; ++i) {
        const float a = 6.2831853f * static_cast<float>(i) / sides, b = 6.2831853f * static_cast<float>(i + 1) / sides;
        SDL_RenderLine(renderer, cx + 3.0f * std::cos(a), cy + 3.0f * std::sin(a), cx + 3.0f * std::cos(b), cy + 3.0f * std::sin(b));
      }
      break;
    }
    case controls::kSquare: {   // pink
      SDL_SetRenderDrawColor(renderer, 245, 140, 215, 255);
      const SDL_FRect r{x + 1.0f, y + 1.0f, 6.0f, 6.0f};
      SDL_RenderRect(renderer, &r);
      break;
    }
    case controls::kTriangle:   // green
      SDL_SetRenderDrawColor(renderer, 100, 230, 160, 255);
      SDL_RenderLine(renderer, cx, y + 0.5f, x + 7.0f, y + 6.5f);
      SDL_RenderLine(renderer, x + 7.0f, y + 6.5f, x + 1.0f, y + 6.5f);
      SDL_RenderLine(renderer, x + 1.0f, y + 6.5f, cx, y + 0.5f);
      break;
    default:
      break;
  }
}

}  // namespace

void drawSymbolText(SDL_Renderer* renderer, float x, float y, const std::string& text)
{
  Uint8 r = 255, g = 255, b = 255, a = 255;
  SDL_GetRenderDrawColor(renderer, &r, &g, &b, &a);
  const float cell = static_cast<float>(SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE);
  std::size_t start = 0;
  float at = x;
  auto flush = [&](std::size_t end) {
    if(end > start) { SDL_RenderDebugText(renderer, at, y, text.substr(start, end - start).c_str()); at += cell * static_cast<float>(end - start); }
  };
  for(std::size_t i = 0; i < text.size(); ++i) {
    if(!controls::isSymbol(text[i])) continue;
    flush(i);
    SDL_SetRenderDrawColor(renderer, r, g, b, a);
    drawSymbol(renderer, at, y, text[i]);
    SDL_SetRenderDrawColor(renderer, r, g, b, a);
    at += cell;
    start = i + 1;
  }
  flush(text.size());
}

}  // namespace host
