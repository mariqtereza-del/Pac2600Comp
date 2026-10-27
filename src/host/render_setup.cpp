#include "render_setup.hpp"

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

}  // namespace host
