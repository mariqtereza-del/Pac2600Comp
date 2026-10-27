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
//        pacman2600 --backdrop-check <png prefix> [rom_file]   every background in the real window: the picture untouched, the space filled (Stage I7)
//        pacman2600 --menu-shot <png> [rom_file]    the Escape menu in the real window, as a screenshot (Stage I5)
//        pacman2600 --menu-check <report> [rom_file] [--settings <ini>]
//                                               no window: the menu and the game played together, all eight games (Stage I5)
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

#include <SDL3/SDL_main.h>

#include "common.hpp"
#include "options.hpp"
#include "rom_choice.hpp"
#include "screen.hpp"
#include "pads.hpp"
#include "pacer.hpp"
#include "window_tracker.hpp"
#include "window_checks.hpp"
#include "play_window.hpp"
#include "headless.hpp"
#include "menu_checks.hpp"

using namespace host;

int main(int argc, char* argv[])
{
  const Options options = parseArguments(argc, argv);
  if(!options.findRomFile.empty()) return findRomReport(options);
  if(!options.headlessPng.empty()) return headlessRun(options);
  if(!options.keysReplayScript.empty()) return keysReplay(options);
  if(!options.menuCheckFile.empty()) return menuCheck(options);
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
  const bool otherCheck = !options.selfCheckFile.empty() || !options.layoutCheckPng.empty() || !options.fullscreenCheckFile.empty() || !options.helpCheckPng.empty() || !options.menuShotPng.empty() || !options.backdropCheckPng.empty() || !options.smoothShotPng.empty();
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
  SDL_SetRenderVSync(renderer, options.presentHz > 0 ? 0 : 1);  // present once per screen refresh, so the loop does not spin (--present-hz: a timer plays the monitor, Stage H6)

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
      } else if(!options.backdropCheckPng.empty()) {
        exitCode = backdropCheck(renderer, options.backdropCheckPng, romPath) ? 0 : 2;
      } else if(!options.smoothShotPng.empty()) {
        exitCode = smoothShot(renderer, options.smoothShotPng, romPath, view) ? 0 : 2;
      } else if(!options.menuShotPng.empty()) {
        exitCode = menuShot(renderer, options.menuShotPng, romPath) ? 0 : 2;
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
