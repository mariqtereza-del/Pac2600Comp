// --hdr-report (Stage I10.1, DECISIONS.md D-016): asks the screen what it says about HDR, and tries a renderer in the HDR colour space, and writes what it
// found. Nothing is drawn that matters; no ROM is needed.
#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "common.hpp"
#include "hdr.hpp"
#include "options.hpp"
#include "render_setup.hpp"
#include "rom_choice.hpp"
#include "screen.hpp"

namespace host {

bool hdrWantedBy(const Options& o)
{
  if(!o.hdr.empty()) return o.hdr != "off";
  return settings::get(o.settingsFile, "hdr") != "off";
}

bool hdrReport(const Options& o)
{
  std::ofstream report(o.hdrReportFile);
  if(!report) return false;
  if(!SDL_Init(SDL_INIT_VIDEO)) { report << "result=SDL could not start\n"; return false; }
  int displays = 0;
  SDL_DisplayID* ids = SDL_GetDisplays(&displays);
  report << "displays=" << displays << '\n';
  for(int i = 0; i < displays; ++i) {
    const char* name = SDL_GetDisplayName(ids[i]);
    report << "display_" << i << "_name=" << (name ? name : "?") << "\ndisplay_" << i << "_hdr_enabled="
           << (SDL_GetBooleanProperty(SDL_GetDisplayProperties(ids[i]), SDL_PROP_DISPLAY_HDR_ENABLED_BOOLEAN, false) ? "yes" : "no") << '\n';
  }
  SDL_free(ids);

  SDL_Window* window = SDL_CreateWindow("HDR report", 320, 240, 0);
  if(!window) { report << "result=window failed\n"; SDL_Quit(); return false; }
  const hdr::ScreenState before = screenStateOf(window);
  report << "window_hdr_on=" << (before.hdrOn ? "yes" : "no") << "\nwindow_sdr_white=" << before.sdrWhite << "\nwindow_hdr_headroom=" << before.headroom << '\n';

  // A renderer in the HDR colour space (linear, 16-bit float): made, drawn on once, and asked what it makes of the screen.
  SDL_PropertiesID props = SDL_CreateProperties();
  SDL_SetPointerProperty(props, SDL_PROP_RENDERER_CREATE_WINDOW_POINTER, window);
  SDL_SetNumberProperty(props, SDL_PROP_RENDERER_CREATE_OUTPUT_COLORSPACE_NUMBER, SDL_COLORSPACE_SRGB_LINEAR);
  SDL_Renderer* renderer = SDL_CreateRendererWithProperties(props);
  SDL_DestroyProperties(props);
  if(!renderer) {
    report << "hdr_renderer=failed: " << SDL_GetError() << '\n';
  } else {
    const SDL_PropertiesID rp = SDL_GetRendererProperties(renderer);
    report << "hdr_renderer=" << SDL_GetRendererName(renderer) << "\nrenderer_output_colorspace=" << SDL_GetNumberProperty(rp, SDL_PROP_RENDERER_OUTPUT_COLORSPACE_NUMBER, 0)
           << "\nrenderer_hdr_enabled=" << (SDL_GetBooleanProperty(rp, SDL_PROP_RENDERER_HDR_ENABLED_BOOLEAN, false) ? "yes" : "no")
           << "\nrenderer_sdr_white=" << SDL_GetFloatProperty(rp, SDL_PROP_RENDERER_SDR_WHITE_POINT_FLOAT, 1.0f)
           << "\nrenderer_hdr_headroom=" << SDL_GetFloatProperty(rp, SDL_PROP_RENDERER_HDR_HEADROOM_FLOAT, 1.0f) << '\n';
    SDL_SetRenderDrawColorFloat(renderer, 1.0f, 1.0f, 1.0f, 1.0f);
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);
    SDL_DestroyRenderer(renderer);
  }
  const hdr::ScreenState after = screenStateOf(window);
  report << "window_hdr_on_after=" << (after.hdrOn ? "yes" : "no") << "\nwindow_hdr_headroom_after=" << after.headroom << '\n';
  report << "program_would_use_hdr=" << (hdr::useHdr(true, after) ? "yes" : "no") << "\nhighlight_ceiling=" << hdr::ceilingOf(after) << '\n';
  report << "result=OK\n";
  SDL_DestroyWindow(window);
  SDL_Quit();
  return true;
}

