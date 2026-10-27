// The longer explanations of the Escape menu's rows (Stage I12: moved out of menu.cpp, the text is unchanged). Each adds lines, cut to `width` columns, to `out`.
#pragma once

#include <string>
#include <vector>

#include "menu.hpp"

namespace menu {

std::string backgroundText(background::Kind kind);                                                     // what one background does
void appendFrameRateText(std::vector<std::string>& out, int width, bool smoothAvailable);              // the Frame rate row
void appendCrtText(std::vector<std::string>& out, int width, crt::Filter filter);                      // the CRT filter row
void appendHdrText(std::vector<std::string>& out, int width, bool windowsHdr, bool slider);            // the HDR row (slider = false) and the HDR highlights row
void appendSoundText(std::vector<std::string>& out, int width, Item item);                             // the Volume, Mute and Sound options rows

}  // namespace menu
