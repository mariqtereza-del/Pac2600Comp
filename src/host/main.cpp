// pacman2600 host. Stage D2: an empty black window that closes cleanly. Stage D3: finds and checks the ROM
// (DECISIONS.md T-D2-1, T-D3-1). Later stages add the emulator core, picture, sound and controls (ARCHITECTURE.md).
//
// Usage: pacman2600 [rom_file]                  open the window; the ROM is looked for in this order: the file given
//                                               here, the saved path, a "roms" folder next to the program, a dialog
//        pacman2600 --self-check <file>         open the window, write what it looks like to <file>, quit
//        pacman2600 --find-rom <file> [rom_file] [--settings <ini>] [--roms-dir <dir>]
//                                               the quiet part of the ROM search, no window; writes the result to <file>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <cstdio>
#include <cstring>
#include <fstream>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

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
};

Options parseArguments(int argc, char* argv[])
{
  Options o;
  for(int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    auto next = [&]() -> std::string { return i + 1 < argc ? argv[++i] : std::string(); };
    if(a == "--self-check") o.selfCheckFile = next();
    else if(a == "--find-rom") o.findRomFile = next();
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

std::string fileNameOf(const std::string& path)
{
  const std::size_t cut = path.find_last_of("/\\");
  return cut == std::string::npos ? path : path.substr(cut + 1);
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

  if(!SDL_Init(SDL_INIT_VIDEO)) { showError("SDL could not start."); return 1; }

  SDL_Window* window = nullptr;
  SDL_Renderer* renderer = nullptr;
  if(!SDL_CreateWindowAndRenderer(kTitle, kWidth, kHeight, 0, &window, &renderer)) {
    showError("The window could not be created.");
    SDL_Quit();
    return 1;
  }

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
      for(bool running = true; running; ) {
        for(SDL_Event e; SDL_PollEvent(&e); ) {
          if(e.type == SDL_EVENT_QUIT || (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_ESCAPE)) running = false;
        }
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        SDL_RenderPresent(renderer);  // waits for the screen refresh, so the loop does not spin
      }
    }
  }

  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return exitCode;
}
