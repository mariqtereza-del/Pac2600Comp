// --filter-check (Stage I11.1, DECISIONS.md D-017 and T-I11-1): proves that our own pixel shaders can run on SDL's Direct3D 11 renderer, in the standard colour space and,
// when Windows is in HDR mode, in the HDR one. The game's picture goes through a shader that changes nothing (it only turns the colours to linear light, which is what the
// texture it draws into holds) and the result is drawn next to the plain picture: every flat pixel must be the same; and through a shader that inverts the colours, which must
// change them. No ROM is needed except for the picture, which is the game's own.
#include <algorithm>
#include <cmath>

#include "common.hpp"
#include "gpu_pass.hpp"
#include "options.hpp"
#include "render_setup.hpp"
#include "rom_choice.hpp"
#include "screen.hpp"

namespace host {

namespace {

const char* kIdentity =
    "Texture2D input : register(t0);\n"
    "SamplerState point0 : register(s0);\n"
    "cbuffer b0 : register(b0) { float inputLinear; float3 unused; };\n"
    "float3 decode(float3 v) { return inputLinear > 0.5 ? v : (v <= 0.04045 ? v / 12.92 : pow((v + 0.055) / 1.055, 2.4)); }\n"   // standard (sRGB) values become linear light, which the target holds
    "float4 main(float4 position : SV_Position, float2 uv : TEXCOORD0) : SV_Target {\n"
    "  return float4(decode(input.Sample(point0, uv).rgb), 1.0);\n"
    "}\n";

const char* kInvert =
    "Texture2D input : register(t0);\n"
    "SamplerState point0 : register(s0);\n"
    "cbuffer b0 : register(b0) { float inputLinear; float3 unused; };\n"
    "float3 decode(float3 v) { return inputLinear > 0.5 ? v : (v <= 0.04045 ? v / 12.92 : pow((v + 0.055) / 1.055, 2.4)); }\n"
    "float4 main(float4 position : SV_Position, float2 uv : TEXCOORD0) : SV_Target {\n"
    "  return float4(1.0 - decode(input.Sample(point0, uv).rgb), 1.0);\n"
    "}\n";

}  // namespace

bool filterCheck(SDL_Window* window, SDL_Renderer*& renderer, const std::string& pngPrefix, const std::string& romPath, const View& view)
{
  std::ofstream report(pngPrefix + ".txt");
  std::vector<std::uint8_t> bytes;
  if(!report || !readRomBytes(romPath, bytes)) return false;
  Emulator emulator;
  if(const std::string error = emulator.load(fileNameOf(romPath), bytes); !error.empty()) { report << "result=" << error << '\n'; return false; }
  for(int f = 0; f < 300; ++f) emulator.runFrame();
  const std::vector<std::uint32_t> picture = emulator.colours();
  const int height = emulator.height();
  const hdr::ScreenState state = screenStateOf(window);
  report << "windows_hdr_on=" << (state.hdrOn ? "yes" : "no") << '\n';

  bool ok = true;
  const int passes = hdr::useHdr(true, state) ? 2 : 1;
  for(int pass = 0; pass < passes; ++pass) {   // 0: the standard renderer, 1: the HDR one
    const char* name = pass == 0 ? "sdr" : "hdr";
    SDL_DestroyRenderer(renderer);
    bool gotHdr = false;
    renderer = createRenderer(window, pass == 1, &gotHdr);
    if(!renderer || (pass == 1 && !gotHdr)) { report << name << "_result=the renderer could not be made\n"; return false; }
    Screen screen;
    screen.renderer = renderer;
    screen.view = view;
    screen.view.mode = layout::Mode::Sharp;
    GpuPasses gpu;
    if(!gpu.open(renderer)) { report << name << "_result=the renderer has no Direct3D 11 device\n"; ok = false; continue; }
    std::string error;
    const int identity = gpu.compile(kIdentity, error);
    if(identity < 0) { report << name << "_result=identity shader: " << error << '\n'; ok = false; continue; }
    const int invert = gpu.compile(kInvert, error);
    if(invert < 0) { report << name << "_result=invert shader: " << error << '\n'; ok = false; continue; }

    // the game's picture as a texture, and a render target of the same size that holds linear light (16-bit float)
    SDL_Texture* source = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_STATIC, Emulator::kWidth, height);
    SDL_Texture* target = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA64_FLOAT, SDL_TEXTUREACCESS_TARGET, Emulator::kWidth, height);
    if(!source || !target) { report << name << "_result=textures could not be made: " << SDL_GetError() << '\n'; ok = false; continue; }
    SDL_UpdateTexture(source, nullptr, picture.data(), Emulator::kWidth * 4);
    SDL_SetTextureScaleMode(source, SDL_SCALEMODE_NEAREST);
    SDL_SetTextureScaleMode(target, SDL_SCALEMODE_PIXELART);

