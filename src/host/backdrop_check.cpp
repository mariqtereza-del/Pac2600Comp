// --backdrop-check (Stage I7, T-I7-1): every background drawn in the real window, 300 frames into a game, at the size of the window it is run in.
// For each one: the picture's rectangle must hold exactly the pixels that Black gives (the picture is never touched), and the space outside it must
// be filled (not black) as far as the background is meant to fill it. One PNG per background is saved for looking at: <prefix>_<name>.png.
#include "menu_checks.hpp"
#include "rom_choice.hpp"
#include "screen.hpp"

namespace host {

namespace {

struct Shot {
  int w = 0, h = 0;
  std::vector<std::uint32_t> pixels;  // 0x00RRGGBB, row by row
};

bool grab(SDL_Renderer* renderer, Shot& out, const std::string& pngPath)
{
  SDL_Surface* shot = SDL_RenderReadPixels(renderer, nullptr);
  if(!shot) return false;
  SDL_Surface* rgb = SDL_ConvertSurface(shot, SDL_PIXELFORMAT_XRGB8888);
  SDL_DestroySurface(shot);
  if(!rgb) return false;
  out.w = rgb->w; out.h = rgb->h;
  out.pixels.resize(static_cast<std::size_t>(rgb->w) * rgb->h);
  for(int y = 0; y < rgb->h; ++y)
    std::memcpy(&out.pixels[static_cast<std::size_t>(y) * rgb->w], static_cast<const std::uint8_t*>(rgb->pixels) + static_cast<std::size_t>(y) * rgb->pitch, static_cast<std::size_t>(rgb->w) * 4);
  for(auto& p : out.pixels) p &= 0x00FFFFFF;
  const bool saved = pngPath.empty() || SDL_SavePNG(rgb, pngPath.c_str());
  SDL_DestroySurface(rgb);
  return saved;
}

}  // namespace

bool backdropCheck(SDL_Renderer* renderer, const std::string& pngPrefix, const std::string& romPath)
{
  std::ofstream report(pngPrefix + ".txt");
  std::vector<std::uint8_t> bytes;
  if(!report || !readRomBytes(romPath, bytes)) return false;
  Emulator emulator;
  if(const std::string error = emulator.load(fileNameOf(romPath), bytes); !error.empty()) { report << "result=" << error << '\n'; return false; }
  for(int f = 0; f < 300; ++f) {  // into a game, so that the picture, the score and the lives are the real thing
    emulator.setConsoleReset(f >= 20 && f < 30);
    emulator.runFrame();
  }
  const std::vector<std::uint32_t> pixels = emulator.colours();
  std::array<std::uint8_t, 128> ram{};
  for(std::size_t i = 0; i < ram.size(); ++i) ram[i] = emulator.ram()[i];

  View view;
  view.mode = layout::Mode::Framed;
  bool ok = true;
  Shot black;
  layout::Rect where{};
  int windowW = 0, windowH = 0;

  struct Case { background::Kind kind; std::string name; };
  const std::vector<Case> cases = {{background::Kind::Black, "black"}, {background::Kind::Glow, "glow"}, {background::Kind::Mirror, "mirror"}, {background::Kind::Solid, "solid"},
                                   {background::Kind::Gradient, "gradient"}, {background::Kind::Arcade, "arcade"}, {background::Kind::Bezel, "bezel"}};

  for(const Case& c : cases) {
    Screen screen;
    screen.renderer = renderer;
    screen.view = view;
    screen.backgroundKind = c.kind;
    screen.ram = ram.data();
    for(int i = 0; i < 40; ++i)  // several times, so that the glow has eased all the way to the picture
      if(!screen.show(pixels, emulator.height())) { report << "result=texture failed\n"; return false; }
    Shot shot;
    if(!grab(renderer, shot, pngPrefix + "_" + c.name + ".png")) { report << "result=cannot read window pixels or save the PNG\n"; return false; }
    if(c.kind == background::Kind::Black) { black = shot; where = screen.where; SDL_GetRenderOutputSize(renderer, &windowW, &windowH); }

    long long insideDifferent = 0, outside = 0, outsideLit = 0;
    for(int y = 0; y < shot.h; ++y)
      for(int x = 0; x < shot.w; ++x) {
        const std::size_t i = static_cast<std::size_t>(y) * shot.w + x;
        if(x >= where.x && x < where.x + where.w && y >= where.y && y < where.y + where.h) {
          if(shot.pixels[i] != black.pixels[i]) ++insideDifferent;
        } else {
          ++outside;
          if(shot.pixels[i] != 0) ++outsideLit;
        }
      }
    const double lit = outside > 0 ? static_cast<double>(outsideLit) / static_cast<double>(outside) : 1.0;
    // How much of the space each background is meant to fill: the glow is dim and the arcade panel leaves most of it plain navy (all lit); Black must stay black.
    const double wanted = c.kind == background::Kind::Black ? 0.0 : c.kind == background::Kind::Glow ? 0.2 : 0.5;
    // Solid colour is the maze colour: the blue the maze is drawn on, which is not the tan of its walls (blue clearly above red), and it is exactly what mazeColour says.
    bool mazeOk = true;
    if(c.kind == background::Kind::Solid) {
      const background::Rgb want = background::mazeColour(emulator.colours().data(), Emulator::kWidth, layout::kGameTop, layout::kGameBottom - layout::kGameTop);
      const std::uint32_t corner = shot.pixels[0];
      mazeOk = corner == ((static_cast<std::uint32_t>(want.r) << 16) | (static_cast<std::uint32_t>(want.g) << 8) | want.b) && want.b > want.r + 40;
      report << "solid_corner_pixel=" << std::hex << corner << std::dec << " maze_colour_rgb=" << int(want.r) << ',' << int(want.g) << ',' << int(want.b) << " maze_colour_ok=" << (mazeOk ? "yes" : "no") << '\n';
    }
    const bool good = mazeOk && insideDifferent == 0 && (c.kind == background::Kind::Black ? outsideLit == 0 : lit >= wanted);
    report << c.name << ": picture_pixels_different_from_black=" << insideDifferent << " space_outside_picture=" << outside << " lit_fraction=" << lit
           << (good ? "  ok" : "  FAIL") << '\n';
    ok = ok && good;
  }
  report << "window_pixels=" << windowW << 'x' << windowH << "\npicture_rect=" << where.x << ',' << where.y << ' ' << where.w << 'x' << where.h << '\n';
  report << "result=" << (ok ? "OK" : "FAIL") << '\n';
  return ok;
}

}  // namespace host
