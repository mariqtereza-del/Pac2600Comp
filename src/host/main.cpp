// pacman2600 host. D2: a window that closes cleanly. D3: finds and checks the ROM. D4: the emulator runs the ROM.
// D5: the game picture is shown in the window (DECISIONS.md T-D2-1 ... T-D5-1). Later stages add scaling, sound and
// controls (ARCHITECTURE.md).
//
// Usage: pacman2600 [rom_file]                  open the window; the ROM is looked for in this order: the file given
//                                               here, the saved path, a "roms" folder next to the program, a dialog
//        pacman2600 --self-check <file>         open the window, write what it looks like to <file>, quit
//        pacman2600 --find-rom <file> [rom_file] [--settings <ini>] [--roms-dir <dir>]
//                                               the quiet part of the ROM search, no window; writes the result to <file>
//        pacman2600 --layout-check <png> [rom_file] [--mode sharp|fill|tv|game] [--window WxH] [--pixel-aspect W:H]
//                                               run 300 frames in the window, check where the picture is, save a screenshot
//        pacman2600 --timed <seconds> <file> [rom_file]  play for that long, then write the frame rate to <file>
//        pacman2600 --fullscreen-check <report> <rom_file> --settings <ini>
//                                               in the real window: odd size and place, Alt+Enter on and off, the picture
//                                               checked in fullscreen, the state saved to <ini> (fullscreen on)
//        pacman2600 --state-report <report> --settings <ini>
//                                               start from the state saved in <ini>, report what was restored, quit
//        pacman2600 --audio-check <seconds> <report> [rom_file] [--mute] [--reset-at N] [--timer-clock] [--settings <ini>]
//                                               no window: runs the game at 60 frames/s into the real sound device for that
//                                               long and writes the device format and the buffer statistics to <report>
//        pacman2600 --keys-replay <script> <csv> [rom_file] [--frames N]
//                                               no window: presses the keys a Phase C input script asks for, in the real
//                                               keyboard code, and writes the picture fingerprint of every frame to <csv>
//        pacman2600 --help-check <png> [rom_file]   the F1 key list in the real window, as a screenshot
//        pacman2600 --pad-check <report> [rom_file] a virtual gamepad plays the real game: every button, the stick,
//                                               unplugging and plugging in again (no window)
//        pacman2600 --record <file> | --replay <file> [--fingerprints <csv>]   (window, or --headless for --replay)
//                                               write every frame's input to <file> when the window closes / play such a
//                                               file back instead of the keyboard (Stage D13)
//        (window mode: F1 = list of keys; arrows/WASD, Space, F2, F3, F5, F6 play the game (controls.cpp);
//         keys 1, 2, 3, 4, 5 = Sharp, Fill, TV frame, Game area, Framed; Alt+Enter = fullscreen on/off;
//         the window can be resized; its size, place and fullscreen are remembered in settings.ini)
//        pacman2600 --headless <png> [rom_file] [--frames N] [--save-frame N] [--raw <file>] [--fingerprints <csv>]
//                   [--settings <ini>] [--roms-dir <dir>] [--reset-at N] [--audio-wav <file>]
//                                               runs the game with no window (default 600 frames) and saves frame
//                                               N (default 300) as a PNG, with a report in <png>.txt

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <atomic>
#include <fstream>
#include <mutex>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "audio.hpp"
#include "controls.hpp"
#include "emulator.hpp"
#include "inputlog.hpp"
#include "layout.hpp"
#include "rom_finder.hpp"
#include "settings.hpp"
#include "windowstate.hpp"

namespace {

constexpr const char* kTitle = "pacman2600 (working name)";  // real name is chosen in Stage J2
constexpr int kWindowW = 1280, kWindowH = 960;  // first window size; 4:3 like the picture

void showError(const char* what)
{
  const char* detail = SDL_GetError();
  char text[512];
  std::snprintf(text, sizeof text, "%s\n\n%s", what, detail);
  SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, kTitle, text, nullptr);
}

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

// How the picture is laid out in the window (Stage D6, T-D6-1).
struct View {
  layout::Mode mode = layout::Mode::Sharp;
  layout::PixelShape shape{0, 0};  // {0, 0} = Stella's shape for the picture's size (layout::stellaShape)
};

const char* modeName(layout::Mode m)
{
  switch(m) {
    case layout::Mode::Sharp: return "Sharp";
    case layout::Mode::Fill: return "Fill";
    case layout::Mode::TvFrame: return "TV frame";
    case layout::Mode::GameArea: return "Game area";
    case layout::Mode::Framed: return "Framed";
  }
  return "?";
}

bool parseMode(const std::string& text, layout::Mode& mode)
{
  if(text == "sharp") mode = layout::Mode::Sharp;
  else if(text == "fill") mode = layout::Mode::Fill;
  else if(text == "tv") mode = layout::Mode::TvFrame;
  else if(text == "game") mode = layout::Mode::GameArea;
  else if(text == "framed") mode = layout::Mode::Framed;
  else return false;
  return true;
}

// Splits "AxB" or "A:B" into two whole numbers; false if it is not in that form.
bool parsePair(const std::string& text, char separator, int& first, int& second)
{
  const std::size_t cut = text.find(separator);
  if(cut == std::string::npos || cut == 0 || cut + 1 >= text.size()) return false;
  try {
    first = std::stoi(text.substr(0, cut));
    second = std::stoi(text.substr(cut + 1));
  } catch(...) {
    return false;
  }
  return first > 0 && second > 0;
}

struct Options {
  std::string selfCheckFile, findRomFile, romArg, settingsFile, romsDir;
  std::string headlessPng, rawFile, fingerprintFile, layoutCheckPng, timedReport, fullscreenCheckFile, stateReportFile;
  std::string audioCheckFile, audioWav, keysReplayScript, keysReplayCsv, helpCheckPng, padCheckFile;
  std::string recordFile, replayFile;  // input recording (--record) and playback (--replay), Stage D13
  long long ramSeed = -1;              // --ram-seed N: power-on memory filled from seed N (-1 = zeros)
  bool softGlow = false;               // --soft-glow: Stella's phosphor glow on (headless tests of F7)
  bool stellaSpeed = false;            // --stella-speed: 60.000 frames a second like Stella, instead of the console's own 59.92 (comparison only)
  double audioCheckSeconds = 0;
  int resetAt = -1;   // frame at which the console's Reset is held for 10 frames (tests; the R key does this by hand)
  bool mute = false;  // play the sound stream with the volume at zero (tests)
  bool pushKeys = false;  // tests: a helper thread presses keys in the real window's event queue (--push-keys)
  bool timerClock = false;  // pace the game from the PC clock instead of the sound card (comparison only)
  int stallMs = 0;    // tests: freeze the loop once, 5 seconds in (--audio-check: the game loop; the game window: the window thread)
  bool windowGiven = false;  // --window on the command line: do not use the remembered window
  layout::Mode mode = layout::Mode::Framed;  // 12:7 pixels, game area, black border on every side (D-006); keys 1-5 switch it while playing
  layout::PixelShape pixelShape{12, 7};        // the standard NTSC shape of a 2600 pixel (D-005); {0, 0} = Stella's 19:10 (--pixel-aspect stella)
  int windowW = kWindowW, windowH = kWindowH;
  double timedSeconds = 0;
  int frames = 600, saveFrame = 300;
};

Options parseArguments(int argc, char* argv[])
{
  Options o;
  for(int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    auto next = [&]() -> std::string { return i + 1 < argc ? argv[++i] : std::string(); };
    if(a == "--self-check") o.selfCheckFile = next();
    else if(a == "--find-rom") o.findRomFile = next();
    else if(a == "--headless") o.headlessPng = next();
    else if(a == "--layout-check") o.layoutCheckPng = next();
    else if(a == "--mode") { if(!parseMode(next(), o.mode)) std::fprintf(stderr, "--mode must be sharp, fill, tv, game or framed\n"); }
    else if(a == "--keys-replay") { o.keysReplayScript = next(); o.keysReplayCsv = next(); }
    else if(a == "--help-check") o.helpCheckPng = next();
    else if(a == "--pad-check") o.padCheckFile = next();
    else if(a == "--stella-speed") o.stellaSpeed = true;
    else if(a == "--soft-glow") o.softGlow = true;
    else if(a == "--ram-seed") o.ramSeed = std::atoll(next().c_str());
    else if(a == "--record") o.recordFile = next();
    else if(a == "--replay") o.replayFile = next();
    else if(a == "--audio-check") { o.audioCheckSeconds = std::atof(next().c_str()); o.audioCheckFile = next(); }
    else if(a == "--audio-wav") o.audioWav = next();
    else if(a == "--reset-at") o.resetAt = std::atoi(next().c_str());
    else if(a == "--mute") o.mute = true;
    else if(a == "--timer-clock") o.timerClock = true;
    else if(a == "--push-keys") o.pushKeys = true;
    else if(a == "--stall-ms") o.stallMs = std::atoi(next().c_str());
    else if(a == "--fullscreen-check") o.fullscreenCheckFile = next();
    else if(a == "--state-report") o.stateReportFile = next();
    else if(a == "--window") { o.windowGiven = true; if(!parsePair(next(), 'x', o.windowW, o.windowH)) std::fprintf(stderr, "--window must look like 1280x960\n"); }
    else if(a == "--pixel-aspect") {
      const std::string text = next();
      if(text == "stella") o.pixelShape = {0, 0};
      else if(!parsePair(text, ':', o.pixelShape.num, o.pixelShape.den)) std::fprintf(stderr, "--pixel-aspect must look like 12:7, or be stella\n");
    }
    else if(a == "--timed") { o.timedSeconds = std::atof(next().c_str()); o.timedReport = next(); }
    else if(a == "--frames") o.frames = std::atoi(next().c_str());
    else if(a == "--save-frame") o.saveFrame = std::atoi(next().c_str());
    else if(a == "--raw") o.rawFile = next();
    else if(a == "--fingerprints") o.fingerprintFile = next();
    else if(a == "--settings") o.settingsFile = next();
    else if(a == "--roms-dir") o.romsDir = next();
    else if(o.romArg.empty()) o.romArg = a;
  }
  const char* base = SDL_GetBasePath();  // folder of the program, with a trailing slash
  const std::string exeDir = base ? base : "";
  if(o.settingsFile.empty()) o.settingsFile = exeDir + "settings.ini";
  if(o.romsDir.empty()) o.romsDir = exeDir + "roms";
  return o;
}