    // the plain picture, as the program draws it
    for(int i = 0; i < 3; ++i) screen.show(picture, height);
    Pixels plain, filtered, inverted;
    std::string format;
    if(!readAsStandard(renderer, plain, pngPrefix + "_" + name + "_plain.png", format, state.sdrWhite)) { report << name << "_result=cannot read the window\n"; ok = false; continue; }

    // the same picture through the shaders, drawn where the plain one was (Screen::show has put `where` and the shown rows there)
    const SDL_FRect src{0.0f, static_cast<float>(screen.shownTop), static_cast<float>(Emulator::kWidth), static_cast<float>(screen.shownHeight)};
    const SDL_FRect dst{static_cast<float>(screen.where.x), static_cast<float>(screen.where.y), static_cast<float>(screen.where.w), static_cast<float>(screen.where.h)};
    for(int which = 0; which < 2; ++which) {
      const float constants[4] = {gpu.readsEightBitAsLinear() ? 1.0f : 0.0f, 0, 0, 0};
      if(!gpu.run(which == 0 ? identity : invert, source, target, constants, sizeof constants, error)) { report << name << "_result=run: " << error << '\n'; ok = false; break; }
      SDL_SetRenderTarget(renderer, nullptr);
      SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
      SDL_RenderClear(renderer);
      SDL_RenderTexture(renderer, target, &src, &dst);
      if(!readAsStandard(renderer, which == 0 ? filtered : inverted, pngPrefix + "_" + name + (which == 0 ? "_identity.png" : "_invert.png"), format, state.sdrWhite)) {
        report << name << "_result=cannot read the window\n";
        ok = false;
        break;
      }
    }
    SDL_DestroyTexture(source);
    SDL_DestroyTexture(target);
    if(filtered.xrgb.empty() || inverted.xrgb.empty()) continue;
    const FlatComparison same = compareFlat(plain, filtered);
    long long invertedDifferent = 0;
    for(std::size_t i = 0; i < plain.xrgb.size() && i < inverted.xrgb.size(); ++i) invertedDifferent += plain.xrgb[i] != inverted.xrgb[i];
    report << name << "_flat_pixels=" << same.inside << '\n' << name << "_flat_pixels_that_differ_by_more_than_one_step=" << same.insideDifferent << '\n' << name
           << "_largest_difference_in_flat_areas=" << same.largestInside << '\n' << name << "_edge_pixels=" << same.edges << '\n' << name << "_edge_pixels_that_differ_by_more_than_one_step="
           << same.edgesDifferent << '\n' << name << "_pixels_changed_by_the_invert_shader=" << invertedDifferent << '\n';
    const bool good = same.inside > 100000 && same.insideDifferent == 0 && invertedDifferent > 100000;
    report << name << "_result=" << (good ? "OK" : "FAIL") << '\n';
    ok = ok && good;
  }
  report << "result=" << (ok ? "OK" : "FAIL") << '\n';
  return ok;
}


