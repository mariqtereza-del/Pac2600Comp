// Running the game with no window: headless runs, the key replay and the frame trace.
// (Stage G4.7: moved out of main.cpp, the code is unchanged.)
#include "headless.hpp"
#include "options.hpp"
#include "pads.hpp"
#include "rom_choice.hpp"

namespace host {

// One line of a frame trace (Stage E1): the input given on this frame and what the console did.
void writeTrace(std::ofstream& out, const Emulator& emulator, long long frame, std::uint32_t input)
{
  trace::Row row;
  row.frame = frame;
  row.input = input;
  row.switches = emulator.consoleSwitches();
  row.picture = emulator.pictureFingerprint();
  row.audio = trace::fnv1a(emulator.audio(), static_cast<std::size_t>(emulator.audioFrames()) * 4);  // 16-bit stereo: 4 bytes a pair
  for(std::size_t i = 0; i < row.ram.size(); ++i) row.ram[i] = emulator.ram()[i];
  out << trace::line(row);
}

// --keys-replay: runs the game with no window and presses the keys that a Phase C input script (lines "from to NAME",
// frames counted from 0, both ends included) asks for, in the real keyboard code, so the recorded Phase C runs can be
// compared frame by frame with what the keys do. Colour/B&W is the F4 switch (D12).
int keysReplay(const Options& o)
{
  std::ofstream report(o.keysReplayCsv + ".txt");
  const auto found = rom::findQuietly(o.romArg, o.settingsFile, o.romsDir);
  std::vector<std::uint8_t> bytes;
  if(!report || !found || found->check.status != rom::Status::Ok || !readRomBytes(found->check.path, bytes)) { if(report) report << "result=no usable ROM\n"; return 5; }
  Emulator emulator;
  if(const std::string error = emulator.load(fileNameOf(found->check.path), bytes); !error.empty()) { report << "result=" << error << '\n'; return 6; }

  struct Span { int from, to; std::string name; };
  std::vector<Span> spans;
  {
    std::ifstream in(o.keysReplayScript);
    if(!in) { report << "result=cannot read the script\n"; return 6; }
    for(std::string line; std::getline(in, line); ) {
      if(line.empty() || line[0] == '#' || line.rfind("dump", 0) == 0) continue;
      std::istringstream ls(line);
      Span sp{};
      if(ls >> sp.from >> sp.to >> sp.name) spans.push_back(sp);
    }
  }
  int last = 0;
  for(const Span& sp : spans) last = std::max(last, sp.to);
  const int frames = o.frames == 600 ? last + 1 : o.frames;  // --frames not given: run to the end of the script

  const struct { const char* name; controls::Action action; } held[] = {
      {"UP", controls::Action::Up}, {"DOWN", controls::Action::Down}, {"LEFT", controls::Action::Left}, {"RIGHT", controls::Action::Right},
      {"FIRE", controls::Action::Fire}, {"SELECT", controls::Action::Select}, {"RESET", controls::Action::Reset}};
  controls::Keyboard keyboard;
  std::ofstream csv(o.keysReplayCsv);
  csv << "frame,hash\n";
  char hash[17];
  for(int f = 0; f < frames; ++f) {
    std::set<std::string> active;
    for(const Span& sp : spans)
      if(f >= sp.from && f <= sp.to) active.insert(sp.name);
    for(const auto& h : held) {
      const SDL_Scancode key = controls::primaryKey(h.action);
      if(active.count(h.name)) keyboard.keyDown(key, false); else keyboard.keyUp(key);
    }
    // The difficulty switches are real switches: a script asking for A or B when it is not there yet presses the key once.
    auto flip = [&](bool wantA, bool isA, controls::Action action) {
      if(wantA == isA) return;
      keyboard.keyDown(controls::primaryKey(action), false);
      keyboard.keyUp(controls::primaryKey(action));
    };
    if(active.count("LDIFF_A")) flip(true, keyboard.input().leftDifficultyA, controls::Action::LeftDifficulty);
    if(active.count("LDIFF_B")) flip(false, keyboard.input().leftDifficultyA, controls::Action::LeftDifficulty);
    if(active.count("RDIFF_A")) flip(true, keyboard.input().rightDifficultyA, controls::Action::RightDifficulty);
    if(active.count("RDIFF_B")) flip(false, keyboard.input().rightDifficultyA, controls::Action::RightDifficulty);
    // The Colour / B&W switch is a real switch too (F4 flips it).
    if(active.count("COLOR")) flip(false, keyboard.input().blackWhite, controls::Action::TvType);
    if(active.count("BW")) flip(true, keyboard.input().blackWhite, controls::Action::TvType);

    applyInput(emulator, keyboard.input(), false);
    emulator.runFrame();
    std::snprintf(hash, sizeof hash, "%016llx", static_cast<unsigned long long>(emulator.pictureFingerprint()));
    csv << f << ',' << hash << '\n';
  }
  char swchb[8];
  std::snprintf(swchb, sizeof swchb, "%02x", emulator.consoleSwitches());
  report << "script=" << o.keysReplayScript << "\nframes=" << frames << "\nlast_frame_fingerprint=" << hash << "\nconsole_switches_at_end=" << swchb << "\nresult=OK\n";
  return 0;
}

// --headless: run the game with no window. Exit code 0 = done, 4 = problem with the ROM, 5 = no ROM found, 6 = other failure.
int headlessRun(const Options& o)
{
  const std::string reportPath = o.headlessPng + ".txt";
  std::ofstream report(reportPath);
  if(!report) return 6;

  const auto found = rom::findQuietly(o.romArg, o.settingsFile, o.romsDir);
  if(!found) { report << "result=no ROM found\n"; return 5; }
  if(found->check.status != rom::Status::Ok) { report << "result=ROM problem\n" << rom::describe(found->check) << '\n'; return 4; }

  std::vector<std::uint8_t> bytes(rom::kExpectedSize);
  {
    std::ifstream in(found->check.path, std::ios::binary);  // read only
    if(!in.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()))) { report << "result=cannot read ROM\n"; return 6; }
  }
  Emulator emulator;
  if(const std::string error = emulator.load(fileNameOf(found->check.path), bytes); !error.empty()) { report << "result=" << error << '\n'; return 6; }

  std::ofstream fingerprints;
  if(!o.fingerprintFile.empty()) { fingerprints.open(o.fingerprintFile); fingerprints << "frame,hash\n"; }

  if(o.ramSeed >= 0) emulator.randomisePowerOnRam(static_cast<std::uint32_t>(o.ramSeed));
  if(o.softGlow) emulator.setSoftBlend(true);

  // --replay: the console is given the recorded input of each frame; without --frames the whole recording is run.
  inputlog::Recording replay;
  const bool replaying = !o.replayFile.empty();
  if(replaying) {
    std::string error;
    if(!inputlog::load(o.replayFile, replay, error)) { report << "result=" << error << '\n'; return 6; }
    if(replay.romSha1 != found->check.sha1) { report << "result=the recording was made with a different ROM\n"; return 6; }
  }
  const int frames = replaying && o.frames == 600 ? static_cast<int>(replay.frames) : o.frames;
  std::ofstream traceOut;
  if(!o.traceFile.empty()) { traceOut.open(o.traceFile, std::ios::binary); traceOut << trace::header(found->check.sha1); }
  const int saveFrame = replaying && o.saveFrame >= frames ? std::max(0, frames - 1) : o.saveFrame;

  // Optional audio file: a standard WAV, 16-bit stereo at the emulator's rate, like the one the Phase C tool wrote.
  std::ofstream wav;
  std::uint32_t wavBytes = 0;
  auto put32 = [&](std::uint32_t v) { for(int i = 0; i < 4; ++i) wav.put(static_cast<char>((v >> (8 * i)) & 0xFF)); };
  auto put16 = [&](std::uint32_t v) { for(int i = 0; i < 2; ++i) wav.put(static_cast<char>((v >> (8 * i)) & 0xFF)); };
  const std::uint32_t rate = static_cast<std::uint32_t>(emulator.audioRate());
  if(!o.audioWav.empty()) {
    wav.open(o.audioWav, std::ios::binary);
    wav.write("RIFF", 4); put32(0); wav.write("WAVEfmt ", 8); put32(16);
    put16(1); put16(2); put32(rate); put32(rate * 4); put16(4); put16(16);
    wav.write("data", 4); put32(0);
  }

  bool saved = false;
  char hash[17];
  for(int f = 0; f < frames; ++f) {
    controls::Input in = controls::Input::unpack(replaying ? replay.bitsAt(f) : 0u);
    if(o.resetAt >= 0 && f >= o.resetAt && f < o.resetAt + 10) in.reset = true;
    if(replaying) applyInput(emulator, in, false);
    else emulator.setConsoleReset(in.reset);
    for(const auto& pk : o.pokes)
      if(pk[0] == f) emulator.pokeRam(pk[1], static_cast<std::uint8_t>(pk[2]));
    emulator.runFrame();
    if(traceOut) writeTrace(traceOut, emulator, f, in.pack());
    if(wav) {
      wav.write(reinterpret_cast<const char*>(emulator.audio()), static_cast<std::streamsize>(emulator.audioFrames()) * 4);
      wavBytes += static_cast<std::uint32_t>(emulator.audioFrames()) * 4;
    }
    std::snprintf(hash, sizeof hash, "%016llx", static_cast<unsigned long long>(emulator.pictureFingerprint()));
    if(fingerprints) fingerprints << f << ',' << hash << '\n';
    if(f != saveFrame) continue;

    const int h = emulator.height();
    std::vector<std::uint32_t> pixels = emulator.colours();
    SDL_Surface* surface = SDL_CreateSurfaceFrom(Emulator::kWidth, h, SDL_PIXELFORMAT_XRGB8888, pixels.data(), Emulator::kWidth * 4);
    saved = surface && SDL_SavePNG(surface, o.headlessPng.c_str());
    if(surface) SDL_DestroySurface(surface);
    if(!o.rawFile.empty()) {
      std::ofstream raw(o.rawFile, std::ios::binary);
      raw.write(reinterpret_cast<const char*>(emulator.indexed()), static_cast<std::streamsize>(h) * Emulator::kWidth);
    }
    report << "saved_frame=" << f << "\nsaved_picture=" << Emulator::kWidth << 'x' << h
           << "\nsaved_frame_fingerprint=" << hash << '\n';
  }
  if(wav) {  // fill in the two sizes of the WAV header
    wav.seekp(4); put32(36 + wavBytes);
    wav.seekp(40); put32(wavBytes);
    wav.close();
    report << "audio_wav=" << o.audioWav << "\naudio_rate=" << rate << "\naudio_bytes=" << wavBytes << '\n';
  }
  report << "final_ram=";
  for(int i = 0; i < 128; ++i) { char b[3]; std::snprintf(b, sizeof b, "%02x", emulator.ram()[i]); report << b; }
  report << '\n';
  report << "rom=" << found->check.path << "\nrom_sha1=" << found->check.sha1 << "\nframes_run=" << frames
         << "\nlast_frame_fingerprint=" << hash << "\npng_saved=" << (saved ? "yes" : "no") << "\nresult=" << (saved ? "OK" : "FAIL") << '\n';
  return saved ? 0 : 6;
}

}  // namespace host