// --find-rom: the quiet search, no window. Exit code 0 = good ROM found, 4 = a problem with the ROM, 5 = nothing found.
int findRomReport(const Options& o)
{
  std::vector<std::string> log;
  const auto found = rom::findQuietly(o.romArg, o.settingsFile, o.romsDir, &log);
  std::ofstream out(o.findRomFile);
  if(!out) return 1;
  for(const std::string& line : log) out << "looked: " << line << '\n';
  if(!found) {
    out << "result=none (the Open file dialog would be shown)\n";
    return 5;
  }
  out << "source=" << found->source << "\nstatus=" << static_cast<int>(found->check.status) << "\nsize=" << found->check.size
      << "\nsha1=" << found->check.sha1 << "\nmessage:\n" << rom::describe(found->check) << '\n';
  return found->check.status == rom::Status::Ok ? 0 : 4;
}

std::string fileNameOf(const std::string& path)
{
  const std::size_t cut = path.find_last_of("/\\");
  return cut == std::string::npos ? path : path.substr(cut + 1);
}

// Reads the 4096 ROM bytes of a file that has already passed the checks (read only).
bool readRomBytes(const std::string& path, std::vector<std::uint8_t>& bytes)
{
  bytes.assign(rom::kExpectedSize, 0);
  std::ifstream in(path, std::ios::binary);
  return static_cast<bool>(in.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size())));
}

// The game picture, enlarged to its correct shape and centred in the window (Stage D6, T-D6-1).
struct Screen {
  SDL_Renderer* renderer = nullptr;
  SDL_Texture* texture = nullptr;
  layout::Rect where{};  // where the picture was drawn in the window, in window pixels
  int shownTop = 0, shownHeight = 0;  // which rows of the 228-row picture are shown (all of them, or the game area)
  View view;
  ~Screen() { if(texture) SDL_DestroyTexture(texture); }

  bool show(const std::vector<std::uint32_t>& pixels, int height)
  {
    if(!texture) {
      texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_STREAMING, Emulator::kWidth, height);
      if(!texture) return false;
      SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_PIXELART);  // sharp pixels, only their edges are blended when the scale is not whole
    }
    int windowW = 0, windowH = 0;
    SDL_GetRenderOutputSize(renderer, &windowW, &windowH);
    // The pixel shape always comes from the whole picture (Stella's); Game area only leaves out the always-black rows.
    const layout::PixelShape shape = view.shape.num > 0 ? view.shape : layout::stellaShape(Emulator::kWidth, height);
    const bool crop = (view.mode == layout::Mode::GameArea || view.mode == layout::Mode::Framed) && layout::kGameBottom <= height;
    const int top = crop ? layout::kGameTop : 0, shownRows = crop ? layout::kGameBottom - layout::kGameTop : height;
    where = layout::pictureRect(windowW, windowH, Emulator::kWidth, shownRows, shape, view.mode);
    const SDL_FRect dst{static_cast<float>(where.x), static_cast<float>(where.y), static_cast<float>(where.w), static_cast<float>(where.h)};
    const SDL_FRect src{0.0f, static_cast<float>(top), static_cast<float>(Emulator::kWidth), static_cast<float>(shownRows)};
    SDL_UpdateTexture(texture, nullptr, pixels.data(), Emulator::kWidth * 4);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_RenderTexture(renderer, texture, &src, &dst);
    shownTop = top;
    shownHeight = shownRows;
    return true;
  }
};

// --layout-check: run 300 frames in the real window, read its pixels back and check that the picture is where the
// layout says: nothing outside its rectangle, and the outline of what is drawn inside it where the arithmetic predicts.
bool layoutCheck(SDL_Renderer* renderer, const std::string& pngPath, const std::string& romPath, const View& view, int resetAt = -1)
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

// ---- Keyboard (Stage D10, DECISIONS.md T-D10-1) ----

// Gives the console what the keyboard adds up to. extraReset is for tests that press Reset by frame number.
void applyInput(Emulator& emulator, const controls::Input& in, bool extraReset)
{
  emulator.setJoystick(in.up, in.down, in.left, in.right, in.fire);
  emulator.setConsoleSelect(in.select);
  emulator.setConsoleReset(in.reset || extraReset);
  emulator.setDifficulty(in.leftDifficultyA, in.rightDifficultyA);
  emulator.setBlackWhite(in.blackWhite);
}

