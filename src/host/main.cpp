// pacman2600 host. D2: a window that closes cleanly. D3: finds and checks the ROM. D4: the emulator runs the ROM.
// D5: the game picture is shown in the window (DECISIONS.md T-D2-1 ... T-D5-1). Later stages add scaling, sound and
// controls (ARCHITECTURE.md).
//
// Usage: pacman2600 [rom_file]                  open the window; the ROM is looked for in this order: the file given
//                                               here, the saved path, a "roms" folder next to the program, a dialog
//        pacman2600 --self-check <file>         open the window, write what it looks like to <file>, quit
//        pacman2600 --find-rom <file> [rom_file] [--settings <ini>] [--roms-dir <dir>]
//                                               the quiet part of the ROM search, no window; writes the result to <file>
//        pacman2600 --screen-check <png> [rom_file]   run 300 frames in the window, compare its pixels with the emulator's
//        pacman2600 --timed <seconds> <file> [rom_file]  play for that long, then write the frame rate to <file>
//        pacman2600 --headless <png> [rom_file] [--frames N] [--save-frame N] [--raw <file>] [--fingerprints <csv>]
//                   [--settings <ini>] [--roms-dir <dir>]
//                                               runs the game with no window (default 600 frames) and saves frame
//                                               N (default 300) as a PNG, with a report in <png>.txt

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "emulator.hpp"
#include "rom_finder.hpp"

namespace {

constexpr const char* kTitle = "pacman2600 (working name)";  // real name is chosen in Stage J2
constexpr int kWidth = 960, kHeight = 720;

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
  const bool ok = shot && w == kWidth && h == kHeight && r == 0 && g == 0 && b == 0
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

struct Options {
  std::string selfCheckFile, findRomFile, romArg, settingsFile, romsDir;
  std::string headlessPng, rawFile, fingerprintFile, screenCheckPng, timedReport;
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
    else if(a == "--screen-check") o.screenCheckPng = next();
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

// The game picture, drawn unscaled (one game pixel = one screen pixel) in the middle of the window (Stage D5, T-D5-1).
struct Screen {
  SDL_Renderer* renderer = nullptr;
  SDL_Texture* texture = nullptr;
  SDL_FRect where{};
  ~Screen() { if(texture) SDL_DestroyTexture(texture); }

  bool show(const std::vector<std::uint32_t>& pixels, int height)
  {
    if(!texture) {
      texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_STREAMING, Emulator::kWidth, height);
      if(!texture) return false;
      SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
      where = {static_cast<float>((kWidth - Emulator::kWidth) / 2), static_cast<float>((kHeight - height) / 2),
               static_cast<float>(Emulator::kWidth), static_cast<float>(height)};
    }
    SDL_UpdateTexture(texture, nullptr, pixels.data(), Emulator::kWidth * 4);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_RenderTexture(renderer, texture, nullptr, &where);
    return true;
  }
};

// --screen-check: run 300 frames, then compare what the window shows with what the emulator drew. No clock, no waiting.
bool screenCheck(SDL_Renderer* renderer, const std::string& pngPath, const std::string& romPath)
{
  std::ofstream report(pngPath + ".txt");
  std::vector<std::uint8_t> bytes;
  if(!report || !readRomBytes(romPath, bytes)) return false;
  Emulator emulator;
  if(const std::string error = emulator.load(fileNameOf(romPath), bytes); !error.empty()) { report << "result=" << error << '\n'; return false; }

  Screen screen;
  screen.renderer = renderer;
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
  const int x0 = static_cast<int>(screen.where.x), y0 = static_cast<int>(screen.where.y);
  long long differing = 0, outsideNotBlack = 0, picturePixelsNotBlack = 0;
  for(int y = 0; y < rgb->h; ++y) {
    const auto* row = reinterpret_cast<const std::uint32_t*>(static_cast<const std::uint8_t*>(rgb->pixels) + y * rgb->pitch);
    for(int x = 0; x < rgb->w; ++x) {
      const bool inside = x >= x0 && x < x0 + Emulator::kWidth && y >= y0 && y < y0 + h;
      const std::uint32_t seen = row[x] & 0x00FFFFFF;
      if(inside) {
        const std::uint32_t expected = pixels[static_cast<std::size_t>(y - y0) * Emulator::kWidth + (x - x0)] & 0x00FFFFFF;
        if(seen != expected) ++differing;
        if(expected != 0) ++picturePixelsNotBlack;
      } else if(seen != 0) {
        ++outsideNotBlack;
      }
    }
  }
  const bool saved = SDL_SavePNG(rgb, pngPath.c_str());
  report << "window_pixels=" << rgb->w << 'x' << rgb->h << "\npicture_at=" << x0 << ',' << y0 << ' ' << Emulator::kWidth << 'x' << h
         << "\npixels_differing_from_emulator=" << differing << "\npixels_outside_picture_not_black=" << outsideNotBlack
         << "\npicture_pixels_not_black=" << picturePixelsNotBlack << "\nscreenshot_saved=" << (saved ? "yes" : "no") << '\n';
  const bool ok = saved && differing == 0 && outsideNotBlack == 0 && picturePixelsNotBlack > 1000 && rgb->w == kWidth && rgb->h == kHeight;
  report << "result=" << (ok ? "OK" : "FAIL") << '\n';
  SDL_DestroySurface(rgb);
  return ok;
}

// The game window: one emulator frame per 1/60 s from a simple clock (temporary; real pacing is Stage D9).
// With maxSeconds > 0 it stops by itself after that long and returns the frames per second it managed (for tests).
double playWindow(SDL_Window* window, SDL_Renderer* renderer, const std::string& romPath, double maxSeconds)
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

  constexpr Uint64 kFrameNs = 1000000000ull / 60;  // Stella plays NTSC at exactly 60 Hz (docs\TIMING.md)
  Uint64 next = SDL_GetTicksNS();
  const Uint64 started = next;
  long long frames = 0;
  std::vector<std::uint32_t> pixels;
  for(bool running = true; running; ) {
    for(SDL_Event e; SDL_PollEvent(&e); ) {
      if(e.type == SDL_EVENT_QUIT || (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_ESCAPE)) running = false;
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
  if(!SDL_CreateWindowAndRenderer(kTitle, kWidth, kHeight, 0, &window, &renderer)) {
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
      const std::string title = std::string(kTitle) + " - " + fileNameOf(romPath);
      SDL_SetWindowTitle(window, title.c_str());
      if(!options.screenCheckPng.empty()) {
        exitCode = screenCheck(renderer, options.screenCheckPng, romPath) ? 0 : 2;
      } else {
        const double rate = playWindow(window, renderer, romPath, options.timedSeconds);
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
