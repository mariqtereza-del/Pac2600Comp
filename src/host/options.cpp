// The command line and the layout view: what the program was asked to do.
// (Stage G4.7: moved out of main.cpp, the code is unchanged.)
#include "options.hpp"

namespace host {

const char* modeName(layout::Mode m)
{
  switch(m) {
    case layout::Mode::Sharp: return "Sharp";
    case layout::Mode::Fill: return "Fill";
    case layout::Mode::TvFrame: return "TV frame";
    case layout::Mode::GameArea: return "Game area";
    case layout::Mode::Framed: return "Framed";
  }
  return "?";
}

namespace {

bool parseMode(const std::string& text, layout::Mode& mode)
{
  if(text == "sharp") mode = layout::Mode::Sharp;
  else if(text == "fill") mode = layout::Mode::Fill;
  else if(text == "tv") mode = layout::Mode::TvFrame;
  else if(text == "game") mode = layout::Mode::GameArea;
  else if(text == "framed") mode = layout::Mode::Framed;
  else return false;
  return true;
}

}  // namespace

namespace {

// Splits "AxB" or "A:B" into two whole numbers; false if it is not in that form.
bool parsePair(const std::string& text, char separator, int& first, int& second)
{
  const std::size_t cut = text.find(separator);
  if(cut == std::string::npos || cut == 0 || cut + 1 >= text.size()) return false;
  try {
    first = std::stoi(text.substr(0, cut));
    second = std::stoi(text.substr(cut + 1));
  } catch(...) {
    return false;
  }
  return first > 0 && second > 0;
}

}  // namespace

Options parseArguments(int argc, char* argv[])
{
  Options o;
  for(int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    auto next = [&]() -> std::string { return i + 1 < argc ? argv[++i] : std::string(); };
    if(a == "--self-check") o.selfCheckFile = next();
    else if(a == "--find-rom") o.findRomFile = next();
    else if(a == "--headless") o.headlessPng = next();
    else if(a == "--layout-check") o.layoutCheckPng = next();
    else if(a == "--mode") { if(!parseMode(next(), o.mode)) std::fprintf(stderr, "--mode must be sharp, fill, tv, game or framed\n"); }
    else if(a == "--keys-replay") { o.keysReplayScript = next(); o.keysReplayCsv = next(); }
    else if(a == "--help-check") o.helpCheckPng = next();
    else if(a == "--menu-check") o.menuCheckFile = next();
    else if(a == "--menu-shot") o.menuShotPng = next();
    else if(a == "--pad-check") o.padCheckFile = next();
    else if(a == "--stella-speed") o.stellaSpeed = true;
    else if(a == "--soft-glow") o.softGlow = true;
    else if(a == "--ram-seed") o.ramSeed = std::atoll(next().c_str());
    else if(a == "--trace") o.traceFile = next();
    else if(a == "--poke") {
      const std::string v = next();
      const std::size_t c1 = v.find(':'), c2 = c1 == std::string::npos ? c1 : v.find(':', c1 + 1);
      if(c2 != std::string::npos)
        o.pokes.push_back({std::atoi(v.substr(0, c1).c_str()), static_cast<int>(std::strtol(v.substr(c1 + 1, c2 - c1 - 1).c_str(), nullptr, 16)),
                           static_cast<int>(std::strtol(v.substr(c2 + 1).c_str(), nullptr, 16))});
      else std::fprintf(stderr, "--poke needs <frame>:<hex address>:<hex value>\n");
    }
    else if(a == "--record") o.recordFile = next();
    else if(a == "--replay") o.replayFile = next();
    else if(a == "--audio-check") { o.audioCheckSeconds = std::atof(next().c_str()); o.audioCheckFile = next(); }
    else if(a == "--audio-wav") o.audioWav = next();
    else if(a == "--reset-at") o.resetAt = std::atoi(next().c_str());
    else if(a == "--mute") o.mute = true;
    else if(a == "--timer-clock") o.timerClock = true;
    else if(a == "--push-keys") o.pushKeys = true;
    else if(a == "--push-menu") o.pushMenu = true;
    else if(a == "--stall-ms") o.stallMs = std::atoi(next().c_str());
    else if(a == "--present-hz") o.presentHz = std::atof(next().c_str());
    else if(a == "--fullscreen-check") o.fullscreenCheckFile = next();
    else if(a == "--state-report") o.stateReportFile = next();
    else if(a == "--window") { o.windowGiven = true; if(!parsePair(next(), 'x', o.windowW, o.windowH)) std::fprintf(stderr, "--window must look like 1280x960\n"); }
    else if(a == "--pixel-aspect") {
      const std::string text = next();
      if(text == "stella") o.pixelShape = {0, 0};
      else if(!parsePair(text, ':', o.pixelShape.num, o.pixelShape.den)) std::fprintf(stderr, "--pixel-aspect must look like 12:7, or be stella\n");
    }
    else if(a == "--timed") { o.timedSeconds = std::atof(next().c_str()); o.timedReport = next(); }
    else if(a == "--frames") o.frames = std::atoi(next().c_str());
    else if(a == "--save-frame") o.saveFrame = std::atoi(next().c_str());
    else if(a == "--raw") o.rawFile = next();
    else if(a == "--fingerprints") o.fingerprintFile = next();
    else if(a == "--settings") o.settingsFile = next();
    else if(a == "--roms-dir") o.romsDir = next();
    else if(o.romArg.empty()) o.romArg = a;
  }
  const char* base = SDL_GetBasePath();  // folder of the program, with a trailing slash
  const std::string exeDir = base ? base : "";
  if(o.settingsFile.empty()) o.settingsFile = exeDir + "settings.ini";
  if(o.romsDir.empty()) o.romsDir = exeDir + "roms";
  return o;
}

}  // namespace host
