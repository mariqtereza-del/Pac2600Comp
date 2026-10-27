#include "controls.hpp"

#include <algorithm>
#include <cstdlib>

namespace controls {

std::uint32_t Input::pack() const
{
  return (up ? 1u : 0) | (down ? 2u : 0) | (left ? 4u : 0) | (right ? 8u : 0) | (fire ? 16u : 0) | (select ? 32u : 0) | (reset ? 64u : 0)
         | (leftDifficultyA ? 128u : 0) | (rightDifficultyA ? 256u : 0) | (blackWhite ? 512u : 0);
}

Input Input::unpack(std::uint32_t bits)
{
  Input i;
  i.up = bits & 1u; i.down = bits & 2u; i.left = bits & 4u; i.right = bits & 8u; i.fire = bits & 16u;
  i.select = bits & 32u; i.reset = bits & 64u; i.leftDifficultyA = bits & 128u; i.rightDifficultyA = bits & 256u; i.blackWhite = bits & 512u;
  return i;
}

const std::vector<Binding>& bindings()
{
  static const std::vector<Binding> table = {
      {Action::Up, {SDL_SCANCODE_UP, SDL_SCANCODE_W}, "Up arrow / W", "Joystick up"},
      {Action::Down, {SDL_SCANCODE_DOWN, SDL_SCANCODE_S}, "Down arrow / S", "Joystick down"},
      {Action::Left, {SDL_SCANCODE_LEFT, SDL_SCANCODE_A}, "Left arrow / A", "Joystick left"},
      {Action::Right, {SDL_SCANCODE_RIGHT, SDL_SCANCODE_D}, "Right arrow / D", "Joystick right"},
      {Action::Fire, {SDL_SCANCODE_SPACE, SDL_SCANCODE_Z}, "Space / Z", "Fire button (starts a game from the title screen)"},
      {Action::Select, {SDL_SCANCODE_F2}, "F2 (hold)", "Game Select: next game variation"},
      {Action::Reset, {SDL_SCANCODE_F3}, "F3 (hold)", "Game Reset: start a new game"},
      {Action::LeftDifficulty, {SDL_SCANCODE_F5}, "F5", "Left difficulty switch A / B"},
      {Action::RightDifficulty, {SDL_SCANCODE_F6}, "F6", "Right difficulty switch A / B"},
      {Action::TvType, {SDL_SCANCODE_F4}, "F4", "TV type switch Colour / B&W"},
  };
  return table;
}

SDL_Scancode primaryKey(Action action)
{
  for(const Binding& b : bindings())
    if(b.action == action) return b.keys.front();
  return SDL_SCANCODE_UNKNOWN;
}

static bool isDirection(Action a) { return a == Action::Up || a == Action::Down || a == Action::Left || a == Action::Right; }

Scheme nextScheme(Scheme s, int step) { return static_cast<Scheme>((static_cast<int>(s) + 3 + step) % 3); }

const char* schemeTitle(Scheme s) { return s == Scheme::Arrows ? "Arrow keys" : s == Scheme::Wasd ? "W A S D" : "Arrow keys and W A S D"; }

const char* schemeSettingName(Scheme s) { return s == Scheme::Arrows ? "arrows" : s == Scheme::Wasd ? "wasd" : "both"; }

Scheme schemeFromSetting(const std::string& text) { return text == "arrows" ? Scheme::Arrows : text == "wasd" ? Scheme::Wasd : Scheme::Both; }

std::string keysText(const Binding& binding, Scheme scheme)
{
  if(!isDirection(binding.action) || scheme == Scheme::Both) return binding.keysText;
  const std::string both = binding.keysText;   // "Up arrow / W": the arrow key first, the letter second
  const std::size_t slash = both.find(" / ");
  return scheme == Scheme::Arrows ? both.substr(0, slash) : both.substr(slash + 3);
}

bool Keyboard::held(Action action) const
{
  for(const Binding& b : bindings())
    if(b.action == action)
      for(std::size_t i = 0; i < b.keys.size(); ++i) {
        if(isDirection(action) && ((scheme_ == Scheme::Arrows && i != 0) || (scheme_ == Scheme::Wasd && i != 1))) continue;   // the other set is switched off
        if(std::find(down_.begin(), down_.end(), b.keys[i]) != down_.end()) return true;
      }
  return false;
}

void Keyboard::keyDown(SDL_Scancode key, bool repeat)
{
  if(repeat) return;  // a key being held down is not a new press
  if(std::find(down_.begin(), down_.end(), key) == down_.end()) down_.push_back(key);
  for(const Binding& b : bindings()) {
    if(std::find(b.keys.begin(), b.keys.end(), key) == b.keys.end()) continue;
    if(b.action == Action::LeftDifficulty) leftA_ = !leftA_;
    if(b.action == Action::RightDifficulty) rightA_ = !rightA_;
    if(b.action == Action::TvType) bw_ = !bw_;
  }
}

void Keyboard::keyUp(SDL_Scancode key)
{
  down_.erase(std::remove(down_.begin(), down_.end(), key), down_.end());
}

void Keyboard::releaseAll() { down_.clear(); }

void Keyboard::setSwitches(bool leftA, bool rightA, bool blackWhite)
{
  leftA_ = leftA;
  rightA_ = rightA;
  bw_ = blackWhite;
}

Input Keyboard::input() const
{
  Input i;
  const bool up = held(Action::Up), down = held(Action::Down), left = held(Action::Left), right = held(Action::Right);
  i.up = up && !down;  // a joystick cannot point both ways: opposite keys cancel
  i.down = down && !up;
  i.left = left && !right;
  i.right = right && !left;
  i.fire = held(Action::Fire);
  i.select = held(Action::Select);
  i.reset = held(Action::Reset);
  i.leftDifficultyA = leftA_;
  i.rightDifficultyA = rightA_;
  i.blackWhite = bw_;
  return i;
}

std::vector<HelpLine> helpLines(const Input& current, const std::string& controller, bool softBlend, Scheme scheme)
{
  std::vector<HelpLine> lines;
  for(const Binding& b : bindings()) {
    std::string meaning = b.meaning;
    if(b.action == Action::LeftDifficulty) meaning += std::string("  [now ") + (current.leftDifficultyA ? "A" : "B") + "]";
    if(b.action == Action::RightDifficulty) meaning += std::string("  [now ") + (current.rightDifficultyA ? "A" : "B") + "]";
    if(b.action == Action::TvType) meaning += std::string("  [now ") + (current.blackWhite ? "B&W" : "Colour") + "]";
    lines.push_back({keysText(b, scheme), meaning});
  }
  lines.push_back({"Controller", controller});
  lines.push_back({"  D-pad / left stick", "Joystick"});
  lines.push_back({"  Cross / Square", "Fire button"});
  lines.push_back({"  Options", "Game Reset: start a new game"});
  lines.push_back({"  Share", "Game Select: next game variation"});
  lines.push_back({"1 / 2 / 3 / 4 / 5", "Picture: Sharp / Fill / TV frame / Game area / Framed"});
  lines.push_back({"F7", std::string("Soft glow: smooths the flicker  [now ") + (softBlend ? "on" : "off") + "]"});
  lines.push_back({"Alt + Enter", "Fullscreen on / off"});
  lines.push_back({"F1", "Show / hide this list"});
  lines.push_back({"Esc", "Menu: pauses the game; games, difficulty, quit"});
  return lines;
}

// ---- Gamepad (Stage D11) ----

StickDirection::Dir StickDirection::update(int x, int y)
{
  constexpr int kPress = 16384;    // 50 % of the way
  constexpr int kRelease = 9830;   // 30 %
  const int ax = std::abs(x), ay = std::abs(y);
  auto strength = [&](Dir d) { return (d == Dir::Left || d == Dir::Right) ? ax : ay; };
  auto otherStrength = [&](Dir d) { return (d == Dir::Left || d == Dir::Right) ? ay : ax; };
  auto pointing = [&](Dir d) {
    switch(d) {
      case Dir::Left: return x < 0;
      case Dir::Right: return x > 0;
      case Dir::Up: return y < 0;
      case Dir::Down: return y > 0;
      default: return false;
    }
  };

  if(previous_ != Dir::None && pointing(previous_) && strength(previous_) >= kRelease
     && otherStrength(previous_) * 4 <= strength(previous_) * 5)
    return previous_;  // still held, and the other axis is not clearly stronger

  if(std::max(ax, ay) < kPress) return previous_ = Dir::None;
  if(ax >= ay) return previous_ = (x < 0 ? Dir::Left : Dir::Right);
  return previous_ = (y < 0 ? Dir::Up : Dir::Down);
}

PadState padState(const PadReading& r, StickDirection& stick)
{
  const StickDirection::Dir d = stick.update(r.stickX, r.stickY);
  PadState p;
  p.up = r.dpadUp || d == StickDirection::Dir::Up;
  p.down = r.dpadDown || d == StickDirection::Dir::Down;
  p.left = r.dpadLeft || d == StickDirection::Dir::Left;
  p.right = r.dpadRight || d == StickDirection::Dir::Right;
  p.fire = r.cross || r.square;
  p.select = r.share;
  p.reset = r.options;
  return p;
}

PadState combine(const PadState& a, const PadState& b)
{
  PadState p;
  p.up = a.up || b.up; p.down = a.down || b.down; p.left = a.left || b.left; p.right = a.right || b.right;
  p.fire = a.fire || b.fire; p.select = a.select || b.select; p.reset = a.reset || b.reset;
  return p;
}

Input merge(const Input& keyboard, const PadState& pad)
{
  Input i = keyboard;
  const bool up = keyboard.up || pad.up, down = keyboard.down || pad.down, left = keyboard.left || pad.left, right = keyboard.right || pad.right;
  i.up = up && !down;  // a joystick cannot point both ways
  i.down = down && !up;
  i.left = left && !right;
  i.right = right && !left;
  i.fire = keyboard.fire || pad.fire;
  i.select = keyboard.select || pad.select;
  i.reset = keyboard.reset || pad.reset;
  return i;  // the three real switches (difficulty, TV type) come from the keyboard only
}

}  // namespace controls
