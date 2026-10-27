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

}  // namespace hdr
