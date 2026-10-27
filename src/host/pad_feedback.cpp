#include "pad_feedback.hpp"

#include <algorithm>

namespace feedback {

Light nextLight(Light light, int step) { return static_cast<Light>((static_cast<int>(light) + kLights + step) % kLights); }

const char* lightTitle(Light light)
{
  switch(light) {
    case Light::Off: return "Off";
    case Light::Yellow: return "Pac-Man yellow";
    case Light::Maze: return "Maze colour";
    case Light::Lives: return "Lives left";
  }
  return "Off";
}

const char* lightSettingName(Light light)
{
  switch(light) {
    case Light::Off: return "off";
    case Light::Yellow: return "yellow";
    case Light::Maze: return "maze";
    case Light::Lives: return "lives";
  }
  return "off";
}

Light lightFromSetting(const std::string& text)
{
  for(int i = 0; i < kLights; ++i)
    if(text == lightSettingName(static_cast<Light>(i))) return static_cast<Light>(i);
  return Light::Off;
}

background::Rgb lightColour(Light light, const background::Rgb& maze, int lives)
{
  switch(light) {
    case Light::Off: return defaultLightColour();
    case Light::Yellow: return {255, 210, 0};
    case Light::Maze: return maze;
    case Light::Lives:
      if(lives >= 3) return {0, 200, 0};
      if(lives == 2) return {255, 200, 0};
      if(lives == 1) return {255, 0, 0};
      return {40, 40, 40};
  }
  return defaultLightColour();
}

background::Rgb defaultLightColour() { return {0, 0, 64}; }

Rumble nextRumble(Rumble rumble, int step) { return static_cast<Rumble>((static_cast<int>(rumble) + kRumbles + step) % kRumbles); }

const char* rumbleTitle(Rumble rumble) { return rumble == Rumble::Light ? "Light" : rumble == Rumble::Strong ? "Strong" : "Off"; }

const char* rumbleSettingName(Rumble rumble) { return rumble == Rumble::Light ? "light" : rumble == Rumble::Strong ? "strong" : "off"; }

Rumble rumbleFromSetting(const std::string& text) { return text == "light" ? Rumble::Light : text == "strong" ? Rumble::Strong : Rumble::Off; }

Pulse pulseFor(unsigned events, Rumble rumble)
{
  Pulse p;
  if(rumble == Rumble::Off) return p;
  const unsigned big = rumble == Rumble::Strong ? 0xC000u : 0x5000u, small = rumble == Rumble::Strong ? 0x8000u : 0x3000u;
  auto add = [&](unsigned low, unsigned high, std::uint32_t ms) {   // low and high as a share of the full strength, in sixteenths
    p.low = static_cast<std::uint16_t>(std::max<unsigned>(p.low, big * low / 16));
    p.high = static_cast<std::uint16_t>(std::max<unsigned>(p.high, small * high / 16));
    p.ms = std::max(p.ms, ms);
  };
  if(events & PowerPill) add(8, 16, 120);     // a short tick
  if(events & GhostEaten) add(12, 12, 220);   // a firm thump
  if(events & Death) add(16, 6, 700);         // a long low rumble
  return p;
}

unsigned EventWatcher::update(const std::uint8_t* ram)
{
  const bool inPlay = ram[0x82 - 0x80] == 0;   // $82 is not 0 on the title screen and after the game is over
  const bool dying = inPlay && (ram[0xE7 - 0x80] & 0x40) != 0;
  const int player = background::playerToPlay(ram);
  const int score = background::scoreOf(ram, player);
  unsigned events = 0;
  if(havePrevious_) {
    if(dying && !dying_) events |= Death;
    if(inPlay && !dying && player == player_) {
      const int rise = score - score_;
      if(rise == 5) events |= PowerPill;
      if(rise == 20 || rise == 40 || rise == 80 || rise == 160) events |= GhostEaten;
    }
  }
  havePrevious_ = true;
  dying_ = dying;
  score_ = score;
  player_ = player;
  return events;
}

void Hub::gameFrame(const std::uint8_t* ram)
{
  if(const unsigned e = watcher_.update(ram)) pending_ |= e;
  lives_ = background::livesOf(ram);
}

}  // namespace feedback
