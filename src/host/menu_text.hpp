// The longer explanations of the Escape menu's rows (Stage I12: moved out of menu.cpp, the text is unchanged). Each adds lines, cut to `width` columns, to `out`.
#pragma once

#include <string>
#include <vector>

#include "menu.hpp"
#include "pad_feedback.hpp"

namespace menu {

std::string backgroundText(background::Kind kind);                                                     // what one background does
void appendFrameRateText(std::vector<std::string>& out, int width, bool smoothAvailable);              // the Frame rate row
void appendCrtText(std::vector<std::string>& out, int width, crt::Filter filter);                      // the CRT filter row
void appendHdrText(std::vector<std::string>& out, int width, bool windowsHdr, bool slider);            // the HDR row (slider = false) and the HDR highlights row
void appendControlsText(std::vector<std::string>& out, int width, controls::Scheme scheme, bool keysRow, controls::PadFamily pad,
                        bool padConnected);   // the Control options row and the Keyboard row (lists the keys and the pad's buttons)
void appendFeedbackText(std::vector<std::string>& out, int width, bool padConnected);   // the Rumble row: also tells how the DualShock 4 light bar behaves (Stage I15)
void appendPlayersText(std::vector<std::string>& out, int width, Item item);                          // the Players, Left difficulty and Right difficulty rows
void appendSoundText(std::vector<std::string>& out, int width, Item item);                             // the Volume, Mute and Sound options rows

}  // namespace menu
