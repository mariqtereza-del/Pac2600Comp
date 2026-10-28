// What the program gives back to a game pad (Stage I15, D-021): the colour of the light bar (a DualShock 4 has one; it is always on and follows the game) and the rumble
// motors (a choice in the menu, off by default). Both are outputs only: they are worked out from the game's memory after each frame, never the other way round, so the
// game is not changed. No window code.
#pragma once

#include <atomic>
#include <cstdint>
#include <string>

#include "background.hpp"

namespace feedback {

// The rumble motors.
enum class Rumble { Off, Light, Strong };
inline constexpr int kRumbles = 3;
Rumble nextRumble(Rumble rumble, int step);
const char* rumbleTitle(Rumble rumble);
const char* rumbleSettingName(Rumble rumble);            // off | light | strong
Rumble rumbleFromSetting(const std::string& text);       // anything else is Off

// Things the game does that are worth a pulse or a change of colour.
enum Event : unsigned { PowerPill = 1, GhostEaten = 2, Death = 4 };

struct Pulse {
  std::uint16_t low = 0, high = 0;   // the big and the small motor, 0..65535
  std::uint32_t ms = 0;
};
Pulse pulseFor(unsigned events, Rumble rumble);          // the strongest and longest of the events in the set; nothing at all when rumble is Off

// The colours of the light bar.
background::Rgb yellowLight();                           // Pac-Man yellow: the colour when nothing is going on
background::Rgb defaultLightColour();                    // what the light bar is set back to when the program ends (the DualShock's usual blue)

// The light bar's reactions, one step per game frame: yellow; while the ghosts are scared (after a power pill) blue, flashing white in the last second; a white flash
// when a ghost is eaten; red while Pac-Man is caught. The scared time is the game's own: 320 steps' worth of frames, 160 with the difficulty switch on A (docs\F16_LEVELS.md),
// then 64 frames of flashing.
class LightShow {
 public:
  background::Rgb step(unsigned events, bool difficultyA);

 private:
  int dying_ = 0, flash_ = 0, scared_ = 0;
};

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
  // game thread, after every frame; the two flags are the console's difficulty switches (A = true), the one of the player to play counts
  void gameFrame(const std::uint8_t* ram, bool leftDifficultyA, bool rightDifficultyA);
  unsigned takeEvents() { return pending_.exchange(0); }   // window thread: the events since the last call
  background::Rgb light() const;                           // the colour the light bar should have now

 private:
  EventWatcher watcher_;
  LightShow show_;
  std::atomic<unsigned> pending_{0};
  std::atomic<std::uint32_t> colour_{0x00FFD200};
};

}  // namespace feedback
