// What the window does about achievements (Stage I20, D-025): takes the unlocks the game thread found, writes them to settings.ini with the day, and shows a notice in the
// window for each. The menu reads `progress()` when it opens.
#pragma once

#include <deque>
#include <string>

#include "achievements.hpp"
#include "common.hpp"

namespace host {

class AchievementDriver {
 public:
  explicit AchievementDriver(const std::string& settingsFile);
  void gameFrame(const std::uint8_t* ram) { watch_.gameFrame(ram); }   // game thread, after every frame
  void update(long long nowMs);                                         // window thread, once each time round the loop: new unlocks are saved and queued for the notice
  void draw(SDL_Renderer* renderer, long long nowMs);                   // the notice, over the picture and the menu
  const achievements::Progress& progress() const { return progress_; }

 private:
  std::string file_;
  achievements::Progress progress_;
  achievements::Watch watch_;
  std::deque<int> queue_;     // unlocked, not shown yet
  int current_ = -1;          // the one on screen
  long long shownAt_ = 0;
};

// The notice itself: a box at the top of the window, "ACHIEVEMENT UNLOCKED" and the title (also used by the menu shot to show it).
void drawAchievementNotice(SDL_Renderer* renderer, const char* title);

}  // namespace host
