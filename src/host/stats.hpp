// Statistics (Stage I21, D-026): the player's own totals across runs (games played, time played, high score, wafers, power pills, ghosts eaten, deaths, most extra lives,
// longest game), worked out on this computer from the game's memory after every frame, like the achievements. Nothing is sent anywhere and nothing is changed in the game.
// No window code, so it can be tested on its own.
#pragma once

#include <array>
#include <cstdint>
#include <mutex>
#include <string>

#include "pad_feedback.hpp"

namespace stats {

inline constexpr int kCount = 9;

// The totals. Frames are the game's own frames (59.9227 a second, T-D14-2).
struct Totals {
  std::uint64_t games = 0;               // games started
  std::uint64_t playFrames = 0;          // frames with a game in play
  std::uint64_t highScore = 0;           // the best six-digit score of either player
  std::uint64_t wafers = 0, powerPills = 0, ghostsEaten = 0, deaths = 0;
  std::uint64_t mostLives = 0;           // the most extra lives held at once
  std::uint64_t longestGameFrames = 0;   // the longest game, in frames (a game still going counts)
  bool operator==(const Totals& o) const
  {
    return games == o.games && playFrames == o.playFrames && highScore == o.highScore && wafers == o.wafers && powerPills == o.powerPills && ghostsEaten == o.ghostsEaten &&
           deaths == o.deaths && mostLives == o.mostLives && longestGameFrames == o.longestGameFrames;
  }
  bool operator!=(const Totals& o) const { return !(*this == o); }
  std::uint64_t value(int index) const;   // 0..8 in the order of the page
};

struct Definition {
  const char* id;            // the name of the setting, stat_<id> in settings.ini
  const char* label;         // the row on the menu page
  const char* explanation;
};
const Definition& definition(int index);

Totals loadTotals(const std::string& settingsFile);                                                      // from settings.ini; a missing or damaged line counts as 0
bool saveTotals(const std::string& settingsFile, const Totals& now, const Totals& saved);                // writes only the lines that differ from `saved`, the other lines kept
std::string rowText(int index, const Totals& totals);                                                    // "Games played:         12", as the menu page shows it
std::string timeText(std::uint64_t frames);                                                              // "1 h 05 min", "12 min 04 s", "9 s"
std::string groupedText(std::uint64_t number);                                                           // 1234567 -> "1,234,567"

// Finds the totals in the game's memory (128 bytes from $80), one call for each game frame the game runs. A game is started when `$82` (the title screen byte) falls to 0, or
// when either player's score falls while a game is in play (Game Reset pressed in the middle of a game, entry BL-058); the first frames after power-on, which read as
// zeros, are not a game. A Reset before anything was eaten cannot be told from a game that goes on, and is not counted.
class Tracker {
 public:
  explicit Tracker(const Totals& start = Totals()) : totals_(start) {}
  bool update(const std::uint8_t* ram);   // true when a total changed
  const Totals& totals() const { return totals_; }

 private:
  Totals totals_;
  feedback::EventWatcher events_;   // the wafers, power pills, ghosts eaten and deaths: the same finder as the rumble and the achievements
  bool seenTitle_ = false;          // the title screen has been seen since the program started (the power-on zeros are not a game)
  bool inPlay_ = false;
  std::uint64_t gameFrames_ = 0;    // frames of the game in play
  int score_[2] = {0, 0};
};

// The hand-over between the game thread, which sees every frame, and the window thread, which saves the numbers and shows them.
class Watch {
 public:
  explicit Watch(const Totals& start = Totals()) : tracker_(start), shared_(start) {}
  void gameFrame(const std::uint8_t* ram)   // game thread, after every frame
  {
    if(!tracker_.update(ram)) return;
    std::lock_guard<std::mutex> guard(lock_);
    shared_ = tracker_.totals();
  }
  Totals snapshot() const { std::lock_guard<std::mutex> guard(lock_); return shared_; }   // window thread

 private:
  Tracker tracker_;           // the game thread only
  mutable std::mutex lock_;
  Totals shared_;
};

}  // namespace stats
