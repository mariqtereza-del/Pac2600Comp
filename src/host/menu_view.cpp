// Drawing the Escape menu, and the menu screenshot test (Stage I5, T-I5-1). Text is SDL's built-in 8 x 8 pixel debug text, as in the F1 list.
#include "menu_checks.hpp"
#include "rom_choice.hpp"
#include "screen.hpp"

namespace host {

namespace {
constexpr int kTextColumns = 64;  // width of the explanation, in characters
}

// Draws the menu: the whole window dimmed, then a box with the title, the rows, the explanation of the marked row and the keys.
void drawMenu(SDL_Renderer* renderer, const menu::Menu& menu)
{
  const std::vector<std::string> rows = menu.rows();
  const std::vector<std::string> words = menu.explanation(kTextColumns);
  const float charSize = static_cast<float>(SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE);
  const float lineH = charSize * 1.4f;  // a little air between the lines
  const float columns = kTextColumns + 4;
  const float lines = static_cast<float>(rows.size() + words.size() + 8);  // title, gaps, keys
  int w = 0, h = 0;
  SDL_GetRenderOutputSize(renderer, &w, &h);
  float scale = std::max(1.0f, std::floor(static_cast<float>(h) / 480.0f));
  while(scale > 1.0f && (columns * charSize * scale > static_cast<float>(w) * 0.95f || lines * lineH * scale > static_cast<float>(h) * 0.95f)) scale -= 1.0f;

  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 150);
  SDL_RenderFillRect(renderer, nullptr);  // dims the whole window
  SDL_SetRenderScale(renderer, scale, scale);
  const float boxW = columns * charSize, boxH = lines * lineH;
  const float x0 = (static_cast<float>(w) / scale - boxW) / 2, y0 = (static_cast<float>(h) / scale - boxH) / 2;
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 240);
  const SDL_FRect box{x0, y0, boxW, boxH};
  SDL_RenderFillRect(renderer, &box);
  SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
  SDL_RenderRect(renderer, &box);
  const float x = x0 + 2 * charSize;
  float y = y0 + lineH;
  SDL_RenderDebugText(renderer, x, y, "MENU   (the game is paused)");
  y += 2 * lineH;
  for(const std::string& row : rows) { SDL_RenderDebugText(renderer, x, y, row.c_str()); y += lineH; }
  y += lineH;
  SDL_SetRenderDrawColor(renderer, 190, 230, 255, 255);
  for(const std::string& line : words) { SDL_RenderDebugText(renderer, x, y, line.c_str()); y += lineH; }
  SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
  y = y0 + boxH - 2 * lineH;
  SDL_RenderDebugText(renderer, x, y, "Up/Down choose   Left/Right change   Enter do   Esc close");
  SDL_SetRenderScale(renderer, 1.0f, 1.0f);
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}

// --menu-shot: 300 frames of the game, the menu opened on the Game row, drawn in the real window, the window's pixels saved. The report counts
// the near-white pixels (the text), which must exist, and the pixels of the dimmed picture behind the box, which must exist too.
bool menuShot(SDL_Renderer* renderer, const std::string& pngPath, const std::string& romPath)
{
  std::ofstream report(pngPath + ".txt");
  std::vector<std::uint8_t> bytes;
  if(!report || !readRomBytes(romPath, bytes)) return false;
  Emulator emulator;
  if(const std::string error = emulator.load(fileNameOf(romPath), bytes); !error.empty()) { report << "result=" << error << '\n'; return false; }
  Screen screen;
  screen.renderer = renderer;
  for(int f = 0; f < 300; ++f) {
    emulator.runFrame();
    if(!screen.show(emulator.colours(), emulator.height())) { report << "result=texture failed\n"; return false; }
  }
  menu::Menu menu;
  menu::Choice now;
  now.game = 3;
  menu.open(now);
  menu.press(menu::Key::Down);
  menu.press(menu::Key::Down);  // the Game row: its explanation is the longest
  drawMenu(renderer, menu);
  SDL_Surface* shot = SDL_RenderReadPixels(renderer, nullptr);
  if(!shot) { report << "result=cannot read window pixels\n"; return false; }
  SDL_Surface* rgb = SDL_ConvertSurface(shot, SDL_PIXELFORMAT_XRGB8888);
  SDL_DestroySurface(shot);
  if(!rgb) return false;
  long long white = 0, other = 0;
  for(int y = 0; y < rgb->h; ++y) {
    const auto* row = reinterpret_cast<const std::uint32_t*>(static_cast<const std::uint8_t*>(rgb->pixels) + y * rgb->pitch);
    for(int x = 0; x < rgb->w; ++x) {
      const std::uint32_t c = row[x] & 0x00FFFFFF;
      if((c >> 16) > 235 && ((c >> 8) & 0xFF) > 235 && (c & 0xFF) > 235) ++white;
      else if(c != 0) ++other;
    }
  }
  const bool saved = SDL_SavePNG(rgb, pngPath.c_str());
  report << "window_pixels=" << rgb->w << 'x' << rgb->h << "\nrows=" << menu.rows().size() << "\nexplanation_lines=" << menu.explanation(kTextColumns).size()
         << "\nnear_white_pixels=" << white << "\nother_non_black_pixels=" << other << "\nscreenshot_saved=" << (saved ? "yes" : "no") << '\n';
  const bool ok = saved && white > 500 && other > 1000;
  report << "result=" << (ok ? "OK" : "FAIL") << '\n';
  SDL_DestroySurface(rgb);
  return ok;
}

}  // namespace host