float halfToFloat(std::uint16_t h)
{
  const int sign = (h >> 15) & 1, exponent = (h >> 10) & 31, mantissa = h & 1023;
  float value;
  if(exponent == 0) value = std::ldexp(static_cast<float>(mantissa), -24);
  else if(exponent == 31) value = mantissa ? 0.0f : 65504.0f;
  else value = std::ldexp(static_cast<float>(mantissa + 1024), exponent - 25);
  return sign ? -value : value;
}

// Linear light (1.0 = normal white) to the standard 8-bit colour value.
int toStandard(float linear)
{
  const float v = std::clamp(linear, 0.0f, 1.0f);
  const float encoded = v <= 0.0031308f ? 12.92f * v : 1.055f * std::pow(v, 1.0f / 2.4f) - 0.055f;
  return static_cast<int>(std::lround(encoded * 255.0f));
}

// The window's picture as standard 8-bit colour, whatever the renderer's colour space. An HDR renderer gives 16-bit float linear pixels in which normal white is
// `sdrWhite` (the screen's white level); they are turned back to the standard colours by hand, so that what is compared is exactly what the screen is sent.
bool readAsStandard(SDL_Renderer* renderer, Pixels& out, const std::string& png, std::string& format, float sdrWhite)
{
  SDL_Surface* raw = SDL_RenderReadPixels(renderer, nullptr);
  if(!raw) return false;
  format = SDL_GetPixelFormatName(raw->format);
  SDL_Surface* rgb = nullptr;
  if(raw->format == SDL_PIXELFORMAT_RGBA64_FLOAT) {
    rgb = SDL_CreateSurface(raw->w, raw->h, SDL_PIXELFORMAT_XRGB8888);
    if(rgb)
      for(int y = 0; y < raw->h; ++y) {
        const auto* in = reinterpret_cast<const std::uint16_t*>(static_cast<const std::uint8_t*>(raw->pixels) + y * raw->pitch);
        auto* row = reinterpret_cast<std::uint32_t*>(static_cast<std::uint8_t*>(rgb->pixels) + y * rgb->pitch);
        for(int x = 0; x < raw->w; ++x)
          row[x] = static_cast<std::uint32_t>(toStandard(halfToFloat(in[4 * x]) / sdrWhite) << 16 | toStandard(halfToFloat(in[4 * x + 1]) / sdrWhite) << 8 | toStandard(halfToFloat(in[4 * x + 2]) / sdrWhite));
      }
  } else {
    rgb = SDL_ConvertSurface(raw, SDL_PIXELFORMAT_XRGB8888);
  }
  SDL_DestroySurface(raw);
  if(!rgb) return false;
  out.w = rgb->w;
  out.h = rgb->h;
  out.xrgb.resize(static_cast<std::size_t>(rgb->w) * static_cast<std::size_t>(rgb->h));
  for(int y = 0; y < rgb->h; ++y) {
    const auto* row = reinterpret_cast<const std::uint32_t*>(static_cast<const std::uint8_t*>(rgb->pixels) + y * rgb->pitch);
    for(int x = 0; x < rgb->w; ++x) out.xrgb[static_cast<std::size_t>(y * rgb->w + x)] = row[x] & 0x00FFFFFF;
  }
  const bool saved = SDL_SavePNG(rgb, png.c_str());
  SDL_DestroySurface(rgb);
  return saved;
}


