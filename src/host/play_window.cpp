// Playing the game in the window.
// (Stage G4.7: moved out of main.cpp, the code is unchanged.)
#include "play_window.hpp"
#include "headless.hpp"
#include "menu_checks.hpp"
#include "menu_session.hpp"
#include "framerate.hpp"
#include "key_pusher.hpp"
#include "options.hpp"
#include "pacer.hpp"
#include "pads.hpp"
#include "render_setup.hpp"
#include "rom_choice.hpp"
#include "screen.hpp"
#include "window_tracker.hpp"

namespace host {

// The game window: one emulator frame per 1/60 s from a simple clock (temporary; real pacing is Stage D9).
// With maxSeconds > 0 it stops by itself after that long and returns the frames per second it managed (for tests).
// Keys 1, 2, 3, 4 choose Sharp, Fill, TV frame, Game area; Alt+Enter toggles fullscreen (temporary; Stage D10 gives the keys their final home).
double playWindow(SDL_Window* window, SDL_Renderer*& renderer, const std::string& romPath, double maxSeconds, View view, Tracker& tracker, const Options& options)
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

  // Stage I12: the volume and the mute button (menu, Sound options; settings.ini volume=0..100, sound_muted=on|off). They change the played samples only.
  const std::string volumeSetting = settings::get(options.settingsFile, "volume");
  int volumePercent = volumeSetting.empty() ? 100 : std::clamp(std::atoi(volumeSetting.c_str()), 0, 100);
  bool soundMuted = settings::get(options.settingsFile, "sound_muted") == "on";
  audio.setVolume(volumePercent, soundMuted);

  Screen screen;
  screen.renderer = renderer;
  screen.view = view;
  auto retitle = [&]() {
    const std::string title = std::string(kTitle) + " - " + fileNameOf(romPath) + " - " + modeName(screen.view.mode) + (replaying ? " - REPLAY" : "") + (options.recordFile.empty() ? "" : " - RECORDING") + (emulator.smoothAvailable() ? "" : " - REFERENCE PROGRAM, no smooth motion") + " - F1 = keys";
    SDL_SetWindowTitle(window, title.c_str());
  };
  retitle();
  // Stage I7: what fills the space around the picture, remembered in settings.ini (background=black|glow|mirror|solid|gradient|arcade|bezel)
  screen.backgroundKind = background::fromSetting(settings::get(options.settingsFile, "background"));
  screen.crt = crt::fromSetting(options.crt.empty() ? settings::get(options.settingsFile, "crt_filter") : options.crt);   // Stage I11: the CRT filter (menu, Graphics options)

