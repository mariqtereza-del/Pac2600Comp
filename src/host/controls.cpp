#include "controls.hpp"

#include <algorithm>

namespace controls {

std::uint32_t Input::pack() const
{
  return (up ? 1u : 0) | (down ? 2u : 0) | (left ? 4u : 0) | (right ? 8u : 0) | (fire ? 16u : 0) | (select ? 32u : 0) | (reset ? 64u : 0)
         | (leftDifficultyA ? 128u : 0) | (rightDifficultyA ? 256u : 0);
}

Input Input::unpack(std::uint32_t bits)
{
  Input i;
  i.up = bits & 1u; i.down = bits & 2u; i.left = bits & 4u; i.right = bits & 8u; i.fire = bits & 16u;
  i.select = bits & 32u; i.reset = bits & 64u; i.leftDifficultyA = bits & 128u; i.rightDifficultyA = bits & 256u;
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
  };
  return table;
}

SDL_Scancode primaryKey(Action action)
{
  for(const Binding& b : bindings())
    if(b.action == action) return b.keys.front();
  return SDL_SCANCODE_UNKNOWN;
}

bool Keyboard::held(Action action) const
{
  for(const Binding& b : bindings())
    if(b.action == action)
      for(SDL_Scancode k : b.keys)
        if(std::find(down_.begin(), down_.end(), k) != down_.end()) return true;
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
  }
}

void Keyboard::keyUp(SDL_Scancode key)
{
  down_.erase(std::remove(down_.begin(), down_.end(), key), down_.end());
}

void Keyboard::releaseAll() { down_.clear(); }

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
  return i;
}

std::vector<HelpLine> helpLines(const Input& current)
{
  std::vector<HelpLine> lines;
  for(const Binding& b : bindings()) {
    std::string meaning = b.meaning;
    if(b.action == Action::LeftDifficulty) meaning += std::string("  [now ") + (current.leftDifficultyA ? "A" : "B") + "]";
    if(b.action == Action::RightDifficulty) meaning += std::string("  [now ") + (current.rightDifficultyA ? "A" : "B") + "]";
    lines.push_back({b.keysText, meaning});
  }
  lines.push_back({"1 / 2 / 3 / 4", "Picture: Sharp / Fill / TV frame / Game area"});
  lines.push_back({"Alt + Enter", "Fullscreen on / off"});
  lines.push_back({"F1", "Show / hide this list"});
  lines.push_back({"Esc", "Quit"});
  return lines;
}

}  // namespace controls
