// Playing the game in the window.
// (Stage G4.7: moved out of main.cpp, the code is unchanged.)
#include "play_window.hpp"
#include "headless.hpp"
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
  if(!pads.start()) std::fprintf(stderr, "Gamepads could not start; the keyboard still works.\n");
  bool helpOn = false;
  // Stella's soft glow (F7): off unless the owner turned it on; remembered in settings.ini as soft_glow=on|off
  std::atomic<bool> softBlend{settings::get(options.settingsFile, "soft_glow") == "on"};
  std::mutex lock;  // guards the shared picture below
  std::vector<std::uint32_t> sharedPixels;
  int sharedHeight = 0;
  long long sharedFrame = 0;  // number of the picture in sharedPixels (frames run so far)
  double longestGapMs = 0;
  int gapsOver30 = 0;
  std::string gapLog;
  bool usedSoundClock = false;
  double averageCorrectionPpm = 0;
  std::uint32_t joystickValuesSeen = 0;      // bit n set = the game's joystick byte (RAM $ED) held the value n
  std::set<int> switchValuesSeen;            // the console switch register values the game could read
  const Uint64 started = SDL_GetTicksNS();

  std::thread game([&]() {
    Pacer pacer(audio, options.timerClock, frameRate(options));
    usedSoundClock = pacer.usesSoundClock();
    Uint64 lastLoop = SDL_GetTicksNS();
    long long frames = 0;
    bool glowApplied = false;  // the emulator starts without the glow
    while(!stop.load()) {
      const Uint64 now = SDL_GetTicksNS();
      const double gap = static_cast<double>(now - lastLoop) / 1e6;  // how long this thread went without running
      lastLoop = now;
      if(gap > 30.0) {
        ++gapsOver30;
        if(gapLog.size() < 400) gapLog += " " + std::to_string(static_cast<int>(gap)) + "ms@frame" + std::to_string(frames);
      }
      longestGapMs = std::max(longestGapMs, gap);

      int steps = 0;
      while(steps < 5 && pacer.due(now)) {
        if(softBlend.load() != glowApplied) { glowApplied = softBlend.load(); emulator.setSoftBlend(glowApplied); }
        // A replay gives the console what was recorded; when it has run out, the keyboard and pads take over again.
        controls::Input in = controls::Input::unpack(replaying && frames < replay.frames ? replay.bitsAt(frames) : inputBits.load());
        if(options.resetAt >= 0 && frames >= options.resetAt && frames < options.resetAt + 10) in.reset = true;
        recorder.add(in.pack());
        applyInput(emulator, in, false);
        emulator.runFrame();
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
        pacer.ran(now); ++steps; ++frames;
      }
      if(steps > 0) {
        std::vector<std::uint32_t> picture = emulator.colours();
        std::lock_guard<std::mutex> guard(lock);
        sharedPixels = std::move(picture);
        sharedHeight = emulator.height();
        sharedFrame = frames;
      }
      waitFor(pacer.untilDue(SDL_GetTicksNS()));
    }
    averageCorrectionPpm = pacer.averageCorrectionPpm();
  });

  // Test only (--push-keys): a helper thread puts key presses into this window's event queue, as the operating system would.
  std::thread pusher;
  if(options.pushKeys) {
    pusher = std::thread([&]() {
      auto key = [](SDL_Scancode code, bool down) {
        SDL_Event e{};
        e.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
        e.key.scancode = code;
        e.key.key = SDL_GetKeyFromScancode(code, SDL_KMOD_NONE, false);
        e.key.down = down;
        SDL_PushEvent(&e);
      };
      const struct { double at; SDL_Scancode code; bool down; } plan[] = {
          {0.5, SDL_SCANCODE_F3, true}, {0.8, SDL_SCANCODE_F3, false},          // Game Reset: a game starts
          {4.0, SDL_SCANCODE_RIGHT, true}, {4.6, SDL_SCANCODE_RIGHT, false},    // joystick right (Pac-Man starts moving at about 2 s)
          {5.0, SDL_SCANCODE_A, true}, {5.6, SDL_SCANCODE_A, false},            // left, with the letter key
          {6.0, SDL_SCANCODE_UP, true}, {6.6, SDL_SCANCODE_UP, false},          // up
          {7.0, SDL_SCANCODE_S, true}, {7.6, SDL_SCANCODE_S, false},            // down, with the letter key
          {8.0, SDL_SCANCODE_F5, true}, {8.1, SDL_SCANCODE_F5, false},          // left difficulty to A
          {8.5, SDL_SCANCODE_F6, true}, {8.6, SDL_SCANCODE_F6, false},          // right difficulty to A
          {9.0, SDL_SCANCODE_F4, true}, {9.1, SDL_SCANCODE_F4, false},          // Colour to B&W
          {9.5, SDL_SCANCODE_F7, true}, {9.6, SDL_SCANCODE_F7, false}};         // soft glow on
      const Uint64 t0 = SDL_GetTicksNS();
      for(const auto& step : plan) {
        while(!stop.load() && static_cast<double>(SDL_GetTicksNS() - t0) < step.at * 1e9) SDL_Delay(5);
        if(stop.load()) break;
        key(step.code, step.down);
      }
    });
  }

  std::vector<std::uint32_t> pixels;
  int height = 0;
  bool failed = false, stalled = false;
  // How many monitor refreshes (presents) each game frame was on screen: [0]=1 ... [4]=5 or more; and frames never shown.
  long long refreshHistogram[5] = {0, 0, 0, 0, 0}, framesNeverShown = 0, presents = 0;
  long long shownFrame = 0, shownCount = 0;
  for(bool running = true; running; ) {
    // Test only (--stall-ms): freeze this thread once, as dragging the window does, to show the sound does not depend on it.
    if(options.stallMs > 0 && !stalled && static_cast<double>(SDL_GetTicksNS() - started) >= 5e9) { SDL_Delay(static_cast<Uint32>(options.stallMs)); stalled = true; }
    for(SDL_Event e; SDL_PollEvent(&e); ) {
      if(e.type == SDL_EVENT_QUIT || (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_ESCAPE)) running = false;
      if(handleWindowEvent(window, tracker, e)) continue;
      if(e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat) {
        if(e.key.key == SDLK_1) { screen.view.mode = layout::Mode::Sharp; retitle(); }
        if(e.key.key == SDLK_2) { screen.view.mode = layout::Mode::Fill; retitle(); }
        if(e.key.key == SDLK_3) { screen.view.mode = layout::Mode::TvFrame; retitle(); }
        if(e.key.key == SDLK_4) { screen.view.mode = layout::Mode::GameArea; retitle(); }
        if(e.key.key == SDLK_5) { screen.view.mode = layout::Mode::Framed; retitle(); }
        if(e.key.scancode == SDL_SCANCODE_F7) { softBlend = !softBlend.load(); settings::set(options.settingsFile, "soft_glow", softBlend.load() ? "on" : "off"); }
      }
      if(e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat && e.key.scancode == SDL_SCANCODE_F1) helpOn = !helpOn;
      if(e.type == SDL_EVENT_KEY_DOWN) keyboard.keyDown(e.key.scancode, e.key.repeat);
      if(e.type == SDL_EVENT_KEY_UP) keyboard.keyUp(e.key.scancode);
      if(e.type == SDL_EVENT_WINDOW_FOCUS_LOST) { keyboard.releaseAll(); pads.setActive(false); }  // a key let go while another window had the focus must not stay held
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
      if(!sharedPixels.empty() && sharedFrame != shownFrame) { pixels = sharedPixels; height = sharedHeight; }
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
    if(!pixels.empty() && !screen.show(pixels, height)) { failed = true; running = false; }  // redrawn every time: a presented buffer is not kept
    if(helpOn) drawHelp(renderer, input, pads.names(), softBlend.load());
    const Uint64 beforePresent = SDL_GetTicksNS();
    SDL_RenderPresent(renderer);
    ++presents;
    if(frameNow > 0) ++shownCount;
    if(SDL_GetTicksNS() - beforePresent < 500000) SDL_Delay(1);  // a present that does not wait for the monitor (window hidden): do not spin
    if(maxSeconds > 0 && static_cast<double>(SDL_GetTicksNS() - started) >= maxSeconds * 1e9) running = false;
  }
  stop = true;
  game.join();
  if(pusher.joinable()) pusher.join();
  if(!options.recordFile.empty() && !inputlog::save(options.recordFile, recorder.finish(romSha1))) std::fprintf(stderr, "The recording could not be written: %s\n", options.recordFile.c_str());
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
