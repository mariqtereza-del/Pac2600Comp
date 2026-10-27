// HDR output (Stage I10, DECISIONS.md D-016): what the screen says about HDR, and the decisions and arithmetic that follow from it. No window code, so it can
// be tested on its own. The original game has no HDR; this only decides how bright the highlights drawn on top of the unchanged picture may be.
#pragma once

namespace hdr {

// What Windows says about the screen the window is on (SDL_PROP_WINDOW_HDR_ENABLED_BOOLEAN, ..._SDR_WHITE_LEVEL_FLOAT, ..._HDR_HEADROOM_FLOAT).
struct ScreenState {
  bool hdrOn = false;       // Windows is in HDR mode for this screen
  float sdrWhite = 1.0f;    // the brightness of normal white in the renderer's linear colours (Windows' "SDR content brightness" slider)
  float headroom = 1.0f;    // how many times brighter than normal white the screen can show; 1 when HDR is off
};

// Whether the program draws through the HDR pipeline: the owner's choice (the menu row, on unless turned off) and Windows being in HDR mode, with room above white.
bool useHdr(bool wanted, const ScreenState& screen);

// The brightness of the highlights (power pills, Pac-Man, the ghosts) as a multiple of normal white: `strength` 0 up to 1 (the slider) of the way from 1 (no
// highlight) to the most the screen can show, `headroom`, which is never exceeded. Without HDR it is 1.
float highlightScale(float strength, const ScreenState& screen, bool hdrInUse);

// The most the slider may take the highlights to, as a multiple of white: the screen's own headroom, but no more than `kMostBrightness`, so that a very bright
// screen does not make the highlights glare.
inline constexpr float kMostBrightness = 4.0f;
float ceilingOf(const ScreenState& screen);

}  // namespace hdr
