// The game controllers (keyboard keys and gamepads) as the console inputs, and the pad check.
// (Stage G4.7: moved out of main.cpp, the code is unchanged.)
#include "pads.hpp"
#include "options.hpp"
#include "rom_choice.hpp"

namespace host {

// ---- Keyboard (Stage D10, DECISIONS.md T-D10-1) ----

// Gives the console what the keyboard adds up to. extraReset is for tests that press Reset by frame number.
bool playerTwoToPlay(const std::uint8_t* ram) { return (ram[0xE6 - 0x80] & 0xC0) == 0xC0; }

void applyInput(Emulator& emulator, const controls::Input& in, bool extraReset, bool sharedControls)
{
  // The fire button stays on the left joystick: the game reads no other, and the game-over restart uses it.
  const bool toRight = sharedControls && playerTwoToPlay(emulator.ram());
  emulator.setJoystick(in.up && !toRight, in.down && !toRight, in.left && !toRight, in.right && !toRight, in.fire);
  emulator.setRightJoystick(in.up && toRight, in.down && toRight, in.left && toRight, in.right && toRight);
  emulator.setConsoleSelect(in.select);
  emulator.setConsoleReset(in.reset || extraReset);
  emulator.setDifficulty(in.leftDifficultyA, in.rightDifficultyA);
  emulator.setBlackWhite(in.blackWhite);
}

// --pad-check: a virtual gamepad (SDL's own test device, which goes through the same gamepad code as a real pad) plays the
// real game through the same Pads code the window uses. Every button and the stick are checked against the game's own
// memory and the console switches; then the pad is unplugged while the stick is held and plugged in again.
bool padCheck(const Options& o)
{
  std::ofstream report(o.padCheckFile);
  const auto found = rom::findQuietly(o.romArg, o.settingsFile, o.romsDir);
  std::vector<std::uint8_t> bytes;
  if(!report || !found || found->check.status != rom::Status::Ok || !readRomBytes(found->check.path, bytes)) { if(report) report << "result=no usable ROM\n"; return false; }
  Emulator emulator;
  if(const std::string error = emulator.load(fileNameOf(found->check.path), bytes); !error.empty()) { report << "result=" << error << '\n'; return false; }

  bool ok = true;
  auto line = [&](const std::string& name, bool pass, const std::string& detail) {
    report << name << (pass ? ": ok  " : ": FAIL  ") << detail << '\n';
    ok = ok && pass;
  };

  Pads pads;
  if(!pads.start()) { report << "result=FAIL gamepads could not start\n"; return false; }
  report << "real_gamepads_found=" << pads.count() << " (" << pads.names() << ")\n";
  const int realPads = pads.count();

  // What the virtual pad was told to do (Stage I15): rumble pulses that were not zero, and light bar colours.
  struct Outputs { int rumbles = 0, leds = 0; Uint16 low = 0, high = 0; Uint8 r = 0, g = 0, b = 0; } outputs;
  SDL_VirtualJoystickDesc desc;
  SDL_INIT_INTERFACE(&desc);
  desc.userdata = &outputs;
  desc.Rumble = [](void* u, Uint16 low, Uint16 high) -> bool {
    Outputs* o = static_cast<Outputs*>(u);
    if(low || high) { ++o->rumbles; o->low = low; o->high = high; }
    return true;
  };
  desc.SetLED = [](void* u, Uint8 r, Uint8 g, Uint8 b) -> bool { Outputs* o = static_cast<Outputs*>(u); ++o->leds; o->r = r; o->g = g; o->b = b; return true; };
  desc.type = SDL_JOYSTICK_TYPE_GAMEPAD;
  desc.naxes = SDL_GAMEPAD_AXIS_COUNT;
  desc.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
  const VirtualPadModel model = virtualPadModel(o.padModel);
  desc.name = model.name;
  desc.vendor_id = model.vendor;    // Sony's numbers for a DualShock 4 (v2) or a DualSense, so that SDL treats the virtual pad as one (Stages I14, I17)
  desc.product_id = model.product;

  SDL_JoystickID virtualId = 0;
  SDL_Joystick* joystick = nullptr;
  auto plugIn = [&]() {
    virtualId = SDL_AttachVirtualJoystick(&desc);
    joystick = virtualId ? SDL_OpenJoystick(virtualId) : nullptr;
    return joystick != nullptr;
  };
  auto button = [&](SDL_GamepadButton b, bool down) { if(joystick) SDL_SetJoystickVirtualButton(joystick, b, down); };
  auto axis = [&](SDL_GamepadAxis a, int value) { if(joystick) SDL_SetJoystickVirtualAxis(joystick, a, static_cast<Sint16>(value)); };

  controls::Input lastInput;
  int lastEd = 0, lastSwitches = 0;
  std::set<int> switchesSeen;
  controls::PadAsks seen;   // what the pad asked of the window since the last time this was cleared (Stage I14)
  long long clockMs = 0;
  controls::Keyboard keyboard;
  // Runs frames, taking the pad from the real Pads code each frame; returns nothing, leaves the last readings behind.
  auto run = [&](int frames) {
    for(int f = 0; f < frames; ++f) {
      for(SDL_Event e; SDL_PollEvent(&e); ) pads.handleEvent(e);
      SDL_UpdateJoysticks();
      clockMs += 17;
      const controls::PadAsks a = pads.asks(clockMs);
      seen.menu = seen.menu || a.menu; seen.leftDifficulty = seen.leftDifficulty || a.leftDifficulty;
      seen.rightDifficulty = seen.rightDifficulty || a.rightDifficulty; seen.tvType = seen.tvType || a.tvType;
      seen.nav.insert(seen.nav.end(), a.nav.begin(), a.nav.end());
      controls::flipSwitches(keyboard, a);
      lastInput = controls::merge(keyboard.input(), pads.poll());
      applyInput(emulator, lastInput, false);
      emulator.runFrame();
      lastEd = emulator.ram()[0xED - 0x80];  // C6: up 1, down 2, left 4, right 8
      lastSwitches = emulator.consoleSwitches();
      switchesSeen.insert(lastSwitches);
    }
  };
  auto hex = [](int v) { char t[8]; std::snprintf(t, sizeof t, "%02x", v); return std::string(t); };
  // A fresh game with Pac-Man alive and free to move (a ghost catches an idle Pac-Man about 420 frames after a game starts,
  // docs\SOUND.md, and the game then stops reading the joystick): Options, then the start tune.
  auto newGame = [&]() {
    button(SDL_GAMEPAD_BUTTON_START, true);
    run(10);
    button(SDL_GAMEPAD_BUTTON_START, false);
    run(190);
  };

  line("plug in a virtual gamepad", plugIn(), SDL_GetError());
  run(30);
  line("the program opened it", pads.count() == realPads + 1, "pads open: " + std::to_string(pads.count()) + " (" + pads.names() + ")");
  line(std::string("known as a ") + controls::familyTitle(model.family) + " pad", realPads > 0 || pads.family() == model.family, std::string("family: ") + controls::familyTitle(pads.family()));

  run(60);  // attract mode
  // Share = Game Select: the select switch (bit 1 of the switch register) goes low while held
  button(SDL_GAMEPAD_BUTTON_BACK, true);
  run(8);
  line("Share = Game Select", lastInput.select && (lastSwitches & 0x02) == 0, "switch register " + hex(lastSwitches));
  button(SDL_GAMEPAD_BUTTON_BACK, false);
  run(30);
  // Options = Game Reset: the reset switch (bit 0) goes low while held, and a game starts
  button(SDL_GAMEPAD_BUTTON_START, true);
  run(10);
  line("Options = Game Reset", lastInput.reset && (lastSwitches & 0x01) == 0, "switch register " + hex(lastSwitches));
  button(SDL_GAMEPAD_BUTTON_START, false);
  run(200);  // the start tune plays, then Pac-Man can move
  line("both released: switches back", (lastSwitches & 0x03) == 0x03, "switch register " + hex(lastSwitches));

  line("nothing pressed: joystick byte 0", lastEd == 0, "RAM $ED = " + std::to_string(lastEd));
  // D-pad and stick, all four directions each (the game's joystick byte: up 1, down 2, left 4, right 8)
  const struct { SDL_GamepadButton pad; int axisValue; bool vertical; int expect; const char* name; } dirs[] = {
      {SDL_GAMEPAD_BUTTON_DPAD_UP, -32767, true, 1, "up"}, {SDL_GAMEPAD_BUTTON_DPAD_DOWN, 32767, true, 2, "down"},
      {SDL_GAMEPAD_BUTTON_DPAD_LEFT, -32767, false, 4, "left"}, {SDL_GAMEPAD_BUTTON_DPAD_RIGHT, 32767, false, 8, "right"}};
  for(const auto& d : dirs) {
    newGame();  // Pac-Man must be alive for each direction
    button(d.pad, true);
    run(6);
    line(std::string("D-pad ") + d.name, lastEd == d.expect, "RAM $ED = " + std::to_string(lastEd) + " (wanted " + std::to_string(d.expect) + ")");
    button(d.pad, false);
    run(6);
    axis(d.vertical ? SDL_GAMEPAD_AXIS_LEFTY : SDL_GAMEPAD_AXIS_LEFTX, d.axisValue);
    run(6);
    line(std::string("left stick ") + d.name, lastEd == d.expect, "RAM $ED = " + std::to_string(lastEd) + " (wanted " + std::to_string(d.expect) + ")");
    axis(d.vertical ? SDL_GAMEPAD_AXIS_LEFTY : SDL_GAMEPAD_AXIS_LEFTX, 0);
    run(6);
    line(std::string("stick back to the middle after ") + d.name, lastEd == 0, "RAM $ED = " + std::to_string(lastEd));
  }
  newGame();
  // The dead zone, and a crooked push (mostly right, a little up: one direction, right)
  axis(SDL_GAMEPAD_AXIS_LEFTX, 8000);
  run(6);
  line("stick at 25 %: inside the dead zone", lastEd == 0, "RAM $ED = " + std::to_string(lastEd));
  axis(SDL_GAMEPAD_AXIS_LEFTX, 30000);
  axis(SDL_GAMEPAD_AXIS_LEFTY, -18000);
  run(6);
  line("crooked push (right 92 %, up 55 %): right only", lastEd == 8, "RAM $ED = " + std::to_string(lastEd));
  axis(SDL_GAMEPAD_AXIS_LEFTX, 0);
  axis(SDL_GAMEPAD_AXIS_LEFTY, 0);
  run(6);
  button(SDL_GAMEPAD_BUTTON_SOUTH, true);
  run(2);
  line("Cross = fire", lastInput.fire, "");
  button(SDL_GAMEPAD_BUTTON_SOUTH, false);
  button(SDL_GAMEPAD_BUTTON_WEST, true);
  run(2);
  line("Square = fire", lastInput.fire, "");
  button(SDL_GAMEPAD_BUTTON_WEST, false);
  run(2);

  // Stage I14 (D-020): the buttons that ask things of the window. None of them may play the game.
  newGame();
  seen = controls::PadAsks{};
  auto tap = [&](SDL_GamepadButton b) { button(b, true); run(3); button(b, false); run(3); };
  tap(SDL_GAMEPAD_BUTTON_NORTH);
  line("Triangle flips the TV type", seen.tvType && lastInput.blackWhite && !lastInput.fire, "");
  tap(SDL_GAMEPAD_BUTTON_NORTH);
  line("Triangle again: back to colour", !lastInput.blackWhite, "");
  tap(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER);
  line("L1 flips the left difficulty", seen.leftDifficulty && lastInput.leftDifficultyA && !lastInput.rightDifficultyA, "");
  tap(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER);
  line("R1 flips the right difficulty", seen.rightDifficulty && lastInput.rightDifficultyA, "");
  tap(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER);
  tap(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER);
  line("L1 and R1 again: both back to B", !lastInput.leftDifficultyA && !lastInput.rightDifficultyA, "");
  seen = controls::PadAsks{};
  tap(SDL_GAMEPAD_BUTTON_TOUCHPAD);
  line("Touchpad asks for the menu", seen.menu && !lastInput.fire && lastEd == 0, "");
  seen = controls::PadAsks{};
  tap(SDL_GAMEPAD_BUTTON_GUIDE);
  line("PS button asks for the menu", seen.menu, "");
  seen = controls::PadAsks{};
  tap(SDL_GAMEPAD_BUTTON_RIGHT_STICK);
  const bool playStation = controls::isPlayStation(model.family);   // an Xbox pad has no touchpad: pressing the right stick in is its second menu button (Stage I18)
  line(playStation ? "pressing the right stick does not, on a PlayStation pad" : "pressing the right stick asks for the menu, on an Xbox pad", seen.menu != playStation, "");
  seen = controls::PadAsks{};
  tap(SDL_GAMEPAD_BUTTON_SOUTH);
  tap(SDL_GAMEPAD_BUTTON_EAST);
  line("Cross = do, Circle = back (in the menu)", seen.nav.size() == 2 && seen.nav[0] == controls::Nav::Enter && seen.nav[1] == controls::Nav::Back, "moves: " + std::to_string(seen.nav.size()));
  seen = controls::PadAsks{};
  button(SDL_GAMEPAD_BUTTON_DPAD_DOWN, true);
  run(60);   // about a second
  button(SDL_GAMEPAD_BUTTON_DPAD_DOWN, false);
  run(3);
  bool allDown = !seen.nav.empty();
  for(controls::Nav n : seen.nav) allDown = allDown && n == controls::Nav::Down;
  line("D-pad down held for a second: one move, then repeats", allDown && seen.nav.size() >= 5 && seen.nav.size() <= 12, "moves: " + std::to_string(seen.nav.size()));
  seen = controls::PadAsks{};
  axis(SDL_GAMEPAD_AXIS_LEFTY, 32767);
  run(3);
  axis(SDL_GAMEPAD_AXIS_LEFTY, 0);
  run(3);
  line("the stick moves in the menu too", seen.nav.size() == 1 && seen.nav[0] == controls::Nav::Down, "moves: " + std::to_string(seen.nav.size()));
  run(30);

  // Stage I15 (D-021): the light bar and the rumble go out to the pad.
  pads.setLight({255, 210, 0});
  run(2);
  if(playStation) {
    line("the light bar is told its colour", outputs.leds == 1 && outputs.r == 255 && outputs.g == 210 && outputs.b == 0, "told " + std::to_string(outputs.leds) + " time(s)");
    pads.setLight({255, 210, 0});
    run(2);
    line("the same colour is not sent again", outputs.leds == 1, "told " + std::to_string(outputs.leds) + " time(s)");
    pads.setLight({0, 200, 0});
    run(2);
    line("a new colour is sent", outputs.leds == 2 && outputs.g == 200, "told " + std::to_string(outputs.leds) + " time(s)");
  } else {
    line("no light bar colour is sent to a pad that has none", outputs.leds == 0, "told " + std::to_string(outputs.leds) + " time(s)");
  }
  pads.rumble(1000, 2000, 100);
  run(2);
  line("the rumble pulse reaches the pad", outputs.rumbles == 1 && outputs.low == 1000 && outputs.high == 2000, "pulses " + std::to_string(outputs.rumbles));
  pads.setActive(false);
  pads.rumble(5000, 5000, 100);
  run(2);
  line("no rumble while the window is not in front", outputs.rumbles == 1, "pulses " + std::to_string(outputs.rumbles));
  pads.setActive(true);
  run(20);

  // Unplug with the stick held right: the direction must go with it
  newGame();
  axis(SDL_GAMEPAD_AXIS_LEFTX, 32767);
  run(10);
  line("stick held right before unplugging", lastEd == 8, "RAM $ED = " + std::to_string(lastEd));
  SDL_CloseJoystick(joystick);
  joystick = nullptr;
  SDL_DetachVirtualJoystick(virtualId);
  run(10);
  line("unplugged: the program closed the pad", pads.count() == realPads, "pads open: " + std::to_string(pads.count()));
  line("unplugged: nothing stays pressed", lastEd == 0 && !lastInput.right, "RAM $ED = " + std::to_string(lastEd));
  run(120);
  line("still running 2 seconds later", lastEd == 0, "RAM $ED = " + std::to_string(lastEd));

  // Plug in again
  line("plug in again", plugIn(), SDL_GetError());
  run(30);
  line("the program opened it again", pads.count() == realPads + 1, "pads open: " + std::to_string(pads.count()));
  newGame();
  axis(SDL_GAMEPAD_AXIS_LEFTX, -32767);
  run(8);
  line("works after plugging in again: left", lastEd == 4, "RAM $ED = " + std::to_string(lastEd));
  axis(SDL_GAMEPAD_AXIS_LEFTX, 0);
  run(4);

  // While the window is not in front the pad is ignored
  newGame();
  axis(SDL_GAMEPAD_AXIS_LEFTY, -32767);
  pads.setActive(false);
  run(6);
  line("window not in front: pad ignored", lastEd == 0, "RAM $ED = " + std::to_string(lastEd));
  pads.setActive(true);
  run(6);
  line("window in front again: pad counts at once", lastEd == 1, "RAM $ED = " + std::to_string(lastEd));
  axis(SDL_GAMEPAD_AXIS_LEFTY, 0);

  if(joystick) { SDL_CloseJoystick(joystick); joystick = nullptr; SDL_DetachVirtualJoystick(virtualId); }
  report << "console_switch_values_seen=";
  for(int v : switchesSeen) report << hex(v) << ' ';
  report << "\nresult=" << (ok ? "OK" : "FAIL") << '\n';
  return ok;
}

}  // namespace host
