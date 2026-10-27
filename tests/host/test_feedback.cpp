// Tests for src\host\pad_feedback (Stage I15): the light bar choices and colours, the rumble pulses and the finding of the game's events in its memory.
// Usage: test_feedback [<trace file> <power pills> <ghosts eaten>]
//   With a trace (reference\baseline\BL-046.trace, local) the events found in the recorded game are counted and must be the number the baseline entry gives.
#include <array>
#include <cstdio>
#include <fstream>
#include <string>

#include "../../src/host/pad_feedback.hpp"
#include "../../src/host/trace.hpp"

static int failures = 0;
static int checks = 0;

static void expect(bool ok, const std::string& what)
{
  ++checks;
  if(!ok) { ++failures; std::printf("FAIL  %s\n", what.c_str()); }
}

using namespace feedback;

// A memory image of a game in play, with the score of player 1 set (four decimal digits in $CE and $CC).
struct Memory {
  std::array<std::uint8_t, 128> ram{};
  std::uint8_t& at(int address) { return ram[static_cast<std::size_t>(address - 0x80)]; }
  void score(int v)
  {
    at(0xCE) = static_cast<std::uint8_t>(((v / 1000 % 10) << 4) | (v / 100 % 10));
    at(0xCC) = static_cast<std::uint8_t>(((v / 10 % 10) << 4) | (v % 10));
  }
};

