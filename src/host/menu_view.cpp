// Drawing the Escape menu, and the menu screenshot test (Stage I5, T-I5-1). Text is SDL's built-in 8 x 8 pixel debug text, as in the F1 list.
#include "menu_checks.hpp"
#include "rom_choice.hpp"
#include "achievement_driver.hpp"
#include "screen.hpp"
#include "symbols.hpp"

namespace host {

namespace {
constexpr int kTextColumns = 64;  // width of the explanation, in characters
}

// The title of the page (Stage I20: one place; the Achievements page also says how many are unlocked).
static std::string pageTitle(const menu::Menu& menu)
{
  switch(menu.page()) {
    case menu::Page::Graphics: return "GRAPHICS OPTIONS   (the game is paused)";
    case menu::Page::GameOptions: return "GAME OPTIONS   (the game is paused)";
    case menu::Page::Sound: return "SOUND OPTIONS   (the game is paused)";
    case menu::Page::Controls: return "CONTROL OPTIONS   (the game is paused)";
    case menu::Page::Achievements: return "ACHIEVEMENTS   (" + std::to_string(menu.choice().progress.count()) + " of " + std::to_string(achievements::kCount) + " unlocked)";
    case menu::Page::Main: break;
  }
  return "MENU   (the game is paused)";
}

// Draws the menu: the whole window dimmed, then a box with the title, the rows, the explanation of the marked row and the keys.
void drawMenu(SDL_Renderer* renderer, const menu::Menu& menu)
{
  if(menu.previewing()) {  // Stage I7: only a one-line hint, so the background can be seen in full
    int pw = 0, ph = 0;
    SDL_GetRenderOutputSize(renderer, &pw, &ph);
    const float c = static_cast<float>(SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE);
    const float s = std::max(1.0f, std::floor(static_cast<float>(ph) / 480.0f));
    const char* hint = "Any key brings the menu back";
    const float hw = (static_cast<float>(std::strlen(hint)) + 2) * c * s;
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 200);
    const SDL_FRect strip{(static_cast<float>(pw) - hw) / 2, static_cast<float>(ph) - 3 * c * s, hw, 2 * c * s};
    SDL_RenderFillRect(renderer, &strip);
    SDL_SetRenderScale(renderer, s, s);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDebugText(renderer, strip.x / s + c, strip.y / s + c * 0.5f, hint);
    SDL_SetRenderScale(renderer, 1.0f, 1.0f);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    return;
  }
  const bool graphicsPage = menu.page() == menu::Page::Graphics;
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
  if(!graphicsPage) {  // the Graphics page leaves the window as it is, so that the background can be judged
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 150);
    SDL_RenderFillRect(renderer, nullptr);  // dims the whole window
  }
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
  SDL_RenderDebugText(renderer, x, y, pageTitle(menu).c_str());
  y += 2 * lineH;
  for(const std::string& row : rows) { SDL_RenderDebugText(renderer, x, y, row.c_str()); y += lineH; }
  y += lineH;
  SDL_SetRenderDrawColor(renderer, 190, 230, 255, 255);
  for(const std::string& line : words) { drawSymbolText(renderer, x, y, line); y += lineH; }
  SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
  y = y0 + boxH - 2 * lineH;
  SDL_RenderDebugText(renderer, x, y, menu.page() != menu::Page::Main ? "Up/Down choose   Left/Right change   Enter do   Esc back" : "Up/Down choose   Left/Right change   Enter do   Esc close");
  SDL_SetRenderScale(renderer, 1.0f, 1.0f);
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}

