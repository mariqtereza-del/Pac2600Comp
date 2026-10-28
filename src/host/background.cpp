// What fills the space around the game picture: choices and small calculations (Stage I7, T-I7-1).
#include "background.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace background {

namespace {
struct Entry { const char* setting; const char* title; };
constexpr Entry kEntries[kKinds] = {{"black", "Black"},       {"glow", "Ambient glow"}, {"solid", "Solid colour"},
                                    {"gradient", "Gradient"}, {"arcade", "Arcade panel"}, {"bezel", "TV bezel"}};
}  // namespace

const char* settingName(Kind kind) { return kEntries[static_cast<int>(kind)].setting; }

Kind fromSetting(const std::string& text)
{
  for(int i = 0; i < kKinds; ++i)
    if(text == kEntries[i].setting) return static_cast<Kind>(i);
  return Kind::Black;
}

const char* title(Kind kind) { return kEntries[static_cast<int>(kind)].title; }

Kind next(Kind kind, int step) { return static_cast<Kind>(((static_cast<int>(kind) + step) % kKinds + kKinds) % kKinds); }

Rgb scaled(Rgb colour, double factor)
{
  auto one = [&](std::uint8_t v) { return static_cast<std::uint8_t>(std::clamp(std::lround(v * factor), 0L, 255L)); };
  return {one(colour.r), one(colour.g), one(colour.b)};
}

Rgb defaultMazeColour() { return {16, 24, 96}; }

Rgb mazeColour(const std::uint32_t* pixels, int width, int firstRow, int rows)
{
  std::array<int, 4096> count{};
  for(int y = firstRow; y < firstRow + rows; ++y)
    for(int x = 0; x < width; ++x) {
      const std::uint32_t c = pixels[static_cast<std::size_t>(y) * width + x] & 0x00FFFFFF;
      if(c == 0) continue;
      ++count[((c >> 20) & 15) << 8 | ((c >> 12) & 15) << 4 | ((c >> 4) & 15)];
    }
  const int best = static_cast<int>(std::max_element(count.begin(), count.end()) - count.begin());
  if(count[static_cast<std::size_t>(best)] == 0) return defaultMazeColour();
  auto channel = [](int nibble) { return static_cast<std::uint8_t>(nibble * 16 + 8); };  // the middle of the 16 steps
  return {channel(best >> 8), channel((best >> 4) & 15), channel(best & 15)};
}

void blockAverages(const std::uint32_t* pixels, int width, int firstRow, int rows, int cols, int gridRows, std::vector<float>& out)
{
  out.assign(static_cast<std::size_t>(cols) * gridRows * 3, 0.0f);
  for(int gy = 0; gy < gridRows; ++gy)
    for(int gx = 0; gx < cols; ++gx) {
      const int x0 = gx * width / cols, x1 = std::max(x0 + 1, (gx + 1) * width / cols);
      const int y0 = firstRow + gy * rows / gridRows, y1 = std::max(y0 + 1, firstRow + (gy + 1) * rows / gridRows);
      double r = 0, g = 0, b = 0;
      int n = 0;
      for(int y = y0; y < y1; ++y)
        for(int x = x0; x < x1; ++x) {
          const std::uint32_t c = pixels[static_cast<std::size_t>(y) * width + x];
          r += (c >> 16) & 255; g += (c >> 8) & 255; b += c & 255;
          ++n;
        }
      float* cell = &out[(static_cast<std::size_t>(gy) * cols + gx) * 3];
      cell[0] = static_cast<float>(r / n); cell[1] = static_cast<float>(g / n); cell[2] = static_cast<float>(b / n);
    }
}

namespace {
int digit(std::uint8_t nibble) { return nibble > 9 ? 0 : nibble; }
}  // namespace

// Player 1: $CE (hundreds and thousands) and $CC (tens and ones); player 2: $CF and $CD (docs\F7_RAM_MAP.md). Each nibble is one decimal digit.
int scoreOf(const std::uint8_t* ram, int player)
{
  const std::uint8_t mid = ram[(player == 0 ? 0xCE : 0xCF) - 0x80], lo = ram[(player == 0 ? 0xCC : 0xCD) - 0x80];
  return digit(mid >> 4) * 1000 + digit(mid & 15) * 100 + digit(lo >> 4) * 10 + digit(lo & 15);
}

int livesOf(const std::uint8_t* ram) { return ram[0x82 - 0x80] != 0 ? 0 : std::min<int>(ram[0x98 - 0x80], 9); }

int playerToPlay(const std::uint8_t* ram) { return (ram[0xE6 - 0x80] & 0x80) != 0 ? 1 : 0; }

bool twoPlayers(const std::uint8_t* ram) { return (ram[0xE6 - 0x80] & 0x40) != 0; }

}  // namespace background
