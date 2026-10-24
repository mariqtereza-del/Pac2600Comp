// Tests for src\host\controls (Stage D10): the key table and what keys add up to.
// Usage: test_controls
#include <cstdio>
#include <set>
#include <string>

#include "../../src/host/controls.hpp"

using controls::Action;
using controls::Input;
using controls::Keyboard;

static int failures = 0;
static int checks = 0;

static void expect(bool ok, const std::string& what)
{
  ++checks;
  if(!ok) { ++failures; std::printf("FAIL  %s\n", what.c_str()); }
}

static bool onlyDirection(const Input& i, bool up, bool down, bool left, bool right)
{
  return i.up == up && i.down == down && i.left == left && i.right == right;
}

int main()
{
  // The table: every key is used once, none is one of the keys the window itself uses.
  std::set<SDL_Scancode> seen;
  bool duplicate = false, reserved = false;
  const std::set<SDL_Scancode> windowKeys = {SDL_SCANCODE_1, SDL_SCANCODE_2, SDL_SCANCODE_3, SDL_SCANCODE_4, SDL_SCANCODE_F1, SDL_SCANCODE_ESCAPE,
                                             SDL_SCANCODE_RETURN, SDL_SCANCODE_KP_ENTER, SDL_SCANCODE_LALT, SDL_SCANCODE_RALT};
  for(const auto& b : controls::bindings())
    for(SDL_Scancode k : b.keys) {
      if(!seen.insert(k).second) duplicate = true;
      if(windowKeys.count(k)) reserved = true;
      expect(k != SDL_SCANCODE_UNKNOWN, std::string("a real key for ") + b.keysText);
    }
  expect(!duplicate, "no key is used for two things");
  expect(!reserved, "no game key is one the window uses (1-4, F1, Esc, Enter, Alt)");
  expect(controls::bindings().size() == 9, "nine actions are bound");

  // Nothing pressed
  {
    Keyboard k;
    expect(k.input().pack() == 0, "no key: nothing pressed, both difficulty switches at B");
  }

  // Each direction by arrow and by WASD
  const struct { SDL_Scancode arrow, letter; bool u, d, l, r; const char* name; } dirs[] = {
      {SDL_SCANCODE_UP, SDL_SCANCODE_W, true, false, false, false, "up"}, {SDL_SCANCODE_DOWN, SDL_SCANCODE_S, false, true, false, false, "down"},
      {SDL_SCANCODE_LEFT, SDL_SCANCODE_A, false, false, true, false, "left"}, {SDL_SCANCODE_RIGHT, SDL_SCANCODE_D, false, false, false, true, "right"}};
  for(const auto& d : dirs)
    for(SDL_Scancode key : {d.arrow, d.letter}) {
      Keyboard k;
      k.keyDown(key, false);
      expect(onlyDirection(k.input(), d.u, d.d, d.l, d.r), std::string(d.name) + ": key pressed");
      k.keyUp(key);
      expect(k.input().pack() == 0, std::string(d.name) + ": key released");
    }

  // Two keys for the same direction: stays on until both are up
  {
    Keyboard k;
    k.keyDown(SDL_SCANCODE_LEFT, false);
    k.keyDown(SDL_SCANCODE_A, false);
    k.keyUp(SDL_SCANCODE_LEFT);
    expect(k.input().left, "left by two keys: still left after one is released");
    k.keyUp(SDL_SCANCODE_A);
    expect(!k.input().left, "left by two keys: off after both are released");
  }

  // Opposite directions cancel; the other axis is untouched
  {
    Keyboard k;
    k.keyDown(SDL_SCANCODE_LEFT, false);
    k.keyDown(SDL_SCANCODE_D, false);
    expect(onlyDirection(k.input(), false, false, false, false), "left and right together: neither");
    k.keyDown(SDL_SCANCODE_UP, false);
    expect(onlyDirection(k.input(), true, false, false, false), "left+right+up: only up");
    k.keyUp(SDL_SCANCODE_D);
    expect(onlyDirection(k.input(), true, false, true, false), "right released: up and left");
    k.keyDown(SDL_SCANCODE_S, false);
    expect(onlyDirection(k.input(), false, false, true, false), "up and down together: neither, left stays");
  }

  // Fire, Select, Reset are held while the key is down
  {
    Keyboard k;
    for(SDL_Scancode key : {SDL_SCANCODE_SPACE, SDL_SCANCODE_Z}) {
      k.keyDown(key, false);
      expect(k.input().fire, "fire key down");
      k.keyUp(key);
      expect(!k.input().fire, "fire key up");
    }
    k.keyDown(SDL_SCANCODE_F2, false);
    expect(k.input().select && !k.input().reset, "F2 = Select only");
    k.keyUp(SDL_SCANCODE_F2);
    k.keyDown(SDL_SCANCODE_F3, false);
    expect(k.input().reset && !k.input().select, "F3 = Reset only");
    k.keyUp(SDL_SCANCODE_F3);
    expect(k.input().pack() == 0, "F2/F3 released");
  }

  // Difficulty switches: flip on a fresh press, not on repeat, not on release; stay put otherwise
  {
    Keyboard k;
    k.keyDown(SDL_SCANCODE_F5, false);
    expect(k.input().leftDifficultyA && !k.input().rightDifficultyA, "F5 once: left difficulty A");
    k.keyDown(SDL_SCANCODE_F5, true);
    k.keyDown(SDL_SCANCODE_F5, true);
    expect(k.input().leftDifficultyA, "F5 held (key repeat): no flipping");
    k.keyUp(SDL_SCANCODE_F5);
    expect(k.input().leftDifficultyA, "F5 released: the switch stays at A");
    k.keyDown(SDL_SCANCODE_F6, false);
    expect(k.input().leftDifficultyA && k.input().rightDifficultyA, "F6: right difficulty A too");
    k.keyUp(SDL_SCANCODE_F6);
    k.keyDown(SDL_SCANCODE_F5, false);
    k.keyUp(SDL_SCANCODE_F5);
    expect(!k.input().leftDifficultyA && k.input().rightDifficultyA, "F5 again: left back to B, right still A");
  }

  // Key repeat of a held direction does nothing odd
  {
    Keyboard k;
    k.keyDown(SDL_SCANCODE_UP, false);
    k.keyDown(SDL_SCANCODE_UP, true);
    k.keyUp(SDL_SCANCODE_UP);
    expect(k.input().pack() == 0, "key repeat then release: up is off");
  }

  // Focus lost: held keys go, switches stay
  {
    Keyboard k;
    k.keyDown(SDL_SCANCODE_UP, false);
    k.keyDown(SDL_SCANCODE_SPACE, false);
    k.keyDown(SDL_SCANCODE_F3, false);
    k.keyDown(SDL_SCANCODE_F5, false);
    k.releaseAll();
    const Input i = k.input();
    expect(!i.up && !i.fire && !i.reset && i.leftDifficultyA, "focus lost: keys released, the difficulty switch stays");
  }

  // Unrelated keys do nothing
  {
    Keyboard k;
    for(SDL_Scancode key : {SDL_SCANCODE_1, SDL_SCANCODE_F1, SDL_SCANCODE_ESCAPE, SDL_SCANCODE_Q, SDL_SCANCODE_R, SDL_SCANCODE_LALT}) k.keyDown(key, false);
    expect(k.input().pack() == 0, "other keys (1, F1, Esc, Q, R, Alt) do nothing to the console");
  }

  // pack / unpack round trip over every state
  {
    bool ok = true;
    for(std::uint32_t bits = 0; bits < 512; ++bits) ok = ok && Input::unpack(bits).pack() == bits;
    expect(ok, "pack and unpack agree for all 512 states");
  }

  // The F1 list names every key and shows the switches
  {
    Input now;
    now.leftDifficultyA = true;
    const auto lines = controls::helpLines(now);
    std::string all;
    for(const auto& l : lines) all += l.keys + " | " + l.meaning + "\n";
    for(const char* needle : {"Up arrow / W", "Space / Z", "F2", "F3", "F5", "F6", "Alt + Enter", "F1", "Esc", "1 / 2 / 3 / 4", "[now A]", "[now B]"})
      expect(all.find(needle) != std::string::npos, std::string("the F1 list contains: ") + needle);
    expect(lines.size() == controls::bindings().size() + 4, "the F1 list has one line per binding plus four window keys");
  }

  // primaryKey is the first key of the binding
  expect(controls::primaryKey(Action::Up) == SDL_SCANCODE_UP && controls::primaryKey(Action::Reset) == SDL_SCANCODE_F3, "primary keys are the first ones listed");

  std::printf("\n%s (%d checks, %d failure%s)\n", failures ? "TESTS FAILED" : "ALL TESTS PASSED", checks, failures, failures == 1 ? "" : "s");
  return failures ? 1 : 0;
}
