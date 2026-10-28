// Test only (Stages I5 and D10): a helper thread that presses keys in the real window's event queue, as the operating system would (--push-keys, --push-menu).
// (Stage I9.4: cut out of play_window.cpp, the code is unchanged.)
#include "key_pusher.hpp"
#include "pads.hpp"

#include <windows.h>
#include <initializer_list>

namespace host {

namespace {

// Stage I16: what happens when Direct3D removes the graphics device: SDL's renderer lets go of the Direct3D libraries and says that the device was reset.
void simulateDeviceLoss()
{
  for(const wchar_t* name : {L"d3d11.dll", L"dxgi.dll"}) if(HMODULE m = GetModuleHandleW(name)) FreeLibrary(m);
  SDL_Event reset{};
  reset.type = SDL_EVENT_RENDER_DEVICE_RESET;
  SDL_PushEvent(&reset);
}

}  // namespace

namespace {

// Stage I14: a virtual DualShock 4 (SDL's own test device, which goes through the same gamepad code as a real pad) is plugged in and played through the real window:
// the options, the TV type, both difficulties, then the menu by the touchpad, its Control options page with Cross, the D-pad and Circle, closed by the PS button, and
// opened again to quit.
std::thread startPadPusher(const Options& options, const std::atomic<bool>& stop)
{
  return std::thread([&options, &stop]() {
      SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");   // the test window may not be in front: SDL would otherwise ignore the pad (test only)
      struct Outputs { std::atomic<int> rumbles{0}, leds{0}, ledYellow{0}, ledRed{0}; } outputs;   // what the window told the pad (Stage I15)
      SDL_VirtualJoystickDesc desc;
      SDL_INIT_INTERFACE(&desc);
      desc.userdata = &outputs;
      desc.Rumble = [](void* u, Uint16 low, Uint16 high) -> bool { Outputs* o = static_cast<Outputs*>(u); if(low || high) ++o->rumbles; return true; };
      desc.SetLED = [](void* u, Uint8 r, Uint8 g, Uint8 b) -> bool {
        Outputs* o = static_cast<Outputs*>(u);
        ++o->leds;
        if(r == 255 && g == 210 && b == 0) ++o->ledYellow;   // Pac-Man yellow
        if(r == 255 && g == 0 && b == 0) ++o->ledRed;         // caught
        return true;
      };
      desc.type = SDL_JOYSTICK_TYPE_GAMEPAD;
      desc.naxes = SDL_GAMEPAD_AXIS_COUNT;
      desc.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
      const VirtualPadModel model = virtualPadModel(options.padModel);
      desc.name = model.name;
      desc.vendor_id = model.vendor;
      desc.product_id = model.product;
      const SDL_JoystickID id = SDL_AttachVirtualJoystick(&desc);
      SDL_Joystick* joystick = id ? SDL_OpenJoystick(id) : nullptr;
      { std::ofstream note(options.settingsFile + ".pad.txt"); note << "virtual_pad_plugged_in=" << (joystick ? "yes" : "no") << " " << (joystick ? "" : SDL_GetError()) << (char)10; }
      if(!joystick) return;
      struct Step { double at; SDL_GamepadButton button; bool down; };
      std::vector<Step> plan;
      auto tap = [&](double at, SDL_GamepadButton b) { plan.push_back({at, b, true}); plan.push_back({at + 0.08, b, false}); };
      tap(0.6, SDL_GAMEPAD_BUTTON_START);            // Options: Game Reset, a game starts
      tap(3.0, SDL_GAMEPAD_BUTTON_NORTH);            // Triangle: TV type to B&W
      tap(3.4, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER);    // L1: left difficulty to A
      tap(3.8, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER);   // R1: right difficulty to A
      tap(4.2, SDL_GAMEPAD_BUTTON_TOUCHPAD);         // the touchpad opens the menu
      for(int i = 0; i < 5; ++i) tap(4.6 + 0.2 * i, SDL_GAMEPAD_BUTTON_DPAD_DOWN);   // down to Control options
      tap(5.8, SDL_GAMEPAD_BUTTON_SOUTH);            // Cross: open the page
      tap(6.1, SDL_GAMEPAD_BUTTON_DPAD_RIGHT);       // the keyboard to the arrow keys only
      tap(6.4, SDL_GAMEPAD_BUTTON_EAST);             // Circle: back to the main page
      tap(6.8, SDL_GAMEPAD_BUTTON_GUIDE);            // the PS button closes the menu
      tap(7.4, SDL_GAMEPAD_BUTTON_NORTH);            // Triangle again: TV type back to colour
      tap(8.0, SDL_GAMEPAD_BUTTON_TOUCHPAD);         // open once more
      for(int i = 0; i < 6; ++i) tap(8.4 + 0.2 * i, SDL_GAMEPAD_BUTTON_DPAD_DOWN);   // down to Quit
      tap(10.0, SDL_GAMEPAD_BUTTON_SOUTH);           // Cross: Quit
      if(!options.replayFile.empty()) plan.clear();   // with --replay the pad is only plugged in: a recording plays the game, and the pad's rumble and light are looked at
      const Uint64 t0 = SDL_GetTicksNS();
      for(const Step& step : plan) {
        while(!stop.load() && static_cast<double>(SDL_GetTicksNS() - t0) < step.at * 1e9) SDL_Delay(5);
        if(stop.load()) break;
        SDL_SetJoystickVirtualButton(joystick, step.button, step.down);
      }
      while(!stop.load()) SDL_Delay(20);
      SDL_CloseJoystick(joystick);
      SDL_DetachVirtualJoystick(id);
      std::ofstream note(options.settingsFile + ".pad.txt", std::ios::app);   // what the pad was told while the window ran
      note << "rumble_pulses=" << outputs.rumbles.load() << (char)10 << "light_bar_colours_sent=" << outputs.leds.load() << (char)10 << "light_bar_yellow_sent=" << outputs.ledYellow.load() << (char)10 << "light_bar_red_sent=" << outputs.ledRed.load() << (char)10;
  });
}

}  // namespace

void writePushReports(std::ostream& log, const Options& options, const Emulator& emulator, int menuOpens, long long menuFramesRun, std::uint32_t joystickValuesSeen,
                      const std::set<int>& switchValuesSeen, int rendererRemakes)
{
  if(options.pushCrt) log << "renderer_remade=" << rendererRemakes << (char)10;
  if(options.pushMenu || options.pushPad) {
    char flags[16];
    std::snprintf(flags, sizeof flags, "%02x", emulator.ram()[0xE7 - 0x80]);
    log << "menu_opens=" << menuOpens << "\nmenu_frames_run_while_open=" << menuFramesRun
        << "\nfinal_game_number_byte=" << static_cast<int>(emulator.ram()[0x84 - 0x80]) << "\nfinal_players_byte=" << static_cast<int>(emulator.ram()[0xE6 - 0x80])
        << "\nfinal_level=" << static_cast<int>(emulator.ram()[0x99 - 0x80]) << "\nfinal_game_flags=" << flags
        << "\nfinal_switches=" << std::hex << static_cast<int>(emulator.consoleSwitches()) << std::dec << '\n';
  }
  if(options.pushKeys) {
    log << "joystick_ram_values_seen=";
    for(int n = 0; n < 32; ++n) if(joystickValuesSeen & (1u << n)) log << n << ' ';
    log << "\nconsole_switch_values_seen=";
    for(int v : switchValuesSeen) { char t[8]; std::snprintf(t, sizeof t, "%02x ", v); log << t; }
    log << '\n';
  }
}

std::thread startKeyPusher(const Options& options, const std::atomic<bool>& stop)
{
  if(options.pushPad) return startPadPusher(options, stop);
  if(!(options.pushKeys || options.pushMenu || options.pushCrt)) return {};
  return std::thread([&options, &stop]() {
      auto key = [](SDL_Scancode code, bool down) {
        SDL_Event e{};
        e.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
        e.key.scancode = code;
        e.key.key = SDL_GetKeyFromScancode(code, SDL_KMOD_NONE, false);
        e.key.down = down;
        SDL_PushEvent(&e);
      };
      struct Step { double at; SDL_Scancode code; bool down; };
      std::vector<Step> plan = {
          {0.5, SDL_SCANCODE_F3, true}, {0.8, SDL_SCANCODE_F3, false},          // Game Reset: a game starts
          {4.0, SDL_SCANCODE_RIGHT, true}, {4.6, SDL_SCANCODE_RIGHT, false},    // joystick right (Pac-Man starts moving at about 2 s)
          {5.0, SDL_SCANCODE_A, true}, {5.6, SDL_SCANCODE_A, false},            // left, with the letter key
          {6.0, SDL_SCANCODE_UP, true}, {6.6, SDL_SCANCODE_UP, false},          // up
          {7.0, SDL_SCANCODE_S, true}, {7.6, SDL_SCANCODE_S, false},            // down, with the letter key
          {8.0, SDL_SCANCODE_F5, true}, {8.1, SDL_SCANCODE_F5, false},          // left difficulty to A
          {8.5, SDL_SCANCODE_F6, true}, {8.6, SDL_SCANCODE_F6, false},          // right difficulty to A
          {9.0, SDL_SCANCODE_F4, true}, {9.1, SDL_SCANCODE_F4, false},          // Colour to B&W
          {9.5, SDL_SCANCODE_F7, true}, {9.6, SDL_SCANCODE_F7, false}};         // soft glow on
      if(options.pushMenu) {  // Stage I5 (then I7): open the menu, choose game 3 and the left difficulty A, start it; open and close it again; quit from it
        plan.clear();
        auto tap = [&](double at, SDL_Scancode code) { plan.push_back({at, code, true}); plan.push_back({at + 0.05, code, false}); };
        tap(1.0, SDL_SCANCODE_ESCAPE);
        tap(1.5, SDL_SCANCODE_DOWN); tap(1.6, SDL_SCANCODE_DOWN); tap(1.65, SDL_SCANCODE_RETURN);  // open the Game options page (Stage I8), on the Game row
        tap(1.7, SDL_SCANCODE_RIGHT); tap(1.8, SDL_SCANCODE_RIGHT);  // game 3
        tap(1.85, SDL_SCANCODE_DOWN);                                // the Players row stays at one player (Stage I6)
        tap(1.9, SDL_SCANCODE_DOWN); tap(2.0, SDL_SCANCODE_RIGHT);  // left difficulty to A
        tap(2.05, SDL_SCANCODE_DOWN); tap(2.1, SDL_SCANCODE_DOWN); tap(2.15, SDL_SCANCODE_RIGHT);  // Stage I9.5: the frame rate one step on (75; 30 in the reference program)
        tap(2.2, SDL_SCANCODE_DOWN); tap(2.25, SDL_SCANCODE_RIGHT);                                // and the frame counter on
        tap(2.27, SDL_SCANCODE_ESCAPE); tap(2.29, SDL_SCANCODE_DOWN); tap(2.31, SDL_SCANCODE_RETURN);   // Stage I10: to the Graphics options page
        tap(2.33, SDL_SCANCODE_DOWN); tap(2.34, SDL_SCANCODE_DOWN); tap(2.36, SDL_SCANCODE_DOWN); tap(2.37, SDL_SCANCODE_RIGHT);   // past the TV type and the CRT filter, HDR off
        tap(2.39, SDL_SCANCODE_DOWN); tap(2.41, SDL_SCANCODE_RIGHT); tap(2.43, SDL_SCANCODE_RIGHT);    // and the highlights 10 more (60)
        tap(2.45, SDL_SCANCODE_ESCAPE); tap(2.5, SDL_SCANCODE_UP); tap(2.55, SDL_SCANCODE_UP);   // back to the main page (on Game options), up to New game
        tap(3.0, SDL_SCANCODE_RETURN);                               // go
        tap(6.0, SDL_SCANCODE_ESCAPE); tap(6.5, SDL_SCANCODE_ESCAPE);  // open and close again
        tap(6.6, SDL_SCANCODE_ESCAPE);                                // Stage I7: open the menu, go to Graphics options, choose the second background on from Black (Solid colour since the Mirrored edge was removed), back, close
        for(int i = 0; i < 3; ++i) tap(6.7 + 0.1 * i, SDL_SCANCODE_DOWN);
        tap(7.4, SDL_SCANCODE_RETURN); tap(7.5, SDL_SCANCODE_RIGHT); tap(7.6, SDL_SCANCODE_RIGHT);
        tap(7.7, SDL_SCANCODE_ESCAPE); tap(7.8, SDL_SCANCODE_ESCAPE);
        tap(8.2, SDL_SCANCODE_ESCAPE);   // Stage I12: the Sound options page (4 rows down), the volume two steps down (90), Mute on, back to the main page
        for(int i = 0; i < 4; ++i) tap(8.25 + 0.02 * i, SDL_SCANCODE_DOWN);
        tap(8.33, SDL_SCANCODE_RETURN); tap(8.35, SDL_SCANCODE_LEFT); tap(8.37, SDL_SCANCODE_LEFT);
        tap(8.39, SDL_SCANCODE_DOWN); tap(8.41, SDL_SCANCODE_RIGHT); tap(8.43, SDL_SCANCODE_ESCAPE);
        tap(8.45, SDL_SCANCODE_DOWN); tap(8.47, SDL_SCANCODE_RETURN); tap(8.49, SDL_SCANCODE_RIGHT); tap(8.51, SDL_SCANCODE_ESCAPE);   // Stage I13: Control options, the keyboard to the arrow keys only, back
        tap(8.55, SDL_SCANCODE_DOWN); tap(8.6, SDL_SCANCODE_RETURN);  // Quit
      }
      if(options.pushCrt) {   // Stage I16: Graphics options, the CRT filter row; Right 14 times (round the four filters three times and a bit), Left 6 times, with a pause for the picture each time
        plan.clear();
        auto tap = [&](double at, SDL_Scancode code) { plan.push_back({at, code, true}); plan.push_back({at + 0.05, code, false}); };
        tap(1.0, SDL_SCANCODE_ESCAPE);
        for(int i = 0; i < 3; ++i) tap(1.3 + 0.1 * i, SDL_SCANCODE_DOWN);
        tap(1.8, SDL_SCANCODE_RETURN);
        tap(2.0, SDL_SCANCODE_DOWN); tap(2.1, SDL_SCANCODE_DOWN);
        for(int i = 0; i < 14; ++i) tap(2.6 + 0.7 * i, SDL_SCANCODE_RIGHT);
        for(int i = 0; i < 6; ++i) tap(12.6 + 0.7 * i, SDL_SCANCODE_LEFT);
        tap(17.2, SDL_SCANCODE_ESCAPE); tap(17.6, SDL_SCANCODE_ESCAPE);   // back to the game
      }
      std::vector<double> resets;   // Stage I16: "the graphics device was lost and made again", as SDL says it when Direct3D reports a removed device; times in seconds
      if(options.pushCrt) resets = {4.4, 7.4, 10.4, 13.4, 16.4};
      const Uint64 t0 = SDL_GetTicksNS();
      std::size_t nextReset = 0;
      for(const auto& step : plan) {
        while(!stop.load() && static_cast<double>(SDL_GetTicksNS() - t0) < step.at * 1e9) {
          if(nextReset < resets.size() && static_cast<double>(SDL_GetTicksNS() - t0) >= resets[nextReset] * 1e9) { simulateDeviceLoss(); ++nextReset; }
          SDL_Delay(5);
        }
        if(stop.load()) break;
        key(step.code, step.down);
      }
  });
}

}  // namespace host
