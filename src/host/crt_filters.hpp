// The HLSL source of each ported CRT filter, and the engine that runs them (Stage I11, DECISIONS.md D-017). Windows only (Direct3D 11), like gpu_pass.
#pragma once
#include <string>
#include <vector>

#include "common.hpp"
#include "crt.hpp"
#include "gpu_pass.hpp"

namespace crt {

const char* lottesHlsl();   // crt_lottes.cpp
const char* geomHlsl();     // crt_geom.cpp

}  // namespace crt

namespace host {

// Runs the chosen filter over the game's picture. Everything it makes belongs to the renderer it was opened with: close() before the renderer is destroyed.
class CrtFilter {
 public:
  ~CrtFilter() { close(); }
  void close();

  // The picture (kWidth x `height` colours as 0xAARRGGBB, rows `top` to `top + rows` are the ones shown) through `filter`, drawn into a texture of `outWidth` x `outHeight`
  // pixels (the size of the picture on the screen). Returns the texture, valid until the next call, or nullptr with `error` set. Opens itself on the first call.
  SDL_Texture* run(SDL_Renderer* renderer, crt::Filter filter, const std::vector<std::uint32_t>& pixels, int height, int top, int rows, int outWidth, int outHeight, std::string& error);

 private:
  GpuPasses gpu_;
  SDL_Renderer* renderer_ = nullptr;
  int shader_[crt::kFilters] = {-1, -1, -1, -1};
  SDL_Texture* source_ = nullptr;
  SDL_Texture* target_ = nullptr;
  int sourceRows_ = 0, targetW_ = 0, targetH_ = 0;
};

}  // namespace host
