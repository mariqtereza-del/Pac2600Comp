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
  const std::set<SDL_Scancode> windowKeys = {SDL_SCANCODE_1, SDL_SCANCODE_2, SDL_SCANCODE_3, SDL_SCANCODE_4, SDL_SCANCODE_5, SDL_SCANCODE_F7, SDL_SCANCODE_F1, SDL_SCANCODE_ESCAPE,
                                             SDL_SCANCODE_RETURN, SDL_SCANCODE_KP_ENTER, SDL_SCANCODE_LALT, SDL_SCANCODE_RALT};
  for(const auto& b : controls::bindings())
    for(SDL_Scancode k : b.keys) {
      if(!seen.insert(k).second) duplicate = true;
      if(windowKeys.count(k)) reserved = true;
      expect(k != SDL_SCANCODE_UNKNOWN, std::string("a real key for ") + b.keysText);
    }
  expect(!duplicate, "no key is used for two things");
  expect(!reserved, "no game key is one the window uses (1-5, F1, Esc, Enter, Alt)");
  expect(controls::bindings().size() == 10, "ten actions are bound");

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

  // The Colour / B&W switch (Stage D12): a real switch on F4, Colour at the start
  {
    Keyboard k;
    expect(!k.input().blackWhite, "start: Colour");
    k.keyDown(SDL_SCANCODE_F4, false);
    expect(k.input().blackWhite && k.input().pack() == 512, "F4 once: B&W, nothing else changes");
    k.keyDown(SDL_SCANCODE_F4, true);
    k.keyUp(SDL_SCANCODE_F4);
    expect(k.input().blackWhite, "F4 held and released: the switch stays at B&W");
    k.releaseAll();
    expect(k.input().blackWhite, "window loses focus: the switch stays where it is");
    k.keyDown(SDL_SCANCODE_F4, false);
    k.keyUp(SDL_SCANCODE_F4);
    expect(!k.input().blackWhite, "F4 again: Colour");
    k.setSwitches(true, false, true);
    expect(k.input().leftDifficultyA && !k.input().rightDifficultyA && k.input().blackWhite, "remembered switches are restored (A, B, B&W)");
    k.keyDown(SDL_SCANCODE_F5, false);
    expect(!k.input().leftDifficultyA && k.input().blackWhite, "after restoring, the keys flip from there");
    expect(controls::merge(k.input(), controls::PadState{}).blackWhite, "a pad does not change the Colour / B&W switch");
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
    for(std::uint32_t bits = 0; bits < 1024; ++bits) ok = ok && Input::unpack(bits).pack() == bits;
    expect(ok, "pack and unpack agree for all 1024 states");
  }

  // The F1 list names every key and shows the switches
  {
    Input now;
    now.leftDifficultyA = true;
    const auto lines = controls::helpLines(now, "Test Pad");
    std::string all;
    for(const auto& l : lines) all += l.keys + " | " + l.meaning + "\n";
    for(const char* needle : {"Up arrow / W", "Space / Z", "F2", "F3", "F5", "F6", "Alt + Enter", "F1", "Esc", "1 / 2 / 3 / 4 / 5", "[now A]", "[now B]", "Controller", "Test Pad", "D-pad / left stick", "Options", "Share", "Cross / Square"})
      expect(all.find(needle) != std::string::npos, std::string("the F1 list contains: ") + needle);
    expect(lines.size() == controls::bindings().size() + 10, "the F1 list has one line per binding, five pad lines and five window keys");
    // The soft glow (F7) is named with its position
    auto glowLine = [](const std::vector<controls::HelpLine>& l) { for(const auto& x : l) if(x.keys == "F7") return x.meaning; return std::string(); };
    expect(glowLine(lines).find("[now off]") != std::string::npos, "F7 line says the glow is off by default");
    expect(glowLine(controls::helpLines(now, "Test Pad", true)).find("[now on]") != std::string::npos, "F7 line says the glow is on when it is");
  }

  // ---- Gamepad (Stage D11) ----
  using controls::PadReading;
  using controls::PadState;
  using controls::StickDirection;
  {
    // Buttons: Cross and Square fire, Options resets, Share selects; the D-pad gives the four directions
    StickDirection st;
    PadReading r;
    expect(!controls::padState(r, st).up && !controls::padState(r, st).fire && !controls::padState(r, st).reset, "pad at rest: nothing pressed");
    r.cross = true;  expect(controls::padState(r, st).fire, "Cross = fire");  r.cross = false;
    r.square = true; expect(controls::padState(r, st).fire, "Square = fire"); r.square = false;
    r.options = true; expect(controls::padState(r, st).reset && !controls::padState(r, st).select, "Options = Reset only"); r.options = false;
    r.share = true; expect(controls::padState(r, st).select && !controls::padState(r, st).reset, "Share = Select only"); r.share = false;
    r.dpadUp = true;    expect(controls::padState(r, st).up && !controls::padState(r, st).down, "D-pad up");       r.dpadUp = false;
    r.dpadDown = true;  expect(controls::padState(r, st).down && !controls::padState(r, st).up, "D-pad down");     r.dpadDown = false;
    r.dpadLeft = true;  expect(controls::padState(r, st).left && !controls::padState(r, st).right, "D-pad left"); r.dpadLeft = false;
    r.dpadRight = true; expect(controls::padState(r, st).right && !controls::padState(r, st).left, "D-pad right"); r.dpadRight = false;
  }
  {
    // The stick: four directions, dead zone, one direction at a time, no flicker at 45 degrees
    auto dir = [](StickDirection& st, int x, int y) {
      PadReading r;
      r.stickX = x; r.stickY = y;
      const PadState p = controls::padState(r, st);
      return std::string(p.up ? "U" : "") + (p.down ? "D" : "") + (p.left ? "L" : "") + (p.right ? "R" : "");
    };
    StickDirection st;
    expect(dir(st, 0, 0).empty(), "stick in the middle: nothing");
    expect(dir(st, 32767, 0) == "R" && dir(st, 0, 0).empty(), "stick right (full), then middle");
    expect(dir(st, -32768, 0) == "L" && dir(st, 0, 0).empty(), "stick left (full), then middle");
    expect(dir(st, 0, -32768) == "U" && dir(st, 0, 0).empty(), "stick up (y negative), then middle");
    expect(dir(st, 0, 32767) == "D" && dir(st, 0, 0).empty(), "stick down (y positive), then middle");
    expect(dir(st, 8000, 0).empty() && dir(st, 0, -8000).empty(), "25 % push: inside the dead zone");
    expect(dir(st, 16000, 0).empty(), "49 % push: not yet");
    expect(dir(st, 16500, 0) == "R", "50.4 % push: counts");
    expect(dir(st, 12000, 0) == "R", "pushed back to 37 %: still counts (stays until 30 %)");
    expect(dir(st, 9000, 0).empty(), "pushed back to 27 %: let go");
    dir(st, 0, 0);
    expect(dir(st, 30000, -18000) == "R", "crooked push (right 92 %, up 55 %): only right");
    expect(dir(st, 18000, -30000) == "U", "crooked push (right 55 %, up 92 %): only up");
    dir(st, 0, 0);
    expect(dir(st, 25000, 25000) == "R", "exact 45 degrees: one direction (right wins a tie)");
    expect(dir(st, 25000, 26000) == "R", "45 degrees, slightly more down: stays right (stickiness)");
    expect(dir(st, 25000, 32000) == "D", "clearly more down: now down");
    expect(dir(st, 25000, 26000) == "D", "and back to nearly 45 degrees: stays down");
    dir(st, 0, 0);
    expect(dir(st, -20000, -20000) == "L", "up-left diagonal: left (tie goes to the horizontal axis)");
    // A direction that reverses is not kept
    dir(st, 0, 0);
    dir(st, 30000, 0);
    expect(dir(st, -30000, 0) == "L", "straight from full right to full left: left, no sticking");
  }
  {
    // Two pads at once, and pad plus keyboard
    PadState a, b;
    a.up = true; b.fire = true;
    const PadState c = controls::combine(a, b);
    expect(c.up && c.fire && !c.down, "two pads: either counts");
    Keyboard k;
    k.keyDown(SDL_SCANCODE_LEFT, false);
    PadState pad;
    pad.right = true; pad.reset = true;
    Input m = controls::merge(k.input(), pad);
    expect(!m.left && !m.right && m.reset, "keyboard left + pad right cancel; the pad's Reset passes");
    pad.right = false; pad.up = true;
    m = controls::merge(k.input(), pad);
    expect(m.left && m.up && !m.right && !m.down, "keyboard left + pad up: both directions");
    k.keyDown(SDL_SCANCODE_F5, false);
    m = controls::merge(k.input(), PadState{});
    expect(m.leftDifficultyA, "the difficulty switches still come from the keyboard");
    expect(controls::merge(Keyboard{}.input(), PadState{}).pack() == 0, "nothing at all: nothing");
  }

  // primaryKey is the first key of the binding
  expect(controls::primaryKey(Action::Up) == SDL_SCANCODE_UP && controls::primaryKey(Action::Reset) == SDL_SCANCODE_F3, "primary keys are the first ones listed");

  std::printf("\n%s (%d checks, %d failure%s)\n", failures ? "TESTS FAILED" : "ALL TESTS PASSED", checks, failures, failures == 1 ? "" : "s");
  return failures ? 1 : 0;
}
