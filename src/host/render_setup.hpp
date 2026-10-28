// Making the window's renderer, in the standard colour space or the HDR one (Stage I10.1, DECISIONS.md D-016), and asking Windows about HDR.
#pragma once
#include "common.hpp"
#include <cstdint>
#include <string>
#include <vector>

#include "hdr.hpp"

namespace host {

// What Windows says about HDR for the screen the window is on now.
hdr::ScreenState screenStateOf(SDL_Window* window);

// A renderer for the window: with `hdrPipeline` in the linear 16-bit float colour space that can show brightness above normal white (only the direct3d11,
// direct3d12 and metal renderers can; if that cannot be made the standard one is, and `*gotHdr` says which it was), else the standard one. The screen's own wait
// for the refresh (vsync) is on, as everywhere in the program.
SDL_Renderer* createRenderer(SDL_Window* window, bool hdrPipeline, bool* gotHdr = nullptr);

// SDL says that the graphics device was lost or has been made again (Stage I16): everything made from the old one, the textures and the filters' objects, must be made again.
bool rendererWasReset(const SDL_Event& e);

// The window's pixels as standard 8-bit colour (0x00RRGGBB), whatever the renderer's colour space: an HDR renderer gives 16-bit float linear pixels, which are turned back
// by hand with the screen's white level `sdrWhite`, so that what is compared is what is sent to the screen. Saved as a PNG too. `format` names the pixel format read.
struct Pixels {
  int w = 0, h = 0;
  std::vector<std::uint32_t> xrgb;
};
bool readAsStandard(SDL_Renderer* renderer, Pixels& out, const std::string& png, std::string& format, float sdrWhite);
float halfToFloat(std::uint16_t half);

// Compares two read-backs where the first is flat (all four neighbours the same colour): the number of such pixels and how many of them differ by more than one step;
// edges between two colours are counted apart (a blend at the edge of a scaled pixel is made in gamma space by the standard renderer and in linear light by the HDR one).
struct FlatComparison {
  long long inside = 0, insideDifferent = 0, edges = 0, edgesDifferent = 0;
  int largestInside = 0;
};
FlatComparison compareFlat(const Pixels& a, const Pixels& b);

}  // namespace host
