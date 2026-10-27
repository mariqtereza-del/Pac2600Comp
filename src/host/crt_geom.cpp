// CRT-Geom ported to HLSL (Stage I11.3, DECISIONS.md D-017 and T-I11-3). The original is CRT-interlaced by cgwg, Themaister and DOLLS, in the libretro collection as
// crt/shaders/crt-geom.slang (third_party\shaders\libretro\crt\shaders\crt-geom.slang, MANIFEST.txt has its SHA-256), licensed under the GNU General Public License, version 2 or
// (at your option) any later version ("Feel free to distribute my shaders under the GPL", cgwg), which is the licence of this project's source. The port keeps the algorithm and
// the default settings of the preset (crt-geom.slangp: point sampling of the picture, one pass at the size of the screen). What differs, because of the languages and of this program:
//  - the original's vertex stage (the angles, the stretch of the curved screen, the size of a texel) is worked out at each pixel here, from the same formulas;
//  - only the settings the program uses are ported: the default screen (curvature on, radius 2, distance 1.5, no tilt, corner 0.03, overscan 100 percent, horizontal scanlines)
//    and the default mask pattern (1, the aperture grille); the other patterns of mask_weights_alpha and the vertical-scanline and aspect-inversion variants are not (they are
//    not reachable from this program's menu), and neither is the interlace field counter (the picture is never taller than 200 lines times two);
//  - the picture comes in as the standard (encoded) colour values the original expects: where the renderer hands an 8-bit texture over already decoded (the HDR renderer),
//    they are encoded again first, so that the original's pow(, CRTgamma) does what it was written to do;
//  - the picture goes out in linear light (a float texture): the original ends with pow(cout, 1 / monitorgamma), the encoding for a gamma 2.2 monitor, which the program does
//    not need because the renderer shows linear light as the screen wants it; what is written is the light the original's colour stands for, cout.
#include "crt_filters.hpp"

