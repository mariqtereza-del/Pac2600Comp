// The picture in the window, and the checks that look at it (self check, layout check, key list).
// (Stage G4.7: moved out of main.cpp, the code is unchanged.)
#include "screen.hpp"
#include "options.hpp"
#include "rom_choice.hpp"

namespace host {

// Writes the window's title, size and the colour of its centre pixel; returns true if it is a black 960x720 window.
bool selfCheck(SDL_Window* window, SDL_Renderer* renderer, const char* path)
{
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
  SDL_RenderClear(renderer);
  SDL_Surface* shot = SDL_RenderReadPixels(renderer, nullptr);  // read before presenting
  int w = 0, h = 0;
  SDL_GetWindowSizeInPixels(window, &w, &h);
  Uint8 r = 255, g = 255, b = 255, a = 0;
  if(shot) {
    SDL_ReadSurfacePixel(shot, shot->w / 2, shot->h / 2, &r, &g, &b, &a);
    SDL_DestroySurface(shot);
  }
  const bool ok = shot && w == kWindowW && h == kWindowH && r == 0 && g == 0 && b == 0
                  && std::strcmp(SDL_GetWindowTitle(window), kTitle) == 0;
  char text[512];
  std::snprintf(text, sizeof text, "title=%s\nsize_pixels=%dx%d\ncentre_pixel_rgb=%d,%d,%d\nrenderer=%s\nsdl=%d.%d.%d\nresult=%s\n",
                SDL_GetWindowTitle(window), w, h, r, g, b, SDL_GetRendererName(renderer),
                SDL_MAJOR_VERSION, SDL_MINOR_VERSION, SDL_MICRO_VERSION, ok ? "OK" : "FAIL");
  std::ofstream out(path);
  if(!out) return false;
  out << text;
  return ok;
}

// --layout-check: run 300 frames in the real window, read its pixels back and check that the picture is where the
// layout says: nothing outside its rectangle, and the outline of what is drawn inside it where the arithmetic predicts.
bool layoutCheck(SDL_Renderer* renderer, const std::string& pngPath, const std::string& romPath, const View& view, int resetAt)
{
  std::ofstream report(pngPath + ".txt");
  std::vector<std::uint8_t> bytes;
  if(!report || !readRomBytes(romPath, bytes)) return false;
  Emulator emulator;
  if(const std::string error = emulator.load(fileNameOf(romPath), bytes); !error.empty()) { report << "result=" << error << '\n'; return false; }

  Screen screen;
  screen.renderer = renderer;
  screen.view = view;
  std::vector<std::uint32_t> pixels;
  for(int f = 0; f < 300; ++f) {
    emulator.setConsoleReset(resetAt >= 0 && f >= resetAt && f < resetAt + 10);
    emulator.runFrame();
    pixels = emulator.colours();
    if(!screen.show(pixels, emulator.height())) { report << "result=texture failed\n"; return false; }
  }
  SDL_Surface* shot = SDL_RenderReadPixels(renderer, nullptr);  // the window's pixels, before presenting
  if(!shot) { report << "result=cannot read window pixels\n"; return false; }
  SDL_Surface* rgb = SDL_ConvertSurface(shot, SDL_PIXELFORMAT_XRGB8888);
  SDL_DestroySurface(shot);
  if(!rgb) return false;

  const int h = emulator.height();
  const layout::Rect r = screen.where;
  const int shownTop = screen.shownTop, shownRows = screen.shownHeight;

  // Outline of what the emulator drew in the rows that are shown (anything not black), in picture pixels,
  // and how many non-black pixels lie in rows that are NOT shown (cut off by Game area: must be none).
  int c0 = Emulator::kWidth, c1 = -1, r0 = h, r1 = -1;
  long long cutOff = 0;
  for(int y = 0; y < h; ++y)
    for(int x = 0; x < Emulator::kWidth; ++x)
      if((pixels[static_cast<std::size_t>(y) * Emulator::kWidth + x] & 0x00FFFFFF) != 0) {
        if(y < shownTop || y >= shownTop + shownRows) { ++cutOff; continue; }
        c0 = std::min(c0, x); c1 = std::max(c1, x); r0 = std::min(r0, y - shownTop); r1 = std::max(r1, y - shownTop);
      }
  // The same outline predicted in window pixels.
  const double sx = static_cast<double>(r.w) / Emulator::kWidth, sy = static_cast<double>(r.h) / shownRows;
  const int ex0 = r.x + static_cast<int>(std::floor(c0 * sx)), ex1 = r.x + static_cast<int>(std::ceil((c1 + 1) * sx)) - 1;
  const int ey0 = r.y + static_cast<int>(std::floor(r0 * sy)), ey1 = r.y + static_cast<int>(std::ceil((r1 + 1) * sy)) - 1;

  // Outline of what the window really shows, and any non-black pixel outside the picture's rectangle.
  int ax0 = rgb->w, ax1 = -1, ay0 = rgb->h, ay1 = -1;
  long long outsideNotBlack = 0;
  for(int y = 0; y < rgb->h; ++y) {
    const auto* row = reinterpret_cast<const std::uint32_t*>(static_cast<const std::uint8_t*>(rgb->pixels) + y * rgb->pitch);
    for(int x = 0; x < rgb->w; ++x) {
      if((row[x] & 0x00FFFFFF) == 0) continue;
      const bool inside = x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
      if(!inside) ++outsideNotBlack;
      ax0 = std::min(ax0, x); ax1 = std::max(ax1, x); ay0 = std::min(ay0, y); ay1 = std::max(ay1, y);
    }
  }
  const int off = std::max({std::abs(ax0 - ex0), std::abs(ax1 - ex1), std::abs(ay0 - ey0), std::abs(ay1 - ey1)});
  const bool saved = SDL_SavePNG(rgb, pngPath.c_str());
  const layout::PixelShape shape = view.shape.num > 0 ? view.shape : layout::stellaShape(Emulator::kWidth, h);
  const double shapeSeen = (static_cast<double>(r.w) / Emulator::kWidth) / (static_cast<double>(r.h) / shownRows);
  report << "mode=" << modeName(view.mode) << "\nwindow_pixels=" << rgb->w << 'x' << rgb->h << "\npicture_rect=" << r.x << ',' << r.y << ' ' << r.w << 'x' << r.h
         << "\npixel_shape_wanted=" << static_cast<double>(shape.num) / shape.den << "\npixel_shape_in_window=" << shapeSeen
         << "\noutline_predicted=" << ex0 << ',' << ey0 << " to " << ex1 << ',' << ey1 << "\noutline_seen=" << ax0 << ',' << ay0 << " to " << ax1 << ',' << ay1
         << "\nrows_shown=" << shownTop << ".." << shownTop + shownRows - 1 << "\nnon_black_pixels_in_rows_not_shown=" << cutOff
         << "\noutline_largest_difference_px=" << off << "\npixels_outside_picture_rect_not_black=" << outsideNotBlack
         << "\nscreenshot_saved=" << (saved ? "yes" : "no") << '\n';
  const bool ok = saved && off <= 2 && outsideNotBlack == 0 && cutOff == 0 && std::fabs(shapeSeen - static_cast<double>(shape.num) / shape.den) / (static_cast<double>(shape.num) / shape.den) <= 0.005;
  report << "result=" << (ok ? "OK" : "FAIL") << '\n';
  SDL_DestroySurface(rgb);
  return ok;
}

// The F1 list, drawn over the picture with SDL's built-in text.
void drawHelp(SDL_Renderer* renderer, const controls::Input& current, const std::string& controller, bool softBlend, controls::Scheme scheme)
{
  const std::vector<controls::HelpLine> lines = controls::helpLines(current, controller, softBlend, scheme);
  std::size_t keyWidth = 0, meaningWidth = 0;
  for(const auto& l : lines) { keyWidth = std::max(keyWidth, l.keys.size()); meaningWidth = std::max(meaningWidth, l.meaning.size()); }
  const float charSize = static_cast<float>(SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE);
  const float columns = static_cast<float>(keyWidth + meaningWidth + 6);
  const float rows = static_cast<float>(lines.size() + 4);
  int w = 0, h = 0;
  SDL_GetRenderOutputSize(renderer, &w, &h);
  float scale = std::max(1.0f, std::floor(static_cast<float>(h) / 480.0f));
  while(scale > 1.0f && (columns * charSize * scale > static_cast<float>(w) * 0.95f || rows * charSize * scale > static_cast<float>(h) * 0.95f)) scale -= 1.0f;

  SDL_SetRenderScale(renderer, scale, scale);
  const float boxW = columns * charSize, boxH = rows * charSize;
  const float x0 = (static_cast<float>(w) / scale - boxW) / 2, y0 = (static_cast<float>(h) / scale - boxH) / 2;
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 235);
  const SDL_FRect box{x0, y0, boxW, boxH};
  SDL_RenderFillRect(renderer, &box);
  SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
  float y = y0 + charSize;
  SDL_RenderDebugText(renderer, x0 + 2 * charSize, y, "KEYS   (F1 closes this list)");
  y += 2 * charSize;
  for(const auto& l : lines) {
    SDL_RenderDebugText(renderer, x0 + 2 * charSize, y, l.keys.c_str());
    SDL_RenderDebugText(renderer, x0 + (2 + static_cast<float>(keyWidth) + 2) * charSize, y, l.meaning.c_str());
    y += charSize;
  }
  SDL_SetRenderScale(renderer, 1.0f, 1.0f);
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}