// --hdr-check: the game's picture drawn by the standard renderer and by the one in the HDR colour space, side by side in numbers (Stage I10.1).
bool hdrCheck(SDL_Window* window, SDL_Renderer*& renderer, const std::string& pngPrefix, const std::string& romPath, const View& view)
{
  std::ofstream report(pngPrefix + ".txt");
  std::vector<std::uint8_t> bytes;
  if(!report || !readRomBytes(romPath, bytes)) return false;
  Emulator emulator;
  if(const std::string error = emulator.load(fileNameOf(romPath), bytes); !error.empty()) { report << "result=" << error << '\n'; return false; }
  for(int f = 0; f < 300; ++f) emulator.runFrame();
  const std::vector<std::uint32_t> picture = emulator.colours();
  const hdr::ScreenState state = screenStateOf(window);
  report << "windows_hdr_on=" << (state.hdrOn ? "yes" : "no") << "\nsdr_white=" << state.sdrWhite << "\nheadroom=" << state.headroom << '\n';
  if(!hdr::useHdr(true, state)) { report << "result=SKIPPED (Windows is not in HDR mode for this screen; nothing to compare)\n"; return true; }

  Pixels shots[2];
  std::string formats[2];
  for(int pass = 0; pass < 2; ++pass) {   // 0: the standard renderer, 1: the HDR one
    SDL_DestroyRenderer(renderer);
    bool gotHdr = false;
    renderer = createRenderer(window, pass == 1, &gotHdr);
    if(!renderer || (pass == 1 && !gotHdr)) { report << "result=the HDR renderer could not be made\n"; return false; }
    Screen screen;
    screen.renderer = renderer;
    screen.view = view;
    screen.view.mode = layout::Mode::Sharp;   // whole-number steps, so that no pixel is a blend of two (a blend is made in gamma space by the standard renderer and in linear light by the HDR one)
    for(int i = 0; i < 3; ++i) if(!screen.show(picture, emulator.height())) { report << "result=texture failed\n"; return false; }
    if(!readAsStandard(renderer, shots[pass], pngPrefix + (pass ? "_hdr.png" : "_sdr.png"), formats[pass], state.sdrWhite)) { report << "result=cannot read the window\n"; return false; }
  }
  // Pixels in the middle of a flat area (all four neighbours the same colour in the standard picture) must be the same through both renderers; pixels on an edge
  // between two colours may differ, because an edge is a blend and the standard renderer blends in gamma space while the HDR one blends in linear light.
  long long inside = 0, insideDifferent = 0, edges = 0, edgesDifferent = 0;
  int largestInside = 0, largestEdge = 0;
  const int w = shots[0].w, h = shots[0].h;
  const std::size_t n = std::min(shots[0].xrgb.size(), shots[1].xrgb.size());
  auto diffOf = [&](std::size_t i) {
    int d = 0;
    for(int shift = 0; shift <= 16; shift += 8) d = std::max(d, std::abs(static_cast<int>((shots[0].xrgb[i] >> shift) & 0xFF) - static_cast<int>((shots[1].xrgb[i] >> shift) & 0xFF)));
    return d;
  };
  for(int y = 1; y + 1 < h; ++y)
    for(int x = 1; x + 1 < w; ++x) {
      const std::size_t i = static_cast<std::size_t>(y * w + x);
      const std::uint32_t c = shots[0].xrgb[i];
      const bool flat = shots[0].xrgb[i - 1] == c && shots[0].xrgb[i + 1] == c && shots[0].xrgb[i - static_cast<std::size_t>(w)] == c && shots[0].xrgb[i + static_cast<std::size_t>(w)] == c;
      const int d = diffOf(i);
      if(flat) { ++inside; if(d > 1) ++insideDifferent; largestInside = std::max(largestInside, d); }
      else { ++edges; if(d > 1) ++edgesDifferent; largestEdge = std::max(largestEdge, d); }
    }
  report << "pixels_in_flat_areas=" << inside << "\nflat_pixels_that_differ_by_more_than_one_step=" << insideDifferent << "\nlargest_difference_in_flat_areas=" << largestInside
         << "\nedge_pixels=" << edges << "\nedge_pixels_that_differ_by_more_than_one_step=" << edgesDifferent << "\nlargest_difference_on_edges=" << largestEdge << '\n';
  report << "window=" << shots[0].w << 'x' << shots[0].h << "\nstandard_read_format=" << formats[0] << "\nhdr_read_format=" << formats[1] << "\npixels=" << n << '\n';
  const bool ok = insideDifferent == 0 && inside > 100000;
  report << "result=" << (ok ? "OK" : "DIFFERENT") << '\n';
  return ok;
}


namespace {

// The window as linear light, 1.0 = normal white, three floats a pixel (an HDR renderer only).
bool readLinear(SDL_Renderer* renderer, std::vector<float>& out, int& w, int& h, float sdrWhite)
{
  SDL_Surface* raw = SDL_RenderReadPixels(renderer, nullptr);
  if(!raw) return false;
  if(raw->format != SDL_PIXELFORMAT_RGBA64_FLOAT) { SDL_DestroySurface(raw); return false; }
  w = raw->w;
  h = raw->h;
  out.resize(static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 3);
  for(int y = 0; y < h; ++y) {
    const auto* in = reinterpret_cast<const std::uint16_t*>(static_cast<const std::uint8_t*>(raw->pixels) + y * raw->pitch);
    for(int x = 0; x < w; ++x)
      for(int c = 0; c < 3; ++c) out[static_cast<std::size_t>((y * w + x) * 3 + c)] = halfToFloat(in[4 * x + c]) / sdrWhite;
  }
  SDL_DestroySurface(raw);
  return true;
}

}  // namespace