int main(int argc, char** argv)
{
  // The choices: setting names round trip, anything else is Off, the lists wrap round.
  for(int i = 0; i < kLights; ++i) {
    const Light l = static_cast<Light>(i);
    expect(lightFromSetting(lightSettingName(l)) == l, std::string("light setting round trip: ") + lightSettingName(l));
    expect(std::string(lightTitle(l)).size() >= 3, std::string("a title for ") + lightSettingName(l));
    expect(nextLight(l, 1) == static_cast<Light>((i + 1) % kLights) && nextLight(l, -1) == static_cast<Light>((i + kLights - 1) % kLights), "Left and Right wrap round the lights");
  }
  for(int i = 0; i < kRumbles; ++i) {
    const Rumble r = static_cast<Rumble>(i);
    expect(rumbleFromSetting(rumbleSettingName(r)) == r, std::string("rumble setting round trip: ") + rumbleSettingName(r));
    expect(nextRumble(r, 1) == static_cast<Rumble>((i + 1) % kRumbles) && nextRumble(r, -1) == static_cast<Rumble>((i + kRumbles - 1) % kRumbles), "Left and Right wrap round the rumbles");
  }
  expect(lightFromSetting("") == Light::Off && lightFromSetting("pink") == Light::Off && rumbleFromSetting("loud") == Rumble::Off, "unknown or empty settings mean Off");

  // The colours.
  const background::Rgb maze{10, 20, 200};
  expect(lightColour(Light::Yellow, maze, 3) == background::Rgb{255, 210, 0}, "yellow is yellow");
  expect(lightColour(Light::Maze, maze, 3) == maze, "the maze colour is the maze colour");
  expect(lightColour(Light::Lives, maze, 3) == background::Rgb{0, 200, 0} && lightColour(Light::Lives, maze, 9) == background::Rgb{0, 200, 0}, "three lives or more: green");
  expect(lightColour(Light::Lives, maze, 2) == background::Rgb{255, 200, 0}, "two lives: yellow");
  expect(lightColour(Light::Lives, maze, 1) == background::Rgb{255, 0, 0}, "one life: red");
  expect(lightColour(Light::Lives, maze, 0) == background::Rgb{40, 40, 40}, "none shown: dim white");
  expect(lightColour(Light::Off, maze, 3) == defaultLightColour(), "Off gives the pad's usual colour back");

  // The pulses.
  expect(pulseFor(PowerPill | GhostEaten | Death, Rumble::Off).ms == 0, "no rumble when it is off");
  expect(pulseFor(0, Rumble::Strong).ms == 0, "no event, no pulse");
  const Pulse tickL = pulseFor(PowerPill, Rumble::Light), tickS = pulseFor(PowerPill, Rumble::Strong);
  expect(tickL.ms > 0 && tickS.low > tickL.low && tickS.high > tickL.high, "strong is stronger than light");
  expect(pulseFor(Death, Rumble::Strong).ms > pulseFor(GhostEaten, Rumble::Strong).ms && pulseFor(GhostEaten, Rumble::Strong).ms > pulseFor(PowerPill, Rumble::Strong).ms,
         "a death rumbles longest, then a ghost, then a power pill");
  expect(pulseFor(PowerPill | Death, Rumble::Strong).ms == pulseFor(Death, Rumble::Strong).ms, "events together give the longest");
  expect(pulseFor(Death, Rumble::Strong).low == 0xC000, "the full strength of the big motor is 0xC000 for Strong");

  // The events, from the game's memory.
  {
    EventWatcher w;
    Memory m;
    m.score(0);
    expect(w.update(m.ram.data()) == 0, "the first look finds nothing (there is nothing to compare with)");
    m.score(1);
    expect(w.update(m.ram.data()) == 0, "a wafer (+1) is not an event");
    m.score(6);
    expect(w.update(m.ram.data()) == PowerPill, "+5 is a power pill");
    m.score(26);
    expect(w.update(m.ram.data()) == GhostEaten, "+20 is a ghost eaten");
    m.score(66);
    expect(w.update(m.ram.data()) == GhostEaten, "+40 is a ghost eaten");
    m.score(146);
    expect(w.update(m.ram.data()) == GhostEaten, "+80 is a ghost eaten");
    m.score(306);
    expect(w.update(m.ram.data()) == GhostEaten, "+160 is a ghost eaten");
    m.score(406);
    expect(w.update(m.ram.data()) == 0, "+100 (the bonus item) is not an event");
    m.score(407);
    w.update(m.ram.data());
    m.score(0);
    expect(w.update(m.ram.data()) == 0, "a score that falls (a new game) is not an event");
    m.score(5);
    expect(w.update(m.ram.data()) == PowerPill, "and it counts again from there");
    m.at(0xE7) = 0x40;
    expect(w.update(m.ram.data()) == Death, "Pac-Man starting to die ($E7 bit 6) is a death");
    expect(w.update(m.ram.data()) == 0, "once, not on every frame of the dying");
    m.at(0xE7) = 0x00;
    expect(w.update(m.ram.data()) == 0, "and not when it ends");
    m.at(0xE7) = 0x40;
    expect(w.update(m.ram.data()) == Death, "the next death is found too");
    m.at(0xE7) = 0x00;
    w.update(m.ram.data());
    m.at(0x82) = 0x5F;   // the title screen / game over: nothing there is an event
    m.score(500);
    m.at(0xE7) = 0x40;
    expect(w.update(m.ram.data()) == 0, "nothing is found on the title screen");
    m.at(0x82) = 0; m.at(0xE7) = 0;
    w.update(m.ram.data());
    m.at(0xE6) = 0xC0;   // two players, player 2's turn: the score looked at changes, which is not a rise
    m.at(0xCF) = 0x00; m.at(0xCD) = 0x70;
    expect(w.update(m.ram.data()) == 0, "a change of player is not an event");
  }
  {
    Hub hub;
    Memory m;
    m.at(0x98) = 3;
    m.score(0);
    hub.gameFrame(m.ram.data());
    m.score(5);
    hub.gameFrame(m.ram.data());
    m.score(25);
    hub.gameFrame(m.ram.data());
    expect(hub.lives() == 3, "the hub knows the lives");
    expect(hub.takeEvents() == (PowerPill | GhostEaten), "two events between two looks of the window are both kept");
    expect(hub.takeEvents() == 0, "and handed over once");
  }

  // A recorded game, when one is given: the events must be those the baseline entry counts by other means.
  if(argc >= 4) {
    std::ifstream in(argv[1], std::ios::binary);
    expect(static_cast<bool>(in), "the trace opens");
    std::string text;
    EventWatcher w;
    int pills = 0, ghosts = 0, deaths = 0, drops = 0, previousLives = -1;
    long long frames = 0;
    while(std::getline(in, text)) {
      trace::Row row;
      std::string error;
      if(text.rfind("PacMan2600-trace", 0) == 0 || text.rfind("rom_sha1=", 0) == 0 || text.rfind("columns=", 0) == 0 || text.empty()) continue;
      if(!trace::parseLine(text, row, error)) { expect(false, "a trace line parses: " + error); break; }
      const unsigned e = w.update(row.ram.data());
      pills += (e & PowerPill) != 0;
      ghosts += (e & GhostEaten) != 0;
      deaths += (e & Death) != 0;
      const bool inPlay = row.ram[0x82 - 0x80] == 0;
      const int lives = inPlay ? background::livesOf(row.ram.data()) : -1;
      if(previousLives >= 0 && lives >= 0 && lives < previousLives) ++drops;
      previousLives = lives;
      ++frames;
    }
    std::printf("trace: %lld frames, %d power pills, %d ghosts eaten, %d deaths, %d drops of the lives shown\n", frames, pills, ghosts, deaths, drops);
    expect(pills == std::atoi(argv[2]), "power pills in the recorded game: " + std::to_string(pills) + " (wanted " + argv[2] + ")");
    expect(ghosts == std::atoi(argv[3]), "ghosts eaten in the recorded game: " + std::to_string(ghosts) + " (wanted " + argv[3] + ")");
    expect(deaths >= drops, "every drop of the lives shown was announced by a death");
  }

  std::printf("%s (%d checks, %d failures)\n", failures == 0 ? "ALL TESTS PASSED" : "SOME TESTS FAILED", checks, failures);
  return failures == 0 ? 0 : 1;
}
