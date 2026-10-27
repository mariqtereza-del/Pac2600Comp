#include "gdv_chain.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

#include "gdv_data.hpp"

namespace host {

namespace {
// The last step: the encoded colours the last pass leaves (the values a screen of the old kind was given) as linear light, which is what the renderer expects of a
// float texture (the same convention as the other filters, T-I11-1).
const char* kToLinear =
    "Texture2D picture : register(t0);\n"
    "SamplerState point_ : register(s0);\n"
    "float4 main(float4 position : SV_Position, float2 uv : TEXCOORD0) : SV_Target {\n"
    "  float3 c = saturate(picture.SampleLevel(point_, uv, 0.0).rgb);\n"
    "  float3 low = c / 12.92;\n"
    "  float3 high = pow((c + 0.055) / 1.055, 2.4);\n"
    "  return float4(c <= 0.04045 ? low : high, 1.0);\n"
    "}\n";

bool endsWith(const std::string& text, const char* tail)
{
  const std::size_t n = std::strlen(tail);
  return text.size() > n && text.compare(text.size() - n, n, tail) == 0;
}

int sizeFor(gdv::Scale kind, float factor, int source, int viewport)
{
  const float base = kind == gdv::Scale::Source ? static_cast<float>(source) * factor : kind == gdv::Scale::Viewport ? static_cast<float>(viewport) * factor : factor;
  return std::max(1, static_cast<int>(base));
}
}  // namespace

void GdvChain::release()
{
  for(PassState& p : passes_)
    for(int& image : p.image) { gpu_.freeImage(image); image = -1; }
  gpu_.freeImage(original_);
  gpu_.freeImage(final_);
  original_ = final_ = -1;
  if(target_) SDL_DestroyTexture(target_);
  target_ = nullptr;
  ready_ = false;
  lastFrame_ = -1;
  lastPicture_.clear();
}

void GdvChain::close()
{
  release();
  passes_.clear();
  tables_.clear();
  toLinear_ = -1;
  gpu_.close();
  renderer_ = nullptr;
  rows_ = outWidth_ = outHeight_ = 0;
}

bool GdvChain::loadTables(std::string& error)
{
  for(int k = 0; k < gdv::kLutCount; ++k) {
    SDL_Surface* png = SDL_LoadPNG_IO(SDL_IOFromConstMem(gdv::kLuts[k].png, static_cast<std::size_t>(gdv::kLuts[k].size)), true);
    SDL_Surface* bgra = png ? SDL_ConvertSurface(png, SDL_PIXELFORMAT_BGRA32) : nullptr;
    if(png) SDL_DestroySurface(png);
    if(!bgra) { error = std::string("a colour table could not be read: ") + SDL_GetError(); return false; }
    const int image = gpu_.makeImage(bgra->w, bgra->h, GpuGraph::Format::Unorm8, false);
    const bool ok = image >= 0 && gpu_.upload(image, bgra->pixels, bgra->pitch);
    SDL_DestroySurface(bgra);
    if(!ok) { error = "a colour table could not be made"; return false; }
    tables_.push_back(image);
  }
  return true;
}

bool GdvChain::build(SDL_Renderer* renderer, int rows, int outWidth, int outHeight, std::string& error)
{
  if(renderer != renderer_) {
    close();
    if(!gpu_.open(renderer)) { error = "the renderer is not a Direct3D 11 one"; return false; }
    renderer_ = renderer;
  }
  release();
  if(passes_.empty()) {   // the shaders and tables, once
    for(int i = 0; i < gdv::kParamCount; ++i) settings_[gdv::kParams[i].name] = gdv::kParams[i].value;
    for(int i = 0; i < gdv::kPassCount; ++i) {
      PassState p;
      std::string why;
      p.shader = gpu_.makeShader(gdv::kPasses[i].vertex, gdv::kPasses[i].pixel, why);
      if(p.shader < 0) { error = std::string("GDV NTSC 2600, pass ") + std::to_string(i) + " (" + gdv::kPasses[i].name + "): " + why; passes_.clear(); return false; }
      passes_.push_back(p);
    }
    toLinear_ = gpu_.makeShader("", kToLinear, error);
    if(toLinear_ < 0 || !loadTables(error)) { passes_.clear(); return false; }
  }
  original_ = gpu_.makeImage(Emulator::kWidth, rows, GpuGraph::Format::Unorm8, false);
  int sourceW = Emulator::kWidth, sourceH = rows;
  for(int i = 0; i < gdv::kPassCount && original_ >= 0; ++i) {
    const gdv::Pass& pass = gdv::kPasses[i];
    PassState& state = passes_[static_cast<std::size_t>(i)];
    state.width = sizeFor(pass.scaleX, pass.factorX, sourceW, outWidth);
    state.height = sizeFor(pass.scaleY, pass.factorY, sourceH, outHeight);
    bool feedback = false;   // read again one frame later by some pass
    for(int j = 0; j < gdv::kPassCount; ++j)
      for(int t = 0; t < gdv::kPasses[j].textureCount; ++t)
        if(*pass.alias && endsWith(gdv::kPasses[j].textures[t], "Feedback") && std::string(gdv::kPasses[j].textures[t]) == std::string(pass.alias) + "Feedback") feedback = true;
    const bool mips = i + 1 < gdv::kPassCount && gdv::kPasses[i + 1].mipmapInput;
    const GpuGraph::Format format = pass.floatFramebuffer ? GpuGraph::Format::Float16 : GpuGraph::Format::Unorm8;
    state.image[0] = gpu_.makeImage(state.width, state.height, format, mips);
    if(feedback) state.image[1] = gpu_.makeImage(state.width, state.height, format, mips);
    state.current = 0;
    if(state.image[0] < 0 || (feedback && state.image[1] < 0)) { original_ = -1; break; }
    sourceW = state.width; sourceH = state.height;
  }
  target_ = original_ < 0 ? nullptr : SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA64_FLOAT, SDL_TEXTUREACCESS_TARGET, outWidth, outHeight);
  if(target_) SDL_SetTextureScaleMode(target_, SDL_SCALEMODE_NEAREST);
  final_ = target_ ? gpu_.wrapTarget(target_) : -1;
  if(final_ < 0) { error = original_ < 0 ? "the images of the filter could not be made (too big a window?)" : SDL_GetError(); release(); return false; }
  rows_ = rows; outWidth_ = outWidth; outHeight_ = outHeight;
  ready_ = true;
  return true;
}