// What the three pictures (slider 0, 50, 100) must show: each moving rectangle in the colour the arithmetic gives, only the moving objects changed, and nothing above
// the ceiling. The numbers go to the report.
static bool judgeHighlights(std::ofstream& report, const std::vector<float> pictures[3], int w, int h, const float maxValue[3], const int strengths[3], const hdr::ScreenState& state,
                            const Screen& screen, const std::vector<SmoothRect>& moving)
{
  // Each moving rectangle, at its centre: the colour measured in the picture at 0 must be its own colour, and at 50 and at 100 the colour that hdr::highlightColour
  // gives for that scale, in the same brightness ratio and the same hue.
  bool ok = true;
  long long rectsChecked = 0, rectsWrong = 0;
  for(const SmoothRect& r : moving) {
    const int px = screen.where.x + static_cast<int>((r.x + r.w / 2) * static_cast<float>(screen.where.w) / 160.0f);
    const int py = screen.where.y + static_cast<int>((r.y - static_cast<float>(screen.shownTop) + r.h / 2) * static_cast<float>(screen.where.h) / static_cast<float>(screen.shownHeight));
    if(px < 0 || py < 0 || px >= w || py >= h) continue;
    const std::size_t j = static_cast<std::size_t>((py * w + px) * 3);
    const hdr::Rgb own{Screen::decode(r.colour >> 16), Screen::decode(r.colour >> 8), Screen::decode(r.colour)};
    for(int i = 0; i < 3; ++i) {
      const float scale = hdr::highlightScale(static_cast<float>(strengths[i]) / 100.0f, state, true);
      const hdr::Rgb want = hdr::highlightColour(own, scale);
      ++rectsChecked;
      auto close = [](float a2, float b2) { return std::fabs(a2 - b2) <= 0.01f + 0.02f * std::fabs(b2); };
      if(!(close(pictures[static_cast<std::size_t>(i)][j], want.r) && close(pictures[static_cast<std::size_t>(i)][j + 1], want.g) && close(pictures[static_cast<std::size_t>(i)][j + 2], want.b))) ++rectsWrong;
    }
    if(rectsChecked <= 6) {   // the first few, to read
      const hdr::Rgb top = hdr::highlightColour(own, hdr::highlightScale(1.0f, state, true));
      char line[200];
      std::snprintf(line, sizeof line, "rect_colour_%06x: plain %.3f %.3f %.3f   at 100: %.3f %.3f %.3f   (measured %.3f %.3f %.3f)%c", r.colour & 0xFFFFFF, own.r, own.g, own.b, top.r, top.g, top.b,
                    pictures[2][j], pictures[2][j + 1], pictures[2][j + 2], 10);
      report << line;
    }
  }
  report << "rectangles_checked=" << rectsChecked << "\nrectangles_not_the_colour_the_arithmetic_gives=" << rectsWrong << '\n';
  ok = ok && rectsChecked >= 9 && rectsWrong == 0;
  // against the plain picture (slider 0): only the moving objects change, and by the brightness the slider says (a channel held to the scale is the one exception)
  for(int i = 1; i < 3; ++i) {
    const float scale = hdr::highlightScale(static_cast<float>(strengths[i]) / 100.0f, state, true);
    long long same = 0, brighter = 0, wrong = 0;
    for(std::size_t j = 0; j < pictures[0].size(); j += 3) {
      const hdr::Rgb a2{pictures[0][j], pictures[0][j + 1], pictures[0][j + 2]}, b2{pictures[static_cast<std::size_t>(i)][j], pictures[static_cast<std::size_t>(i)][j + 1], pictures[static_cast<std::size_t>(i)][j + 2]};
      if(std::fabs(a2.r - b2.r) <= 0.0005f && std::fabs(a2.g - b2.g) <= 0.0005f && std::fabs(a2.b - b2.b) <= 0.0005f) { ++same; continue; }
      ++brighter;
      const float ya = hdr::luminance(a2), yb = hdr::luminance(b2);
      const bool capped = std::max({b2.r, b2.g, b2.b}) >= scale * 0.98f;
      if(!(ya > 0.01f && yb > 0.0f && (capped ? yb <= ya * scale * 1.03f : (yb > ya * scale * 0.97f && yb < ya * scale * 1.03f)))) ++wrong;
    }
    report << "strength_" << strengths[i] << "_pixels_unchanged=" << same << "\nstrength_" << strengths[i] << "_pixels_brighter=" << brighter << "\nstrength_" << strengths[i]
           << "_pixels_brighter_by_other_than_the_slider=" << wrong << '\n';
    ok = ok && brighter > 100 && wrong == 0 && maxValue[i] <= hdr::ceilingOf(state) * 1.02f;
  }
  ok = ok && maxValue[0] <= 1.02f;   // the plain picture is never above normal white
  return ok;
}

