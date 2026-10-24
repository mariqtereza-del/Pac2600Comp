// pacman2600 host, Stage D2: an empty black window that closes cleanly (DECISIONS.md T-D2-1).
// Later stages add the emulator core, picture, sound and controls (ARCHITECTURE.md).
//
// Usage: pacman2600                       open the window until it is closed
//        pacman2600 --self-check <file>   open the window, write what it looks like to <file>, quit

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <cstdio>
#include <cstring>
#include <fstream>

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

}  // namespace

int main(int argc, char* argv[])
{
  const char* checkFile = (argc >= 3 && std::strcmp(argv[1], "--self-check") == 0) ? argv[2] : nullptr;

  if(!SDL_Init(SDL_INIT_VIDEO)) { showError("SDL could not start."); return 1; }

  SDL_Window* window = nullptr;
  SDL_Renderer* renderer = nullptr;
  if(!SDL_CreateWindowAndRenderer(kTitle, kWidth, kHeight, 0, &window, &renderer)) {
    showError("The window could not be created.");
    SDL_Quit();
    return 1;
  }

  int exitCode = 0;
  if(checkFile) {
    exitCode = selfCheck(window, renderer, checkFile) ? 0 : 2;
  } else {
    for(bool running = true; running; ) {
      for(SDL_Event e; SDL_PollEvent(&e); ) {
        if(e.type == SDL_EVENT_QUIT || (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_ESCAPE)) running = false;
      }
      SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
      SDL_RenderClear(renderer);
      SDL_RenderPresent(renderer);  // waits for the screen refresh, so the loop does not spin
    }
  }

  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return exitCode;
}