// The F1 list, drawn over the picture with SDL's built-in text.
void drawHelp(SDL_Renderer* renderer, const controls::Input& current, const std::string& controller, bool softBlend = false)
{
  const std::vector<controls::HelpLine> lines = controls::helpLines(current, controller, softBlend);
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

// --keys-replay: runs the game with no window and presses the keys that a Phase C input script (lines "from to NAME",
// frames counted from 0, both ends included) asks for, in the real keyboard code, so the recorded Phase C runs can be
// compared frame by frame with what the keys do. Colour/B&W is the F4 switch (D12).
int keysReplay(const Options& o)
{
  std::ofstream report(o.keysReplayCsv + ".txt");
  const auto found = rom::findQuietly(o.romArg, o.settingsFile, o.romsDir);
  std::vector<std::uint8_t> bytes;
  if(!report || !found || found->check.status != rom::Status::Ok || !readRomBytes(found->check.path, bytes)) { if(report) report << "result=no usable ROM\n"; return 5; }
  Emulator emulator;
  if(const std::string error = emulator.load(fileNameOf(found->check.path), bytes); !error.empty()) { report << "result=" << error << '\n'; return 6; }

  struct Span { int from, to; std::string name; };
  std::vector<Span> spans;
  {
    std::ifstream in(o.keysReplayScript);
    if(!in) { report << "result=cannot read the script\n"; return 6; }
    for(std::string line; std::getline(in, line); ) {
      if(line.empty() || line[0] == '#' || line.rfind("dump", 0) == 0) continue;
      std::istringstream ls(line);
      Span sp{};
      if(ls >> sp.from >> sp.to >> sp.name) spans.push_back(sp);
    }
  }
  int last = 0;
  for(const Span& sp : spans) last = std::max(last, sp.to);
  const int frames = o.frames == 600 ? last + 1 : o.frames;  // --frames not given: run to the end of the script

  const struct { const char* name; controls::Action action; } held[] = {
      {"UP", controls::Action::Up}, {"DOWN", controls::Action::Down}, {"LEFT", controls::Action::Left}, {"RIGHT", controls::Action::Right},
      {"FIRE", controls::Action::Fire}, {"SELECT", controls::Action::Select}, {"RESET", controls::Action::Reset}};
  controls::Keyboard keyboard;
  std::ofstream csv(o.keysReplayCsv);
  csv << "frame,hash\n";
  char hash[17];
  for(int f = 0; f < frames; ++f) {
    std::set<std::string> active;
    for(const Span& sp : spans)
      if(f >= sp.from && f <= sp.to) active.insert(sp.name);
    for(const auto& h : held) {
      const SDL_Scancode key = controls::primaryKey(h.action);
      if(active.count(h.name)) keyboard.keyDown(key, false); else keyboard.keyUp(key);
    }
    // The difficulty switches are real switches: a script asking for A or B when it is not there yet presses the key once.
    auto flip = [&](bool wantA, bool isA, controls::Action action) {
      if(wantA == isA) return;
      keyboard.keyDown(controls::primaryKey(action), false);
      keyboard.keyUp(controls::primaryKey(action));
    };
    if(active.count("LDIFF_A")) flip(true, keyboard.input().leftDifficultyA, controls::Action::LeftDifficulty);
    if(active.count("LDIFF_B")) flip(false, keyboard.input().leftDifficultyA, controls::Action::LeftDifficulty);
    if(active.count("RDIFF_A")) flip(true, keyboard.input().rightDifficultyA, controls::Action::RightDifficulty);
    if(active.count("RDIFF_B")) flip(false, keyboard.input().rightDifficultyA, controls::Action::RightDifficulty);
    // The Colour / B&W switch is a real switch too (F4 flips it).
    if(active.count("COLOR")) flip(false, keyboard.input().blackWhite, controls::Action::TvType);
    if(active.count("BW")) flip(true, keyboard.input().blackWhite, controls::Action::TvType);

    applyInput(emulator, keyboard.input(), false);
    emulator.runFrame();
    std::snprintf(hash, sizeof hash, "%016llx", static_cast<unsigned long long>(emulator.pictureFingerprint()));
    csv << f << ',' << hash << '\n';
  }
  char swchb[8];
  std::snprintf(swchb, sizeof swchb, "%02x", emulator.consoleSwitches());
  report << "script=" << o.keysReplayScript << "\nframes=" << frames << "\nlast_frame_fingerprint=" << hash << "\nconsole_switches_at_end=" << swchb << "\nresult=OK\n";
  return 0;
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

// ---- Gamepads (Stage D11, DECISIONS.md T-D11-1) ----

// All the gamepads SDL knows, opened when they appear and closed when they go. What they are doing is read from them
// each time round the loop (not remembered from events), so an unplugged pad can never leave a button "held".
class Pads {
public:
  Pads() = default;
  ~Pads() { stop(); }
  Pads(const Pads&) = delete;
  Pads& operator=(const Pads&) = delete;

  bool start()
  {
    if(started_) return true;
    if(!SDL_InitSubSystem(SDL_INIT_GAMEPAD)) { problem_ = SDL_GetError(); return false; }
    started_ = true;
    int count = 0;
    SDL_JoystickID* ids = SDL_GetGamepads(&count);
    for(int i = 0; ids && i < count; ++i) open(ids[i]);
    SDL_free(ids);
    return true;
  }

  void stop()
  {
    for(Pad& p : pads_) SDL_CloseGamepad(p.gamepad);
    pads_.clear();
    if(started_) { SDL_QuitSubSystem(SDL_INIT_GAMEPAD); started_ = false; }
  }

  void handleEvent(const SDL_Event& e)
  {
    if(e.type == SDL_EVENT_GAMEPAD_ADDED) open(e.gdevice.which);
    if(e.type == SDL_EVENT_GAMEPAD_REMOVED) close(e.gdevice.which);
  }

  // While the window is not in front, the pads do nothing (the keyboard is released the same way).
  void setActive(bool active) { active_ = active; }

  int count() const { return static_cast<int>(pads_.size()); }

  std::string names() const
  {
    if(pads_.empty()) return "none";
    std::string text;
    for(const Pad& p : pads_) text += (text.empty() ? "" : ", ") + p.name;
    return text;
  }

  // What all the open pads together are doing now.
  controls::PadState poll()
  {
    controls::PadState total;
    if(!active_) return total;
    for(Pad& p : pads_) {
      controls::PadReading r;
      r.dpadUp = SDL_GetGamepadButton(p.gamepad, SDL_GAMEPAD_BUTTON_DPAD_UP);
      r.dpadDown = SDL_GetGamepadButton(p.gamepad, SDL_GAMEPAD_BUTTON_DPAD_DOWN);
      r.dpadLeft = SDL_GetGamepadButton(p.gamepad, SDL_GAMEPAD_BUTTON_DPAD_LEFT);
      r.dpadRight = SDL_GetGamepadButton(p.gamepad, SDL_GAMEPAD_BUTTON_DPAD_RIGHT);
      r.cross = SDL_GetGamepadButton(p.gamepad, SDL_GAMEPAD_BUTTON_SOUTH);
      r.square = SDL_GetGamepadButton(p.gamepad, SDL_GAMEPAD_BUTTON_WEST);
      r.options = SDL_GetGamepadButton(p.gamepad, SDL_GAMEPAD_BUTTON_START);
      r.share = SDL_GetGamepadButton(p.gamepad, SDL_GAMEPAD_BUTTON_BACK);
      r.stickX = SDL_GetGamepadAxis(p.gamepad, SDL_GAMEPAD_AXIS_LEFTX);
      r.stickY = SDL_GetGamepadAxis(p.gamepad, SDL_GAMEPAD_AXIS_LEFTY);
      total = controls::combine(total, controls::padState(r, p.stick));
    }
    return total;
  }

private:
  struct Pad {
    SDL_Gamepad* gamepad = nullptr;
    SDL_JoystickID id = 0;
    std::string name;
    controls::StickDirection stick;
  };

  void open(SDL_JoystickID id)
  {
    for(const Pad& p : pads_)
      if(p.id == id) return;  // already open (it was found at start-up and the "added" event came too)
    SDL_Gamepad* g = SDL_OpenGamepad(id);
    if(!g) return;
    Pad p;
    p.gamepad = g;
    p.id = id;
    const char* name = SDL_GetGamepadName(g);
    p.name = name ? name : "gamepad";
    pads_.push_back(std::move(p));
  }

  void close(SDL_JoystickID id)
  {
    for(auto it = pads_.begin(); it != pads_.end(); ++it)
      if(it->id == id) { SDL_CloseGamepad(it->gamepad); pads_.erase(it); return; }
  }

  std::vector<Pad> pads_;
  bool started_ = false, active_ = true;
  std::string problem_;
};

// --pad-check: a virtual gamepad (SDL's own test device, which goes through the same gamepad code as a real pad) plays the
// real game through the same Pads code the window uses. Every button and the stick are checked against the game's own
// memory and the console switches; then the pad is unplugged while the stick is held and plugged in again.
bool padCheck(const Options& o)
{
  std::ofstream report(o.padCheckFile);
  const auto found = rom::findQuietly(o.romArg, o.settingsFile, o.romsDir);
  std::vector<std::uint8_t> bytes;
  if(!report || !found || found->check.status != rom::Status::Ok || !readRomBytes(found->check.path, bytes)) { if(report) report << "result=no usable ROM\n"; return false; }
  Emulator emulator;
  if(const std::string error = emulator.load(fileNameOf(found->check.path), bytes); !error.empty()) { report << "result=" << error << '\n'; return false; }

  bool ok = true;
  auto line = [&](const std::string& name, bool pass, const std::string& detail) {
    report << name << (pass ? ": ok  " : ": FAIL  ") << detail << '\n';
    ok = ok && pass;
  };

  Pads pads;
  if(!pads.start()) { report << "result=FAIL gamepads could not start\n"; return false; }
  report << "real_gamepads_found=" << pads.count() << " (" << pads.names() << ")\n";
  const int realPads = pads.count();

  SDL_VirtualJoystickDesc desc;
  SDL_INIT_INTERFACE(&desc);
  desc.type = SDL_JOYSTICK_TYPE_GAMEPAD;
  desc.naxes = SDL_GAMEPAD_AXIS_COUNT;
  desc.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
  desc.name = "Test pad (virtual)";

  SDL_JoystickID virtualId = 0;
  SDL_Joystick* joystick = nullptr;
  auto plugIn = [&]() {
    virtualId = SDL_AttachVirtualJoystick(&desc);
    joystick = virtualId ? SDL_OpenJoystick(virtualId) : nullptr;
    return joystick != nullptr;
  };
  auto button = [&](SDL_GamepadButton b, bool down) { if(joystick) SDL_SetJoystickVirtualButton(joystick, b, down); };
  auto axis = [&](SDL_GamepadAxis a, int value) { if(joystick) SDL_SetJoystickVirtualAxis(joystick, a, static_cast<Sint16>(value)); };

  controls::Input lastInput;
  int lastEd = 0, lastSwitches = 0;
  std::set<int> switchesSeen;
  // Runs frames, taking the pad from the real Pads code each frame; returns nothing, leaves the last readings behind.
  auto run = [&](int frames) {
    for(int f = 0; f < frames; ++f) {
      for(SDL_Event e; SDL_PollEvent(&e); ) pads.handleEvent(e);
      SDL_UpdateJoysticks();
      lastInput = controls::merge(controls::Input{}, pads.poll());
      applyInput(emulator, lastInput, false);
      emulator.runFrame();
      lastEd = emulator.ram()[0xED - 0x80];  // C6: up 1, down 2, left 4, right 8
      lastSwitches = emulator.consoleSwitches();
      switchesSeen.insert(lastSwitches);
    }
  };
  auto hex = [](int v) { char t[8]; std::snprintf(t, sizeof t, "%02x", v); return std::string(t); };
  // A fresh game with Pac-Man alive and free to move (a ghost catches an idle Pac-Man about 420 frames after a game starts,
  // docs\SOUND.md, and the game then stops reading the joystick): Options, then the start tune.
  auto newGame = [&]() {
    button(SDL_GAMEPAD_BUTTON_START, true);
    run(10);
    button(SDL_GAMEPAD_BUTTON_START, false);
    run(190);
  };

  line("plug in a virtual gamepad", plugIn(), SDL_GetError());
  run(30);
  line("the program opened it", pads.count() == realPads + 1, "pads open: " + std::to_string(pads.count()) + " (" + pads.names() + ")");

  run(60);  // attract mode
  // Share = Game Select: the select switch (bit 1 of the switch register) goes low while held
  button(SDL_GAMEPAD_BUTTON_BACK, true);
  run(8);
  line("Share = Game Select", lastInput.select && (lastSwitches & 0x02) == 0, "switch register " + hex(lastSwitches));
  button(SDL_GAMEPAD_BUTTON_BACK, false);
  run(30);
  // Options = Game Reset: the reset switch (bit 0) goes low while held, and a game starts
  button(SDL_GAMEPAD_BUTTON_START, true);
  run(10);
  line("Options = Game Reset", lastInput.reset && (lastSwitches & 0x01) == 0, "switch register " + hex(lastSwitches));
  button(SDL_GAMEPAD_BUTTON_START, false);
  run(200);  // the start tune plays, then Pac-Man can move
  line("both released: switches back", (lastSwitches & 0x03) == 0x03, "switch register " + hex(lastSwitches));

  line("nothing pressed: joystick byte 0", lastEd == 0, "RAM $ED = " + std::to_string(lastEd));
  // D-pad and stick, all four directions each (the game's joystick byte: up 1, down 2, left 4, right 8)
  const struct { SDL_GamepadButton pad; int axisValue; bool vertical; int expect; const char* name; } dirs[] = {
      {SDL_GAMEPAD_BUTTON_DPAD_UP, -32767, true, 1, "up"}, {SDL_GAMEPAD_BUTTON_DPAD_DOWN, 32767, true, 2, "down"},
      {SDL_GAMEPAD_BUTTON_DPAD_LEFT, -32767, false, 4, "left"}, {SDL_GAMEPAD_BUTTON_DPAD_RIGHT, 32767, false, 8, "right"}};
  for(const auto& d : dirs) {
    newGame();  // Pac-Man must be alive for each direction
    button(d.pad, true);
    run(6);
    line(std::string("D-pad ") + d.name, lastEd == d.expect, "RAM $ED = " + std::to_string(lastEd) + " (wanted " + std::to_string(d.expect) + ")");
    button(d.pad, false);
    run(6);
    axis(d.vertical ? SDL_GAMEPAD_AXIS_LEFTY : SDL_GAMEPAD_AXIS_LEFTX, d.axisValue);
    run(6);
    line(std::string("left stick ") + d.name, lastEd == d.expect, "RAM $ED = " + std::to_string(lastEd) + " (wanted " + std::to_string(d.expect) + ")");
    axis(d.vertical ? SDL_GAMEPAD_AXIS_LEFTY : SDL_GAMEPAD_AXIS_LEFTX, 0);
    run(6);
    line(std::string("stick back to the middle after ") + d.name, lastEd == 0, "RAM $ED = " + std::to_string(lastEd));
  }
  newGame();
  // The dead zone, and a crooked push (mostly right, a little up: one direction, right)
  axis(SDL_GAMEPAD_AXIS_LEFTX, 8000);
  run(6);
  line("stick at 25 %: inside the dead zone", lastEd == 0, "RAM $ED = " + std::to_string(lastEd));
  axis(SDL_GAMEPAD_AXIS_LEFTX, 30000);
  axis(SDL_GAMEPAD_AXIS_LEFTY, -18000);
  run(6);
  line("crooked push (right 92 %, up 55 %): right only", lastEd == 8, "RAM $ED = " + std::to_string(lastEd));
  axis(SDL_GAMEPAD_AXIS_LEFTX, 0);
  axis(SDL_GAMEPAD_AXIS_LEFTY, 0);
  run(6);
  button(SDL_GAMEPAD_BUTTON_SOUTH, true);
  run(2);
  line("Cross = fire", lastInput.fire, "");
  button(SDL_GAMEPAD_BUTTON_SOUTH, false);
  button(SDL_GAMEPAD_BUTTON_WEST, true);
  run(2);
  line("Square = fire", lastInput.fire, "");
  button(SDL_GAMEPAD_BUTTON_WEST, false);
  run(2);

  // Unplug with the stick held right: the direction must go with it
  newGame();
  axis(SDL_GAMEPAD_AXIS_LEFTX, 32767);
  run(10);
  line("stick held right before unplugging", lastEd == 8, "RAM $ED = " + std::to_string(lastEd));
  SDL_CloseJoystick(joystick);
  joystick = nullptr;
  SDL_DetachVirtualJoystick(virtualId);
  run(10);
  line("unplugged: the program closed the pad", pads.count() == realPads, "pads open: " + std::to_string(pads.count()));
  line("unplugged: nothing stays pressed", lastEd == 0 && !lastInput.right, "RAM $ED = " + std::to_string(lastEd));
  run(120);
  line("still running 2 seconds later", lastEd == 0, "RAM $ED = " + std::to_string(lastEd));

  // Plug in again
  line("plug in again", plugIn(), SDL_GetError());
  run(30);
  line("the program opened it again", pads.count() == realPads + 1, "pads open: " + std::to_string(pads.count()));
  newGame();
  axis(SDL_GAMEPAD_AXIS_LEFTX, -32767);
  run(8);
  line("works after plugging in again: left", lastEd == 4, "RAM $ED = " + std::to_string(lastEd));
  axis(SDL_GAMEPAD_AXIS_LEFTX, 0);
  run(4);

  // While the window is not in front the pad is ignored
  newGame();
  axis(SDL_GAMEPAD_AXIS_LEFTY, -32767);
  pads.setActive(false);
  run(6);
  line("window not in front: pad ignored", lastEd == 0, "RAM $ED = " + std::to_string(lastEd));
  pads.setActive(true);
  run(6);
  line("window in front again: pad counts at once", lastEd == 1, "RAM $ED = " + std::to_string(lastEd));
  axis(SDL_GAMEPAD_AXIS_LEFTY, 0);

  if(joystick) { SDL_CloseJoystick(joystick); joystick = nullptr; SDL_DetachVirtualJoystick(virtualId); }
  report << "console_switch_values_seen=";
  for(int v : switchesSeen) report << hex(v) << ' ';
  report << "\nresult=" << (ok ? "OK" : "FAIL") << '\n';
  return ok;
}

// ---- Frame pacing (Stage D9, DECISIONS.md T-D9-1 and T-D9-2; the authentic speed: D-004 and T-D14-2) ----

// A real NTSC Atari 2600 runs from the colour clock of 3,579,545.45 Hz: 228 colour clocks a line, 262 lines a frame =
// 59.9227 frames a second, and its sound chip makes 31,399.5 samples a second. Stella plays at exactly 60 frames and
// 31,440 samples (0.13 % fast). The port follows the console (the owner's rule: authentic first); --stella-speed brings
// back Stella's speed for comparison. The emulation itself is counted in CPU cycles and does not change.
constexpr double kConsoleFrameHz = 3579545.4545 / (228.0 * 262.0);

double frameRate(const Options& o) { return o.stellaSpeed ? 60.0 : kConsoleFrameHz; }

// The rate the sound device is told the emulator's samples have: the sound chip's own rate, which sets pitch and speed.
int soundRate(const Options& o, const Emulator& emulator)
{
  return o.stellaSpeed ? emulator.audioRate() : static_cast<int>(std::lround(emulator.audioRate() * kConsoleFrameHz / 60.0));
}

// Decides when the next game frame is due. Frames are started from a steady PC timer (so they are evenly spaced, which a
// monitor needs to show each one for the same number of refreshes), but the timer's speed is steered by the sound card:
// when more sound is waiting than the target the next frames are started a little later, when less, a little sooner.
// So on average the game runs exactly as fast as the sound card plays (59.92 frames a second for this game: 524 sound
// pairs per frame at 31,400 per second), and music and picture cannot drift apart. Without a sound device, or with
// --timer-clock, the steering is simply off and the PC's clock is used.
class Pacer {
public:
  Pacer(const AudioOutput& audio, bool timerOnly, double frameHz)
      : audio_(audio), useAudio_(audio.isOpen() && !timerOnly), frameNs_(1e9 / frameHz), next_(SDL_GetTicksNS()) {}

  bool due(Uint64 now) const { return now >= next_; }

  // Nanoseconds until the next frame is due (0 if it is due now).
  Uint64 untilDue(Uint64 now) const { return now >= next_ ? 0 : next_ - now; }

  void ran(Uint64 now)
  {
    double correction = 0;  // fraction by which this frame's period is lengthened (waiting sound above target) or shortened
    if(useAudio_) {
      const double queued = audio_.queuedPairs();
      smoothed_ = frames_ == 0 ? queued : smoothed_ + 0.02 * (queued - smoothed_);  // the device takes sound in chunks: average them out
      correction = std::clamp(kGain * (smoothed_ - audio_.targetPairs()), -kMaxCorrection, kMaxCorrection);
      correctionSum_ += correction;
    }
    ++frames_;
    next_ += static_cast<Uint64>(frameNs_ * (1.0 + correction));
    if(now > next_ + static_cast<Uint64>(5 * frameNs_)) next_ = now;  // far behind (the PC was busy): do not try to catch up
  }

  bool usesSoundClock() const { return useAudio_; }
  double averageCorrectionPpm() const { return frames_ ? 1e6 * correctionSum_ / static_cast<double>(frames_) : 0.0; }

private:
  static constexpr double kGain = 4e-6;           // period change per sound pair above target (a time constant of about 8 s)
  static constexpr double kMaxCorrection = 1e-3;  // never more than 0.1 %
  const AudioOutput& audio_;
  bool useAudio_;
  double frameNs_;  // one frame of the console's clock, in nanoseconds
  Uint64 next_;
  double smoothed_ = 0, correctionSum_ = 0;
  long long frames_ = 0;
};

// Sleeps until the time given (nanoseconds from now), in small steps and ending with a precise wait, so that frames start
// within a fraction of a millisecond of when they are due.
void waitFor(Uint64 ns)
{
  if(ns > 2000000) ns = 1000000;  // look again after 1 ms: the clock may have been steered, or the program may be closing
  SDL_DelayPrecise(ns);
}

// ---- Sound (Stage D8, DECISIONS.md T-D8-1) ----

std::string audioStatsText(const AudioOutput& audio, double seconds)
{
  const AudioOutput::Stats st = audio.stats();
  char text[1024];
  std::snprintf(text, sizeof text,
                "%s\nseconds=%.1f\npushes=%lld\npushed_pairs=%lld\npushed_pairs_per_second=%.1f\npadded_pairs=%lld\ndropped_pairs=%lld\n"
                "underruns=%lld\nshortfall_bytes=%lld\nqueued_ms_target=%.1f\nqueued_ms_smallest=%.1f\nqueued_ms_largest=%.1f\n",
                audio.describe().c_str(), seconds, st.pushes, st.pushedFrames, seconds > 0 ? static_cast<double>(st.pushedFrames) / seconds : 0.0,
                st.paddedFrames, st.droppedFrames, st.underruns, st.shortfallBytes, st.targetMs, st.minQueuedMs, st.maxQueuedMs);
  return text;
}

// --audio-check: the game runs at 60 frames per second (no window) and its sound goes to the real sound device.
bool audioCheck(const Options& o)
{
  std::ofstream report(o.audioCheckFile);
  if(!report) return false;
  const auto found = rom::findQuietly(o.romArg, o.settingsFile, o.romsDir);
  std::vector<std::uint8_t> bytes;
  if(!found || found->check.status != rom::Status::Ok || !readRomBytes(found->check.path, bytes)) { report << "result=no usable ROM\n"; return false; }
  Emulator emulator;
  if(const std::string error = emulator.load(fileNameOf(found->check.path), bytes); !error.empty()) { report << "result=" << error << '\n'; return false; }

  AudioOutput audio;
  if(!audio.open(soundRate(o, emulator), o.mute)) { report << "result=FAIL " << audio.problem() << '\n'; return false; }

  Pacer pacer(audio, o.timerClock, frameRate(o));  // the same pacing as the game window
  const Uint64 started = SDL_GetTicksNS();
  long long frames = 0;
  bool stalled = false;
  while(static_cast<double>(SDL_GetTicksNS() - started) < o.audioCheckSeconds * 1e9) {
    if(o.stallMs > 0 && !stalled && static_cast<double>(SDL_GetTicksNS() - started) >= 5e9) { SDL_Delay(static_cast<Uint32>(o.stallMs)); stalled = true; }
    const Uint64 now = SDL_GetTicksNS();
    int steps = 0;
    while(steps < 5 && pacer.due(now)) {
      emulator.setConsoleReset(o.resetAt >= 0 && frames >= o.resetAt && frames < o.resetAt + 10);
      emulator.runFrame();
      audio.push(emulator.audio(), emulator.audioFrames());
      pacer.ran(now); ++steps; ++frames;
    }
    waitFor(pacer.untilDue(SDL_GetTicksNS()));
  }
  const double seconds = static_cast<double>(SDL_GetTicksNS() - started) / 1e9;
  const AudioOutput::Stats st = audio.stats();
  report << audioStatsText(audio, seconds) << "clock=" << (pacer.usesSoundClock() ? "sound card" : "PC timer") << "\naverage_clock_correction_ppm=" << pacer.averageCorrectionPpm() << "\nframes=" << frames << "\nframes_per_second=" << frames / seconds << '\n';
  const bool ok = st.underruns == 0 && st.pushedFrames > 0;
  report << "result=" << (ok ? "OK" : "CHECK underruns or no sound pushed") << '\n';
  return ok;
}

// ---- Window state and fullscreen (Stage D7, DECISIONS.md T-D7-1) ----

// The monitors' usable rectangles (without the taskbar), the first being the primary one.
std::vector<windowstate::Area> monitorAreas()
{
  std::vector<windowstate::Area> areas;
  int count = 0;
  SDL_DisplayID* ids = SDL_GetDisplays(&count);
  const SDL_DisplayID primary = SDL_GetPrimaryDisplay();
  auto add = [&](SDL_DisplayID id) {
    SDL_Rect r{};
    if(SDL_GetDisplayUsableBounds(id, &r) || SDL_GetDisplayBounds(id, &r)) areas.push_back({r.x, r.y, r.w, r.h});
  };
  if(primary) add(primary);
  for(int i = 0; ids && i < count; ++i)
    if(ids[i] != primary) add(ids[i]);
  SDL_free(ids);
  return areas;
}

// What the program knows about its window: the rectangle it has when not fullscreen, and whether it is fullscreen.
struct Tracker {
  windowstate::State state;  // state.known becomes true once a windowed rectangle has been seen
};

bool windowIsFree(SDL_Window* window)  // neither fullscreen, maximized nor minimized: its rectangle is the "windowed" one
{
  const SDL_WindowFlags f = SDL_GetWindowFlags(window);
  return (f & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_MAXIMIZED | SDL_WINDOW_MINIMIZED)) == 0;
}

void rememberWindowedRect(SDL_Window* window, Tracker& t)
{
  if(!windowIsFree(window)) return;
  windowstate::Area a;
  SDL_GetWindowPosition(window, &a.x, &a.y);
  SDL_GetWindowSize(window, &a.w, &a.h);
  if(a.w < windowstate::kMinW || a.h < windowstate::kMinH) return;
  t.state.area = a;
  t.state.known = true;
}

void setFullscreen(SDL_Window* window, Tracker& t, bool on)
{
  if(on) rememberWindowedRect(window, t);  // before the window jumps to the monitor's size
  t.state.fullscreen = on;
  SDL_SetWindowFullscreenMode(window, nullptr);  // no mode: borderless fullscreen at the desktop's own size
  SDL_SetWindowFullscreen(window, on);
  if(on) SDL_HideCursor(); else SDL_ShowCursor();
}

// Events about the window itself, and Alt+Enter; shared by the game window and the checks. True if the event was used.
bool handleWindowEvent(SDL_Window* window, Tracker& t, const SDL_Event& e)
{
  switch(e.type) {
    case SDL_EVENT_WINDOW_MOVED:
    case SDL_EVENT_WINDOW_RESIZED:
    case SDL_EVENT_WINDOW_RESTORED:
      rememberWindowedRect(window, t);
      return true;
    case SDL_EVENT_KEY_DOWN:
      if(!e.key.repeat && (e.key.key == SDLK_RETURN || e.key.key == SDLK_KP_ENTER) && (e.key.mod & SDL_KMOD_ALT)) {
        setFullscreen(window, t, !t.state.fullscreen);
        return true;
      }
      return false;
    default:
      return false;
  }
}

// A made-up Alt+Enter, handled by the same code as the real key.
SDL_Event altEnter()
{
  SDL_Event e{};
  e.type = SDL_EVENT_KEY_DOWN;
  e.key.key = SDLK_RETURN;
  e.key.mod = SDL_KMOD_LALT;
  return e;
}

// Lets the window system finish (fullscreen changes are asynchronous) and feeds the events to the tracker.
void settle(SDL_Window* window, Tracker& t, int milliseconds = 400)
{
  SDL_SyncWindow(window);
  for(int waited = 0; waited < milliseconds; waited += 10) {
    for(SDL_Event e; SDL_PollEvent(&e); ) handleWindowEvent(window, t, e);
    SDL_Delay(10);
  }
  SDL_SyncWindow(window);
}

std::string where(int w, int h, int x, int y)
{
  return std::to_string(w) + "x" + std::to_string(h) + " at " + std::to_string(x) + "," + std::to_string(y);
}

// --fullscreen-check: the real window goes to an odd size and place, into fullscreen (as Alt+Enter does), is checked there,
// comes back, and goes into fullscreen again; the state is saved as the program does when it closes.
bool fullscreenCheck(SDL_Window* window, SDL_Renderer* renderer, const Options& o, const std::string& romPath)
{
  std::ofstream report(o.fullscreenCheckFile);
  if(!report) return false;
  bool ok = true;
  auto line = [&](const char* name, bool pass, const std::string& detail) {
    report << name << (pass ? ": ok  " : ": FAIL  ") << detail << '\n';
    ok = ok && pass;
  };
  Tracker t;

  constexpr int oddX = 123, oddY = 87, oddW = 1011, oddH = 757;
  SDL_SetWindowSize(window, oddW, oddH);
  SDL_SetWindowPosition(window, oddX, oddY);
  settle(window, t);
  int x = 0, y = 0, w = 0, h = 0;
  SDL_GetWindowPosition(window, &x, &y);
  SDL_GetWindowSize(window, &w, &h);
  const int winX = x, winY = y, winW = w, winH = h;  // what Windows really gave (it may adjust a little)
  line("odd windowed size and place", w == oddW && h == oddH && std::abs(x - oddX) <= 8 && std::abs(y - oddY) <= 40, where(w, h, x, y));
  rememberWindowedRect(window, t);

  const SDL_Event first = altEnter();
  handleWindowEvent(window, t, first);
  settle(window, t);
  SDL_Rect bounds{};
  SDL_GetDisplayBounds(SDL_GetDisplayForWindow(window), &bounds);
  int pw = 0, ph = 0;
  SDL_GetWindowSizeInPixels(window, &pw, &ph);
  SDL_GetWindowPosition(window, &x, &y);
  const bool fullFlag = (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) != 0;
  line("Alt+Enter enters fullscreen", fullFlag && t.state.fullscreen, fullFlag ? "window flag set" : "window flag not set");
  line("borderless (no video mode change)", SDL_GetWindowFullscreenMode(window) == nullptr, "fullscreen mode = desktop");
  line("covers the whole monitor", x == bounds.x && y == bounds.y && pw == bounds.w && ph == bounds.h,
       where(pw, ph, x, y) + "; monitor " + where(bounds.w, bounds.h, bounds.x, bounds.y));
  line("windowed place remembered while fullscreen", t.state.area.w == winW && t.state.area.h == winH && t.state.area.x == winX && t.state.area.y == winY,
       where(t.state.area.w, t.state.area.h, t.state.area.x, t.state.area.y));

  View view;
  view.mode = o.mode;
  view.shape = o.pixelShape;
  const bool pictureOk = layoutCheck(renderer, o.fullscreenCheckFile + ".fullscreen.png", romPath, view, o.resetAt);
  line("picture in fullscreen (D6 check, details in .fullscreen.png.txt)", pictureOk, "");

  const SDL_Event second = altEnter();
  handleWindowEvent(window, t, second);
  settle(window, t);
  SDL_GetWindowPosition(window, &x, &y);
  SDL_GetWindowSize(window, &w, &h);
  line("Alt+Enter leaves fullscreen", (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) == 0 && !t.state.fullscreen, "");
  line("the odd size and place came back", w == winW && h == winH && x == winX && y == winY, where(w, h, x, y));

  const SDL_Event third = altEnter();
  handleWindowEvent(window, t, third);
  settle(window, t);
  line("fullscreen again", t.state.fullscreen && (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) != 0, "");
  line("state saved", windowstate::save(o.settingsFile, t.state),
       "window " + where(t.state.area.w, t.state.area.h, t.state.area.x, t.state.area.y) + ", fullscreen " + (t.state.fullscreen ? "on" : "off"));
  report << "result=" << (ok ? "OK" : "FAIL") << '\n';
  return ok;
}

// --state-report: what the window looks like right after start-up from the saved state, then after leaving fullscreen.
bool stateReport(SDL_Window* window, const Options& o, const windowstate::State& wanted)
{
  std::ofstream report(o.stateReportFile);
  if(!report) return false;
  Tracker t;
  t.state = wanted;
  settle(window, t);
  int x = 0, y = 0, w = 0, h = 0;
  SDL_GetWindowPosition(window, &x, &y);
  SDL_GetWindowSize(window, &w, &h);
  const bool full = (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) != 0;
  report << "saved_state_found=" << (wanted.known ? "yes" : "no") << "\nsaved=" << where(wanted.area.w, wanted.area.h, wanted.area.x, wanted.area.y)
         << " fullscreen " << (wanted.fullscreen ? "on" : "off") << "\nstarted_fullscreen=" << (full ? "yes" : "no")
         << "\nstarted_window=" << where(w, h, x, y) << '\n';
  bool ok = full == wanted.fullscreen;
  if(full) {
    const SDL_Event leave = altEnter();
    handleWindowEvent(window, t, leave);
    settle(window, t);
    SDL_GetWindowPosition(window, &x, &y);
    SDL_GetWindowSize(window, &w, &h);
    report << "after_leaving_fullscreen=" << where(w, h, x, y) << '\n';
  }
  if(wanted.known) ok = ok && w == wanted.area.w && h == wanted.area.h && x == wanted.area.x && y == wanted.area.y;
  else ok = !full && w == kWindowW && h == kWindowH;  // nothing remembered: the program's own first window
  report << "result=" << (ok ? "OK" : "FAIL") << '\n';
  return ok;
}

// The game window: one emulator frame per 1/60 s from a simple clock (temporary; real pacing is Stage D9).
// With maxSeconds > 0 it stops by itself after that long and returns the frames per second it managed (for tests).
// Keys 1, 2, 3, 4 choose Sharp, Fill, TV frame, Game area; Alt+Enter toggles fullscreen (temporary; Stage D10 gives the keys their final home).
double playWindow(SDL_Window* window, SDL_Renderer* renderer, const std::string& romPath, double maxSeconds, View view, Tracker& tracker, const Options& options)
{
  std::vector<std::uint8_t> bytes;
  Emulator emulator;
  if(!readRomBytes(romPath, bytes)) { SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, kTitle, "The ROM file could not be read.", window); return 0; }
  if(const std::string error = emulator.load(fileNameOf(romPath), bytes); !error.empty()) {
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, kTitle, error.c_str(), window);
    return 0;
  }
  const std::string romSha1 = rom::sha1Hex(bytes.data(), bytes.size());
  const bool replaying = !options.replayFile.empty();
  inputlog::Recording replay;  // --replay: what the console is given on each frame, instead of the keyboard and pads
  if(replaying) {
    std::string error;
    if(!inputlog::load(options.replayFile, replay, error)) { SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, kTitle, error.c_str(), window); return 0; }
    if(replay.romSha1 != romSha1) { SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, kTitle, "This recording was made with a different ROM than the one that is loaded.", window); return 0; }
  }
  inputlog::Recorder recorder;  // --record: written when the window closes
  std::ofstream fingerprints;   // --fingerprints: the picture's fingerprint of every frame (tests)
  if(!options.fingerprintFile.empty()) { fingerprints.open(options.fingerprintFile); fingerprints << "frame,hash\n"; }

  AudioOutput audio;
  if(!audio.open(soundRate(options, emulator), options.mute)) std::fprintf(stderr, "%s\nThe game goes on without sound.\n", audio.problem().c_str());

  Screen screen;
  screen.renderer = renderer;
  screen.view = view;
  auto retitle = [&]() {
    const std::string title = std::string(kTitle) + " - " + fileNameOf(romPath) + " - " + modeName(screen.view.mode) + (replaying ? " - REPLAY" : "") + (options.recordFile.empty() ? "" : " - RECORDING") + " - F1 = keys";
    SDL_SetWindowTitle(window, title.c_str());
  };
  retitle();

  // The game and its sound run on their own thread (DECISIONS.md T-D8-2), so that a window that is being dragged or
  // resized, which freezes this thread on Windows, cannot starve the sound card. This thread only shows the pictures.
  std::atomic<bool> stop{false};
  std::atomic<std::uint32_t> inputBits{0};  // what the keyboard adds up to (controls::Input::pack), set here, read by the game thread
  controls::Keyboard keyboard;
  // The three real switches start where they were left (settings.ini; the defaults are B, B and Colour).
  keyboard.setSwitches(settings::get(options.settingsFile, "left_difficulty") == "a", settings::get(options.settingsFile, "right_difficulty") == "a",
                       settings::get(options.settingsFile, "tv_type") == "bw");
  controls::Input savedSwitches = keyboard.input();
  Pads pads;
  if(!pads.start()) std::fprintf(stderr, "Gamepads could not start; the keyboard still works.\n");
  bool helpOn = false;
  // Stella's soft glow (F7): off unless the owner turned it on; remembered in settings.ini as soft_glow=on|off
  std::atomic<bool> softBlend{settings::get(options.settingsFile, "soft_glow") == "on"};
  std::mutex lock;  // guards the shared picture below
  std::vector<std::uint32_t> sharedPixels;
  int sharedHeight = 0;
  long long sharedFrame = 0;  // number of the picture in sharedPixels (frames run so far)
  double longestGapMs = 0;
  int gapsOver30 = 0;
  std::string gapLog;
  bool usedSoundClock = false;
  double averageCorrectionPpm = 0;
  std::uint32_t joystickValuesSeen = 0;      // bit n set = the game's joystick byte (RAM $ED) held the value n
  std::set<int> switchValuesSeen;            // the console switch register values the game could read
  const Uint64 started = SDL_GetTicksNS();

  std::thread game([&]() {
    Pacer pacer(audio, options.timerClock, frameRate(options));
    usedSoundClock = pacer.usesSoundClock();
    Uint64 lastLoop = SDL_GetTicksNS();
    long long frames = 0;
    bool glowApplied = false;  // the emulator starts without the glow
    while(!stop.load()) {
      const Uint64 now = SDL_GetTicksNS();
      const double gap = static_cast<double>(now - lastLoop) / 1e6;  // how long this thread went without running
      lastLoop = now;
      if(gap > 30.0) {
        ++gapsOver30;
        if(gapLog.size() < 400) gapLog += " " + std::to_string(static_cast<int>(gap)) + "ms@frame" + std::to_string(frames);
      }
      longestGapMs = std::max(longestGapMs, gap);

      int steps = 0;
      while(steps < 5 && pacer.due(now)) {
        if(softBlend.load() != glowApplied) { glowApplied = softBlend.load(); emulator.setSoftBlend(glowApplied); }
        // A replay gives the console what was recorded; when it has run out, the keyboard and pads take over again.
        controls::Input in = controls::Input::unpack(replaying && frames < replay.frames ? replay.bitsAt(frames) : inputBits.load());
        if(options.resetAt >= 0 && frames >= options.resetAt && frames < options.resetAt + 10) in.reset = true;
        recorder.add(in.pack());
        applyInput(emulator, in, false);
        emulator.runFrame();
        if(fingerprints) {
          char hash[17];
          std::snprintf(hash, sizeof hash, "%016llx", static_cast<unsigned long long>(emulator.pictureFingerprint()));
          fingerprints << frames << ',' << hash << '\n';
        }
        audio.push(emulator.audio(), emulator.audioFrames());
        if(options.pushKeys) {
          const int joystick = emulator.ram()[0xED - 0x80];  // C6: up 1, down 2, left 4, right 8
          if(joystick < 32) joystickValuesSeen |= 1u << joystick;
          switchValuesSeen.insert(emulator.consoleSwitches());
        }
        pacer.ran(now); ++steps; ++frames;
      }
      if(steps > 0) {
        std::vector<std::uint32_t> picture = emulator.colours();
        std::lock_guard<std::mutex> guard(lock);
        sharedPixels = std::move(picture);
        sharedHeight = emulator.height();
        sharedFrame = frames;
      }
      waitFor(pacer.untilDue(SDL_GetTicksNS()));
    }
    averageCorrectionPpm = pacer.averageCorrectionPpm();
  });

  // Test only (--push-keys): a helper thread puts key presses into this window's event queue, as the operating system would.
  std::thread pusher;
  if(options.pushKeys) {
    pusher = std::thread([&]() {
      auto key = [](SDL_Scancode code, bool down) {
        SDL_Event e{};
        e.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
        e.key.scancode = code;
        e.key.key = SDL_GetKeyFromScancode(code, SDL_KMOD_NONE, false);
        e.key.down = down;
        SDL_PushEvent(&e);
      };
      const struct { double at; SDL_Scancode code; bool down; } plan[] = {
          {0.5, SDL_SCANCODE_F3, true}, {0.8, SDL_SCANCODE_F3, false},          // Game Reset: a game starts
          {4.0, SDL_SCANCODE_RIGHT, true}, {4.6, SDL_SCANCODE_RIGHT, false},    // joystick right (Pac-Man starts moving at about 2 s)
          {5.0, SDL_SCANCODE_A, true}, {5.6, SDL_SCANCODE_A, false},            // left, with the letter key
          {6.0, SDL_SCANCODE_UP, true}, {6.6, SDL_SCANCODE_UP, false},          // up
          {7.0, SDL_SCANCODE_S, true}, {7.6, SDL_SCANCODE_S, false},            // down, with the letter key
          {8.0, SDL_SCANCODE_F5, true}, {8.1, SDL_SCANCODE_F5, false},          // left difficulty to A
          {8.5, SDL_SCANCODE_F6, true}, {8.6, SDL_SCANCODE_F6, false},          // right difficulty to A
          {9.0, SDL_SCANCODE_F4, true}, {9.1, SDL_SCANCODE_F4, false},          // Colour to B&W
          {9.5, SDL_SCANCODE_F7, true}, {9.6, SDL_SCANCODE_F7, false}};         // soft glow on
      const Uint64 t0 = SDL_GetTicksNS();
      for(const auto& step : plan) {
        while(!stop.load() && static_cast<double>(SDL_GetTicksNS() - t0) < step.at * 1e9) SDL_Delay(5);
        if(stop.load()) break;
        key(step.code, step.down);
      }
    });
  }

  std::vector<std::uint32_t> pixels;
  int height = 0;
  bool failed = false, stalled = false;
  // How many monitor refreshes (presents) each game frame was on screen: [0]=1 ... [4]=5 or more; and frames never shown.
  long long refreshHistogram[5] = {0, 0, 0, 0, 0}, framesNeverShown = 0, presents = 0;
  long long shownFrame = 0, shownCount = 0;
  for(bool running = true; running; ) {
    // Test only (--stall-ms): freeze this thread once, as dragging the window does, to show the sound does not depend on it.
    if(options.stallMs > 0 && !stalled && static_cast<double>(SDL_GetTicksNS() - started) >= 5e9) { SDL_Delay(static_cast<Uint32>(options.stallMs)); stalled = true; }
    for(SDL_Event e; SDL_PollEvent(&e); ) {
      if(e.type == SDL_EVENT_QUIT || (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_ESCAPE)) running = false;
      if(handleWindowEvent(window, tracker, e)) continue;
      if(e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat) {
        if(e.key.key == SDLK_1) { screen.view.mode = layout::Mode::Sharp; retitle(); }
        if(e.key.key == SDLK_2) { screen.view.mode = layout::Mode::Fill; retitle(); }
        if(e.key.key == SDLK_3) { screen.view.mode = layout::Mode::TvFrame; retitle(); }
        if(e.key.key == SDLK_4) { screen.view.mode = layout::Mode::GameArea; retitle(); }
        if(e.key.key == SDLK_5) { screen.view.mode = layout::Mode::Framed; retitle(); }
        if(e.key.scancode == SDL_SCANCODE_F7) { softBlend = !softBlend.load(); settings::set(options.settingsFile, "soft_glow", softBlend.load() ? "on" : "off"); }
      }
      if(e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat && e.key.scancode == SDL_SCANCODE_F1) helpOn = !helpOn;
      if(e.type == SDL_EVENT_KEY_DOWN) keyboard.keyDown(e.key.scancode, e.key.repeat);
      if(e.type == SDL_EVENT_KEY_UP) keyboard.keyUp(e.key.scancode);
      if(e.type == SDL_EVENT_WINDOW_FOCUS_LOST) { keyboard.releaseAll(); pads.setActive(false); }  // a key let go while another window had the focus must not stay held
      if(e.type == SDL_EVENT_WINDOW_FOCUS_GAINED) pads.setActive(true);
      pads.handleEvent(e);
    }
    const controls::Input input = controls::merge(keyboard.input(), pads.poll());  // keyboard and pads together
    inputBits = input.pack();
    if(input.leftDifficultyA != savedSwitches.leftDifficultyA || input.rightDifficultyA != savedSwitches.rightDifficultyA || input.blackWhite != savedSwitches.blackWhite) {
      settings::set(options.settingsFile, "left_difficulty", input.leftDifficultyA ? "a" : "b");
      settings::set(options.settingsFile, "right_difficulty", input.rightDifficultyA ? "a" : "b");
      settings::set(options.settingsFile, "tv_type", input.blackWhite ? "bw" : "colour");
      savedSwitches = input;
    }
    long long frameNow = 0;
    {
      std::lock_guard<std::mutex> guard(lock);
      if(!sharedPixels.empty() && sharedFrame != shownFrame) { pixels = sharedPixels; height = sharedHeight; }
      frameNow = sharedFrame;
    }
    if(frameNow != shownFrame) {  // a new game frame: close the count of the previous one
      if(shownFrame > 0) {
        ++refreshHistogram[std::min<long long>(shownCount, 5) - 1];
        framesNeverShown += frameNow - shownFrame - 1;
      }
      shownFrame = frameNow;
      shownCount = 0;
    }
    if(!pixels.empty() && !screen.show(pixels, height)) { failed = true; running = false; }  // redrawn every time: a presented buffer is not kept
    if(helpOn) drawHelp(renderer, input, pads.names(), softBlend.load());
    const Uint64 beforePresent = SDL_GetTicksNS();
    SDL_RenderPresent(renderer);
    ++presents;
    if(frameNow > 0) ++shownCount;
    if(SDL_GetTicksNS() - beforePresent < 500000) SDL_Delay(1);  // a present that does not wait for the monitor (window hidden): do not spin
    if(maxSeconds > 0 && static_cast<double>(SDL_GetTicksNS() - started) >= maxSeconds * 1e9) running = false;
  }
  stop = true;
  game.join();
  if(pusher.joinable()) pusher.join();
  if(!options.recordFile.empty() && !inputlog::save(options.recordFile, recorder.finish(romSha1))) std::fprintf(stderr, "The recording could not be written: %s\n", options.recordFile.c_str());
  if(failed) return 0;

  const double elapsed = static_cast<double>(SDL_GetTicksNS() - started) / 1e9;
  if(audio.isOpen()) {  // a short note on how the sound did, next to the settings (read by whoever tests)
    std::ofstream log(options.settingsFile + ".audio.txt");
    log << audioStatsText(audio, elapsed) << "game_thread_longest_gap_ms=" << longestGapMs << "\ngame_thread_gaps_over_30ms=" << gapsOver30
        << "\ngame_thread_gaps=" << gapLog << "\nclock=" << (usedSoundClock ? "sound card" : "PC timer") << "\naverage_clock_correction_ppm=" << averageCorrectionPpm
        << "\npresents=" << presents << "\npresents_per_second=" << static_cast<double>(presents) / elapsed
        << "\ngame_frames=" << sharedFrame << "\ngame_frames_per_second=" << static_cast<double>(sharedFrame) / elapsed
        << "\nrefreshes_per_game_frame_1=" << refreshHistogram[0] << "\nrefreshes_per_game_frame_2=" << refreshHistogram[1]
        << "\nrefreshes_per_game_frame_3=" << refreshHistogram[2] << "\nrefreshes_per_game_frame_4=" << refreshHistogram[3]
        << "\nrefreshes_per_game_frame_5_or_more=" << refreshHistogram[4] << "\ngame_frames_never_shown=" << framesNeverShown << '\n';
    if(options.pushKeys) {
      log << "joystick_ram_values_seen=";
      for(int n = 0; n < 32; ++n) if(joystickValuesSeen & (1u << n)) log << n << ' ';
      log << "\nconsole_switch_values_seen=";
      for(int v : switchValuesSeen) { char t[8]; std::snprintf(t, sizeof t, "%02x ", v); log << t; }
      log << '\n';
    }
  }
  return static_cast<double>(sharedFrame) / elapsed;
}

