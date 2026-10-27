#include "crt.hpp"

namespace crt {

namespace {
struct Entry { const char* setting; const char* title; bool implemented; };
constexpr Entry kEntries[kFilters] = {{"off", "Off", true}, {"lottes", "CRT-Lottes", true}, {"geom", "CRT-Geom", true}, {"gdv-ntsc", "GDV NTSC 2600", true}};
}  // namespace

const char* settingName(Filter filter) { return kEntries[static_cast<int>(filter)].setting; }
const char* title(Filter filter) { return kEntries[static_cast<int>(filter)].title; }
bool implemented(Filter filter) { return kEntries[static_cast<int>(filter)].implemented; }

Filter fromSetting(const std::string& text)
{
  for(int i = 0; i < kFilters; ++i)
    if(text == kEntries[i].setting && kEntries[i].implemented) return static_cast<Filter>(i);
  return Filter::Off;
}

Filter next(Filter filter, int step)
{
  int i = static_cast<int>(filter);
  for(int tries = 0; tries < kFilters; ++tries) {
    i = ((i + step) % kFilters + kFilters) % kFilters;
    if(kEntries[i].implemented) return static_cast<Filter>(i);
  }
  return filter;
}

}  // namespace crt
