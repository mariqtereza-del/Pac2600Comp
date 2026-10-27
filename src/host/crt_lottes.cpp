// CRT-Lottes ported to HLSL (Stage I11.2, DECISIONS.md D-017 and T-I11-2). The original is PUBLIC DOMAIN CRT STYLED SCAN-LINE SHADER by Timothy Lottes, in the libretro
// collection as crt/shaders/crt-lottes.slang (third_party\shaders\libretro\crt\shaders\crt-lottes.slang, MANIFEST.txt has its SHA-256). It is public domain ("Please take
// and use, change, or whatever"). The port keeps the algorithm and the default settings of the original; only what the languages and this program need differs:
//  - the picture comes in as a texture (Source) and goes out in linear light into a float texture (the original ends with ToSrgb for an sRGB framebuffer; SDL treats
//    a float texture as linear light, which it then shows correctly), so the ToSrgb at the end is left out;
//  - `inputLinear` says whether the renderer hands an 8-bit texture over already decoded (the HDR renderer) or as the standard values (the standard one); the original's
//    ToLinear (scaleInLinearGamma = 1) is then applied or not;
//  - the pixel position for the shadow mask is SV_Position (the same number as vTexCoord / OutputSize.zw).
#include "crt_filters.hpp"

namespace crt {

const char* lottesHlsl()
{
  return R"hlsl(
Texture2D Source : register(t0);
SamplerState point0 : register(s0);

cbuffer b0 : register(b0)
{
  float4 SourceSize;    // width, height, 1 / width, 1 / height of the picture the game made
  float4 OutputSize;    // the same for the picture drawn
  float4 p0;            // hardScan, hardPix, warpX, warpY
  float4 p1;            // maskDark, maskLight, shadowMask, brightBoost
  float4 p2;            // hardBloomPix, hardBloomScan, bloomAmount, shape
  float4 p3;            // inputLinear, scaleInLinearGamma, unused, unused
};

#define hardScan p0.x
#define hardPix p0.y
#define warpX p0.z
#define warpY p0.w
#define maskDark p1.x
#define maskLight p1.y
#define shadowMask p1.z
#define brightBoost p1.w
#define hardBloomPix p2.x
#define hardBloomScan p2.y
#define bloomAmount p2.z
#define shape p2.w
#define inputLinear p3.x
#define scaleInLinearGamma p3.y

float ToLinear1(float c)
{
  if(scaleInLinearGamma == 0) return c;
  return (c <= 0.04045) ? c / 12.92 : pow((c + 0.055) / 1.055, 2.4);
}
float3 ToLinear(float3 c)
{
  if(inputLinear > 0.5) return c;   // already linear light
  return float3(ToLinear1(c.r), ToLinear1(c.g), ToLinear1(c.b));
}

// Nearest emulated sample given floating point position and texel offset.
float3 Fetch(float2 pos, float2 off)
{
  pos = (floor(pos * SourceSize.xy + off) + float2(0.5, 0.5)) / SourceSize.xy;
  if(pos.x < 0.0 || pos.x > 1.0 || pos.y < 0.0 || pos.y > 1.0) return float3(0.0, 0.0, 0.0);   // off the picture is black (the original's clamp_to_border)
  return ToLinear(brightBoost * Source.SampleLevel(point0, pos, 0).rgb);
}

// Distance in emulated pixels to nearest texel.
float2 Dist(float2 pos)
{
  pos = pos * SourceSize.xy;
  return -((pos - floor(pos)) - float2(0.5, 0.5));
}

// 1D Gaussian.
float Gaus(float pos, float scale) { return exp2(scale * pow(abs(pos), shape)); }

// 3-tap Gaussian filter along horz line.
float3 Horz3(float2 pos, float off)
{
  float3 b = Fetch(pos, float2(-1.0, off));
  float3 c = Fetch(pos, float2(0.0, off));
  float3 d = Fetch(pos, float2(1.0, off));
  float dst = Dist(pos).x;
  float scale = hardPix;
  float wb = Gaus(dst - 1.0, scale);
  float wc = Gaus(dst + 0.0, scale);
  float wd = Gaus(dst + 1.0, scale);
  return (b * wb + c * wc + d * wd) / (wb + wc + wd);
}

// 5-tap Gaussian filter along horz line.
float3 Horz5(float2 pos, float off)
{
  float3 a = Fetch(pos, float2(-2.0, off));
  float3 b = Fetch(pos, float2(-1.0, off));
  float3 c = Fetch(pos, float2(0.0, off));
  float3 d = Fetch(pos, float2(1.0, off));
  float3 e = Fetch(pos, float2(2.0, off));
  float dst = Dist(pos).x;
  float scale = hardPix;
  float wa = Gaus(dst - 2.0, scale);
  float wb = Gaus(dst - 1.0, scale);
  float wc = Gaus(dst + 0.0, scale);
  float wd = Gaus(dst + 1.0, scale);
  float we = Gaus(dst + 2.0, scale);
  return (a * wa + b * wb + c * wc + d * wd + e * we) / (wa + wb + wc + wd + we);
}

// 7-tap Gaussian filter along horz line.
float3 Horz7(float2 pos, float off)
{
  float3 a = Fetch(pos, float2(-3.0, off));
  float3 b = Fetch(pos, float2(-2.0, off));
  float3 c = Fetch(pos, float2(-1.0, off));
  float3 d = Fetch(pos, float2(0.0, off));
  float3 e = Fetch(pos, float2(1.0, off));
  float3 f = Fetch(pos, float2(2.0, off));
  float3 g = Fetch(pos, float2(3.0, off));
  float dst = Dist(pos).x;
  float scale = hardBloomPix;
  float wa = Gaus(dst - 3.0, scale);
  float wb = Gaus(dst - 2.0, scale);
  float wc = Gaus(dst - 1.0, scale);
  float wd = Gaus(dst + 0.0, scale);
  float we = Gaus(dst + 1.0, scale);
  float wf = Gaus(dst + 2.0, scale);
  float wg = Gaus(dst + 3.0, scale);
  return (a * wa + b * wb + c * wc + d * wd + e * we + f * wf + g * wg) / (wa + wb + wc + wd + we + wf + wg);
}

// Return scanline weight.
float Scan(float2 pos, float off) { float dst = Dist(pos).y; return Gaus(dst + off, hardScan); }

// Return scanline weight for bloom.
float BloomScan(float2 pos, float off) { float dst = Dist(pos).y; return Gaus(dst + off, hardBloomScan); }

// Allow nearest three lines to effect pixel.
float3 Tri(float2 pos)
{
  float3 a = Horz3(pos, -1.0);
  float3 b = Horz5(pos, 0.0);
  float3 c = Horz3(pos, 1.0);
  float wa = Scan(pos, -1.0);
  float wb = Scan(pos, 0.0);
  float wc = Scan(pos, 1.0);
  return a * wa + b * wb + c * wc;
}

// Small bloom.
float3 Bloom(float2 pos)
{
  float3 a = Horz5(pos, -2.0);
  float3 b = Horz7(pos, -1.0);
  float3 c = Horz7(pos, 0.0);
  float3 d = Horz7(pos, 1.0);
  float3 e = Horz5(pos, 2.0);
  float wa = BloomScan(pos, -2.0);
  float wb = BloomScan(pos, -1.0);
  float wc = BloomScan(pos, 0.0);
  float wd = BloomScan(pos, 1.0);
  float we = BloomScan(pos, 2.0);
  return a * wa + b * wb + c * wc + d * wd + e * we;
}

// Distortion of scanlines, and end of screen alpha.
float2 Warp(float2 pos)
{
  pos = pos * 2.0 - 1.0;
  pos *= float2(1.0 + (pos.y * pos.y) * warpX, 1.0 + (pos.x * pos.x) * warpY);
  return pos * 0.5 + 0.5;
}

// Shadow mask.
float3 Mask(float2 pos)
{
  float3 mask = float3(maskDark, maskDark, maskDark);

  // Very compressed TV style shadow mask.
  if(shadowMask == 1.0) {
    float lineMask = maskLight;
    float odd = 0.0;
    if(frac(pos.x * 0.166666666) < 0.5) odd = 1.0;
    if(frac((pos.y + odd) * 0.5) < 0.5) lineMask = maskDark;
    pos.x = frac(pos.x * 0.333333333);
    if(pos.x < 0.333) mask.r = maskLight;
    else if(pos.x < 0.666) mask.g = maskLight;
    else mask.b = maskLight;
    mask *= lineMask;
  }
  // Aperture-grille.
  else if(shadowMask == 2.0) {
    pos.x = frac(pos.x * 0.333333333);
    if(pos.x < 0.333) mask.r = maskLight;
    else if(pos.x < 0.666) mask.g = maskLight;
    else mask.b = maskLight;
  }
  // Stretched VGA style shadow mask (same as prior shaders).
  else if(shadowMask == 3.0) {
    pos.x += pos.y * 3.0;
    pos.x = frac(pos.x * 0.166666666);
    if(pos.x < 0.333) mask.r = maskLight;
    else if(pos.x < 0.666) mask.g = maskLight;
    else mask.b = maskLight;
  }
  // VGA style shadow mask.
  else if(shadowMask == 4.0) {
    pos.xy = floor(pos.xy * float2(1.0, 0.5));
    pos.x += pos.y * 3.0;
    pos.x = frac(pos.x * 0.166666666);
    if(pos.x < 0.333) mask.r = maskLight;
    else if(pos.x < 0.666) mask.g = maskLight;
    else mask.b = maskLight;
  }
  return mask;
}

float4 main(float4 position : SV_Position, float2 uv : TEXCOORD0) : SV_Target
{
  float2 pos = Warp(uv);
  float3 outColor = Tri(pos);
  outColor.rgb += Bloom(pos) * bloomAmount;   // DO_BLOOM
  if(shadowMask > 0.0) outColor.rgb *= Mask(position.xy * 1.000001);
  return float4(outColor.rgb, 1.0);   // linear light: the original's ToSrgb is for an sRGB framebuffer and is left out (see the top of this file)
}
)hlsl";
}

}  // namespace crt
