// --smooth-shot (Stage I9.4, DECISIONS.md T-I9-4): the smooth picture in the real window. A game is started and played on for a few hundred frames with smooth
// motion on, Pac-Man walking to the right; then, for three steps in a row, the picture of the step is drawn at three moments of the step in the real
// window (the start, the middle, the end) and each window's pixels are saved as a PNG (<prefix>_s<step>_p<moment>.png). The report says how many window
// pixels differ between the start and the end of a step (Pac-Man and the ghosts move, the rest of the picture does not), and that the picture without the moving
// objects is the same at all three moments.
#include <algorithm>

#include "common.hpp"
#include "menu_checks.hpp"
#include "rom_choice.hpp"
#include "screen.hpp"

namespace host {

namespace {

// The window's pixels, as 0x00RRGGBB, with the size.
struct Shot {
  int w = 0, h = 0;
  std::vector<std::uint32_t> pixels;
};

bool readWindow(SDL_Renderer* renderer, Shot& out, const std::string& png)
{
  SDL_Surface* raw = SDL_RenderReadPixels(renderer, nullptr);
  if(!raw) return false;
  SDL_Surface* rgb = SDL_ConvertSurface(raw, SDL_PIXELFORMAT_XRGB8888);
  SDL_DestroySurface(raw);
  if(!rgb) return false;
  out.w = rgb->w;
  out.h = rgb->h;
  out.pixels.resize(static_cast<std::size_t>(rgb->w) * static_cast<std::size_t>(rgb->h));
  for(int y = 0; y < rgb->h; ++y) {
    const auto* row = reinterpret_cast<const std::uint32_t*>(static_cast<const std::uint8_t*>(rgb->pixels) + y * rgb->pitch);
    for(int x = 0; x < rgb->w; ++x) out.pixels[static_cast<std::size_t>(y * rgb->w + x)] = row[x] & 0x00FFFFFF;
  }
  const bool saved = SDL_SavePNG(rgb, png.c_str());
  SDL_DestroySurface(rgb);
  return saved;
}

long long differing(const Shot& a, const Shot& b)
{
  long long n = 0;
  for(std::size_t i = 0; i < a.pixels.size() && i < b.pixels.size(); ++i) n += a.pixels[i] != b.pixels[i];
  return n;
}

}  // namespace

bool smoothShot(SDL_Renderer* renderer, const std::string& pngPrefix, const std::string& romPath, const View& view)
{
  std::ofstream report(pngPrefix + ".txt");
  std::vector<std::uint8_t> bytes;
  if(!report || !readRomBytes(romPath, bytes)) return false;
  Emulator emulator;
  if(const std::string error = emulator.load(fileNameOf(romPath), bytes); !error.empty()) { report << "result=" << error << '\n'; return false; }
  if(!emulator.smoothAvailable()) { report << "result=this program has no smooth motion (the reference program)\n"; return false; }
  emulator.setSmooth(true);
  Screen screen;
  screen.renderer = renderer;
  screen.view = view;
  // a game: Reset, then Pac-Man along the bottom corridor, to the left and to the right in turn (the ghosts leave the pen as the game goes on)
  const int walkStart = 400;
  for(int f = 0; f < 1500; ++f) {
    emulator.setConsoleReset(f >= 120 && f < 130);
    const bool goLeft = f >= walkStart && ((f - walkStart) / 90) % 2 == 0, goRight = f >= walkStart && !goLeft;
    emulator.setJoystick(false, false, goLeft, goRight, false);
    emulator.runFrame();
  }
  emulator.setJoystick(false, false, false, true, false);
  const double moments[3] = {0.0, 0.5, 0.95};
  long long movedWindowPixels = 0, backgroundChanged = 0;
  int saved = 0, steps = 0;
  std::vector<SmoothRect> moving;
  for(int step = 0; step < 3; ++step) {
    emulator.runFrame();
    const std::shared_ptr<const SmoothSnapshot> snapshot = emulator.smoothSnapshot();
    if(!snapshot) { report << "result=no smooth picture\n"; return false; }
    ++steps;
    Shot shots[3], plain[3];
    for(int m = 0; m < 3; ++m) {
      snapshot->compose(moments[m], true, moving);
      if(!screen.show(snapshot->background, emulator.height(), &moving)) { report << "result=texture failed\n"; return false; }
      saved += readWindow(renderer, shots[m], pngPrefix + "_s" + std::to_string(step) + "_p" + std::to_string(m) + ".png");
      if(!screen.show(snapshot->background, emulator.height(), nullptr)) return false;   // the same without the moving objects
      readWindow(renderer, plain[m], pngPrefix + "_background_only_" + std::to_string(step) + "_" + std::to_string(m) + ".png");
    }
    movedWindowPixels += differing(shots[0], shots[2]);
    backgroundChanged += differing(plain[0], plain[1]) + differing(plain[1], plain[2]);
  }
  // The TV type switch on black and white: the moving objects, like the rest, are shades of grey (red, green and blue within 8 of each other), and a picture is saved.
  emulator.setBlackWhite(true);
  for(int f = 0; f < 4; ++f) emulator.runFrame();
  long long greyRects = 0, colouredRects = 0;
  if(const std::shared_ptr<const SmoothSnapshot> snapshot = emulator.smoothSnapshot()) {
    snapshot->compose(0.5, true, moving);
    for(const SmoothRect& r : moving) {
      const std::uint32_t red = (r.colour >> 16) & 0xFF, green = (r.colour >> 8) & 0xFF, blue = r.colour & 0xFF;
      const auto spread = std::max({red, green, blue}) - std::min({red, green, blue});   // the console's grey is not quite exact in the palette
      (spread <= 8 ? greyRects : colouredRects) += 1;
    }
    Shot bw;
    if(screen.show(snapshot->background, emulator.height(), &moving)) saved += readWindow(renderer, bw, pngPrefix + "_blackwhite.png");
  }
  emulator.setBlackWhite(false);
  report << "black_and_white_moving_rectangles_grey=" << greyRects << "\nblack_and_white_moving_rectangles_coloured=" << colouredRects << '\n';
  report << "steps=" << steps << "\npngs_saved=" << saved << "\nwindow_pixels_that_differ_between_the_start_and_the_end_of_a_step=" << movedWindowPixels
         << "\nbackground_pixels_that_change_during_a_step=" << backgroundChanged << '\n';
  const bool ok = saved == 10 && movedWindowPixels > 0 && backgroundChanged == 0 && greyRects > 0 && colouredRects == 0;
  report << "result=" << (ok ? "OK" : "FAIL") << '\n';
  return ok;
}

}  // namespace host