int GdvChain::imageOf(const std::string& name, int pass, bool* linear) const
{
  *linear = gdv::kPasses[pass].linear;
  if(name == "Source") return pass == 0 ? original_ : passes_[static_cast<std::size_t>(pass) - 1].image[passes_[static_cast<std::size_t>(pass) - 1].current];
  if(name == "Original" || name == "OriginalHistory0") return original_;
  for(int k = 0; k < gdv::kLutCount; ++k)
    if(name == gdv::kLuts[k].name) { *linear = gdv::kLuts[k].linear; return tables_[static_cast<std::size_t>(k)]; }
  const bool feedback = endsWith(name, "Feedback");
  const std::string alias = feedback ? name.substr(0, name.size() - 8) : name;
  for(int p = 0; p < gdv::kPassCount; ++p) {
    if(alias != gdv::kPasses[p].alias || !*gdv::kPasses[p].alias) continue;
    const PassState& s = passes_[static_cast<std::size_t>(p)];
    if(!feedback) return s.image[s.current];
    return s.image[1] < 0 ? -1 : s.image[pass < p ? s.current : 1 - s.current];   // the frame before: what the pass left last time
  }
  return -1;
}

bool GdvChain::sizeOf(const std::string& name, int pass, float out[4]) const
{
  int w = 0, h = 0;
  if(name == "Output") { w = passes_[static_cast<std::size_t>(pass)].width; h = passes_[static_cast<std::size_t>(pass)].height; }
  else {
    bool linear = false;
    const int image = imageOf(name, pass, &linear);
    if(image < 0) return false;
    w = gpu_.width(image); h = gpu_.height(image);
  }
  out[0] = static_cast<float>(w); out[1] = static_cast<float>(h); out[2] = 1.0f / static_cast<float>(w); out[3] = 1.0f / static_cast<float>(h);
  return true;
}

