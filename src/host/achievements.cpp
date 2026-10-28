#include "achievements.hpp"

#include <ctime>

#include "settings.hpp"

namespace achievements {

namespace {
const Definition kDefinitions[kCount] = {
    {"score_500", "A Little Snack", "Score 500 points"},
    {"score_1000", "What's for Breakfast?", "Score 1,000 points"},
    {"score_1500", "We Have Waffles!", "Score 1,500 points"},
    {"score_2000", "Out to Lunch", "Score 2,000 points"},
    {"score_2500", "Dinner Is Served", "Score 2,500 points"},
    {"score_3000", "Supper-Time", "Score 3,000 points"},
    {"score_3500", "Midnight Snack", "Score 3,500 points"},
    {"score_4000", "Big Eater", "Score 4,000 points"},
    {"score_5000", "And Finally Monsieur, a Wafer Thin Mint", "Score 5,000 points"},
    {"score_10000", "Pac-Man Fever", "Score 10,000 points"},
    {"lives_4", "4 Pac", "Have 4 extra lives"},
    {"lives_5", "Hi-Five", "Have 5 extra lives"},
    {"lives_9", "Just Like A Cat", "Reach the maximum of 9 extra lives"},
    {"four_ghosts", "Blue Plate Special", "Eat 4 ghosts in succession with only 1 power pill"},
};
constexpr int kScores[10] = {500, 1000, 1500, 2000, 2500, 3000, 3500, 4000, 5000, 10000};
}  // namespace

const Definition& definition(int index) { return kDefinitions[index < 0 ? 0 : index >= kCount ? kCount - 1 : index]; }

int Progress::count() const
{
  int n = 0;
  for(int i = 0; i < kCount; ++i) n += has(i) ? 1 : 0;
  return n;
}

static std::string keyOf(int index) { return std::string("achievement_") + definition(index).id; }

Progress loadProgress(const std::string& settingsFile)
{
  Progress p;
  for(int i = 0; i < kCount; ++i) {
    const std::string value = settings::get(settingsFile, keyOf(i));
    if(value.empty()) continue;
    p.unlocked |= 1u << i;
    p.dates[static_cast<std::size_t>(i)] = value;
  }
  return p;
}

bool saveUnlock(const std::string& settingsFile, int index, const std::string& date) { return settings::set(settingsFile, keyOf(index), date.empty() ? "unlocked" : date); }

std::string todayText()
{
  const std::time_t now = std::time(nullptr);
  std::tm local{};
  localtime_s(&local, &now);
  char text[16];
  std::strftime(text, sizeof text, "%Y-%m-%d", &local);
  return text;
}

// The score of the six digits, as the game keeps them: $CC / $CE / $D0 for player 1 and $CD / $CF / $D1 for player 2 (background::fullScoreOf).
std::uint32_t Tracker::update(const std::uint8_t* ram)
{
  const unsigned events = events_.update(ram);
  std::uint32_t now = 0;
  if(ram[0x82 - 0x80] == 0) {   // a game is in play (the title screen and the attract mode are not)
    const int score = background::fullScoreOf(ram, background::playerToPlay(ram));
    for(int i = 0; i < 10; ++i)
      if(score >= kScores[i]) now |= 1u << i;
    const int lives = background::livesOf(ram);
    if(lives >= 4) now |= 1u << 10;
    if(lives >= 5) now |= 1u << 11;
    if(lives >= 9) now |= 1u << 12;
  }
  if(events & (feedback::PowerPill | feedback::Death)) ghostChain_ = 0;
  if((events & feedback::GhostEaten) && ++ghostChain_ >= 4) now |= 1u << 13;
  const std::uint32_t fresh = now & ~unlocked_;
  unlocked_ |= now;
  return fresh;
}

}  // namespace achievements
