#include "framerate.hpp"

#include <algorithm>
#include <cmath>

namespace framerate {

namespace {
struct Entry { const char* setting; const char* title; int perSecond; };
constexpr Entry kEntries[kRates] = {{"30", "30", 30}, {"60", "60", 60}, {"75", "75", 75}, {"120", "120", 120}, {"144", "144", 144}, {"unlocked", "Unlocked", 0}};
}  // namespace

const char* settingName(Rate rate) { return kEntries[static_cast<int>(rate)].setting; }
const char* title(Rate rate) { return kEntries[static_cast<int>(rate)].title; }

Rate fromSetting(const std::string& text)
{
  for(int i = 0; i < kRates; ++i)
    if(text == kEntries[i].setting) return static_cast<Rate>(i);
  return Rate::Fps60;
}

Rate next(Rate rate, int step) { return static_cast<Rate>(((static_cast<int>(rate) + step) % kRates + kRates) % kRates); }

bool smooth(Rate rate) { return rate == Rate::Fps75 || rate == Rate::Fps120 || rate == Rate::Fps144 || rate == Rate::Unlocked; }
bool unlocked(Rate rate) { return rate == Rate::Unlocked; }
int perSecond(Rate rate) { return kEntries[static_cast<int>(rate)].perSecond; }

long long pictureIndex(Rate rate, double seconds) { return unlocked(rate) ? -1 : static_cast<long long>(std::floor(seconds * perSecond(rate))); }

double pictureTime(Rate rate, double seconds)
{
  return unlocked(rate) ? seconds : static_cast<double>(pictureIndex(rate, seconds)) / perSecond(rate);
}

double progress(double at, double stepStart, double period) { return std::clamp((at - stepStart) / period, 0.0, 0.999); }

long long shownStep(Rate rate, long long step) { return rate == Rate::Fps30 ? step - (step & 1) : step; }

}  // namespace framerate