// --hdr-highlights: Pac-Man, the pills and the ghost drawn brighter than white (Stage I10.3). The same moment of a game is drawn with the slider at 0, 50 and 100;
// against the picture with the slider at 0 only the moving objects may differ, each of them by exactly the scale the arithmetic gives, and nothing may go above the
// most the screen can show.
bool hdrHighlights(SDL_Window* window, SDL_Renderer*& renderer, const std::string& pngPrefix, const std::string& romPath, const View& view)
{
  std::ofstream report(pngPrefix + ".txt");
  std::vector<std::uint8_t> bytes;
  if(!report || !readRomBytes(romPath, bytes)) return false;
  Emulator emulator;
  if(const std::string error = emulator.load(fileNameOf(romPath), bytes); !error.empty()) { report << "result=" << error << '\n'; return false; }
  const hdr::ScreenState state = screenStateOf(window);
  report << "windows_hdr_on=" << (state.hdrOn ? "yes" : "no") << "\nsdr_white=" << state.sdrWhite << "\nheadroom=" << state.headroom << '\n';
  if(!hdr::useHdr(true, state)) { report << "result=SKIPPED (Windows is not in HDR mode for this screen; nothing to draw)\n"; return true; }
  SDL_DestroyRenderer(renderer);
  bool gotHdr = false;
  renderer = createRenderer(window, true, &gotHdr);
  if(!renderer || !gotHdr) { report << "result=the HDR renderer could not be made\n"; return false; }
  emulator.setSmooth(true);
  for(int f = 0; f < 1500; ++f) {   // a game with Pac-Man walking along the bottom corridor, as --smooth-shot
    emulator.setConsoleReset(f >= 120 && f < 130);
    const bool left = f >= 400 && ((f - 400) / 90) % 2 == 0;
    emulator.setJoystick(false, false, left, f >= 400 && !left, false);
    emulator.runFrame();
  }
  const std::shared_ptr<const SmoothSnapshot> snapshot = emulator.smoothSnapshot();
  if(!snapshot) { report << "result=no smooth picture\n"; return false; }
  std::vector<SmoothRect> moving;
  snapshot->compose(0.0, false, moving);
  Screen screen;
  screen.renderer = renderer;
  screen.view = view;
  screen.view.mode = layout::Mode::Sharp;
  const int strengths[3] = {0, 50, 100};
  std::vector<float> pictures[3];
  int w = 0, h = 0;
  float maxValue[3] = {0, 0, 0};
  for(int i = 0; i < 3; ++i) {
    const float scale = hdr::highlightScale(static_cast<float>(strengths[i]) / 100.0f, state, true);
    for(int k = 0; k < 3; ++k) if(!screen.show(snapshot->background, emulator.height(), &moving, scale)) { report << "result=texture failed\n"; return false; }
    if(!readLinear(renderer, pictures[i], w, h, state.sdrWhite)) { report << "result=cannot read the window as float\n"; return false; }
    for(float v : pictures[i]) maxValue[i] = std::max(maxValue[i], v);
    report << "strength_" << strengths[i] << "_scale=" << scale << "\nstrength_" << strengths[i] << "_brightest_value=" << maxValue[i] << '\n';
  }
  const bool ok = judgeHighlights(report, pictures, w, h, maxValue, strengths, state, screen, moving);
  report << "result=" << (ok ? "OK" : "FAIL") << '\n';
  return ok;
}

}  // namespace host
