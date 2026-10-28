// Achievements (Stage I20, D-025): the 14 achievements of the RetroAchievements set for Pac-Man on the Atari 2600 (retroachievements.org/game/13094), worked out on this
// computer from the game's memory after every frame. Nothing is sent anywhere and nothing is changed in the game. No window code, so it can be tested on its own.
#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <string>

#include "pad_feedback.hpp"

namespace achievements {

inline constexpr int kCount = 14;

struct Definition {
  const char* id;           // the name of the setting, achievement_<id> in settings.ini: stable, unlike the title
  const char* title;
  const char* description;
};
const Definition& definition(int index);   // 0..13 in the order of the set: ten scores, three lives, the four ghosts

// Which are unlocked, and on what day (YYYY-MM-DD; empty while locked).
struct Progress {
  std::uint32_t unlocked = 0;
  std::array<std::string, kCount> dates;
  bool has(int index) const { return (unlocked >> index & 1u) != 0; }
  int count() const;
};

Progress loadProgress(const std::string& settingsFile);                                   // from settings.ini
bool saveUnlock(const std::string& settingsFile, int index, const std::string& date);     // one key, the other lines kept
std::string todayText();                                                                    // YYYY-MM-DD, the PC's local date

// Finds the achievements in the game's memory (128 bytes from $80), one call for each game frame. Scores and lives are looked at only while a game is in play ($82 is 0);
// "four ghosts" counts the ghosts eaten since the last power pill (or death): a power pill eaten while ghosts are still scared starts the count again, as it starts the
// game's own 20, 40, 80, 160.
class Tracker {
 public:
  explicit Tracker(std::uint32_t alreadyUnlocked = 0) : unlocked_(alreadyUnlocked) {}
  std::uint32_t update(const std::uint8_t* ram);   // the bits unlocked by this frame (not those unlocked before)
  std::uint32_t unlocked() const { return unlocked_; }

 private:
  std::uint32_t unlocked_;
  int ghostChain_ = 0;
  feedback::EventWatcher events_;   // the power pills, ghosts eaten and deaths, found for the rumble as well
};

// The hand-over between the game thread, which sees every frame, and the window thread, which tells the player and writes the file.
class Watch {
 public:
  explicit Watch(std::uint32_t alreadyUnlocked = 0) : tracker_(alreadyUnlocked) {}
  void gameFrame(const std::uint8_t* ram) { if(const std::uint32_t fresh = tracker_.update(ram)) fresh_ |= fresh; }   // game thread, after every frame
  std::uint32_t takeNew() { return fresh_.exchange(0); }                                                              // window thread: the bits unlocked since the last call

 private:
  Tracker tracker_;
  std::atomic<std::uint32_t> fresh_{0};
};

}  // namespace achievements
