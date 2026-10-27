// What the program gives back to a game pad (Stage I15, D-021): the colour of the light bar (a DualShock 4 has one) and the rumble motors. Both are outputs only: they
// are worked out from the game's memory after each frame, never the other way round, so the game is not changed. No window code.
#pragma once

#include <atomic>
#include <cstdint>
#include <string>

#include "background.hpp"

namespace feedback {

// The light bar. Off leaves it alone (a DualShock 4 shows its own colour).
enum class Light { Off, Yellow, Maze, Lives };
inline constexpr int kLights = 4;
Light nextLight(Light light, int step);                  // goes round
const char* lightTitle(Light light);                     // as the menu shows it
const char* lightSettingName(Light light);               // as settings.ini keeps it: off | yellow | maze | lives
Light lightFromSetting(const std::string& text);         // anything else is Off
// The colour for a choice, given the colour of the maze and the lives left (Lives: green with 3 or more, yellow with 2, red with 1, dim white when none are shown).
background::Rgb lightColour(Light light, const background::Rgb& maze, int lives);
background::Rgb defaultLightColour();                    // what the light bar is set back to when the choice goes back to Off or the program ends (the DualShock's usual blue)

// The rumble motors.
enum class Rumble { Off, Light, Strong };
inline constexpr int kRumbles = 3;
Rumble nextRumble(Rumble rumble, int step);
const char* rumbleTitle(Rumble rumble);
const char* rumbleSettingName(Rumble rumble);            // off | light | strong
Rumble rumbleFromSetting(const std::string& text);       // anything else is Off

// Things the game does that are worth a pulse.
enum Event : unsigned { PowerPill = 1, GhostEaten = 2, Death = 4 };

struct Pulse {
  std::uint16_t low = 0, high = 0;   // the big and the small motor, 0..65535
  std::uint32_t ms = 0;
};
Pulse pulseFor(unsigned events, Rumble rumble);          // the strongest and longest of the events in the set; nothing at all when rumble is Off

// Finds the events by comparing the game's memory (128 bytes from $80) frame after frame: Pac-Man starting to die ($E7 bit 6), and the score of the player to play rising
// by 5 (a power pill) or by 20, 40, 80 or 160 (a ghost eaten); the other rises (a wafer 1, the bonus item 100) and a new game (the score falls) are not events.
class EventWatcher {
 public:
  unsigned update(const std::uint8_t* ram);

 private:
  bool havePrevious_ = false, dying_ = false;
  int score_ = 0, player_ = 0;
};

// The hand-over between the game thread, which sees every frame, and the window thread, which talks to the pad.
class Hub {
 public:
  void gameFrame(const std::uint8_t* ram);               // game thread, after every frame
  unsigned takeEvents() { return pending_.exchange(0); } // window thread: the events since the last call
  int lives() const { return lives_.load(); }

 private:
  EventWatcher watcher_;
  std::atomic<unsigned> pending_{0};
  std::atomic<int> lives_{0};
};

}  // namespace feedback