// --help-check: the F1 list in the real window. 300 frames, then the list is drawn, the window's pixels are read back and
// saved, and the check counts the near-white pixels (the text), which must exist, and that the picture is still there.
bool helpCheck(SDL_Renderer* renderer, const std::string& pngPath, const std::string& romPath)
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
  drawHelp(renderer, controls::Input{}, "PS4 Controller");
  SDL_Surface* shot = SDL_RenderReadPixels(renderer, nullptr);
  if(!shot) { report << "result=cannot read window pixels\n"; return false; }
  SDL_Surface* rgb = SDL_ConvertSurface(shot, SDL_PIXELFORMAT_XRGB8888);
  SDL_DestroySurface(shot);
  if(!rgb) return false;
  long long white = 0, coloured = 0;
  for(int y = 0; y < rgb->h; ++y) {
    const auto* row = reinterpret_cast<const std::uint32_t*>(static_cast<const std::uint8_t*>(rgb->pixels) + y * rgb->pitch);
    for(int x = 0; x < rgb->w; ++x) {
      const std::uint32_t c = row[x] & 0x00FFFFFF;
      if((c >> 16) > 235 && ((c >> 8) & 0xFF) > 235 && (c & 0xFF) > 235) ++white;
      else if(c != 0) ++coloured;
    }
  }
  const bool saved = SDL_SavePNG(rgb, pngPath.c_str());
  const std::size_t listed = controls::helpLines(controls::Input{}, "PS4 Controller").size();
  report << "window_pixels=" << rgb->w << 'x' << rgb->h << "\nlines_listed=" << listed << "\nnear_white_pixels=" << white << "\nother_non_black_pixels=" << coloured
         << "\nscreenshot_saved=" << (saved ? "yes" : "no") << '\n';
  const bool ok = saved && white > 500 && coloured > 1000;
  report << "result=" << (ok ? "OK" : "FAIL") << '\n';
  SDL_DestroySurface(rgb);
  return ok;
}

void drawFrameCounter(SDL_Renderer* renderer, framerate::Rate rate, double picturesPerSecond)
{
  char text[96];
  std::snprintf(text, sizeof text, "Frame rate %s%s   %.0f pictures a second", framerate::title(rate), framerate::smooth(rate) ? " (smooth)" : "", picturesPerSecond);
  SDL_SetRenderScale(renderer, 2.0f, 2.0f);
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
  const SDL_FRect back{2.0f, 2.0f, static_cast<float>(std::strlen(text)) * 8.0f + 8.0f, 14.0f};
  SDL_RenderFillRect(renderer, &back);
  SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
  SDL_RenderDebugText(renderer, 6.0f, 6.0f, text);
  SDL_SetRenderScale(renderer, 1.0f, 1.0f);
}

}  // namespace host
