#include "hdr.hpp"

#include <algorithm>

namespace hdr {

bool useHdr(bool wanted, const ScreenState& screen) { return wanted && screen.hdrOn && screen.headroom > 1.0f; }

float ceilingOf(const ScreenState& screen) { return std::clamp(screen.headroom, 1.0f, kMostBrightness); }

float highlightScale(float strength, const ScreenState& screen, bool hdrInUse)
{
  if(!hdrInUse) return 1.0f;
  const float s = std::clamp(strength, 0.0f, 1.0f);
  return 1.0f + s * (ceilingOf(screen) - 1.0f);
}

float luminance(const Rgb& c) { return 0.2126f * c.r + 0.7152f * c.g + 0.0722f * c.b; }

Rgb highlightColour(const Rgb& c, float scale)
{
  if(scale <= 1.0f) return c;
  const float y = luminance(c);
  if(y <= 0.0f) return {0.0f, 0.0f, 0.0f};
  const float vivid = 1.0f + 0.6f * (1.0f - 1.0f / scale);
  Rgb v{std::max(0.0f, y + vivid * (c.r - y)), std::max(0.0f, y + vivid * (c.g - y)), std::max(0.0f, y + vivid * (c.b - y))};
  const float after = luminance(v);
  const float k = after > 0.0f ? scale * y / after : 0.0f;   // the brightness is exactly scale times the original
  Rgb out{v.r * k, v.g * k, v.b * k};
  const float most = std::max({out.r, out.g, out.b});
  if(most > scale) {   // no channel goes above the scale (which is never above what the screen can show): a very vivid colour is made a little less bright instead
    out.r *= scale / most;
    out.g *= scale / most;
    out.b *= scale / most;
  }
  return out;
}

}  // namespace hdr
