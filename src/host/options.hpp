// The command line and the layout view: what the program was asked to do.
// (Stage G4.7: moved out of main.cpp, the code is unchanged.)
#pragma once
#include "common.hpp"

namespace host {

// How the picture is laid out in the window (Stage D6, T-D6-1).
struct View {
  layout::Mode mode = layout::Mode::Sharp;
  layout::PixelShape shape{0, 0};  // {0, 0} = Stella's shape for the picture's size (layout::stellaShape)
};

const char* modeName(layout::Mode m);

struct Options {
  std::string selfCheckFile, findRomFile, romArg, settingsFile, romsDir;
  std::string headlessPng, rawFile, fingerprintFile, layoutCheckPng, timedReport, fullscreenCheckFile, stateReportFile;
  std::string menuCheckFile, menuShotPng;  // --menu-check <report> / --menu-shot <png> (Stage I5)
  std::string audioCheckFile, audioWav, keysReplayScript, keysReplayCsv, helpCheckPng, padCheckFile;
  std::string recordFile, replayFile;  // input recording (--record) and playback (--replay), Stage D13
  std::vector<std::array<int, 3>> pokes;  // --poke <frame>:<hex address>:<hex value> (headless only, TEST ONLY, Stage E8): memory edits before a frame
  std::string traceFile;               // --trace <file>: what the game did on every frame (Stage E1, docs\BASELINE.md)
  long long ramSeed = -1;              // --ram-seed N: power-on memory filled from seed N (-1 = zeros)
  bool softGlow = false;               // --soft-glow: Stella's phosphor glow on (headless tests of F7)
  bool stellaSpeed = false;            // --stella-speed: 60.000 frames a second like Stella, instead of the console's own 59.92 (comparison only)
  double audioCheckSeconds = 0;
  int resetAt = -1;   // frame at which the console's Reset is held for 10 frames (tests; the R key does this by hand)
  bool mute = false;  // play the sound stream with the volume at zero (tests)
  bool pushMenu = false;  // tests (Stage I5): a helper thread works the Escape menu through the real window's event queue (--push-menu)
  bool pushKeys = false;  // tests: a helper thread presses keys in the real window's event queue (--push-keys)
  bool timerClock = false;  // pace the game from the PC clock instead of the sound card (comparison only)
  int stallMs = 0;    // tests: freeze the loop once, 5 seconds in (--audio-check: the game loop; the game window: the window thread)
  double presentHz = 0;  // tests (Stage H6): --present-hz N shows pictures at N a second from a timer, as a monitor of N Hz would (the real monitor's wait is off)
  bool windowGiven = false;  // --window on the command line: do not use the remembered window
  layout::Mode mode = layout::Mode::Framed;  // 12:7 pixels, game area, black border on every side (D-006); keys 1-5 switch it while playing
  layout::PixelShape pixelShape{12, 7};        // the standard NTSC shape of a 2600 pixel (D-005); {0, 0} = Stella's 19:10 (--pixel-aspect stella)
  int windowW = kWindowW, windowH = kWindowH;
  double timedSeconds = 0;
  int frames = 600, saveFrame = 300;
};

Options parseArguments(int argc, char* argv[]);

}  // namespace host