// --headless: run the game with no window. Exit code 0 = done, 4 = problem with the ROM, 5 = no ROM found, 6 = other failure.
int headlessRun(const Options& o)
{
  const std::string reportPath = o.headlessPng + ".txt";
  std::ofstream report(reportPath);
  if(!report) return 6;

  const auto found = rom::findQuietly(o.romArg, o.settingsFile, o.romsDir);
  if(!found) { report << "result=no ROM found\n"; return 5; }
  if(found->check.status != rom::Status::Ok) { report << "result=ROM problem\n" << rom::describe(found->check) << '\n'; return 4; }

  std::vector<std::uint8_t> bytes(rom::kExpectedSize);
  {
    std::ifstream in(found->check.path, std::ios::binary);  // read only
    if(!in.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()))) { report << "result=cannot read ROM\n"; return 6; }
  }
  Emulator emulator;
  if(const std::string error = emulator.load(fileNameOf(found->check.path), bytes); !error.empty()) { report << "result=" << error << '\n'; return 6; }

  std::ofstream fingerprints;
  if(!o.fingerprintFile.empty()) { fingerprints.open(o.fingerprintFile); fingerprints << "frame,hash\n"; }

  if(o.ramSeed >= 0) emulator.randomisePowerOnRam(static_cast<std::uint32_t>(o.ramSeed));
  if(o.softGlow) emulator.setSoftBlend(true);

  // --replay: the console is given the recorded input of each frame; without --frames the whole recording is run.
  inputlog::Recording replay;
  const bool replaying = !o.replayFile.empty();
  if(replaying) {
    std::string error;
    if(!inputlog::load(o.replayFile, replay, error)) { report << "result=" << error << '\n'; return 6; }
    if(replay.romSha1 != found->check.sha1) { report << "result=the recording was made with a different ROM\n"; return 6; }
  }
  const int frames = replaying && o.frames == 600 ? static_cast<int>(replay.frames) : o.frames;
  const int saveFrame = replaying && o.saveFrame >= frames ? std::max(0, frames - 1) : o.saveFrame;

  // Optional audio file: a standard WAV, 16-bit stereo at the emulator's rate, like the one the Phase C tool wrote.
  std::ofstream wav;
  std::uint32_t wavBytes = 0;
  auto put32 = [&](std::uint32_t v) { for(int i = 0; i < 4; ++i) wav.put(static_cast<char>((v >> (8 * i)) & 0xFF)); };
  auto put16 = [&](std::uint32_t v) { for(int i = 0; i < 2; ++i) wav.put(static_cast<char>((v >> (8 * i)) & 0xFF)); };
  const std::uint32_t rate = static_cast<std::uint32_t>(emulator.audioRate());
  if(!o.audioWav.empty()) {
    wav.open(o.audioWav, std::ios::binary);
    wav.write("RIFF", 4); put32(0); wav.write("WAVEfmt ", 8); put32(16);
    put16(1); put16(2); put32(rate); put32(rate * 4); put16(4); put16(16);
    wav.write("data", 4); put32(0);
  }

  bool saved = false;
  char hash[17];
  for(int f = 0; f < frames; ++f) {
    if(replaying) {
      controls::Input in = controls::Input::unpack(replay.bitsAt(f));
      if(o.resetAt >= 0 && f >= o.resetAt && f < o.resetAt + 10) in.reset = true;
      applyInput(emulator, in, false);
    } else {
      emulator.setConsoleReset(o.resetAt >= 0 && f >= o.resetAt && f < o.resetAt + 10);
    }
    emulator.runFrame();
    if(wav) {
      wav.write(reinterpret_cast<const char*>(emulator.audio()), static_cast<std::streamsize>(emulator.audioFrames()) * 4);
      wavBytes += static_cast<std::uint32_t>(emulator.audioFrames()) * 4;
    }
    std::snprintf(hash, sizeof hash, "%016llx", static_cast<unsigned long long>(emulator.pictureFingerprint()));
    if(fingerprints) fingerprints << f << ',' << hash << '\n';
    if(f != saveFrame) continue;

    const int h = emulator.height();
    std::vector<std::uint32_t> pixels = emulator.colours();
    SDL_Surface* surface = SDL_CreateSurfaceFrom(Emulator::kWidth, h, SDL_PIXELFORMAT_XRGB8888, pixels.data(), Emulator::kWidth * 4);
    saved = surface && SDL_SavePNG(surface, o.headlessPng.c_str());
    if(surface) SDL_DestroySurface(surface);
    if(!o.rawFile.empty()) {
      std::ofstream raw(o.rawFile, std::ios::binary);
      raw.write(reinterpret_cast<const char*>(emulator.indexed()), static_cast<std::streamsize>(h) * Emulator::kWidth);
    }
    report << "saved_frame=" << f << "\nsaved_picture=" << Emulator::kWidth << 'x' << h
           << "\nsaved_frame_fingerprint=" << hash << '\n';
  }
  if(wav) {  // fill in the two sizes of the WAV header
    wav.seekp(4); put32(36 + wavBytes);
    wav.seekp(40); put32(wavBytes);
    wav.close();
    report << "audio_wav=" << o.audioWav << "\naudio_rate=" << rate << "\naudio_bytes=" << wavBytes << '\n';
  }
  report << "final_ram=";
  for(int i = 0; i < 128; ++i) { char b[3]; std::snprintf(b, sizeof b, "%02x", emulator.ram()[i]); report << b; }
  report << '\n';
  report << "rom=" << found->check.path << "\nrom_sha1=" << found->check.sha1 << "\nframes_run=" << frames
         << "\nlast_frame_fingerprint=" << hash << "\npng_saved=" << (saved ? "yes" : "no") << "\nresult=" << (saved ? "OK" : "FAIL") << '\n';
  return saved ? 0 : 6;
}