namespace crt {

const char* geomHlsl()
{
  return R"hlsl(
Texture2D Source : register(t0);
SamplerState point0 : register(s0);

cbuffer b0 : register(b0)
{
  float4 SourceSize;   // width, height, 1 / width, 1 / height of the picture the game made
  float4 OutputSize;   // the same for the picture drawn
  float4 p0;           // CRTgamma, monitorgamma, d, R
  float4 p1;           // cornersize, cornersmooth, overscan_x, overscan_y
  float4 p2;           // DOTMASK, DOTMASK_brightboost, SHARPER, scanline_weight
  float4 p3;           // lum, interlace_detect, CURVATURE, inputLinear
};

#define CRTgamma p0.x
#define monitorgamma p0.y
#define d p0.z
#define R p0.w
#define cornersize p1.x
#define cornersmooth p1.y
#define overscan_x p1.z
#define overscan_y p1.w
#define DOTMASK p2.x
#define DOTMASK_brightboost p2.y
#define SHARPER p2.z
#define scanline_weight p2.w
#define lum p3.x
#define interlace_detect p3.y
#define CURVATURE p3.z
#define inputLinear p3.w

#define FIX(c) max(abs(c), 1e-5)
#define PI 3.141592653589

static const float2 aspect = float2(1.0, 0.75);
static const float2 sinangle = float2(0.0, 0.0);   // x_tilt = y_tilt = 0
static const float2 cosangle = float2(1.0, 1.0);

float3 encodeSrgb(float3 c)
{
  return c <= 0.0031308 ? 12.92 * c : 1.055 * pow(c, 1.0 / 2.4) - 0.055;
}

float4 TEX2D(float2 c)
{
  float4 t = Source.SampleLevel(point0, c, 0);
  if(inputLinear > 0.5) t.rgb = encodeSrgb(t.rgb);   // the original expects the standard values
  return pow(t, float4(CRTgamma, CRTgamma, CRTgamma, CRTgamma));
}

float intersect(float2 xy)
{
  float A = dot(xy, xy) + d * d;
  float B = 2.0 * (R * (dot(xy, sinangle) - d * cosangle.x * cosangle.y) - d * d);
  float C = d * d + 2.0 * R * d * cosangle.x * cosangle.y;
  return (-B - sqrt(B * B - 4.0 * A * C)) / (2.0 * A);
}

float2 bkwtrans(float2 xy)
{
  float c = intersect(xy);
  float2 point2 = (float2(c, c) * xy - float2(-R, -R) * sinangle) / float2(R, R);
  float2 poc = point2 / cosangle;
  float2 tang = sinangle / cosangle;
  float A = dot(tang, tang) + 1.0;
  float B = -2.0 * dot(poc, tang);
  float C = dot(poc, poc) - 1.0;
  float a = (-B + sqrt(B * B - 4.0 * A * C)) / (2.0 * A);
  float2 uv = (point2 - a * sinangle) / cosangle;
  float r = FIX(R * acos(a));
  return uv * r / sin(r / R);
}

float2 fwtrans(float2 uv)
{
  float r = FIX(sqrt(dot(uv, uv)));
  uv *= sin(r / R) / r;
  float x = 1.0 - cos(r / R);
  float D = d / R + x * cosangle.x * cosangle.y + dot(uv, sinangle);
  return d * (uv * cosangle - x * sinangle) / D;
}

float3 maxscale()
{
  float2 c = bkwtrans(-R * sinangle / (1.0 + R / d * cosangle.x * cosangle.y));
  float2 a = float2(0.5, 0.5) * aspect;
  float2 lo = float2(fwtrans(float2(-a.x, c.y)).x, fwtrans(float2(c.x, -a.y)).y) / aspect;
  float2 hi = float2(fwtrans(float2(+a.x, c.y)).x, fwtrans(float2(c.x, +a.y)).y) / aspect;
  return float3((hi + lo) * aspect * 0.5, max(hi.x - lo.x, hi.y - lo.y));
}

// Calculate the influence of a scanline on the current pixel (the beam profile, gaussian if the width is 2 and not for larger widths).
float4 scanlineWeights(float distance, float4 color)
{
  float4 wid = 2.0 + 2.0 * pow(color, float4(4.0, 4.0, 4.0, 4.0));
  float4 weights = float4(distance / scanline_weight, distance / scanline_weight, distance / scanline_weight, distance / scanline_weight);
  return (lum + 1.4) * exp(-pow(weights * rsqrt(0.5 * wid), wid)) / (0.6 + 0.2 * wid);
}

float2 transform(float2 coord, float3 stretch)
{
  coord = (coord - float2(0.5, 0.5)) * aspect * stretch.z + stretch.xy;
  return bkwtrans(coord) / float2(overscan_x / 100.0, overscan_y / 100.0) / aspect + float2(0.5, 0.5);
}

float corner(float2 coord)
{
  coord = (coord - float2(0.5, 0.5)) * float2(overscan_x / 100.0, overscan_y / 100.0) + float2(0.5, 0.5);
  coord = min(coord, float2(1.0, 1.0) - coord) * aspect;
  float2 cdist = float2(cornersize, cornersize);
  coord = (cdist - min(coord, cdist));
  float dist = sqrt(dot(coord, coord));
  return clamp((cdist.x - dist) * cornersmooth, 0.0, 1.0);
}

float4 main(float4 position : SV_Position, float2 uv0 : TEXCOORD0) : SV_Target
{
  float2 vTexCoord = uv0 * 1.00001;
  // what the original's vertex stage works out
  float3 stretch = maxscale();
  float2 TextureSize = float2(SHARPER * SourceSize.x, SourceSize.y);
  float2 ilfac = float2(1.0, clamp(floor(SourceSize.y / (interlace_detect > 0.5 ? 200.0 : 1000.0)), 1.0, 2.0));
  float2 one = ilfac / TextureSize;   // the size of one texel, in texture coordinates

  // Texture coordinates of the texel containing the active pixel.
  float2 xy;
  if(CURVATURE > 0.5) xy = transform(vTexCoord, stretch);
  else xy = vTexCoord;
  float cval = corner(xy);

  // Of all the pixels that are mapped onto the texel we are rendering, which one is it?
  float2 ilvec = float2(0.0, 0.0);
  float2 ratio_scale = (xy * TextureSize - float2(0.5, 0.5) + ilvec) / ilfac;
  float2 uv_ratio = frac(ratio_scale);

  // Snap to the center of the underlying texel.
  xy = (floor(ratio_scale) * ilfac + float2(0.5, 0.5) - ilvec) / TextureSize;

  // Lanczos scaling coefficients of the neighbour texels in a scanline.
  float4 coeffs = PI * float4(1.0 + uv_ratio.x, uv_ratio.x, 1.0 - uv_ratio.x, 2.0 - uv_ratio.x);
  coeffs = FIX(coeffs);                                       // prevent division by zero
  coeffs = 2.0 * sin(coeffs) * sin(coeffs / 2.0) / (coeffs * coeffs);   // Lanczos2 kernel
  coeffs /= dot(coeffs, float4(1.0, 1.0, 1.0, 1.0));          // normalize

  // The effective colour of the current and the next scanline at the horizontal place of the current pixel.
  float4 col = clamp(TEX2D(xy + float2(-one.x, 0.0)) * coeffs.x + TEX2D(xy) * coeffs.y + TEX2D(xy + float2(one.x, 0.0)) * coeffs.z + TEX2D(xy + float2(2.0 * one.x, 0.0)) * coeffs.w, 0.0, 1.0);
  float4 col2 = clamp(TEX2D(xy + float2(-one.x, one.y)) * coeffs.x + TEX2D(xy + float2(0.0, one.y)) * coeffs.y + TEX2D(xy + one) * coeffs.z + TEX2D(xy + float2(2.0 * one.x, one.y)) * coeffs.w, 0.0, 1.0);

  // The influence of the current and next scanlines, with 3 times oversampling of the beam profile (OVERSAMPLE is defined in the original).
  float4 weights = scanlineWeights(uv_ratio.y, col);
  float4 weights2 = scanlineWeights(1.0 - uv_ratio.y, col2);
  float filter_ = fwidth(ratio_scale.y);
  uv_ratio.y = uv_ratio.y + 1.0 / 3.0 * filter_;
  weights = (weights + scanlineWeights(uv_ratio.y, col)) / 3.0;
  weights2 = (weights2 + scanlineWeights(abs(1.0 - uv_ratio.y), col2)) / 3.0;
  uv_ratio.y = uv_ratio.y - 2.0 / 3.0 * filter_;
  weights = weights + scanlineWeights(abs(uv_ratio.y), col) / 3.0;
  weights2 = weights2 + scanlineWeights(abs(1.0 - uv_ratio.y), col2) / 3.0;

  float3 mul_res = (col * weights + col2 * weights2).rgb * float3(cval, cval, cval);

  // Shadow mask: the aperture grille (pattern 1), magenta and green columns of the screen's pixels (mask_weights_alpha with intensity 1: off = 0).
  float2 u_tex_size1 = OutputSize.xy * SourceSize.zw;
  float pixelX = vTexCoord.x * OutputSize.x;
  float3 maskWeights = lerp(float3(1.0, 0.0, 1.0), float3(0.0, 1.0, 0.0), floor(pixelX - 2.0 * floor(pixelX / 2.0)));
  float alpha = 3.0 / 6.0;
  float nbright = 255.0 - 255.0 * alpha;                       // count of total bright pixels, as the original encodes it in the alpha of the mask
  float fbright = nbright / (u_tex_size1.x * u_tex_size1.y);   // fraction of bright pixels in the mask
  float aperture_average = lerp(1.0 - DOTMASK * (1.0 - DOTMASK_brightboost), 1.0, fbright);   // average darkening factor of the mask
  float3 clow = float3(1.0 - DOTMASK, 1.0 - DOTMASK, 1.0 - DOTMASK) * mul_res + float3(DOTMASK * DOTMASK_brightboost, DOTMASK * DOTMASK_brightboost, DOTMASK * DOTMASK_brightboost) * mul_res * mul_res;
  float ifbright = 1.0 / fbright;
  float3 chi = float3(ifbright * aperture_average, ifbright * aperture_average, ifbright * aperture_average) * mul_res - float3(ifbright - 1.0, ifbright - 1.0, ifbright - 1.0) * clow;
  float3 cout = lerp(clow, chi, maskWeights);   // the mask picks dark or bright

  // cout is the light of the colour; the original's last line (pow(cout, 1 / monitorgamma)) encodes it for a gamma 2.2 monitor, which is not needed here (see the top of this file).
  return float4(cout, 1.0);
}
)hlsl";
}

}  // namespace crt
