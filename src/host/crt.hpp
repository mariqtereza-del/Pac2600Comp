// The CRT filter choices (Stage I11, DECISIONS.md D-017): which filter the picture goes through, with no window code, so it can be tested on its own.
#pragma once
#include <string>

namespace crt {

// Off is the program's own picture, as it always was (the default). The others are the real shaders of the libretro collection, ported to HLSL (third_party\shaders).
enum class Filter { Off, Lottes, Geom, GuestNtsc };

inline constexpr int kFilters = 4;

const char* settingName(Filter filter);   // "off", "lottes", "geom", "gdv-ntsc": the value in settings.ini and on the command line
const char* title(Filter filter);         // as shown to the player
Filter fromSetting(const std::string& text);   // anything unknown is Off
bool implemented(Filter filter);          // the filters that are ported so far (the menu offers only these)
Filter next(Filter filter, int step = 1); // round the implemented ones, either way

}  // namespace crt
