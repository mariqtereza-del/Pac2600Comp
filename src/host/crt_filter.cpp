#include "crt_filters.hpp"

#include <cstring>

namespace host {

void CrtFilter::close()
{
  if(source_) SDL_DestroyTexture(source_);
  if(target_) SDL_DestroyTexture(target_);
  source_ = target_ = nullptr;
  sourceRows_ = targetW_ = targetH_ = 0;
  gpu_.close();
  for(int& s : shader_) s = -1;
  renderer_ = nullptr;
}

namespace {
// The numbers a ported shader reads: the sizes, and its settings (the defaults of the original shader).
struct Constants {
  float sourceSize[4], outputSize[4];
  float p0[4], p1[4], p2[4], p3[4];
};
}  // namespace

SDL_Texture* CrtFilter::run(SDL_Renderer* renderer, crt::Filter filter, const std::vector<std::uint32_t>& pixels, int height, int top, int rows, int outWidth, int outHeight, std::string& error)
{
  if(renderer != renderer_) {
    close();
    if(!gpu_.open(renderer)) { error = "the renderer is not a Direct3D 11 one"; return nullptr; }
    renderer_ = renderer;
  }
  int& shader = shader_[static_cast<int>(filter)];
  if(shader < 0) {
    const char* text = filter == crt::Filter::Lottes ? crt::lottesHlsl() : filter == crt::Filter::Geom ? crt::geomHlsl() : nullptr;
    if(!text) { error = "this filter is not ported yet"; return nullptr; }
    shader = gpu_.compile(text, error);
    if(shader < 0) return nullptr;
  }
  if(!source_ || sourceRows_ != rows) {   // the picture's texture: the rows that are shown
    if(source_) SDL_DestroyTexture(source_);
    gpu_.forgetTextures();
    source_ = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_STATIC, Emulator::kWidth, rows);
    sourceRows_ = rows;
    if(!source_) { error = SDL_GetError(); return nullptr; }
    SDL_SetTextureScaleMode(source_, SDL_SCALEMODE_NEAREST);
  }
  if(!target_ || targetW_ != outWidth || targetH_ != outHeight) {   // what the filter draws: linear light, 16-bit float, one texel to a pixel of the screen
    if(target_) SDL_DestroyTexture(target_);
    gpu_.forgetTextures();
    target_ = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA64_FLOAT, SDL_TEXTUREACCESS_TARGET, outWidth, outHeight);
    targetW_ = outWidth;
    targetH_ = outHeight;
    if(!target_) { error = SDL_GetError(); return nullptr; }
    SDL_SetTextureScaleMode(target_, SDL_SCALEMODE_NEAREST);
  }
  (void)height;
  SDL_UpdateTexture(source_, nullptr, pixels.data() + static_cast<std::size_t>(top) * Emulator::kWidth, Emulator::kWidth * 4);

  Constants c{};
  const float sw = static_cast<float>(Emulator::kWidth), sh = static_cast<float>(rows), ow = static_cast<float>(outWidth), oh = static_cast<float>(outHeight);
  const float sourceSize[4] = {sw, sh, 1.0f / sw, 1.0f / sh}, outputSize[4] = {ow, oh, 1.0f / ow, 1.0f / oh};
  std::memcpy(c.sourceSize, sourceSize, sizeof sourceSize);
  std::memcpy(c.outputSize, outputSize, sizeof outputSize);
  const float inputLinear = gpu_.readsEightBitAsLinear() ? 1.0f : 0.0f;
  if(filter == crt::Filter::Lottes) {   // the defaults of crt-lottes.slang
    const float p0[4] = {-8.0f, -3.0f, 0.031f, 0.041f};   // hardScan, hardPix, warpX, warpY
    const float p1[4] = {0.5f, 1.5f, 3.0f, 1.0f};          // maskDark, maskLight, shadowMask, brightBoost
    const float p2[4] = {-1.5f, -2.0f, 0.15f, 2.0f};       // hardBloomPix, hardBloomScan, bloomAmount, shape
    std::memcpy(c.p0, p0, sizeof p0);
    std::memcpy(c.p1, p1, sizeof p1);
    std::memcpy(c.p2, p2, sizeof p2);
  }
  if(filter == crt::Filter::Geom) {   // the defaults of crt-geom.slang
    const float p0[4] = {2.4f, 2.2f, 1.5f, 2.0f};          // CRTgamma, monitorgamma, d, R
    const float p1[4] = {0.03f, 1000.0f, 100.0f, 100.0f};  // cornersize, cornersmooth, overscan_x, overscan_y
    const float p2[4] = {0.3f, 0.0f, 1.0f, 0.3f};          // DOTMASK, DOTMASK_brightboost, SHARPER, scanline_weight
    const float p3[4] = {0.0f, 1.0f, 1.0f, inputLinear};   // lum, interlace_detect, CURVATURE, inputLinear
    std::memcpy(c.p0, p0, sizeof p0);
    std::memcpy(c.p1, p1, sizeof p1);
    std::memcpy(c.p2, p2, sizeof p2);
    std::memcpy(c.p3, p3, sizeof p3);
  }
  if(filter == crt::Filter::Lottes) {
    c.p3[0] = inputLinear;
    c.p3[1] = 1.0f;   // scaleInLinearGamma
  }
  if(!gpu_.run(shader, source_, target_, &c, sizeof c, error)) return nullptr;
  return target_;
}

}  // namespace host