void GdvChain::fill(int pass, long long frame, std::vector<std::uint8_t>& b0, std::vector<std::uint8_t>& b1) const
{
  const gdv::Pass& p = gdv::kPasses[pass];
  std::size_t n0 = 0, n1 = 0;
  for(int i = 0; i < p.memberCount; ++i) {
    std::size_t& n = p.members[i].block == 'p' ? n0 : n1;
    n = std::max(n, static_cast<std::size_t>(p.members[i].offset) + (p.members[i].kind == 'v' ? 16u : 4u));
  }
  b0.assign((n0 + 15) / 16 * 16, 0);
  b1.assign((n1 + 15) / 16 * 16, 0);
  for(int i = 0; i < p.memberCount; ++i) {
    const gdv::Member& m = p.members[i];
    std::uint8_t* at = (m.block == 'p' ? b0 : b1).data() + m.offset;
    const std::string name = m.name;
    if(m.kind == 'v') {
      float size[4] = {0, 0, 0, 0};
      if(endsWith(name, "Size")) sizeOf(name.substr(0, name.size() - 4), pass, size);
      std::memcpy(at, size, sizeof size);
    } else if(name == "FrameCount") {
      const std::uint32_t n = static_cast<std::uint32_t>(frame);
      std::memcpy(at, &n, sizeof n);
    } else {
      const auto found = settings_.find(name);
      const float value = found == settings_.end() ? 0.0f : found->second;
      if(m.kind == 'f') std::memcpy(at, &value, sizeof value);
      else { const std::int32_t n = static_cast<std::int32_t>(value); std::memcpy(at, &n, sizeof n); }
    }
  }
}

bool GdvChain::runPass(int index, long long frame, std::string& error)
{
  const gdv::Pass& pass = gdv::kPasses[index];
  PassState& state = passes_[static_cast<std::size_t>(index)];
  if(state.image[1] >= 0) state.current ^= 1;   // this pass's output of the frame before stays in the other image
  std::vector<GpuGraph::Input> inputs;
  for(int t = 0; t < pass.textureCount; ++t) {
    const std::string name = pass.textures[t];
    bool linear = false;
    const int image = imageOf(name, index, &linear);
    if(image < 0) { error = std::string("GDV NTSC 2600: pass ") + pass.name + " reads \"" + name + "\", which does not exist"; return false; }
    inputs.push_back({image, linear, pass.mipmapInput && name == "Source"});
  }
  std::vector<std::uint8_t> b0, b1;
  fill(index, frame, b0, b1);
  return gpu_.draw(state.shader, inputs, state.image[state.current], b0.data(), static_cast<int>(b0.size()), b1.empty() ? nullptr : b1.data(), static_cast<int>(b1.size()), error);
}

SDL_Texture* GdvChain::run(SDL_Renderer* renderer, const std::vector<std::uint32_t>& pixels, int top, int rows, int outWidth, int outHeight, long long frame, std::string& error)
{
  if(!ready_ || renderer != renderer_ || rows != rows_ || outWidth != outWidth_ || outHeight != outHeight_)
    if(!build(renderer, rows, outWidth, outHeight, error)) return nullptr;
  std::vector<std::uint32_t> picture(pixels.begin() + static_cast<std::ptrdiff_t>(top) * Emulator::kWidth, pixels.begin() + static_cast<std::ptrdiff_t>(top + rows) * Emulator::kWidth);
  for(std::uint32_t& c : picture) c |= 0xFF000000u;
  if(frame == lastFrame_ && picture == lastPicture_) return target_;
  if(!gpu_.upload(original_, picture.data(), Emulator::kWidth * 4)) { error = "the picture could not be handed to the filter"; return nullptr; }
  const char* stop = SDL_getenv("PACMAN_GDV_STOP");   // TEST ONLY: show the output of this pass instead of the last one
  const int stopAfter = stop ? std::clamp(std::atoi(stop), 0, gdv::kPassCount - 1) : gdv::kPassCount - 1;
  for(int i = 0; i <= stopAfter; ++i)
    if(!runPass(i, frame, error)) return nullptr;
  const PassState& last = passes_[static_cast<std::size_t>(stopAfter)];
  if(!gpu_.draw(toLinear_, {{last.image[last.current], false, false}}, final_, nullptr, 0, nullptr, 0, error)) return nullptr;
  lastFrame_ = frame;
  lastPicture_ = std::move(picture);
  return target_;
}

}  // namespace host