// The Open file dialog reports back from SDL, possibly on another thread.
struct PickResult {
  std::mutex lock;
  bool done = false;
  std::string path;  // empty = cancelled
};

void SDLCALL onPicked(void* userdata, const char* const* files, int)
{
  auto* r = static_cast<PickResult*>(userdata);
  std::lock_guard<std::mutex> guard(r->lock);
  r->path = (files && files[0]) ? files[0] : "";
  r->done = true;
}

// Shows a ROM problem. Returns true for "Use anyway".
bool askAboutProblem(SDL_Window* window, const rom::Check& check)
{
  std::vector<SDL_MessageBoxButtonData> buttons;
  buttons.push_back({SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT | SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 1, "Choose another"});
  if(check.status == rom::Status::UnknownFingerprint) buttons.push_back({0, 2, "Use anyway"});
  const std::string text = rom::describe(check);
  SDL_MessageBoxData box{SDL_MESSAGEBOX_WARNING, window, "This is not the expected game file", text.c_str(),
                         static_cast<int>(buttons.size()), buttons.data(), nullptr};
  int pressed = 1;
  if(!SDL_ShowMessageBox(&box, &pressed)) pressed = 1;
  return pressed == 2;
}


// Finds a usable ROM, asking the person when needed. Returns the chosen path, or empty if they gave up.
std::string resolveRom(SDL_Window* window, const Options& o)
{
  std::optional<rom::Found> found = rom::findQuietly(o.romArg, o.settingsFile, o.romsDir);
  const std::string savedBefore = rom::loadSavedPath(o.settingsFile);
  for(;;) {
    if(found) {
      if(found->check.status == rom::Status::Ok) {
        if(found->check.path != savedBefore) rom::savePath(o.settingsFile, found->check.path);  // remember only a good ROM
        return found->check.path;
      }
      if(askAboutProblem(window, found->check)) return found->check.path;  // this run only, not remembered
      found.reset();
    }

    static const SDL_DialogFileFilter filters[] = {{"Atari 2600 game (*.a26)", "a26"}, {"All files", "*"}};
    PickResult result;
    SDL_ShowOpenFileDialog(onPicked, &result, window, filters, 2, nullptr, false);
    for(;;) {
      {
        std::lock_guard<std::mutex> guard(result.lock);
        if(result.done) break;
      }
      for(SDL_Event e; SDL_PollEvent(&e); )
        if(e.type == SDL_EVENT_QUIT) return {};
      SDL_Delay(20);
    }
    if(result.path.empty()) return {};  // cancelled
    found = rom::Found{"dialog", rom::checkFile(result.path)};
  }
}

}  // namespace