// Stages I14 and I18: the Control options page with a pad listed, saved as <png>.controls.png (a DualShock 4) or <png>.controls_xbox.png (an Xbox pad). The four button symbols must
// be there, each in its own colour.
static bool controlsShot(SDL_Renderer* renderer, menu::Menu& menu, const menu::Choice& now, const std::string& pngPath, std::ofstream& report, bool& symbolsDrawn, bool& saved,
                         controls::PadFamily family = controls::PadFamily::PlayStation)
{
  const bool xbox = family == controls::PadFamily::Xbox;
  menu::Choice pad = now;
  pad.padConnected = true;
  pad.pad = family;
  menu.open(pad);
  for(int i = 0; i < 10 && menu.cursor() != menu::Item::ControlOptions; ++i) menu.press(menu::Key::Down);
  menu.press(menu::Key::Enter);
  drawMenu(renderer, menu);
  SDL_Surface* shot = SDL_RenderReadPixels(renderer, nullptr);
  if(!shot) { report << "result=cannot read window pixels\n"; return false; }
  SDL_Surface* rgb = SDL_ConvertSurface(shot, SDL_PIXELFORMAT_XRGB8888);
  SDL_DestroySurface(shot);
  if(!rgb) return false;
  long long symbolPixels[4] = {0, 0, 0, 0};
  const std::uint32_t psColours[4] = {0x6EAAFF, 0xFF5A5A, 0xF58CD7, 0x64E6A0}, xboxColours[4] = {0x50C850, 0xE64646, 0x4682FF, 0xFAC828};   // Cross, Circle, Square, Triangle; A, B, X, Y (symbols.cpp)
  const std::uint32_t* symbolColours = xbox ? xboxColours : psColours;
  for(int y = 0; y < rgb->h; ++y) {
    const auto* row = reinterpret_cast<const std::uint32_t*>(static_cast<const std::uint8_t*>(rgb->pixels) + y * rgb->pitch);
    for(int x = 0; x < rgb->w; ++x)
      for(int k = 0; k < 4; ++k) if((row[x] & 0x00FFFFFF) == symbolColours[k]) ++symbolPixels[k];
  }
  saved = SDL_SavePNG(rgb, (pngPath + (xbox ? ".controls_xbox.png" : ".controls.png")).c_str());
  SDL_DestroySurface(rgb);
  symbolsDrawn = true;
  for(long long n : symbolPixels) symbolsDrawn = symbolsDrawn && n >= 8;
  report << (xbox ? "xbox_" : "") << "controls_page=" << (menu.page() == menu::Page::Controls ? "yes" : "no") << (xbox ? "\nxbox_symbol_pixels_a_b_x_y=" : "\nsymbol_pixels_cross_circle_square_triangle=") << symbolPixels[0] << ',' << symbolPixels[1]
         << ',' << symbolPixels[2] << ',' << symbolPixels[3] << "\ncontrols_screenshot_saved=" << (saved ? "yes" : "no") << '\n';
  return true;
}

