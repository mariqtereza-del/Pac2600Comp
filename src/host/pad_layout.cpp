#include "pad_layout.hpp"

namespace controls {

bool isSymbol(char c) { return c >= kCross && c <= kButtonY; }   // the eight symbol characters are consecutive

std::string plainSymbols(const std::string& text)
{
  std::string out;
  for(char c : text) {
    switch(c) {
      case kCross: out += "Cross"; break;
      case kCircle: out += "Circle"; break;
      case kSquare: out += "Square"; break;
      case kTriangle: out += "Triangle"; break;
      case kButtonA: out += "A"; break;
      case kButtonB: out += "B"; break;
      case kButtonX: out += "X"; break;
      case kButtonY: out += "Y"; break;
      default: out += c;
    }
  }
  return out;
}

const char* familyTitle(PadFamily family)
{
  switch(family) {
    case PadFamily::PlayStation: return "PlayStation (DualShock 4)";
    case PadFamily::DualSense: return "PlayStation (DualSense)";
    case PadFamily::Xbox: return "Xbox (XInput)";
    case PadFamily::Other: break;
  }
  return "other game pad";
}

std::vector<HelpLine> padLines(PadFamily family)
{
  const std::string cross(1, kCross), circle(1, kCircle), square(1, kSquare), triangle(1, kTriangle);
  const std::string a(1, kButtonA), b(1, kButtonB), x(1, kButtonX), y(1, kButtonY);
  switch(family) {
    case PadFamily::PlayStation:
    case PadFamily::DualSense:
      return {{"  D-pad / left stick", "Joystick"},
              {"  " + cross + " / " + square, "Fire button"},
              {"  Options", "Game Reset: start a new game"},
              {family == PadFamily::DualSense ? "  Create" : "  Share", "Game Select: next game variation"},
              {"  L1 / R1", "Left / right difficulty switch A / B"},
              {"  " + triangle, "TV type switch Colour / B&W"},
              {"  PS / Touchpad", "Menu: open and close"},
              {"  In the menu", "D-pad / stick move, " + cross + " do, " + circle + " back"}};
    case PadFamily::Xbox:
      return {{"  D-pad / left stick", "Joystick"},
              {"  " + a + " / " + x, "Fire button"},
              {"  Menu", "Game Reset: start a new game"},
              {"  View", "Game Select: next game variation"},
              {"  LB / RB", "Left / right difficulty switch A / B"},
              {"  " + y, "TV type switch Colour / B&W"},
              {"  Guide / right stick", "Menu: open and close (press the stick in for the second)"},
              {"  In the menu", "D-pad / stick move, " + a + " do, " + b + " back"}};
    case PadFamily::Other:
      break;
  }
  return {{"  D-pad / left stick", "Joystick"},
          {"  Bottom / left button", "Fire button"},
          {"  Start", "Game Reset: start a new game"},
          {"  Back", "Game Select: next game variation"},
          {"  Left / right bumper", "Left / right difficulty switch A / B"},
          {"  Top button", "TV type switch Colour / B&W"},
          {"  Guide / right stick", "Menu: open and close (press the stick in for the second)"},
          {"  In the menu", "D-pad / stick move, bottom button do, right button back"}};
}

PadAsks PadPresses::update(const PadReading& r, long long nowMs)
{
  PadAsks asks;
  auto menuButton = [&](const PadReading& p) { return p.home || p.touchpad || (!isPlayStation(family_) && p.rightStick); };
  asks.menu = menuButton(r) && !menuButton(before_);
  asks.leftDifficulty = r.l1 && !before_.l1;
  asks.rightDifficulty = r.r1 && !before_.r1;
  asks.tvType = r.triangle && !before_.triangle;
  if(r.cross && !before_.cross) asks.nav.push_back(Nav::Enter);
  if(r.circle && !before_.circle) asks.nav.push_back(Nav::Back);

  const StickDirection::Dir s = stick_.update(r.stickX, r.stickY);
  int dir = -1;   // the D-pad first, then the stick; one direction at a time
  if(r.dpadUp) dir = 0; else if(r.dpadDown) dir = 1; else if(r.dpadLeft) dir = 2; else if(r.dpadRight) dir = 3;
  else if(s == StickDirection::Dir::Up) dir = 0;
  else if(s == StickDirection::Dir::Down) dir = 1;
  else if(s == StickDirection::Dir::Left) dir = 2;
  else if(s == StickDirection::Dir::Right) dir = 3;
  if(dir != heldDirection_) {
    heldDirection_ = dir;
    if(dir >= 0) { asks.nav.push_back(static_cast<Nav>(dir)); nextRepeatAt_ = nowMs + 400; }
  } else if(dir >= 0 && nowMs >= nextRepeatAt_) {
    asks.nav.push_back(static_cast<Nav>(dir));
    nextRepeatAt_ = nowMs + 90;
  }
  before_ = r;
  return asks;
}

void flipSwitches(Keyboard& keyboard, const PadAsks& asks)
{
  if(asks.leftDifficulty) keyboard.flip(Action::LeftDifficulty);
  if(asks.rightDifficulty) keyboard.flip(Action::RightDifficulty);
  if(asks.tvType) keyboard.flip(Action::TvType);
}

}  // namespace controls
