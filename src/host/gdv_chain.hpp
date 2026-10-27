// The "GDV NTSC 2600" CRT filter (Stage I11.4, DECISIONS.md D-017 and T-I11-4): the 19 passes of the crt-guest-advanced-ntsc preset (gdv_data) run one after the other on the
// game's picture, as a RetroArch preset runs them: each pass reads the output of the one before it ("Source"), the picture itself ("Original"), the outputs of earlier
// passes by their names, the output of a pass in the frame before ("...Feedback") and four colour tables; each draws into an image of the size its settings give.
// Windows only (Direct3D 11), like gpu_graph.
#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "common.hpp"
#include "gpu_graph.hpp"

namespace host {

class GdvChain {
 public:
  ~GdvChain() { close(); }
  void close();

  // The picture (kWidth x rows colours as 0xAARRGGBB; `top` = its first row in `pixels`) through all passes, into a texture of `outWidth` x `outHeight` pixels (the size of
  // the picture on the screen) in linear light. `frame` is the game's frame number: the passes run once for each new one (the NTSC colour and the afterglow depend on it);
  // asked again for the same frame and size, the texture is returned as it was. The texture is valid until the next call; nullptr with `error` set if it could not be made.
  SDL_Texture* run(SDL_Renderer* renderer, const std::vector<std::uint32_t>& pixels, int top, int rows, int outWidth, int outHeight, long long frame, std::string& error);

 private:
  struct PassState {
    int shader = -1;
    int image[2] = {-1, -1};   // [1] only for a pass whose output is read again one frame later
    int current = 0;
    int width = 0, height = 0;
  };

  bool build(SDL_Renderer* renderer, int rows, int outWidth, int outHeight, std::string& error);
  void release();
  bool loadTables(std::string& error);
  bool runPass(int index, long long frame, std::string& error);
  int imageOf(const std::string& name, int pass, bool* linear) const;
  bool sizeOf(const std::string& name, int pass, float out[4]) const;
  void fill(int pass, long long frame, std::vector<std::uint8_t>& b0, std::vector<std::uint8_t>& b1) const;

  GpuGraph gpu_;
  SDL_Renderer* renderer_ = nullptr;
  std::vector<PassState> passes_;
  std::vector<int> tables_;             // images of the colour tables
  std::unordered_map<std::string, float> settings_;
  int original_ = -1;                   // the game's picture
  int toLinear_ = -1;                   // the last small step, from the encoded colours of the last pass to linear light
  int final_ = -1;                      // the image wrapped around target_
  SDL_Texture* target_ = nullptr;
  int rows_ = 0, outWidth_ = 0, outHeight_ = 0;
  long long lastFrame_ = -1;
  std::vector<std::uint32_t> lastPicture_;   // what the passes last ran on
  bool ready_ = false;
};

}  // namespace host