// --crt-shot: the picture through a CRT filter in the real window, in the standard colour space and, when Windows is in HDR mode, in the HDR one. Saved as PNGs (<prefix>_<sdr
// or hdr>_<filter>.png) and measured: the filter must not leave the picture black or unchanged, must make scanlines (the brightness of the rows of the picture varies from row to
// row where the plain picture's does not), and the picture with the filter off must be what it always was.
bool crtShot(SDL_Window* window, SDL_Renderer*& renderer, const std::string& pngPrefix, const std::string& romPath, const View& view, crt::Filter filter)
{
  std::ofstream report(pngPrefix + ".txt");
  std::vector<std::uint8_t> bytes;
  if(!report || !readRomBytes(romPath, bytes)) return false;
  Emulator emulator;
  if(const std::string error = emulator.load(fileNameOf(romPath), bytes); !error.empty()) { report << "result=" << error << '\n'; return false; }
  for(int f = 0; f < 700; ++f) emulator.runFrame();
  const std::vector<std::uint32_t> picture = emulator.colours();
  const hdr::ScreenState state = screenStateOf(window);
  report << "filter=" << crt::settingName(filter) << "\nwindows_hdr_on=" << (state.hdrOn ? "yes" : "no") << '\n';
  bool ok = true;
  const int passes = hdr::useHdr(true, state) ? 2 : 1;
  for(int pass = 0; pass < passes; ++pass) {
    const char* name = pass == 0 ? "sdr" : "hdr";
    SDL_DestroyRenderer(renderer);
    bool gotHdr = false;
    renderer = createRenderer(window, pass == 1, &gotHdr);
    if(!renderer || (pass == 1 && !gotHdr)) { report << name << "_result=the renderer could not be made\n"; return false; }
    Screen screen;
    screen.renderer = renderer;
    screen.view = view;
    Pixels plain, filtered;
    std::string format;
    screen.crt = crt::Filter::Off;
    for(int i = 0; i < 3; ++i) screen.show(picture, emulator.height());
    if(!readAsStandard(renderer, plain, pngPrefix + "_" + name + "_off.png", format, state.sdrWhite)) { report << name << "_result=cannot read the window\n"; ok = false; continue; }
    screen.crt = filter;
    for(int i = 0; i < 3; ++i) screen.show(picture, emulator.height());
    if(!screen.crtError.empty()) { report << name << "_result=the filter could not run: " << screen.crtError << '\n'; ok = false; continue; }
    if(!readAsStandard(renderer, filtered, pngPrefix + "_" + name + "_" + crt::settingName(filter) + ".png", format, state.sdrWhite)) { report << name << "_result=cannot read the window\n"; ok = false; continue; }
    // the picture's own area (where the plain picture is not black): the mean brightness, and how much the rows differ from each other
    auto rowsOf = [&](const Pixels& px, double& mean, double& spread) {
      std::vector<double> rows;
      for(int y = screen.where.y; y < screen.where.y + screen.where.h && y < px.h; ++y) {
        double sum = 0;
        for(int x = screen.where.x; x < screen.where.x + screen.where.w && x < px.w; ++x) {
          const std::uint32_t c = px.xrgb[static_cast<std::size_t>(y * px.w + x)];
          sum += 0.2126 * ((c >> 16) & 0xFF) + 0.7152 * ((c >> 8) & 0xFF) + 0.0722 * (c & 0xFF);
        }
        rows.push_back(sum / std::max(1, screen.where.w));
      }
      mean = 0;
      for(double r : rows) mean += r;
      mean /= std::max<std::size_t>(1, rows.size());
      double var = 0;
      for(double r : rows) var += (r - mean) * (r - mean);
      spread = std::sqrt(var / std::max<std::size_t>(1, rows.size()));
    };
    double meanOff, spreadOff, meanOn, spreadOn;
    rowsOf(plain, meanOff, spreadOff);
    rowsOf(filtered, meanOn, spreadOn);
    long long different = 0;
    for(std::size_t i = 0; i < plain.xrgb.size() && i < filtered.xrgb.size(); ++i) different += plain.xrgb[i] != filtered.xrgb[i];
    report << name << "_mean_brightness_off=" << meanOff << '\n' << name << "_mean_brightness_filtered=" << meanOn << '\n' << name << "_row_to_row_spread_off=" << spreadOff << '\n' << name
           << "_row_to_row_spread_filtered=" << spreadOn << '\n' << name << "_pixels_changed_by_the_filter=" << different << '\n';
    const bool good = meanOn > 10.0 && meanOn < 250.0 && different > 100000;
    report << name << "_result=" << (good ? "OK" : "FAIL") << '\n';
    ok = ok && good;
  }
  report << "result=" << (ok ? "OK" : "FAIL") << '\n';
  return ok;
}

}  // namespace host