int main(int argc, char* argv[])
{
  const Options options = parseArguments(argc, argv);
  if(!options.findRomFile.empty()) return findRomReport(options);
  if(!options.headlessPng.empty()) return headlessRun(options);
  if(!options.keysReplayScript.empty()) return keysReplay(options);
  if(!options.padCheckFile.empty()) {
    if(!SDL_Init(SDL_INIT_GAMEPAD)) { showError("SDL could not start."); return 1; }
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    const bool ok = padCheck(options);
    SDL_Quit();
    return ok ? 0 : 2;
  }
  if(options.audioCheckSeconds > 0) {
    if(!SDL_Init(SDL_INIT_AUDIO)) { showError("SDL could not start."); return 1; }
    const bool ok = audioCheck(options);
    SDL_Quit();
    return ok ? 0 : 2;
  }

  if(!SDL_Init(SDL_INIT_VIDEO)) { showError("SDL could not start."); return 1; }

  // The remembered window (Stage D7) is used for playing and for --state-report, not for the other checks or when --window is given.
  const bool otherCheck = !options.selfCheckFile.empty() || !options.layoutCheckPng.empty() || !options.fullscreenCheckFile.empty() || !options.helpCheckPng.empty();
  windowstate::State remembered;
  if(!otherCheck && !options.windowGiven) remembered = windowstate::fitToMonitors(windowstate::load(options.settingsFile), monitorAreas());

  SDL_Window* window = nullptr;
  SDL_Renderer* renderer = nullptr;
  const int startW = remembered.known ? remembered.area.w : options.windowW, startH = remembered.known ? remembered.area.h : options.windowH;
  if(!SDL_CreateWindowAndRenderer(kTitle, startW, startH, SDL_WINDOW_RESIZABLE, &window, &renderer)) {
    showError("The window could not be created.");
    SDL_Quit();
    return 1;
  }
  SDL_SetRenderVSync(renderer, 1);  // present once per screen refresh, so the loop does not spin

  Tracker tracker;
  if(remembered.known) {
    SDL_SetWindowPosition(window, remembered.area.x, remembered.area.y);
    tracker.state = remembered;
    tracker.state.fullscreen = false;
    if(remembered.fullscreen) setFullscreen(window, tracker, true);
  }

  int exitCode = 0;
  if(!options.selfCheckFile.empty()) {
    exitCode = selfCheck(window, renderer, options.selfCheckFile.c_str()) ? 0 : 2;
  } else if(!options.stateReportFile.empty()) {
    exitCode = stateReport(window, options, remembered) ? 0 : 2;
  } else {
    // Black while the Open file dialog or a message is up, so the window is not left undrawn.
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);

    const std::string romPath = resolveRom(window, options);
    if(romPath.empty()) {
      exitCode = 3;  // no ROM chosen: nothing to run yet
    } else {
      View view;
      view.mode = options.mode;
      view.shape = options.pixelShape;
      if(!options.layoutCheckPng.empty()) {
        exitCode = layoutCheck(renderer, options.layoutCheckPng, romPath, view, options.resetAt) ? 0 : 2;
      } else if(!options.helpCheckPng.empty()) {
        exitCode = helpCheck(renderer, options.helpCheckPng, romPath) ? 0 : 2;
      } else if(!options.fullscreenCheckFile.empty()) {
        exitCode = fullscreenCheck(window, renderer, options, romPath) ? 0 : 2;
      } else {
        const double rate = playWindow(window, renderer, romPath, options.timedSeconds, view, tracker, options);
        // Remember the window for next time (not for timed test runs, which must not change the settings).
        if(options.timedSeconds <= 0 && !options.windowGiven) {
          rememberWindowedRect(window, tracker);
          if(tracker.state.known) windowstate::save(options.settingsFile, tracker.state);
        }
        if(options.timedSeconds > 0) {
          std::ofstream out(options.timedReport);
          out << "seconds=" << options.timedSeconds << "\nframes_per_second=" << rate << '\n';
        }
      }
    }
  }

  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return exitCode;
}