  // The game and its sound run on their own thread (DECISIONS.md T-D8-2), so that a window that is being dragged or
  // resized, which freezes this thread on Windows, cannot starve the sound card. This thread only shows the pictures.
  std::atomic<bool> stop{false};
  std::atomic<std::uint32_t> inputBits{0};  // what the keyboard adds up to (controls::Input::pack), set here, read by the game thread
  controls::Keyboard keyboard;
  // The three real switches start where they were left (settings.ini; the defaults are B, B and Colour).
  keyboard.setSwitches(settings::get(options.settingsFile, "left_difficulty") == "a", settings::get(options.settingsFile, "right_difficulty") == "a",
                       settings::get(options.settingsFile, "tv_type") == "bw");
  keyboard.setScheme(controls::schemeFromSetting(settings::get(options.settingsFile, "keyboard")));   // Stage I13: which keys steer (menu, Control options)
  controls::Input savedSwitches = keyboard.input();
  Pads pads;
  menu::Session menuSession(!replaying);  // the Escape menu (Stage I5); a replay decides what the console is given, so it cannot start a game
  if(!pads.start()) std::fprintf(stderr, "Gamepads could not start; the keyboard still works.\n");
  bool helpOn = false;
  // Stella's soft glow (F7): off unless the owner turned it on; remembered in settings.ini as soft_glow=on|off
  std::atomic<bool> softBlend{settings::get(options.settingsFile, "soft_glow") == "on"};
  // Stage I9.4: the frame rate of the picture (settings.ini frame_rate=30|60|75|120|144|unlocked, --fps). Above 60 the moving objects are drawn between their places.
  framerate::Rate rate = framerate::fromSetting(options.fps.empty() ? settings::get(options.settingsFile, "frame_rate") : options.fps);
  if(!emulator.smoothAvailable() && framerate::smooth(rate)) rate = framerate::Rate::Fps60;   // the reference program has no smooth motion
  // Stage I10.1: whether the renderer is in the HDR colour space; made again in the other one when Windows turns HDR on or off (SDL_EVENT_WINDOW_HDR_STATE_CHANGED)
  bool hdrWanted = hdrWantedBy(options);
  int hdrPercent = std::clamp(std::atoi(settings::get(options.settingsFile, "hdr_highlights").empty() ? "50" : settings::get(options.settingsFile, "hdr_highlights").c_str()), 0, 100);
  hdr::ScreenState screenState = screenStateOf(window);
  bool hdrPipeline = SDL_GetNumberProperty(SDL_GetRendererProperties(renderer), SDL_PROP_RENDERER_OUTPUT_COLORSPACE_NUMBER, 0) == SDL_COLORSPACE_SRGB_LINEAR;
  std::atomic<bool> smoothWanted{framerate::smooth(rate) || (hdrPipeline && hdrPercent > 0)};
  // how many times brighter than normal white the moving objects are drawn (1 = not brighter: no HDR, or the slider at 0)
  auto highlight = [&]() { return hdr::highlightScale(static_cast<float>(hdrPercent) / 100.0f, screenState, hdrPipeline); };
  // the layers and the look-ahead are made when the frame rate is smooth, and for the highlights, which are drawn on the moving objects alone
  auto updateSmoothWanted = [&]() { smoothWanted = framerate::smooth(rate) || (hdrPipeline && hdrPercent > 0); };
  bool counterOn = settings::get(options.settingsFile, "frame_counter") == "on";   // the pictures-a-second counter (menu, Game options; frame_counter=on|off)
  auto applyVSync = [&]() { SDL_SetRenderVSync(renderer, (options.presentHz > 0 || framerate::unlocked(rate)) ? 0 : 1); };
  applyVSync();   // the screen's own wait is off while the frame rate is unlocked
  auto adaptToHdr = [&]() {
    screenState = screenStateOf(window);
    const bool want = hdr::useHdr(hdrWanted, screenState);
    if(want == hdrPipeline) { updateSmoothWanted(); return; }
    screen.dropTextures();
    SDL_DestroyRenderer(renderer);
    renderer = createRenderer(window, want, &hdrPipeline);
    screen.renderer = renderer;
    if(renderer) applyVSync();
    updateSmoothWanted();
  };
  const double stepSeconds = 1.0 / frameRate(options);   // the length of one game step
  std::mutex lock;  // guards the shared picture below
  std::vector<std::uint32_t> sharedPixels;
  int sharedHeight = 0;
  std::array<std::uint8_t, 128> sharedRam{};  // the game's memory after the frame that sharedPixels shows (for the arcade panel)
  long long sharedFrame = 0;  // number of the picture in sharedPixels (frames run so far)
  std::shared_ptr<const SmoothSnapshot> sharedSnapshot;  // the same frame as a smooth picture, when smooth motion is on
  Uint64 sharedStepAt = 0;                                 // when the game thread had it ready (SDL ticks, nanoseconds)
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
    bool smoothApplied = false;
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
        if(smoothWanted.load() != smoothApplied) { smoothApplied = smoothWanted.load(); emulator.setSmooth(smoothApplied); }
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
        std::shared_ptr<const SmoothSnapshot> snapshot = emulator.smoothSnapshot();
        const Uint64 readyAt = SDL_GetTicksNS();
        std::lock_guard<std::mutex> guard(lock);
        sharedPixels = std::move(picture);
        sharedSnapshot = std::move(snapshot);
        sharedStepAt = readyAt;
        for(std::size_t i = 0; i < sharedRam.size(); ++i) sharedRam[i] = emulator.ram()[i];
        sharedHeight = emulator.height();
        sharedFrame = frames;
      }
      waitFor(pacer->untilDue(SDL_GetTicksNS()));
    }
    averageCorrectionPpm = pacer->averageCorrectionPpm();
  });

  std::thread pusher = startKeyPusher(options, stop);   // test only: --push-keys, --push-menu

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
      menuSession.open(keyboard.input(), menu::Session::Window{screen.backgroundKind, rate, counterOn, emulator.smoothAvailable(), screen.crt, hdrWanted, hdrPercent, screenState.hdrOn,
                                                                         volumePercent, soundMuted, keyboard.scheme()});
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
      auto remember = [&](const char* key, const std::string& value) { settings::set(options.settingsFile, key, value); };   // every choice takes effect at once and is remembered
      if(c.crt != screen.crt) { screen.crt = c.crt; remember("crt_filter", crt::settingName(screen.crt)); }
      if(c.hdr != hdrWanted) { hdrWanted = c.hdr; remember("hdr", hdrWanted ? "on" : "off"); adaptToHdr(); }
      if(c.hdrStrength != hdrPercent) { hdrPercent = c.hdrStrength; remember("hdr_highlights", std::to_string(hdrPercent)); updateSmoothWanted(); }
      if(c.frameRate != rate) { rate = c.frameRate; updateSmoothWanted(); applyVSync(); remember("frame_rate", framerate::settingName(rate)); }
      if(c.keyboard != keyboard.scheme()) { keyboard.setScheme(c.keyboard); remember("keyboard", controls::schemeSettingName(c.keyboard)); }
      if(c.volume != volumePercent || c.muted != soundMuted) {
        volumePercent = c.volume; soundMuted = c.muted; audio.setVolume(volumePercent, soundMuted);
        remember("volume", std::to_string(volumePercent)); remember("sound_muted", soundMuted ? "on" : "off");
      }
      if(c.frameCounter != counterOn) { counterOn = c.frameCounter; remember("frame_counter", counterOn ? "on" : "off"); }
      if(result == menu::Result::Quit) running = false;
      return true;
    }
    return false;
  };

  std::vector<std::uint32_t> pixels;
  std::shared_ptr<const SmoothSnapshot> snapshot;  // the smooth picture of the latest game step
  Uint64 snapshotAt = 0;
  std::vector<SmoothRect> moving;
  long long picturesShown = 0, picturesAtMark = 0;
  Uint64 markAt = SDL_GetTicksNS();
  double picturesPerSecond = 0;
  long long lastShownFrame = -1, lastGrid = -2;
  std::shared_ptr<const SmoothSnapshot> lastSnapshot;   // the snapshot the picture on screen was made from
  std::shared_ptr<const SmoothSnapshot> shownSnapshot;  // the snapshot of the game picture on screen (30 and 60: highlights are drawn on it)
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
      if(e.type == SDL_EVENT_WINDOW_HDR_STATE_CHANGED) adaptToHdr();   // Windows turned HDR on or off for this screen (or the window moved to another)
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
      if(!sharedPixels.empty() && sharedFrame != shownFrame && framerate::shownStep(rate, sharedFrame) == sharedFrame) { pixels = sharedPixels; height = sharedHeight; shownRam = sharedRam; shownSnapshot = sharedSnapshot; }
      snapshot = sharedSnapshot;
      snapshotAt = sharedStepAt;
      if(snapshot) height = sharedHeight;
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
    screen.gameFrame = frameNow;
    const bool smoothNow = screen.crt == crt::Filter::Off && framerate::smooth(rate) && snapshot && !snapshot->background.empty();
    if(smoothNow) {   // the picture without the moving objects, and Pac-Man, the pills and the ghosts between their places (no delay: each is on its way to where the original shows it next)
      const double nowSeconds = static_cast<double>(SDL_GetTicksNS()) / 1e9;
      const long long grid = framerate::pictureIndex(rate, nowSeconds);   // the picture of this moment of the clock: made once, kept until the next
      if(grid < 0 || grid != lastGrid || !lastSnapshot) {
        const double progress = framerate::progress(framerate::pictureTime(rate, nowSeconds), static_cast<double>(snapshotAt) / 1e9, stepSeconds);
        snapshot->compose(progress, true, moving);
        lastSnapshot = snapshot;
        lastGrid = grid;
        ++picturesShown;
      }
      if(!screen.show(lastSnapshot->background, height, &moving, highlight())) { failed = true; running = false; }
    } else {
      lastSnapshot.reset();
      const bool brighter = screen.crt == crt::Filter::Off && highlight() > 1.0f && shownSnapshot && !shownSnapshot->background.empty();
      if(brighter) {   // 30 and 60 with HDR highlights: the game's own picture, its moving objects drawn again brighter on top (the same pixels, composed exactly as the original)
        shownSnapshot->compose(0.0, false, moving);
        if(!screen.show(shownSnapshot->background, height, &moving, highlight())) { failed = true; running = false; }
      } else if(!pixels.empty() && !screen.show(pixels, height)) { failed = true; running = false; }  // redrawn every time: a presented buffer is not kept
      if(frameNow != lastShownFrame && framerate::shownStep(rate, frameNow) == frameNow) ++picturesShown;   // a new picture of the game
    }
    lastShownFrame = frameNow;
    if(helpOn) drawHelp(renderer, input, pads.names(), softBlend.load(), keyboard.scheme());
    if(menuSession.isOpen()) drawMenu(renderer, menuSession.menu());
    if(const Uint64 t = SDL_GetTicksNS(); t - markAt >= 500000000ull) {   // pictures a second, over the last half second
      picturesPerSecond = static_cast<double>(picturesShown - picturesAtMark) * 1e9 / static_cast<double>(t - markAt);
      picturesAtMark = picturesShown;
      markAt = t;
    }
    if(counterOn) drawFrameCounter(renderer, rate, picturesPerSecond);
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
        << "\nhdr_pipeline=" << (hdrPipeline ? "yes" : "no") << "\nwindows_hdr_on=" << (screenStateOf(window).hdrOn ? "yes" : "no")
        << "\nwindows_hdr_headroom=" << screenStateOf(window).headroom << "\nhdr_wanted=" << (hdrWanted ? "yes" : "no") << "\nhdr_highlights_percent=" << hdrPercent
        << "\nvolume_percent=" << volumePercent << "\nsound_muted=" << (soundMuted ? "yes" : "no")
        << "\nframe_rate=" << framerate::settingName(rate) << "\npictures_shown=" << picturesShown << "\npictures_per_second=" << static_cast<double>(picturesShown) / elapsed
        << "\ngame_frames=" << sharedFrame << "\ngame_frames_per_second=" << static_cast<double>(sharedFrame) / elapsed
        << "\nrefreshes_per_game_frame_1=" << refreshHistogram[0] << "\nrefreshes_per_game_frame_2=" << refreshHistogram[1]
        << "\nrefreshes_per_game_frame_3=" << refreshHistogram[2] << "\nrefreshes_per_game_frame_4=" << refreshHistogram[3]
        << "\nrefreshes_per_game_frame_5_or_more=" << refreshHistogram[4] << "\ngame_frames_never_shown=" << framesNeverShown << '\n';
    log << frameTimingText(computeMs, lateMs, firstPictureMs.load(), options.presentHz);   // Stage H6: the game thread's own timing (it has ended, so the lists can be read here)
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
