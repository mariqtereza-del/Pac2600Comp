// The shader passes of the "GDV NTSC 2600" CRT filter (Stage I11.4, DECISIONS.md D-017 and T-I11-4): the crt-guest-advanced-ntsc preset of the libretro shader collection,
// translated to HLSL by tools/i11_slang_to_hlsl.py. The tables are in gdv_generated.cpp and gdv_luts.cpp (generated, never edited by hand); gdv_chain runs them.
#pragma once

namespace gdv {

enum class Scale { Source, Viewport, Absolute };

// One value of a constant buffer: where it is (`block` 'p' = push constants, register b0; 'u' = the UBO of the shader, register b1), its kind (v = float4, f = float, u = uint,
// i = int) and its byte offset. A float4 whose name ends in "Size" is the size of a texture (w, h, 1/w, 1/h); FrameCount is the frame number; the rest are settings (kParams).
struct Member {
  char block, kind;
  const char* name;
  int offset;
};

struct Pass {
  const char* name;
  const char* vertex;                // HLSL, entry point `main`
  const char* pixel;                 // HLSL, entry point `main`
  const Member* members;
  int memberCount;
  const char* const* textures;       // in the order of their registers t0, t1, ...: Source, Original, OriginalHistory0, an alias, <alias>Feedback or a table
  int textureCount;
  bool linear, floatFramebuffer, mipmapInput;
  Scale scaleX, scaleY;
  float factorX, factorY;
  const char* alias;                 // "" = none
};

struct Param {
  const char* name;
  float value;
};

struct Lut {
  const char* name;
  const unsigned char* png;
  unsigned long long size;
  bool linear;
};

extern const Pass kPasses[];
extern const int kPassCount;
extern const Param kParams[];
extern const int kParamCount;
extern const Lut kLuts[];
extern const int kLutCount;

}  // namespace gdv
