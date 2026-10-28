// What the window does about the statistics (Stage I21, D-026): loads the totals from settings.ini, takes the numbers the game thread found, saves them (at most every
// 10 seconds, and when the window closes), and hands them to the menu when it opens. A replayed recording is not the player's own play and is not counted.
#pragma once

#include <cstdint>
#include <string>

#include "stats.hpp"

namespace host {

class StatsDriver {
 public:
  StatsDriver(const std::string& settingsFile, bool counting);
  ~StatsDriver() { flush(); }
  void gameFrame(const std::uint8_t* ram) { if(counting_) watch_.gameFrame(ram); }   // game thread, after every frame
  void update(long long nowMs);                                                       // window thread, once each time round the loop: saves when 10 seconds have passed
  void flush();                                                                       // saves now if anything changed
  stats::Totals totals() const { return counting_ ? watch_.snapshot() : saved_; }     // for the menu

 private:
  std::string file_;
  bool counting_;
  stats::Totals saved_;         // what settings.ini holds now
  stats::Watch watch_;
  long long lastSaveMs_ = 0;
};

}  // namespace host
