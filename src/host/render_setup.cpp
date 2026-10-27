#include "render_setup.hpp"

#include <algorithm>
#include <cstdlib>

namespace host {

hdr::ScreenState screenStateOf(SDL_Window* window)
{
  hdr::ScreenState state;
  const SDL_PropertiesID props = SDL_GetWindowProperties(window);
  state.hdrOn = SDL_GetBooleanProperty(props, SDL_PROP_WINDOW_HDR_ENABLED_BOOLEAN, false);
  state.sdrWhite = SDL_GetFloatProperty(props, SDL_PROP_WINDOW_SDR_WHITE_LEVEL_FLOAT, 1.0f);
  state.headroom = SDL_GetFloatProperty(props, SDL_PROP_WINDOW_HDR_HEADROOM_FLOAT, 1.0f);
  return state;
}

SDL_Renderer* createRenderer(SDL_Window* window, bool hdrPipeline, bool* gotHdr)
{
  if(gotHdr) *gotHdr = false;
  if(hdrPipeline) {
    SDL_PropertiesID props = SDL_CreateProperties();
    SDL_SetPointerProperty(props, SDL_PROP_RENDERER_CREATE_WINDOW_POINTER, window);
    SDL_SetNumberProperty(props, SDL_PROP_RENDERER_CREATE_OUTPUT_COLORSPACE_NUMBER, SDL_COLORSPACE_SRGB_LINEAR);
    SDL_SetNumberProperty(props, SDL_PROP_RENDERER_CREATE_PRESENT_VSYNC_NUMBER, 1);
    SDL_Renderer* renderer = SDL_CreateRendererWithProperties(props);
    SDL_DestroyProperties(props);
    if(renderer) {
      if(gotHdr) *gotHdr = SDL_GetBooleanProperty(SDL_GetRendererProperties(renderer), SDL_PROP_RENDERER_HDR_ENABLED_BOOLEAN, false);
      return renderer;
    }
  }
  SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
  if(renderer) SDL_SetRenderVSync(renderer, 1);
  return renderer;
}

FlatComparison compareFlat(const Pixels& a, const Pixels& b)
{
  FlatComparison r;
  const int w = a.w, h = a.h;
  auto diffOf = [&](std::size_t i) {
    int d = 0;
    for(int shift = 0; shift <= 16; shift += 8) d = std::max(d, std::abs(static_cast<int>((a.xrgb[i] >> shift) & 0xFF) - static_cast<int>((b.xrgb[i] >> shift) & 0xFF)));
    return d;
  };
  if(a.xrgb.size() != b.xrgb.size()) return r;
  for(int y = 1; y + 1 < h; ++y)
    for(int x = 1; x + 1 < w; ++x) {
      const std::size_t i = static_cast<std::size_t>(y * w + x);
      const std::uint32_t c = a.xrgb[i];
      const bool flat = a.xrgb[i - 1] == c && a.xrgb[i + 1] == c && a.xrgb[i - static_cast<std::size_t>(w)] == c && a.xrgb[i + static_cast<std::size_t>(w)] == c;
      const int d = diffOf(i);
      if(flat) { ++r.inside; if(d > 1) ++r.insideDifferent; r.largestInside = std::max(r.largestInside, d); }
      else { ++r.edges; if(d > 1) ++r.edgesDifferent; }
    }
  return r;
}

}  // namespace host
