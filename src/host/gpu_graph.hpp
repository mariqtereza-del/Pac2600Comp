// Chains of shader passes on SDL's own Direct3D 11 renderer (Stage I11.4, DECISIONS.md D-017 and T-I11-4). GpuPasses (gpu_pass) runs one pixel shader over one input; the
// GDV NTSC 2600 filter has 19 passes with several inputs each, their own vertex shaders, targets of different sizes and formats, mipmaps and tables, so this class holds
// "images" (Direct3D textures that can be drawn into and read from) and draws a pass from any number of them into another. Like GpuPasses it works beside SDL: what SDL
// has queued is done first (SDL_FlushRenderer) and SDL's Direct3D state is put back after each pass. Windows only, like the program.
#pragma once
#include <memory>
#include <string>
#include <vector>

#include "common.hpp"

namespace host {

class GpuGraph {
 public:
  GpuGraph();
  ~GpuGraph();
  GpuGraph(const GpuGraph&) = delete;
  GpuGraph& operator=(const GpuGraph&) = delete;

  enum class Format { Unorm8, Float16 };   // Unorm8: B8G8R8A8, the values as stored (no sRGB decoding); Float16: four 16-bit floats

  // Takes the Direct3D 11 device of the renderer (false when it is not a Direct3D 11 one). Everything made here belongs to that renderer: close() before it is destroyed.
  bool open(SDL_Renderer* renderer);
  void close();
  bool isOpen() const;

  // An image of width x height texels to draw into and read from; `mips` = with a chain of smaller sizes (made by generateMips). Returns its number, or -1.
  int makeImage(int width, int height, Format format, bool mips);
  // An SDL texture made as a render target, to draw into (not owned: free the image before the texture is destroyed). Returns the image number, or -1.
  int wrapTarget(SDL_Texture* texture);
  void freeImage(int image);
  int width(int image) const;
  int height(int image) const;
  // Sets the contents of an Unorm8 image from BGRA bytes (`pitch` bytes a row).
  bool upload(int image, const void* bgra, int pitch);

  // Compiles a pass: vertex shader and pixel shader (HLSL, entry point `main`; the vertex shader gets SV_VertexID of a triangle that covers the target). An empty vertex
  // text means the plain one that gives `float4 position : SV_Position, float2 uv : TEXCOORD0`. Returns the shader's number, or -1 with the message in `error`.
  int makeShader(const std::string& vertex, const std::string& pixel, std::string& error);

  struct Input {
    int image;
    bool linear;   // linear or nearest filtering; outside the image it reads black (clamp to border)
    bool mips;     // with the smaller sizes of the image made first
  };
  // Draws `shader` into `target`, reading `inputs` (registers t0, t1, ... with samplers s0, s1, ...). b0 and b1 are the contents of the two constant buffers (null = none).
  bool draw(int shader, const std::vector<Input>& inputs, int target, const void* b0, int b0Bytes, const void* b1, int b1Bytes, std::string& error);

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace host
