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
//        (window mode: keys 1, 2, 3, 4 = Sharp, Fill, TV frame, Game area; the window can be resized)
//        pacman2600 --headless <png> [rom_file] [--frames N] [--save-frame N] [--raw <file>] [--fingerprints <csv>]
//                   [--settings <ini>] [--roms-dir <dir>]
//                                               runs the game with no window (default 600 frames) and saves frame
//                                               N (default 300) as a PNG, with a report in <png>.txt

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "emulator.hpp"
#include "layout.hpp"
#include "rom_finder.hpp"

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
  }
  return "?";
}

bool parseMode(const std::string& text, layout::Mode& mode)
{
  if(text == "sharp") mode = layout::Mode::Sharp;
  else if(text == "fill") mode = layout::Mode::Fill;
  else if(text == "tv") mode = layout::Mode::TvFrame;
  else if(text == "game") mode = layout::Mode::GameArea;
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
  std::string headlessPng, rawFile, fingerprintFile, layoutCheckPng, timedReport;
  layout::Mode mode = layout::Mode::Sharp;
  layout::PixelShape pixelShape{0, 0};  // {0, 0} = Stella's shape for the picture
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
    else if(a == "--mode") { if(!parseMode(next(), o.mode)) std::fprintf(stderr, "--mode must be sharp, fill, tv or game\n"); }
    else if(a == "--window") { if(!parsePair(next(), 'x', o.windowW, o.windowH)) std::fprintf(stderr, "--window must look like 1280x960\n"); }
    else if(a == "--pixel-aspect") { if(!parsePair(next(), ':', o.pixelShape.num, o.pixelShape.den)) std::fprintf(stderr, "--pixel-aspect must look like 12:7\n"); }
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
    const bool crop = view.mode == layout::Mode::GameArea && layout::kGameBottom <= height;
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
bool layoutCheck(SDL_Renderer* renderer, const std::string& pngPath, const std::string& romPath, const View& view)
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

// The game window: one emulator frame per 1/60 s from a simple clock (temporary; real pacing is Stage D9).
// With maxSeconds > 0 it stops by itself after that long and returns the frames per second it managed (for tests).
// Keys 1, 2, 3, 4 choose Sharp, Fill, TV frame, Game area (temporary; Stage D10 gives the keys their final home).
double playWindow(SDL_Window* window, SDL_Renderer* renderer, const std::string& romPath, double maxSeconds, View view)
{
  std::vector<std::uint8_t> bytes;
  Emulator emulator;
  if(!readRomBytes(romPath, bytes)) { SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, kTitle, "The ROM file could not be read.", window); return 0; }
  if(const std::string error = emulator.load(fileNameOf(romPath), bytes); !error.empty()) {
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, kTitle, error.c_str(), window);
    return 0;
  }
  Screen screen;
  screen.renderer = renderer;
  screen.view = view;
  auto retitle = [&]() {
    const std::string title = std::string(kTitle) + " - " + fileNameOf(romPath) + " - " + modeName(screen.view.mode);
    SDL_SetWindowTitle(window, title.c_str());
  };
  retitle();

  constexpr Uint64 kFrameNs = 1000000000ull / 60;  // Stella plays NTSC at exactly 60 Hz (docs\TIMING.md)
  Uint64 next = SDL_GetTicksNS();
  const Uint64 started = next;
  long long frames = 0;
  std::vector<std::uint32_t> pixels;
  for(bool running = true; running; ) {
    for(SDL_Event e; SDL_PollEvent(&e); ) {
      if(e.type == SDL_EVENT_QUIT || (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_ESCAPE)) running = false;
      if(e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat) {
        if(e.key.key == SDLK_1) { screen.view.mode = layout::Mode::Sharp; retitle(); }
        if(e.key.key == SDLK_2) { screen.view.mode = layout::Mode::Fill; retitle(); }
        if(e.key.key == SDLK_3) { screen.view.mode = layout::Mode::TvFrame; retitle(); }
        if(e.key.key == SDLK_4) { screen.view.mode = layout::Mode::GameArea; retitle(); }
      }
    }
    const Uint64 now = SDL_GetTicksNS();
    int steps = 0;
    while(now >= next && steps < 5) { emulator.runFrame(); next += kFrameNs; ++steps; ++frames; }
    if(steps == 5 && now >= next) next = now;  // far behind (window dragged, for example): do not try to catch up
    if(steps > 0) pixels = emulator.colours();
    if(!pixels.empty() && !screen.show(pixels, emulator.height())) return 0;  // redrawn every time: a presented buffer is not kept
    SDL_RenderPresent(renderer);
    if(maxSeconds > 0 && static_cast<double>(now - started) >= maxSeconds * 1e9) running = false;
  }
  return static_cast<double>(frames) * 1e9 / static_cast<double>(SDL_GetTicksNS() - started);
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

  bool saved = false;
  char hash[17];
  for(int f = 0; f < o.frames; ++f) {
    emulator.runFrame();
    std::snprintf(hash, sizeof hash, "%016llx", static_cast<unsigned long long>(emulator.pictureFingerprint()));
    if(fingerprints) fingerprints << f << ',' << hash << '\n';
    if(f != o.saveFrame) continue;

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
  report << "rom=" << found->check.path << "\nrom_sha1=" << found->check.sha1 << "\nframes_run=" << o.frames
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

  if(!SDL_Init(SDL_INIT_VIDEO)) { showError("SDL could not start."); return 1; }

  SDL_Window* window = nullptr;
  SDL_Renderer* renderer = nullptr;
  if(!SDL_CreateWindowAndRenderer(kTitle, options.windowW, options.windowH, SDL_WINDOW_RESIZABLE, &window, &renderer)) {
    showError("The window could not be created.");
    SDL_Quit();
    return 1;
  }
  SDL_SetRenderVSync(renderer, 1);  // present once per screen refresh, so the loop does not spin

  int exitCode = 0;
  if(!options.selfCheckFile.empty()) {
    exitCode = selfCheck(window, renderer, options.selfCheckFile.c_str()) ? 0 : 2;
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
        exitCode = layoutCheck(renderer, options.layoutCheckPng, romPath, view) ? 0 : 2;
      } else {
        const double rate = playWindow(window, renderer, romPath, options.timedSeconds, view);
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
