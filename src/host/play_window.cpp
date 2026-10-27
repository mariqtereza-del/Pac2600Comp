// Playing the game in the window.
// (Stage G4.7: moved out of main.cpp, the code is unchanged.)
#include "play_window.hpp"
#include "headless.hpp"
#include "menu_checks.hpp"
#include "menu_session.hpp"
#include "options.hpp"
#include "pacer.hpp"
#include "pads.hpp"
#include "rom_choice.hpp"
#include "screen.hpp"
#include "window_tracker.hpp"

namespace host {

// The game window: one emulator frame per 1/60 s from a simple clock (temporary; real pacing is Stage D9).
// With maxSeconds > 0 it stops by itself after that long and returns the frames per second it managed (for tests).
// Keys 1, 2, 3, 4 choose Sharp, Fill, TV frame, Game area; Alt+Enter toggles fullscreen (temporary; Stage D10 gives the keys their final home).
double playWindow(SDL_Window* window, SDL_Renderer* renderer, const std::string& romPath, double maxSeconds, View view, Tracker& tracker, const Options& options)
{
  std::vector<std::uint8_t> bytes;
  Emulator emulator;
  if(!readRomBytes(romPath, bytes)) { SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, kTitle, "The ROM file could not be read.", window); return 0; }
  if(const std::string error = emulator.load(fileNameOf(romPath), bytes); !error.empty()) {
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, kTitle, error.c_str(), window);
    return 0;
  }
  const std::string romSha1 = rom::sha1Hex(bytes.data(), bytes.size());
  const bool replaying = !options.replayFile.empty();
  inputlog::Recording replay;  // --replay: what the console is given on each frame, instead of the keyboard and pads
  if(replaying) {
    std::string error;
    if(!inputlog::load(options.replayFile, replay, error)) { SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, kTitle, error.c_str(), window); return 0; }
    if(replay.romSha1 != romSha1) { SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, kTitle, "This recording was made with a different ROM than the one that is loaded.", window); return 0; }
  }
  // Stage I6 (D-011): live play shares the controls between the two players of a two-player game; a replay does as its recording says.
  const bool sharedControls = replaying ? replay.sharedControls : true;
  inputlog::Recorder recorder;  // --record: written when the window closes
  std::ofstream traceOut;       // --trace: what the game did on every frame
  if(!options.traceFile.empty()) { traceOut.open(options.traceFile, std::ios::binary); traceOut << trace::header(romSha1); }
  std::ofstream fingerprints;   // --fingerprints: the picture's fingerprint of every frame (tests)
  if(!options.fingerprintFile.empty()) { fingerprints.open(options.fingerprintFile); fingerprints << "frame,hash\n"; }

  AudioOutput audio;
  if(!audio.open(soundRate(options, emulator), options.mute)) std::fprintf(stderr, "%s\nThe game goes on without sound.\n", audio.problem().c_str());

  Screen screen;
  screen.renderer = renderer;
  screen.view = view;
  auto retitle = [&]() {
    const std::string title = std::string(kTitle) + " - " + fileNameOf(romPath) + " - " + modeName(screen.view.mode) + (replaying ? " - REPLAY" : "") + (options.recordFile.empty() ? "" : " - RECORDING") + " - F1 = keys";
    SDL_SetWindowTitle(window, title.c_str());
  };
  retitle();
  // Stage I7: what fills the space around the picture, remembered in settings.ini (background=black|glow|mirror|solid|gradient|arcade|bezel)
  screen.backgroundKind = background::fromSetting(settings::get(options.settingsFile, "background"));

  // The game and its sound run on their own thread (DECISIONS.md T-D8-2), so that a window that is being dragged or
  // resized, which freezes this thread on Windows, cannot starve the sound card. This thread only shows the pictures.
  std::atomic<bool> stop{false};
  std::atomic<std::uint32_t> inputBits{0};  // what the keyboard adds up to (controls::Input::pack), set here, read by the game thread
  controls::Keyboard keyboard;
  // The three real switches start where they were left (settings.ini; the defaults are B, B and Colour).
  keyboard.setSwitches(settings::get(options.settingsFile, "left_difficulty") == "a", settings::get(options.settingsFile, "right_difficulty") == "a",
                       settings::get(options.settingsFile, "tv_type") == "bw");
  controls::Input savedSwitches = keyboard.input();
  Pads pads;
  menu::Session menuSession(!replaying);  // the Escape menu (Stage I5); a replay decides what the console is given, so it cannot start a game
  if(!pads.start()) std::fprintf(stderr, "Gamepads could not start; the keyboard still works.\n");
  bool helpOn = false;
  // Stella's soft glow (F7): off unless the owner turned it on; remembered in settings.ini as soft_glow=on|off
  std::atomic<bool> softBlend{settings::get(options.settingsFile, "soft_glow") == "on"};
  std::mutex lock;  // guards the shared picture below
  std::vector<std::uint32_t> sharedPixels;
  int sharedHeight = 0;
  std::array<std::uint8_t, 128> sharedRam{};  // the game's memory after the frame that sharedPixels shows (for the arcade panel)
  long long sharedFrame = 0;  // number of the picture in sharedPixels (frames run so far)
  double longestGapMs = 0;
  int gapsOver30 = 0;
  std::string gapLog;
  bool usedSoundClock = false;
  double averageCorrectionPpm = 0;
  std::uint32_t joystickValuesSeen = 0;      // bit n set = the game's joystick byte (RAM $ED) held the value n
  std::set<int> switchValuesSeen;            // the console switch register values the game could read
  const Uint64 started = SDL_GetTicksNS();
  // Stage H6: how long each game frame took to compute, and how late it started after the moment it was due (milliseconds).
  std::vector<float> computeMs, lateMs;
  computeMs.reserve(1 << 16);
  lateMs.reserve(1 << 16);
  std::atomic<double> firstPictureMs{-1};  // from the start of this function to the first picture handed to the window

  std::thread game([&]() {
    auto pacer = std::make_unique<Pacer>(audio, options.timerClock, frameRate(options));
    usedSoundClock = pacer->usesSoundClock();
    bool wasPaused = false;
    Uint64 lastLoop = SDL_GetTicksNS();
    long long frames = 0;
    bool glowApplied = false;  // the emulator starts without the glow
    while(!stop.load()) {
      // The menu is open: no frames, so no sound is made either. Afterwards the pacer starts afresh (no frames to catch up).
      if(menuSession.paused()) { wasPaused = true; waitFor(1000000); continue; }
      if(wasPaused) { wasPaused = false; pacer = std::make_unique<Pacer>(audio, options.timerClock, frameRate(options)); lastLoop = SDL_GetTicksNS(); }
      const Uint64 now = SDL_GetTicksNS();
      const double gap = static_cast<double>(now - lastLoop) / 1e6;  // how long this thread went without running
      lastLoop = now;
      if(gap > 30.0) {
        ++gapsOver30;
        if(gapLog.size() < 400) gapLog += " " + std::to_string(static_cast<int>(gap)) + "ms@frame" + std::to_string(frames);
      }
      longestGapMs = std::max(longestGapMs, gap);

      int steps = 0;
      while(steps < 5 && pacer->due(now)) {
        const Uint64 frameBegan = SDL_GetTicksNS();
        lateMs.push_back(static_cast<float>(static_cast<double>(frameBegan - pacer->dueAt()) / 1e6));
        if(softBlend.load() != glowApplied) { glowApplied = softBlend.load(); emulator.setSoftBlend(glowApplied); }
        // A replay gives the console what was recorded; when it has run out, the keyboard and pads take over again.
        controls::Input in = controls::Input::unpack(replaying && frames < replay.frames ? replay.bitsAt(frames) : inputBits.load());
        if(options.resetAt >= 0 && frames >= options.resetAt && frames < options.resetAt + 10) in.reset = true;
        menuSession.apply(in);  // the Select and Reset presses of a new game chosen in the menu (recorded like any other input)
        recorder.add(in.pack());
        applyInput(emulator, in, false, sharedControls);
        emulator.runFrame();
        menuSession.publish(emulator.ram());
        if(fingerprints) {
          char hash[17];
          std::snprintf(hash, sizeof hash, "%016llx", static_cast<unsigned long long>(emulator.pictureFingerprint()));
          fingerprints << frames << ',' << hash << '\n';
        }
        if(traceOut) writeTrace(traceOut, emulator, frames, in.pack());
        audio.push(emulator.audio(), emulator.audioFrames());
        if(options.pushKeys) {
          const int joystick = emulator.ram()[0xED - 0x80];  // C6: up 1, down 2, left 4, right 8
          if(joystick < 32) joystickValuesSeen |= 1u << joystick;
          switchValuesSeen.insert(emulator.consoleSwitches());
        }
        computeMs.push_back(static_cast<float>(static_cast<double>(SDL_GetTicksNS() - frameBegan) / 1e6));
        pacer->ran(now); ++steps; ++frames;
      }
      if(steps > 0) {
        if(firstPictureMs.load() < 0) firstPictureMs = static_cast<double>(SDL_GetTicksNS() - started) / 1e6;
        std::vector<std::uint32_t> picture = emulator.colours();
        std::lock_guard<std::mutex> guard(lock);
        sharedPixels = std::move(picture);
        for(std::size_t i = 0; i < sharedRam.size(); ++i) sharedRam[i] = emulator.ram()[i];
        sharedHeight = emulator.height();
        sharedFrame = frames;
      }
      waitFor(pacer->untilDue(SDL_GetTicksNS()));
    }
    averageCorrectionPpm = pacer->averageCorrectionPpm();
  });

  // Test only (--push-keys): a helper thread puts key presses into this window's event queue, as the operating system would.
  std::thread pusher;
  if(options.pushKeys || options.pushMenu) {
    pusher = std::thread([&]() {
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
        tap(2.05, SDL_SCANCODE_ESCAPE); tap(2.1, SDL_SCANCODE_UP);   // back to the main page (on Game options), up to New game
        tap(3.0, SDL_SCANCODE_RETURN);                               // go
        tap(6.0, SDL_SCANCODE_ESCAPE); tap(6.5, SDL_SCANCODE_ESCAPE);  // open and close again
        tap(6.6, SDL_SCANCODE_ESCAPE);                                // Stage I7: open the menu, go to Graphics options, choose the Mirrored edge, back, close
        for(int i = 0; i < 3; ++i) tap(6.7 + 0.1 * i, SDL_SCANCODE_DOWN);
        tap(7.4, SDL_SCANCODE_RETURN); tap(7.5, SDL_SCANCODE_RIGHT); tap(7.6, SDL_SCANCODE_RIGHT);
        tap(7.7, SDL_SCANCODE_ESCAPE); tap(7.8, SDL_SCANCODE_ESCAPE);
        tap(8.2, SDL_SCANCODE_ESCAPE); tap(8.4, SDL_SCANCODE_UP); tap(8.6, SDL_SCANCODE_RETURN);  // Quit
      }
      const Uint64 t0 = SDL_GetTicksNS();
      for(const auto& step : plan) {
        while(!stop.load() && static_cast<double>(SDL_GetTicksNS() - t0) < step.at * 1e9) SDL_Delay(5);
        if(stop.load()) break;
        key(step.code, step.down);
      }
    });
  }

  // Stage I5: Escape opens the menu (and closes it); while it is open the arrow keys (or WASD), Enter and Escape belong to it. True = the key was used.
  long long menuOpenedAtFrame = 0, menuFramesRun = 0;  // frames the game ran while the menu was open (must be 0; read for the --push-menu report)
  int menuOpens = 0;
  auto framesNow = [&]() { std::lock_guard<std::mutex> guard(lock); return sharedFrame; };
  auto menuKey = [&](const SDL_Event& e, bool& running) -> bool {
    if(e.type != SDL_EVENT_KEY_DOWN) return false;
    const SDL_Scancode code = e.key.scancode;
    if(!menuSession.isOpen()) {
      if(code != SDL_SCANCODE_ESCAPE || e.key.repeat) return false;
      keyboard.releaseAll();  // no direction may stay held while the game is stopped
      menuSession.open(keyboard.input(), screen.backgroundKind);
      menuOpenedAtFrame = framesNow();
      ++menuOpens;
      return true;
    }
    static const struct { SDL_Scancode a, b; menu::Key key; bool repeats; } keys[] = {
        {SDL_SCANCODE_UP, SDL_SCANCODE_W, menu::Key::Up, true}, {SDL_SCANCODE_DOWN, SDL_SCANCODE_S, menu::Key::Down, true},
        {SDL_SCANCODE_LEFT, SDL_SCANCODE_A, menu::Key::Left, true}, {SDL_SCANCODE_RIGHT, SDL_SCANCODE_D, menu::Key::Right, true},
        {SDL_SCANCODE_RETURN, SDL_SCANCODE_KP_ENTER, menu::Key::Enter, false}, {SDL_SCANCODE_ESCAPE, SDL_SCANCODE_ESCAPE, menu::Key::Escape, false}};
    for(const auto& k : keys) {
      if(code != k.a && code != k.b) continue;
      if(e.key.repeat && !k.repeats) return true;
      const menu::Result result = menuSession.key(k.key);
      if(!menuSession.isOpen()) menuFramesRun += framesNow() - menuOpenedAtFrame;
      const menu::Choice& c = menuSession.menu().choice();
      keyboard.setSwitches(c.leftA, c.rightA, c.blackWhite);  // the switches take effect at once; the loop below remembers them in settings.ini
      if(c.background != screen.backgroundKind) {  // so does the background, which is remembered here
        screen.backgroundKind = c.background;
        settings::set(options.settingsFile, "background", background::settingName(c.background));
      }
      if(result == menu::Result::Quit) running = false;
      return true;
    }
    return false;
  };

  std::vector<std::uint32_t> pixels;
  std::array<std::uint8_t, 128> shownRam{};
  int height = 0;
  bool failed = false, stalled = false;
  // How many monitor refreshes (presents) each game frame was on screen: [0]=1 ... [4]=5 or more; and frames never shown.
  long long refreshHistogram[5] = {0, 0, 0, 0, 0}, framesNeverShown = 0, presents = 0;
  long long shownFrame = 0, shownCount = 0;
  for(bool running = true; running; ) {
    // Test only (--stall-ms): freeze this thread once, as dragging the window does, to show the sound does not depend on it.
    if(options.stallMs > 0 && !stalled && static_cast<double>(SDL_GetTicksNS() - started) >= 5e9) { SDL_Delay(static_cast<Uint32>(options.stallMs)); stalled = true; }
    for(SDL_Event e; SDL_PollEvent(&e); ) {
      if(e.type == SDL_EVENT_QUIT) running = false;
      if(handleWindowEvent(window, tracker, e)) continue;
      if(menuKey(e, running)) continue;
      if(e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat) {
        if(e.key.key == SDLK_1) { screen.view.mode = layout::Mode::Sharp; retitle(); }
        if(e.key.key == SDLK_2) { screen.view.mode = layout::Mode::Fill; retitle(); }
        if(e.key.key == SDLK_3) { screen.view.mode = layout::Mode::TvFrame; retitle(); }
        if(e.key.key == SDLK_4) { screen.view.mode = layout::Mode::GameArea; retitle(); }
        if(e.key.key == SDLK_5) { screen.view.mode = layout::Mode::Framed; retitle(); }
        if(e.key.scancode == SDL_SCANCODE_F7) { softBlend = !softBlend.load(); settings::set(options.settingsFile, "soft_glow", softBlend.load() ? "on" : "off"); }
      }
      if(e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat && e.key.scancode == SDL_SCANCODE_F1) helpOn = !helpOn;
      if(e.type == SDL_EVENT_KEY_DOWN && !menuSession.isOpen()) keyboard.keyDown(e.key.scancode, e.key.repeat);
      if(e.type == SDL_EVENT_KEY_UP && !menuSession.isOpen()) keyboard.keyUp(e.key.scancode);
      // a key let go while another window had the focus must not stay held
      if(e.type == SDL_EVENT_WINDOW_FOCUS_LOST) { keyboard.releaseAll(); pads.setActive(false); }
      if(e.type == SDL_EVENT_WINDOW_FOCUS_GAINED) pads.setActive(true);
      pads.handleEvent(e);
    }
    const controls::Input input = controls::merge(keyboard.input(), pads.poll());  // keyboard and pads together
    inputBits = input.pack();
    if(input.leftDifficultyA != savedSwitches.leftDifficultyA || input.rightDifficultyA != savedSwitches.rightDifficultyA || input.blackWhite != savedSwitches.blackWhite) {
      settings::set(options.settingsFile, "left_difficulty", input.leftDifficultyA ? "a" : "b");
      settings::set(options.settingsFile, "right_difficulty", input.rightDifficultyA ? "a" : "b");
      settings::set(options.settingsFile, "tv_type", input.blackWhite ? "bw" : "colour");
      savedSwitches = input;
    }
    long long frameNow = 0;
    {
      std::lock_guard<std::mutex> guard(lock);
      if(!sharedPixels.empty() && sharedFrame != shownFrame) { pixels = sharedPixels; height = sharedHeight; shownRam = sharedRam; }
      frameNow = sharedFrame;
    }
    if(frameNow != shownFrame) {  // a new game frame: close the count of the previous one
      if(shownFrame > 0) {
        ++refreshHistogram[std::min<long long>(shownCount, 5) - 1];
        framesNeverShown += frameNow - shownFrame - 1;
      }
      shownFrame = frameNow;
      shownCount = 0;
    }
    screen.ram = shownRam.data();
    if(!pixels.empty() && !screen.show(pixels, height)) { failed = true; running = false; }  // redrawn every time: a presented buffer is not kept
    if(helpOn) drawHelp(renderer, input, pads.names(), softBlend.load());
    if(menuSession.isOpen()) drawMenu(renderer, menuSession.menu());
    const Uint64 beforePresent = SDL_GetTicksNS();
    SDL_RenderPresent(renderer);
    ++presents;
    if(frameNow > 0) ++shownCount;
    if(options.presentHz > 0) {  // Stage H6 test: wait for the next tick of a pretend monitor
      const double period = 1e9 / options.presentHz;
      const Uint64 sinceStart = SDL_GetTicksNS() - started;
      const Uint64 nextTick = static_cast<Uint64>((std::floor(static_cast<double>(sinceStart) / period) + 1) * period);
      SDL_DelayPrecise(nextTick - sinceStart);
    } else if(SDL_GetTicksNS() - beforePresent < 500000) SDL_Delay(1);  // a present that does not wait for the monitor (window hidden): do not spin
    if(maxSeconds > 0 && static_cast<double>(SDL_GetTicksNS() - started) >= maxSeconds * 1e9) running = false;
  }
  stop = true;
  game.join();
  if(pusher.joinable()) pusher.join();
  if(!options.recordFile.empty() && !inputlog::save(options.recordFile, recorder.finish(romSha1, sharedControls))) std::fprintf(stderr, "The recording could not be written: %s\n", options.recordFile.c_str());
  if(failed) return 0;

  const double elapsed = static_cast<double>(SDL_GetTicksNS() - started) / 1e9;
  if(audio.isOpen()) {  // a short note on how the sound did, next to the settings (read by whoever tests)
    std::ofstream log(options.settingsFile + ".audio.txt");
    log << audioStatsText(audio, elapsed) << "game_thread_longest_gap_ms=" << longestGapMs << "\ngame_thread_gaps_over_30ms=" << gapsOver30
        << "\ngame_thread_gaps=" << gapLog << "\nclock=" << (usedSoundClock ? "sound card" : "PC timer") << "\naverage_clock_correction_ppm=" << averageCorrectionPpm
        << "\npresents=" << presents << "\npresents_per_second=" << static_cast<double>(presents) / elapsed
        << "\ngame_frames=" << sharedFrame << "\ngame_frames_per_second=" << static_cast<double>(sharedFrame) / elapsed
        << "\nrefreshes_per_game_frame_1=" << refreshHistogram[0] << "\nrefreshes_per_game_frame_2=" << refreshHistogram[1]
        << "\nrefreshes_per_game_frame_3=" << refreshHistogram[2] << "\nrefreshes_per_game_frame_4=" << refreshHistogram[3]
        << "\nrefreshes_per_game_frame_5_or_more=" << refreshHistogram[4] << "\ngame_frames_never_shown=" << framesNeverShown << '\n';
    // Stage H6: the game thread's own timing (the game thread has ended, so the lists can be read here)
    auto percentile = [](std::vector<float> v, double p) { if(v.empty()) return 0.0f; std::sort(v.begin(), v.end()); return v[static_cast<size_t>(p * static_cast<double>(v.size() - 1))]; };
    auto mean = [](const std::vector<float>& v) { double s = 0; for(float x : v) s += x; return v.empty() ? 0.0 : s / static_cast<double>(v.size()); };
    const auto over = [](const std::vector<float>& v, float limit) { return std::count_if(v.begin(), v.end(), [limit](float x) { return x > limit; }); };
    log << "frames_compute_over_16_7_ms=" << over(computeMs, 16.7f) << "\nframes_compute_over_5_ms=" << over(computeMs, 5.0f)
        << "\nframe_compute_ms_mean=" << mean(computeMs) << "\nframe_compute_ms_p99=" << percentile(computeMs, 0.99) << "\nframe_compute_ms_max=" << percentile(computeMs, 1.0)
        << "\nframe_start_late_ms_p99=" << percentile(lateMs, 0.99) << "\nframe_start_late_ms_max=" << percentile(lateMs, 1.0)
        << "\nfirst_picture_ms=" << firstPictureMs.load() << "\npresent_hz_simulated=" << options.presentHz << '\n';
    if(options.pushMenu) {
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
  return static_cast<double>(sharedFrame) / elapsed;
}

}  // namespace host
