// Drawing what fills the space around the game picture (Stage I7, T-I7-1). Everything is drawn with SDL's own drawing calls; no image files, no library.
#include "backdrop.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

namespace host {

namespace {

constexpr int kGridCols = 16, kGridRows = 12;  // the blurred picture of the glow
constexpr float kEase = 0.12f;                  // how far the glow moves towards the new picture each time

SDL_FColor fcolour(background::Rgb c, float alpha = 1.0f) { return SDL_FColor{c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, alpha}; }

void setColour(SDL_Renderer* r, background::Rgb c, int alpha = 255) { SDL_SetRenderDrawColor(r, c.r, c.g, c.b, static_cast<Uint8>(alpha)); }

void fillRect(SDL_Renderer* r, float x, float y, float w, float h)
{
  const SDL_FRect rect{x, y, w, h};
  SDL_RenderFillRect(r, &rect);
}

// A quadrilateral with one colour on each corner (SDL blends between them): the gradients and the cabinet.
void quad(SDL_Renderer* r, SDL_FPoint p0, SDL_FPoint p1, SDL_FPoint p2, SDL_FPoint p3, SDL_FColor c0, SDL_FColor c1, SDL_FColor c2, SDL_FColor c3)
{
  const SDL_Vertex v[4] = {{p0, c0, {0, 0}}, {p1, c1, {0, 0}}, {p2, c2, {0, 0}}, {p3, c3, {0, 0}}};
  const int idx[6] = {0, 1, 2, 0, 2, 3};
  SDL_RenderGeometry(r, nullptr, v, 4, idx, 6);
}

// A filled circle (or a Pac-Man with his mouth open towards the right when mouth > 0) as a fan of triangles.
void disc(SDL_Renderer* r, float cx, float cy, float radius, background::Rgb c, float mouthHalfAngle = 0.0f)
{
  constexpr int kSegments = 28;
  std::vector<SDL_Vertex> v;
  std::vector<int> idx;
  v.push_back({{cx, cy}, fcolour(c), {0, 0}});
  const float pi = 3.14159265f;
  const float from = mouthHalfAngle, to = 2 * pi - mouthHalfAngle;
  for(int i = 0; i <= kSegments; ++i) {
    const float a = from + (to - from) * static_cast<float>(i) / kSegments;
    v.push_back({{cx + radius * std::cos(a), cy + radius * std::sin(a)}, fcolour(c), {0, 0}});
    if(i > 0) { idx.push_back(0); idx.push_back(i); idx.push_back(i + 1); }
  }
  SDL_RenderGeometry(r, nullptr, v.data(), static_cast<int>(v.size()), idx.data(), static_cast<int>(idx.size()));
}

// SDL's 8 x 8 debug text, enlarged by a whole number. (x, y) is the top left corner of the text.
void text(SDL_Renderer* r, float x, float y, int scale, const std::string& s, background::Rgb c)
{
  SDL_SetRenderScale(r, static_cast<float>(scale), static_cast<float>(scale));
  setColour(r, c);
  SDL_RenderDebugText(r, x / static_cast<float>(scale), y / static_cast<float>(scale), s.c_str());
  SDL_SetRenderScale(r, 1.0f, 1.0f);
}

constexpr float kCharPx = static_cast<float>(SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE);

}  // namespace

Backdrop::~Backdrop() { forget(); }

void Backdrop::forget()
{
  if(ambient_) SDL_DestroyTexture(ambient_);
  ambient_ = nullptr;
  primed_ = false;
}

void Backdrop::draw(const BackdropScene& s, background::Kind kind)
{
  using background::Kind;
  if(kind == Kind::Black) return;
  SDL_SetRenderDrawBlendMode(s.renderer, SDL_BLENDMODE_BLEND);
  const bool needsMaze = kind == Kind::Gradient || kind == Kind::Solid;  // the colour the maze is drawn on right now
  const background::Rgb maze = needsMaze ? background::mazeColour(s.pixels, s.pixelWidth, s.firstRow, s.rows) : background::defaultMazeColour();
  switch(kind) {
    case Kind::Glow: glow(s); break;
    case Kind::Solid:
      setColour(s.renderer, maze);
      SDL_RenderClear(s.renderer);
      break;
    case Kind::Gradient: gradient(s, maze); break;
    case Kind::Arcade: arcade(s); break;
    case Kind::Bezel: bezel(s); break;
    case Kind::Black: break;
  }
  SDL_SetRenderDrawBlendMode(s.renderer, SDL_BLENDMODE_NONE);
}

// The picture's own colours, a handful of blocks, stretched over the whole window with smooth scaling and dimmed: a blurred copy that follows the action.
void Backdrop::glow(const BackdropScene& s)
{
  std::vector<float> now;
  background::blockAverages(s.pixels, s.pixelWidth, s.firstRow, s.rows, kGridCols, kGridRows, now);
  if(!primed_ || smooth_.size() != now.size()) { smooth_ = now; primed_ = true; }
  else for(std::size_t i = 0; i < now.size(); ++i) smooth_[i] += (now[i] - smooth_[i]) * kEase;
  if(!ambient_) {
    ambient_ = SDL_CreateTexture(s.renderer, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_STREAMING, kGridCols, kGridRows);
    if(!ambient_) return;
    SDL_SetTextureScaleMode(ambient_, SDL_SCALEMODE_LINEAR);
  }
  std::uint32_t grid[kGridCols * kGridRows];
  for(int i = 0; i < kGridCols * kGridRows; ++i) {
    // brightened a little before it is dimmed again below: the maze colours are dark
    auto ch = [&](int k) { return static_cast<std::uint32_t>(std::clamp(smooth_[static_cast<std::size_t>(i) * 3 + k] * 1.6f, 0.0f, 255.0f)); };
    grid[i] = (ch(0) << 16) | (ch(1) << 8) | ch(2);
  }
  SDL_UpdateTexture(ambient_, nullptr, grid, kGridCols * 4);
  SDL_RenderTexture(s.renderer, ambient_, nullptr, nullptr);
  SDL_SetRenderDrawColor(s.renderer, 0, 0, 0, 120);  // dimmed, so the picture stays the brightest thing in the window
  SDL_RenderFillRect(s.renderer, nullptr);
}

// The maze colour, fairly bright next to the picture and fading to black at the edges of the window.
void Backdrop::gradient(const BackdropScene& s, background::Rgb colour)
{
  const float W = static_cast<float>(s.windowW), H = static_cast<float>(s.windowH);
  const float x0 = static_cast<float>(s.where.x), y0 = static_cast<float>(s.where.y);
  const float x1 = static_cast<float>(s.where.x + s.where.w), y1 = static_cast<float>(s.where.y + s.where.h);
  const SDL_FColor inner = fcolour(background::scaled(colour, 0.8)), outer = fcolour({0, 0, 0});
  quad(s.renderer, {0, 0}, {W, 0}, {x1, y0}, {x0, y0}, outer, outer, inner, inner);  // top
  quad(s.renderer, {0, H}, {W, H}, {x1, y1}, {x0, y1}, outer, outer, inner, inner);  // bottom
  quad(s.renderer, {0, 0}, {0, H}, {x0, y1}, {x0, y0}, outer, outer, inner, inner);  // left
  quad(s.renderer, {W, 0}, {W, H}, {x1, y1}, {x1, y0}, outer, outer, inner, inner);  // right
}

// A panel in the style of an arcade cabinet: the title, the score of each player and the lives, in the space around the picture. Where the space is too
// thin for readable text, only the frame is drawn.
void Backdrop::arcade(const BackdropScene& s)
{
  using background::Rgb;
  const Rgb yellow{255, 220, 0}, white{235, 235, 235}, cyan{80, 220, 255};
  const float W = static_cast<float>(s.windowW), H = static_cast<float>(s.windowH);
  const SDL_FColor top = fcolour({6, 6, 30}), bottom = fcolour({16, 16, 70});
  quad(s.renderer, {0, 0}, {W, 0}, {W, H}, {0, H}, top, top, bottom, bottom);
  const float wx = static_cast<float>(s.where.x), wy = static_cast<float>(s.where.y), ww = static_cast<float>(s.where.w), wh = static_cast<float>(s.where.h);
  setColour(s.renderer, yellow);  // a yellow frame around the picture, two lines thick
  for(int k = 3; k <= 4; ++k) { const SDL_FRect f{wx - k, wy - k, ww + 2 * k, wh + 2 * k}; SDL_RenderRect(s.renderer, &f); }

  const std::uint8_t blank[128] = {};
  const std::uint8_t* ram = s.ram ? s.ram : blank;
  char score[2][8];
  for(int p = 0; p < 2; ++p) std::snprintf(score[p], sizeof score[p], "%04d", background::scoreOf(ram, p));
  const bool two = background::twoPlayers(ram);
  const int lives = background::livesOf(ram), turn = background::playerToPlay(ram);
  const float leftBar = wx, rightBar = W - (wx + ww), topBar = wy, bottomBar = H - (wy + wh);

  auto lifeIcons = [&](float x, float y, float step, int scale, bool vertical) {
    for(int i = 0; i < std::min(lives, 7); ++i)
      disc(s.renderer, vertical ? x : x + step * static_cast<float>(i), vertical ? y + step * static_cast<float>(i) : y, 3.2f * static_cast<float>(scale), yellow, 0.6f);
  };

  if(std::min(leftBar, rightBar) >= std::min(topBar, bottomBar)) {  // wide window: the panel is at the sides
    const std::string title = "PAC-MAN";
    // left bar: the title, one letter under the other
    int sc = static_cast<int>(std::min(leftBar * 0.7f / kCharPx, H * 0.9f / (static_cast<float>(title.size()) * kCharPx * 1.5f)));
    if(sc >= 1) {
      float y = (H - static_cast<float>(title.size()) * kCharPx * 1.5f * static_cast<float>(sc)) / 2;
      for(char ch : title) { text(s.renderer, (leftBar - kCharPx * static_cast<float>(sc)) / 2, y, sc, std::string(1, ch), yellow); y += kCharPx * 1.5f * static_cast<float>(sc); }
    }
    // right bar: "1UP", its four digits one under the other, then the same for player 2, then the lives
    const int lines = 1 + 4 + (two ? 5 : 0) + 1 + (lives > 0 ? 1 : 0);
    sc = static_cast<int>(std::min(rightBar * 0.9f / (3 * kCharPx), H * 0.9f / (static_cast<float>(lines) * kCharPx * 1.5f)));
    if(sc >= 1) {
      const float step = kCharPx * 1.5f * static_cast<float>(sc);
      float y = (H - static_cast<float>(lines) * step) / 2;
      const float x = wx + ww + (rightBar - 3 * kCharPx * static_cast<float>(sc)) / 2;
      for(int p = 0; p < (two ? 2 : 1); ++p) {
        text(s.renderer, x, y, sc, p == 0 ? "1UP" : "2UP", turn == p && two ? yellow : cyan);
        y += step;
        for(int d = 0; d < 4; ++d) { text(s.renderer, x + kCharPx * static_cast<float>(sc), y, sc, std::string(1, score[p][d]), white); y += step; }
        y += step;
      }
      lifeIcons(x + 1.5f * kCharPx * static_cast<float>(sc), y + step / 2, step, std::max(1, sc / 2), true);
    }
  } else {  // tall window: the panel is above and below
    const std::string title = "PAC-MAN";
    int sc = static_cast<int>(std::min(topBar * 0.6f / kCharPx, W * 0.8f / (static_cast<float>(title.size()) * kCharPx)));
    if(sc >= 1) text(s.renderer, (W - static_cast<float>(title.size()) * kCharPx * static_cast<float>(sc)) / 2, (topBar - kCharPx * static_cast<float>(sc)) / 2, sc, title, yellow);
    std::string line = std::string("1UP ") + score[0];
    if(two) line += std::string("   2UP ") + score[1];
    sc = static_cast<int>(std::min(bottomBar * 0.6f / kCharPx, W * 0.8f / (static_cast<float>(line.size() + 10) * kCharPx)));
    if(sc >= 1) {
      const float iconStep = 8.0f * static_cast<float>(sc), textW = static_cast<float>(line.size()) * kCharPx * static_cast<float>(sc);
      const float total = textW + (lives > 0 ? 2 * kCharPx * static_cast<float>(sc) + iconStep * static_cast<float>(std::min(lives, 7)) : 0.0f);
      const float x = (W - total) / 2, y = wy + wh + (bottomBar - kCharPx * static_cast<float>(sc)) / 2;
      text(s.renderer, x, y, sc, line, white);
      lifeIcons(x + textW + 2 * kCharPx * static_cast<float>(sc), y + kCharPx * static_cast<float>(sc) / 2, iconStep, sc, false);
    }
  }
}

// A drawn TV set: a wooden cabinet, a dark bezel with a bevel round the picture, knobs and a power light below it, speaker slits at the side. Parts that do
// not fit in the space that is left are left out.
void Backdrop::bezel(const BackdropScene& s)
{
  using background::Rgb;
  const float W = static_cast<float>(s.windowW), H = static_cast<float>(s.windowH);
  const SDL_FColor woodTop = fcolour({92, 56, 30}), woodBottom = fcolour({46, 27, 15});
  quad(s.renderer, {0, 0}, {W, 0}, {W, H}, {0, H}, woodTop, woodTop, woodBottom, woodBottom);
  setColour(s.renderer, {30, 18, 10}, 90);  // the grain
  for(float y = 5; y < H; y += 9 + std::fmod(y * 0.37f, 7.0f)) fillRect(s.renderer, 0, y, W, 1);

  const float wx = static_cast<float>(s.where.x), wy = static_cast<float>(s.where.y), ww = static_cast<float>(s.where.w), wh = static_cast<float>(s.where.h);
  const float pad = std::max(6.0f, wh * 0.035f);
  setColour(s.renderer, {10, 10, 12});  // the bezel
  fillRect(s.renderer, wx - pad, wy - pad, ww + 2 * pad, wh + 2 * pad);
  setColour(s.renderer, {88, 88, 96});  // light edge above and left, shadow below and right: the bevel
  fillRect(s.renderer, wx - pad, wy - pad, ww + 2 * pad, 1.5f);
  fillRect(s.renderer, wx - pad, wy - pad, 1.5f, wh + 2 * pad);
  setColour(s.renderer, {0, 0, 0});
  fillRect(s.renderer, wx - pad, wy + wh + pad - 1.5f, ww + 2 * pad, 1.5f);
  fillRect(s.renderer, wx + ww + pad - 1.5f, wy - pad, 1.5f, wh + 2 * pad);
  setColour(s.renderer, {0, 0, 0});  // the dark rim round the glass
  fillRect(s.renderer, wx - 3, wy - 3, ww + 6, 3);
  fillRect(s.renderer, wx - 3, wy + wh, ww + 6, 3);
  fillRect(s.renderer, wx - 3, wy, 3, wh);
  fillRect(s.renderer, wx + ww, wy, 3, wh);

  const float below = H - (wy + wh + pad);
  if(below >= 20) {  // knobs and the light
    const float r = std::min(14.0f, below * 0.32f), cy = wy + wh + pad + below / 2, cx = wx + ww - pad - 3 * r;
    disc(s.renderer, cx, cy, r, {70, 70, 76});
    disc(s.renderer, cx, cy, r * 0.7f, {120, 120, 128});
    disc(s.renderer, cx + 3.2f * r, cy, r, {70, 70, 76});
    disc(s.renderer, cx + 3.2f * r, cy, r * 0.7f, {120, 120, 128});
    disc(s.renderer, wx + pad + r, cy, r * 0.35f, {230, 30, 30});
  }
  const float right = W - (wx + ww + pad);
  if(right >= 24) {  // speaker slits
    const float slitW = std::min(right * 0.5f, 60.0f), x = wx + ww + pad + (right - slitW) / 2;
    setColour(s.renderer, {18, 10, 6});
    for(float y = wy + wh * 0.55f; y < wy + wh * 0.9f; y += 9) fillRect(s.renderer, x, y, slitW, 3);
  }
}

}  // namespace host
