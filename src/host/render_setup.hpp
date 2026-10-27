// Making the window's renderer, in the standard colour space or the HDR one (Stage I10.1, DECISIONS.md D-016), and asking Windows about HDR.
#pragma once
#include "common.hpp"
#include "hdr.hpp"

namespace host {

// What Windows says about HDR for the screen the window is on now.
hdr::ScreenState screenStateOf(SDL_Window* window);

// A renderer for the window: with `hdrPipeline` in the linear 16-bit float colour space that can show brightness above normal white (only the direct3d11,
// direct3d12 and metal renderers can; if that cannot be made the standard one is, and `*gotHdr` says which it was), else the standard one. The screen's own wait
// for the refresh (vsync) is on, as everywhere in the program.
SDL_Renderer* createRenderer(SDL_Window* window, bool hdrPipeline, bool* gotHdr = nullptr);

}  // namespace host
