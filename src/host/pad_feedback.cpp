#include "pad_feedback.hpp"

#include <algorithm>

namespace feedback {

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
  if(events & Wafer) add(6, 16, 50);          // a tiny tap, felt on both strengths: Pac-Man eats a wafer about every eight frames
  if(events & PowerPill) add(8, 16, 120);     // a short tick
  if(events & GhostEaten) add(12, 12, 220);   // a firm thump
  if(events & Death) add(16, 6, 700);         // a long low rumble
  return p;
}

background::Rgb yellowLight() { return {255, 210, 0}; }

background::Rgb defaultLightColour() { return {0, 0, 64}; }

background::Rgb LightShow::step(unsigned events, bool difficultyA)
{
  constexpr int kFlashingFrames = 64, kFlashFrames = 14, kDyingFrames = 130;
  if(events & Death) { dying_ = kDyingFrames; scared_ = 0; flash_ = 0; }
  if(events & PowerPill) scared_ = (difficultyA ? 160 : 320) + kFlashingFrames;
  if(events & GhostEaten) flash_ = kFlashFrames;
  const background::Rgb colour = dying_ > 0 ? background::Rgb{255, 0, 0}
                                 : flash_ > 0 ? background::Rgb{255, 255, 255}
                                 : scared_ > kFlashingFrames ? background::Rgb{30, 60, 255}
                                 : scared_ > 0 ? (scared_ / 8 % 2 ? background::Rgb{255, 255, 255} : background::Rgb{30, 60, 255})
                                               : yellowLight();
  if(dying_ > 0) --dying_;
  if(flash_ > 0) --flash_;
  if(scared_ > 0) --scared_;
  return colour;
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
      if(rise == 1) events |= Wafer;
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

void Hub::gameFrame(const std::uint8_t* ram, bool leftDifficultyA, bool rightDifficultyA)
{
  const unsigned e = watcher_.update(ram);
  if(e) pending_ |= e;
  const background::Rgb c = show_.step(e, background::playerToPlay(ram) == 1 ? rightDifficultyA : leftDifficultyA);
  colour_ = (static_cast<std::uint32_t>(c.r) << 16) | (static_cast<std::uint32_t>(c.g) << 8) | c.b;
}

background::Rgb Hub::light() const
{
  const std::uint32_t c = colour_.load();
  return {static_cast<std::uint8_t>(c >> 16), static_cast<std::uint8_t>(c >> 8), static_cast<std::uint8_t>(c)};
}

}  // namespace feedback
