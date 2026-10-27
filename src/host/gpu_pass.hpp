// Running our own pixel shaders on SDL's own Direct3D 11 renderer (Stage I11.1, DECISIONS.md D-017 and T-I11-1). SDL's standard renderer has no custom shaders, but it
// gives out its Direct3D 11 device and the Direct3D textures behind its textures, and says how to work beside it (SDL_FlushRenderer): so a shader pass here reads one
// SDL texture and draws into another, and SDL then draws that texture like any other. This keeps everything else as it is: the window, the vsync, HDR (the renderer's
// linear colour space), the menu and the smooth picture. Windows only, like the program.
#pragma once
#include <memory>
#include <string>
#include <vector>

#include "common.hpp"

namespace host {

class GpuPasses {
 public:
  GpuPasses();
  ~GpuPasses();
  GpuPasses(const GpuPasses&) = delete;
  GpuPasses& operator=(const GpuPasses&) = delete;

  // Takes the Direct3D 11 device of the renderer (false when the renderer is not a Direct3D 11 one). Everything made here belongs to that renderer: close() before it
  // is destroyed.
  bool open(SDL_Renderer* renderer);
  void close();
  bool isOpen() const;

  // Compiles a pixel shader written in HLSL (entry point `main`; it is given `float4 position : SV_Position, float2 uv : TEXCOORD0`; constants in `cbuffer b0 : register(b0)`;
  // `Texture2D input : register(t0)`; samplers `s0` point and `s1` linear). Returns a number to run it with, or -1 with the compiler's message in `error`.
  int compile(const std::string& hlsl, std::string& error);

  // Whether a texture of ordinary 8-bit colours is read by a shader as linear light (the renderer is in the HDR colour space, and SDL then makes such textures sRGB
  // textures that are decoded as they are read) or as the standard (encoded) values (the standard renderer). Float textures are linear light in both.
  bool readsEightBitAsLinear() const;

  // Draws `input` through shader `shader` over the whole of `output` (an SDL texture made as a render target, SDL_TEXTUREACCESS_TARGET). `constants` (a multiple of 16 bytes,
  // at most 4096) are the contents of the constant buffer. False with a message in `error` if it could not.
  bool run(int shader, SDL_Texture* input, SDL_Texture* output, const void* constants, int constantBytes, std::string& error);

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace host
