// Tests for src\host\controls (Stage D10): the key table and what keys add up to.
// Usage: test_controls
#include <cstdio>
#include <set>
#include <string>

#include "../../src/host/controls.hpp"
#include "../../src/host/pad_layout.hpp"

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
    for(const char* needle : {"Up arrow / W", "Space / Z", "F2", "F3", "F5", "F6", "Alt + Enter", "F1", "Esc", "1 / 2 / 3 / 4 / 5", "[now A]", "[now B]", "Controller", "Test Pad", "D-pad / left stick", "Options", "Share", "L1 / R1", "PS / Touchpad"})
      expect(all.find(needle) != std::string::npos, std::string("the F1 list contains: ") + needle);
    expect(lines.size() == controls::bindings().size() + 14, "the F1 list has one line per binding, the controller line and eight pad lines, and five window keys");
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

  // Stage I13: which keys steer the joystick
  {
    using controls::Scheme;
    const SDL_Scancode arrows[4] = {SDL_SCANCODE_UP, SDL_SCANCODE_DOWN, SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT};
    const SDL_Scancode letters[4] = {SDL_SCANCODE_W, SDL_SCANCODE_S, SDL_SCANCODE_A, SDL_SCANCODE_D};
    for(Scheme scheme : {Scheme::Both, Scheme::Arrows, Scheme::Wasd})
      for(int set = 0; set < 2; ++set)
        for(int d = 0; d < 4; ++d) {
          Keyboard k;
          k.setScheme(scheme);
          k.keyDown(set == 0 ? arrows[d] : letters[d], false);
          const controls::Input in = k.input();
          const bool moved = in.up || in.down || in.left || in.right;
          const bool shouldMove = scheme == Scheme::Both || (scheme == Scheme::Arrows) == (set == 0);
          expect(moved == shouldMove, std::string("scheme ") + controls::schemeSettingName(scheme) + (set == 0 ? ", an arrow key " : ", a letter key ") + std::to_string(d));
        }
    Keyboard k;
    k.setScheme(Scheme::Wasd);
    k.keyDown(SDL_SCANCODE_SPACE, false);
    expect(k.input().fire, "fire works whatever keys steer");
    expect(controls::nextScheme(Scheme::Both, 1) == Scheme::Arrows && controls::nextScheme(Scheme::Arrows, 1) == Scheme::Wasd && controls::nextScheme(Scheme::Wasd, 1) == Scheme::Both
               && controls::nextScheme(Scheme::Both, -1) == Scheme::Wasd, "the choice goes round both ways");
    for(Scheme s : {Scheme::Both, Scheme::Arrows, Scheme::Wasd}) expect(controls::schemeFromSetting(controls::schemeSettingName(s)) == s, "the setting name round trips");
    expect(controls::schemeFromSetting("") == Scheme::Both && controls::schemeFromSetting("nonsense") == Scheme::Both, "anything else is both (today's behaviour)");
    const controls::Binding& up = controls::bindings()[0];
    expect(controls::keysText(up, Scheme::Both) == "Up arrow / W" && controls::keysText(up, Scheme::Arrows) == "Up arrow" && controls::keysText(up, Scheme::Wasd) == "W", "the keys are shown as chosen");
    expect(controls::keysText(controls::bindings()[4], Scheme::Wasd) == "Space / Z", "fire keeps both keys");
    bool listed = false;
    for(const auto& l : controls::helpLines(controls::Input{}, "none", false, Scheme::Wasd)) if(l.keys == "A" && l.meaning == "Joystick left") listed = true;
    expect(listed, "the F1 list shows the chosen keys");
  }

  // Stage I14 (D-020): the DualShock 4 buttons, their names and symbols, and what presses ask of the window.
  {
    using controls::Nav;
    using controls::PadFamily;
    using controls::PadReading;
    // The symbols: four characters that no text uses, turned into words for reports.
    expect(controls::isSymbol(controls::kCross) && controls::isSymbol(controls::kCircle) && controls::isSymbol(controls::kSquare) && controls::isSymbol(controls::kTriangle),
           "four symbol characters");
    expect(!controls::isSymbol('A') && !controls::isSymbol(' '), "ordinary characters are not symbols");
    expect(controls::plainSymbols(std::string("X ") + controls::kCross + "/" + controls::kSquare + controls::kCircle + controls::kTriangle) == "X Cross/SquareCircleTriangle",
           "symbols in words");
    // The names: every family has the same seven things, and only the PlayStation names carry symbols.
    for(PadFamily f : {PadFamily::PlayStation, PadFamily::DualSense, PadFamily::Xbox, PadFamily::Other}) {
      const auto lines = controls::padLines(f);
      expect(lines.size() == 8, std::string("eight pad lines for ") + controls::familyTitle(f));
      bool symbols = false;
      for(const auto& l : lines) for(char c : l.keys + l.meaning) symbols = symbols || controls::isSymbol(c);
      expect(symbols == (f != PadFamily::Other), std::string("symbols on the pads that have them (PlayStation: shapes, Xbox: lettered buttons): ") + controls::familyTitle(f));
    }
    const auto ps = controls::padLines(PadFamily::PlayStation);
    expect(controls::plainSymbols(ps[1].keys) == "  Cross / Square" && ps[2].keys == "  Options" && ps[3].keys == "  Share" && ps[4].keys == "  L1 / R1"
               && controls::plainSymbols(ps[5].keys) == "  Triangle" && ps[6].keys == "  PS / Touchpad", "the DualShock 4 names");
    expect(controls::plainSymbols(ps[7].meaning) == "D-pad / stick move, Cross do, Circle back", "the menu buttons are named");
    // The DualSense: the same buttons and symbols, but its Share button is called Create.
    const auto ds5 = controls::padLines(PadFamily::DualSense);
    expect(controls::plainSymbols(ds5[1].keys) == "  Cross / Square" && ds5[2].keys == "  Options" && ds5[3].keys == "  Create" && ds5[4].keys == "  L1 / R1" && ds5[6].keys == "  PS / Touchpad",
           "the DualSense names: Create instead of Share, the rest as on the DualShock 4");
    for(std::size_t i = 0; i < ps.size(); ++i) if(i != 3) expect(ps[i].keys == ds5[i].keys && ps[i].meaning == ds5[i].meaning, "the other DualSense lines are the DualShock 4's");
    expect(std::string(controls::familyTitle(PadFamily::DualSense)).find("DualSense") != std::string::npos && std::string(controls::familyTitle(PadFamily::PlayStation)).find("DualShock 4") != std::string::npos,
           "each pad is named by its own name");
    expect(controls::isPlayStation(PadFamily::PlayStation) && controls::isPlayStation(PadFamily::DualSense) && !controls::isPlayStation(PadFamily::Xbox) && !controls::isPlayStation(PadFamily::Other),
           "both PlayStation pads count as PlayStation pads (light bar, touchpad and PS button)");
    {
      controls::PadPresses d(PadFamily::DualSense);
      auto rd = [](auto set) { PadReading r; set(r); return r; };
      expect(!d.update(rd([](PadReading& r) { r.rightStick = true; }), 17).menu, "DualSense: pressing the right stick does not open the menu (it has a touchpad and a PS button)");
      d.update(PadReading{}, 34);
      expect(d.update(rd([](PadReading& r) { r.touchpad = true; }), 51).menu, "DualSense: the touchpad opens the menu");
      d.update(PadReading{}, 68);
      expect(d.update(rd([](PadReading& r) { r.home = true; }), 85).menu, "DualSense: the PS button opens the menu");
    }
    // The Xbox pad (XInput): the same things, with its own names and the lettered buttons.
    const auto xb = controls::padLines(PadFamily::Xbox);
    expect(controls::plainSymbols(xb[1].keys) == "  A / X" && xb[2].keys == "  Menu" && xb[3].keys == "  View" && xb[4].keys == "  LB / RB" && controls::plainSymbols(xb[5].keys) == "  Y"
               && xb[6].keys == "  Guide / right stick", "the Xbox names: A / X fire, Menu = Reset, View = Select, LB / RB, Y");
    expect(controls::plainSymbols(xb[7].meaning) == "D-pad / stick move, A do, B back", "the Xbox menu buttons are named");
    expect(controls::isSymbol(controls::kButtonA) && controls::isSymbol(controls::kButtonY) && !controls::isSymbol('A') && !controls::isSymbol('\x09'), "the letter symbols are symbols, the letters themselves are not");
    expect(controls::plainSymbols(std::string(1, controls::kButtonA) + controls::kButtonB + controls::kButtonX + controls::kButtonY) == "ABXY", "the lettered buttons in words");
    expect(std::string(controls::familyTitle(PadFamily::Xbox)).find("Xbox") != std::string::npos, "the Xbox pad is named");
    {
      controls::PadPresses x(PadFamily::Xbox);
      auto rx = [](auto set) { PadReading r; set(r); return r; };
      expect(x.update(rx([](PadReading& r) { r.l1 = true; }), 17).leftDifficulty, "Xbox: LB flips the left difficulty");
      x.update(PadReading{}, 34);
      expect(x.update(rx([](PadReading& r) { r.r1 = true; }), 51).rightDifficulty, "Xbox: RB flips the right difficulty");
      x.update(PadReading{}, 68);
      expect(x.update(rx([](PadReading& r) { r.triangle = true; }), 85).tvType, "Xbox: Y (the top button) flips the TV type");
      x.update(PadReading{}, 102);
      expect(x.update(rx([](PadReading& r) { r.home = true; }), 119).menu, "Xbox: the Guide button opens the menu");
      x.update(PadReading{}, 136);
      expect(x.update(rx([](PadReading& r) { r.cross = true; }), 153).nav.size() == 1, "Xbox: A does (Enter) in the menu");
      x.update(PadReading{}, 170);
      expect(x.update(rx([](PadReading& r) { r.circle = true; }), 187).nav.size() == 1, "Xbox: B goes back in the menu");
    }
    bool inHelp = false;
    for(const auto& l : controls::helpLines(controls::Input{}, "x", false, controls::Scheme::Both, PadFamily::Xbox)) inHelp = inHelp || l.keys == "  LB / RB";
    expect(inHelp, "the F1 list uses the names of the pad family");

    // Presses. 17 ms per turn, as at 60 a second.
    auto reading = [](auto set) { PadReading r; set(r); return r; };
    {
      controls::PadPresses p(PadFamily::PlayStation);
      long long t = 0;
      auto turn = [&](const PadReading& r) { t += 17; return p.update(r, t); };
      expect(!turn(PadReading{}).any(), "nothing pressed asks nothing");
      expect(turn(reading([](PadReading& r) { r.triangle = true; })).tvType, "Triangle: TV type");
      expect(!turn(reading([](PadReading& r) { r.triangle = true; })).tvType, "a held button asks once");
      turn(PadReading{});
      expect(turn(reading([](PadReading& r) { r.l1 = true; })).leftDifficulty, "L1: left difficulty");
      turn(PadReading{});
      expect(turn(reading([](PadReading& r) { r.r1 = true; })).rightDifficulty, "R1: right difficulty");
      turn(PadReading{});
      expect(turn(reading([](PadReading& r) { r.home = true; })).menu, "PS button: menu");
      turn(PadReading{});
      expect(turn(reading([](PadReading& r) { r.touchpad = true; })).menu, "touchpad: menu");
      turn(PadReading{});
      expect(!turn(reading([](PadReading& r) { r.rightStick = true; })).menu, "right stick press: not on a PlayStation pad");
      turn(PadReading{});
      const controls::PadAsks enter = turn(reading([](PadReading& r) { r.cross = true; }));
      expect(enter.nav.size() == 1 && enter.nav[0] == Nav::Enter && !enter.menu, "Cross: Enter");
      turn(PadReading{});
      const controls::PadAsks back = turn(reading([](PadReading& r) { r.circle = true; }));
      expect(back.nav.size() == 1 && back.nav[0] == Nav::Back, "Circle: Back");
      turn(PadReading{});
      expect(!turn(reading([](PadReading& r) { r.square = true; r.options = true; r.share = true; })).any(), "fire, Reset and Select ask nothing of the window");
      turn(PadReading{});
      // A held direction: at once, nothing for 400 ms, then every 90 ms.
      const PadReading down = reading([](PadReading& r) { r.dpadDown = true; });
      int moves = 0;
      long long firstAt = -1;
      const long long loopStart = t + 17;
      for(int i = 0; i < 59; ++i) {   // 59 turns of 17 ms: just over a second
        const controls::PadAsks a = turn(down);
        if(!a.nav.empty() && firstAt < 0) firstAt = t;
        moves += static_cast<int>(a.nav.size());
        for(Nav n : a.nav) expect(n == Nav::Down, "the repeat is the same direction");
      }
      expect(firstAt == loopStart, "the first move comes at once");
      expect(moves >= 7 && moves <= 9, "one move, then repeats from 400 ms every 90 ms: " + std::to_string(moves));
      turn(PadReading{});
      const controls::PadAsks stick = turn(reading([](PadReading& r) { r.stickX = 30000; }));
      expect(stick.nav.size() == 1 && stick.nav[0] == Nav::Right, "the stick pushed right: one move");
      expect(turn(reading([](PadReading& r) { r.stickX = 30000; })).nav.empty(), "and none until the repeat time");
      turn(PadReading{});
      expect(turn(reading([](PadReading& r) { r.dpadUp = true; r.dpadDown = true; })).nav[0] == Nav::Up, "up and down together: up (one move at a time)");
    }
    {
      controls::PadPresses x(PadFamily::Xbox);
      expect(x.update(reading([](PadReading& r) { r.rightStick = true; }), 17).menu, "Xbox pad: pressing the right stick opens the menu");
    }
    {
      Keyboard k;
      controls::PadAsks a;
      a.tvType = true; a.leftDifficulty = true;
      controls::flipSwitches(k, a);
      expect(k.input().blackWhite && k.input().leftDifficultyA && !k.input().rightDifficultyA, "flipSwitches flips only what was asked");
      k.keyDown(SDL_SCANCODE_F4, false);
      expect(!k.input().blackWhite, "the key flips the same switch back");
      k.flip(Action::Fire);
      expect(!k.input().fire, "flip does nothing for a button that is not a switch");
    }
  }

  // primaryKey is the first key of the binding
  expect(controls::primaryKey(Action::Up) == SDL_SCANCODE_UP && controls::primaryKey(Action::Reset) == SDL_SCANCODE_F3, "primary keys are the first ones listed");

  std::printf("\n%s (%d checks, %d failure%s)\n", failures ? "TESTS FAILED" : "ALL TESTS PASSED", checks, failures, failures == 1 ? "" : "s");
  return failures ? 1 : 0;
}
