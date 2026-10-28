#include "stats.hpp"

#include <algorithm>
#include <charconv>
#include <cstdio>

#include "settings.hpp"

namespace stats {

namespace {
const Definition kDefinitions[kCount] = {
    {"games", "Games played:", "How many games you have started. A game counts when it starts from the title screen, or when Game Reset is pressed in the middle of a game after something was eaten."},
    {"play_frames", "Time played:", "How long a game was in play, from the first moment of a game to its end. The time the menu is open is not counted."},
    {"high_score", "High score:", "The best score you have reached in any game, with the game's six digits. In a two-player game the better of the two scores counts."},
    {"wafers", "Wafers eaten:", "All the wafers Pac-Man has eaten."},
    {"power_pills", "Power pills eaten:", "All the power pills Pac-Man has eaten."},
    {"ghosts_eaten", "Ghosts eaten:", "All the ghosts Pac-Man has eaten after a power pill."},
    {"deaths", "Times caught:", "How often a ghost has caught Pac-Man."},
    {"most_lives", "Most extra lives:", "The most extra lives you have had at one time (the game allows 9)."},
    {"longest_game_frames", "Longest game:", "The longest time one game lasted."},
};
constexpr double kFramesPerSecond = 59.9227;   // the console's own speed (T-D14-2)

std::string keyOf(int index) { return std::string("stat_") + definition(index).id; }

std::uint64_t parse(const std::string& text)
{
  std::uint64_t value = 0;
  const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
  return result.ec == std::errc() && result.ptr == text.data() + text.size() ? value : 0;
}

constexpr std::uint64_t Totals::* kFields[kCount] = {&Totals::games,       &Totals::playFrames, &Totals::highScore, &Totals::wafers,           &Totals::powerPills,
                                                      &Totals::ghostsEaten, &Totals::deaths,     &Totals::mostLives, &Totals::longestGameFrames};
}  // namespace

const Definition& definition(int index) { return kDefinitions[std::clamp(index, 0, kCount - 1)]; }

std::uint64_t Totals::value(int index) const { return this->*kFields[std::clamp(index, 0, kCount - 1)]; }

Totals loadTotals(const std::string& settingsFile)
{
  Totals t;
  for(int i = 0; i < kCount; ++i) t.*kFields[i] = parse(settings::get(settingsFile, keyOf(i)));
  return t;
}

bool saveTotals(const std::string& settingsFile, const Totals& now, const Totals& saved)
{
  bool ok = true;
  for(int i = 0; i < kCount; ++i)
    if(now.value(i) != saved.value(i)) ok = settings::set(settingsFile, keyOf(i), std::to_string(now.value(i))) && ok;
  return ok;
}

std::string groupedText(std::uint64_t number)
{
  std::string digits = std::to_string(number), out;
  for(std::size_t i = 0; i < digits.size(); ++i) {
    if(i > 0 && (digits.size() - i) % 3 == 0) out += ',';
    out += digits[i];
  }
  return out;
}

std::string timeText(std::uint64_t frames)
{
  const std::uint64_t seconds = static_cast<std::uint64_t>(static_cast<double>(frames) / kFramesPerSecond);
  char text[48];
  if(seconds >= 3600) std::snprintf(text, sizeof text, "%llu h %02llu min", static_cast<unsigned long long>(seconds / 3600), static_cast<unsigned long long>(seconds / 60 % 60));
  else if(seconds >= 60) std::snprintf(text, sizeof text, "%llu min %02llu s", static_cast<unsigned long long>(seconds / 60), static_cast<unsigned long long>(seconds % 60));
  else std::snprintf(text, sizeof text, "%llu s", static_cast<unsigned long long>(seconds));
  return text;
}

std::string rowText(int index, const Totals& totals)
{
  const bool isTime = index == 1 || index == 8;
  const std::string value = isTime ? timeText(totals.value(index)) : groupedText(totals.value(index));
  char text[96];
  std::snprintf(text, sizeof text, "%-19s %s", definition(index).label, value.c_str());
  return text;
}

bool Tracker::update(const std::uint8_t* ram)
{
  const unsigned events = events_.update(ram);   // always run, so its memory of the last frame stays current
  const Totals before = totals_;
  const bool title = ram[0x82 - 0x80] != 0;   // the title screen and the attract mode: no game in play
  if(title) seenTitle_ = true;
  const bool play = !title && seenTitle_;
  if(play) {
    const int s0 = background::fullScoreOf(ram, 0), s1 = background::fullScoreOf(ram, 1);
    if(!inPlay_ || s0 < score_[0] || s1 < score_[1]) { ++totals_.games; gameFrames_ = 0; }   // a new game: from the title, or Game Reset in the middle of one
    score_[0] = s0;
    score_[1] = s1;
    ++gameFrames_;
    ++totals_.playFrames;
    totals_.longestGameFrames = std::max(totals_.longestGameFrames, gameFrames_);
    totals_.highScore = std::max<std::uint64_t>(totals_.highScore, static_cast<std::uint64_t>(std::max(s0, s1)));
    totals_.mostLives = std::max<std::uint64_t>(totals_.mostLives, static_cast<std::uint64_t>(std::max(0, background::livesOf(ram))));
    if(events & feedback::Wafer) ++totals_.wafers;
    if(events & feedback::PowerPill) ++totals_.powerPills;
    if(events & feedback::GhostEaten) ++totals_.ghostsEaten;
    if(events & feedback::Death) ++totals_.deaths;
  }
  inPlay_ = play;
  return totals_ != before;
}

}  // namespace stats