// Stage I20: the Achievements page with some unlocked, saved as <png>.achievements.png, and the notice of an unlock over the game, saved as <png>.notice.png. The page must show
// the [x] marks and the notice must have its yellow frame.
static bool achievementsShot(SDL_Renderer* renderer, menu::Menu& menu, const menu::Choice& now, const std::string& pngPath, std::ofstream& report)
{
  menu::Choice c = now;
  c.progress.unlocked = 0x2c07;   // three scores, two lives and the ghosts
  c.progress.dates[0] = c.progress.dates[1] = c.progress.dates[2] = c.progress.dates[10] = c.progress.dates[11] = c.progress.dates[13] = "2026-10-29";
  menu.open(c);
  for(int i = 0; i < 10 && menu.cursor() != menu::Item::Achievements; ++i) menu.press(menu::Key::Down);
  menu.press(menu::Key::Enter);
  menu.press(menu::Key::Down);
  drawMenu(renderer, menu);
  bool ok = true;
  for(int pass = 0; pass < 2; ++pass) {
    if(pass == 1) drawAchievementNotice(renderer, achievements::definition(7).title);   // over the page: as it appears in the window
    SDL_Surface* shot = SDL_RenderReadPixels(renderer, nullptr);
    SDL_Surface* rgb = shot ? SDL_ConvertSurface(shot, SDL_PIXELFORMAT_XRGB8888) : nullptr;
    if(shot) SDL_DestroySurface(shot);
    if(!rgb) { report << "result=cannot read window pixels\n"; return false; }
    long long yellow = 0;
    for(int y = 0; y < rgb->h; ++y) {
      const auto* row = reinterpret_cast<const std::uint32_t*>(static_cast<const std::uint8_t*>(rgb->pixels) + y * rgb->pitch);
      for(int x = 0; x < rgb->w; ++x) yellow += (row[x] & 0x00FFFFFF) == 0xFFD200;
    }
    const bool saved = SDL_SavePNG(rgb, (pngPath + (pass == 0 ? ".achievements.png" : ".notice.png")).c_str());
    SDL_DestroySurface(rgb);
    report << (pass == 0 ? "achievements_page_yellow_pixels=" : "notice_yellow_pixels=") << yellow << (pass == 0 ? "\nachievements_screenshot_saved=" : "\nnotice_screenshot_saved=") << (saved ? "yes" : "no") << '\n';
    ok = ok && saved && (pass == 0 ? menu.page() == menu::Page::Achievements : yellow > 400);
  }
  return ok;
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
  menu.press(menu::Key::Down);
  menu.press(menu::Key::Enter);  // the Game options page, on the Game row: its explanation is the longest
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
  SDL_DestroySurface(rgb);

  // Stage I7: the Graphics options page, drawn over the ambient glow (the window is not dimmed there), saved as <png>.graphics.png.
  menu::Choice glowing = now;
  glowing.background = background::Kind::Glow;
  menu.open(glowing);
  for(int i = 0; i < 3; ++i) menu.press(menu::Key::Down);  // the Graphics row
  menu.press(menu::Key::Enter);
  screen.backgroundKind = glowing.background;
  for(int i = 0; i < 30; ++i) screen.show(emulator.colours(), emulator.height());
  drawMenu(renderer, menu);
  SDL_Surface* shot2 = SDL_RenderReadPixels(renderer, nullptr);
  if(!shot2) { report << "result=cannot read window pixels\n"; return false; }
  SDL_Surface* rgb2 = SDL_ConvertSurface(shot2, SDL_PIXELFORMAT_XRGB8888);
  SDL_DestroySurface(shot2);
  if(!rgb2) return false;
  long long white2 = 0;
  for(int y = 0; y < rgb2->h; ++y) {
    const auto* row = reinterpret_cast<const std::uint32_t*>(static_cast<const std::uint8_t*>(rgb2->pixels) + y * rgb2->pitch);
    for(int x = 0; x < rgb2->w; ++x) {
      const std::uint32_t c = row[x] & 0x00FFFFFF;
      if((c >> 16) > 235 && ((c >> 8) & 0xFF) > 235 && (c & 0xFF) > 235) ++white2;
    }
  }
  const bool saved2 = SDL_SavePNG(rgb2, (pngPath + ".graphics.png").c_str());
  SDL_DestroySurface(rgb2);
  report << "graphics_page=" << (menu.page() == menu::Page::Graphics ? "yes" : "no") << "\ngraphics_near_white_pixels=" << white2
         << "\ngraphics_screenshot_saved=" << (saved2 ? "yes" : "no") << '\n';

  bool symbolsDrawn = false, saved3 = false, xboxDrawn = false, saved4 = false;
  if(!controlsShot(renderer, menu, now, pngPath, report, symbolsDrawn, saved3)) return false;
  if(!controlsShot(renderer, menu, now, pngPath, report, xboxDrawn, saved4, controls::PadFamily::Xbox)) return false;
  const bool onControlsPage = menu.page() == menu::Page::Controls;   // (the Achievements picture below moves the menu to another page)
  const bool achievementsOk = achievementsShot(renderer, menu, now, pngPath, report);
  const bool ok = achievementsOk && saved && white > 500 && other > 1000 && onControlsPage && white2 > 300 && saved2 && saved3 && symbolsDrawn && saved4 && xboxDrawn;
  report << "result=" << (ok ? "OK" : "FAIL") << '\n';
  return ok;
}

}  // namespace host
