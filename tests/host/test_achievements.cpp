// Tests for src\host\achievements (Stage I20): the fourteen achievements found in the game's memory, the saved progress and the six-digit score.
// Usage: test_achievements [<reference\baseline folder>]
//   With the folder of the local traces (reference\baseline) the achievements are also looked for in recorded games: BL-056 (a long computer-played game: scores up to 4,094 and
//   7 lives), BL-046 (four ghosts on one power pill at frame 3496) and the two ARTIFICIAL games BL-065 and BL-066 (a score poked to 10,002 and 5,002).
#include <array>
#include <cstdio>
#include <fstream>
#include <string>

#include "../../src/host/achievements.hpp"
#include "../../src/host/settings.hpp"
#include "../../src/host/trace.hpp"

static int failures = 0;
static int checks = 0;

static void expect(bool ok, const std::string& what)
{
  ++checks;
  if(!ok) { ++failures; std::printf("FAIL  %s\n", what.c_str()); }
}

using namespace achievements;

// A memory image of a game in play. The score is written as the game keeps it: three bytes of two decimal digits each ($CC, $CE, $D0 for player 1).
struct Memory {
  std::array<std::uint8_t, 128> ram{};
  std::uint8_t& at(int address) { return ram[static_cast<std::size_t>(address - 0x80)]; }
  void score(int v)
  {
    at(0xCC) = static_cast<std::uint8_t>(((v / 10 % 10) << 4) | (v % 10));
    at(0xCE) = static_cast<std::uint8_t>(((v / 1000 % 10) << 4) | (v / 100 % 10));
    at(0xD0) = static_cast<std::uint8_t>(((v / 100000 % 10) << 4) | (v / 10000 % 10));
  }
};

static std::uint32_t bit(int i) { return 1u << i; }

// The achievements a recorded game unlocks, and the frame of each.
struct Run {
  std::uint32_t unlocked = 0;
  std::array<long long, kCount> frame{};
  bool opened = false;
  long long frames = 0;
};

static Run runTrace(const std::string& path)
{
  Run run;
  std::ifstream in(path, std::ios::binary);
  run.opened = static_cast<bool>(in);
  Tracker tracker;
  std::string text;
  run.frame.fill(-1);
  while(std::getline(in, text)) {
    trace::Row row;
    std::string error;
    if(text.empty() || text.rfind("PacMan2600-trace", 0) == 0 || text.rfind("rom_sha1=", 0) == 0 || text.rfind("columns=", 0) == 0) continue;
    if(!trace::parseLine(text, row, error)) break;
    const std::uint32_t fresh = tracker.update(row.ram.data());
    for(int i = 0; i < kCount; ++i)
      if(fresh >> i & 1u) run.frame[static_cast<std::size_t>(i)] = row.frame;
    ++run.frames;
  }
  run.unlocked = tracker.unlocked();
  return run;
}

int main(int argc, char** argv)
{
  // The set: fourteen, in order, each with a title and a description, and ids that differ.
  expect(kCount == 14, "fourteen achievements");
  for(int i = 0; i < kCount; ++i) {
    expect(std::string(definition(i).title).size() > 2 && std::string(definition(i).description).size() > 5, "a title and a description for " + std::string(definition(i).id));
    for(int j = 0; j < i; ++j) expect(std::string(definition(i).id) != definition(j).id && std::string(definition(i).title) != definition(j).title, "ids and titles differ");
  }
  expect(std::string(definition(0).title) == "A Little Snack" && std::string(definition(9).title) == "Pac-Man Fever" && std::string(definition(13).title) == "Blue Plate Special"
             && std::string(definition(12).description) == "Reach the maximum of 9 extra lives", "the titles and descriptions are those of the set");

  // Scores: each unlocks at its number, not before; the game's six digits; a blank pattern is 0.
  const int scores[10] = {500, 1000, 1500, 2000, 2500, 3000, 3500, 4000, 5000, 10000};
  for(int i = 0; i < 10; ++i) {
    Tracker t;
    Memory m;
    m.score(scores[i] - 1);
    expect((t.update(m.ram.data()) & bit(i)) == 0, "score " + std::to_string(scores[i] - 1) + " is not enough for " + definition(i).title);
    m.score(scores[i]);
    expect(t.update(m.ram.data()) == bit(i), std::string(definition(i).title) + " is reported at " + std::to_string(scores[i]) + ", and only it");
    expect((t.unlocked() & bit(i)) != 0, std::string(definition(i).title) + " is unlocked at " + std::to_string(scores[i]));
  }
  {
    Tracker t;
    Memory m;
    m.score(10000);
    expect(t.update(m.ram.data()) == 0x3FFu, "a score of 10,000 unlocks all ten score achievements at once (the lives and the ghosts are not touched)");
    expect(t.update(m.ram.data()) == 0, "and they are reported once");
    Memory blank;
    blank.at(0xCC) = 0xCB; blank.at(0xCE) = 0xED; blank.at(0xD0) = 0x0F;   // the game's blank patterns before the first game
    Tracker u;
    expect(u.update(blank.ram.data()) == 0, "the blank pattern of the score is 0");
  }
  expect(background::fullScoreOf(Memory{}.ram.data(), 0) == 0, "a score of 0");
  {
    Memory m;
    m.score(123456);
    expect(background::fullScoreOf(m.ram.data(), 0) == 123456, "the six digits");
    m.at(0xE6) = 0xC0;   // two players, player 2's turn: the score of player 2 ($CD, $CF, $D1), which is 0 here
    expect(background::fullScoreOf(m.ram.data(), 1) == 0, "player 2 has a score of his own");
    m.at(0xCD) = 0x50; m.at(0xCF) = 0x07; m.at(0xD1) = 0x02;
    expect(background::fullScoreOf(m.ram.data(), 1) == 20750, "player 2's six digits");
    Tracker t;
    expect((t.update(m.ram.data()) & bit(9)) != 0, "the score that counts is the one of the player to play (player 2 has 20,750)");
  }
  {
    Tracker t;
    Memory m;
    m.score(20000);
    m.at(0x82) = 0x5F;   // the title screen (the game is over): the score still shown does not count
    expect(t.update(m.ram.data()) == 0, "nothing is unlocked outside a game in play");
  }

  // Lives.
  for(int lives = 0; lives <= 9; ++lives) {
    Tracker t;
    Memory m;
    m.at(0x98) = static_cast<std::uint8_t>(lives);
    const std::uint32_t fresh = t.update(m.ram.data());
    const std::uint32_t want = (lives >= 4 ? bit(10) : 0) | (lives >= 5 ? bit(11) : 0) | (lives >= 9 ? bit(12) : 0);
    expect(fresh == want, "lives " + std::to_string(lives) + " unlock the right ones");
  }

  // Blue Plate Special: four ghosts after one power pill (a rise of 20, 40, 80, 160).
  {
    auto play = [&](const int* rises, int n, bool pillBefore) {
      Tracker t;
      Memory m;
      int score = 0;
      m.score(score);
      t.update(m.ram.data());
      if(pillBefore) { score += 5; m.score(score); t.update(m.ram.data()); }
      std::uint32_t fresh = 0;
      for(int i = 0; i < n; ++i) {
        if(rises[i] < 0) { m.at(0xE7) = 0x40; t.update(m.ram.data()); m.at(0xE7) = 0; t.update(m.ram.data()); continue; }   // a death
        score += rises[i];
        m.score(score);
        fresh |= t.update(m.ram.data());
      }
      return (fresh & bit(13)) != 0;
    };
    const int chain[4] = {20, 40, 80, 160};
    expect(play(chain, 4, true), "four ghosts after a power pill: Blue Plate Special");
    expect(!play(chain, 3, true), "three ghosts are not enough");
    const int twoPills[6] = {20, 40, 5, 20, 40, 80};
    expect(!play(twoPills, 6, true), "a second power pill starts the count again (20, 40, pill, 20, 40, 80: only three since the last pill)");
    const int twoPillsFour[7] = {20, 40, 5, 20, 40, 80, 160};
    expect(play(twoPillsFour, 7, true), "four since the last power pill count, whatever was eaten before it");
    const int died[6] = {20, 40, -1, 80, 160, 20};
    expect(!play(died, 6, true), "a death starts the count again");
    const int waferBetween[5] = {20, 1, 40, 80, 160};
    expect(play(waferBetween, 5, true), "wafers eaten in between do not matter");
  }

  // The saved progress: one key each in settings.ini, the other lines kept, read back.
  {
    const std::string file = "test_achievements_scratch.ini";
    std::remove(file.c_str());
    settings::set(file, "volume", "80");
    expect(loadProgress(file).count() == 0, "nothing is unlocked in a new file");
    expect(saveUnlock(file, 0, "2026-10-29") && saveUnlock(file, 13, "2026-11-01"), "two unlocks are saved");
    const Progress p = loadProgress(file);
    expect(p.count() == 2 && p.has(0) && p.has(13) && !p.has(1) && p.dates[0] == "2026-10-29" && p.dates[13] == "2026-11-01", "and read back with their days");
    expect(settings::get(file, "volume") == "80" && settings::get(file, "achievement_score_500") == "2026-10-29", "the other lines are kept, the key is achievement_<id>");
    Tracker already(p.unlocked);
    Memory m;
    m.score(600);
    expect(already.update(m.ram.data()) == 0, "an achievement unlocked in an earlier run is not reported again");
    std::remove(file.c_str());
    const std::string today = todayText();
    expect(today.size() == 10 && today[4] == '-' && today[7] == '-' && today.substr(0, 2) == "20", "today is a date: " + today);
  }
  {
    Watch w;
    Memory m;
    m.score(500);
    w.gameFrame(m.ram.data());
    m.score(1000);
    w.gameFrame(m.ram.data());
    expect(w.takeNew() == 0x3u, "the hand-over keeps what was unlocked between two looks of the window");
    expect(w.takeNew() == 0, "and gives it once");
  }

  // Recorded games, when the folder of traces is given.
  if(argc >= 2) {
    const std::string dir = argv[1];
    const Run big = runTrace(dir + "/BL-056.trace");
    expect(big.opened && big.frames > 30000, "the long recorded game BL-056 opens");
    std::uint32_t want = 0;
    for(int i = 0; i <= 7; ++i) want |= bit(i);   // 500 to 4,000: the game reaches 4,094
    want |= bit(10) | bit(11);                   // 4 and 5 extra lives (it reaches 7)
    std::printf("BL-056: unlocked 0x%x (wanted at least 0x%x), first score frames:", big.unlocked, want);
    for(int i = 0; i < 10; ++i) std::printf(" %lld", big.frame[static_cast<std::size_t>(i)]);
    std::printf("\n");
    expect((big.unlocked & want) == want, "BL-056 (scores to 4,094, 7 lives) unlocks the eight scores to 4,000 and the lives 4 and 5");
    expect((big.unlocked & (bit(8) | bit(9) | bit(12))) == 0, "and not 5,000, 10,000 or 9 lives, which it never reaches");
    bool inOrder = true;
    for(int i = 1; i < 8; ++i) inOrder = inOrder && big.frame[static_cast<std::size_t>(i)] >= big.frame[static_cast<std::size_t>(i - 1)];
    expect(inOrder, "the scores unlock in order");
    const Run eaten = runTrace(dir + "/BL-046.trace");
    std::printf("BL-046: Blue Plate Special at frame %lld\n", eaten.frame[13]);
    expect(eaten.opened && (eaten.unlocked & bit(13)) != 0 && eaten.frame[13] == 3496, "BL-046: four ghosts on one power pill at frame 3496 (+20 +40 +80 +160 of its entry)");
    expect((eaten.unlocked & 0x3FFu) == bit(0), "and of the scores only the first (it ends at 597: its entry says so)");
    const Run high = runTrace(dir + "/BL-065.trace");
    const Run mid = runTrace(dir + "/BL-066.trace");
    expect(high.opened && (high.unlocked & bit(8)) != 0 && (high.unlocked & bit(9)) != 0, "BL-065 (ARTIFICIAL, a score poked to 10,002): 5,000 and 10,000 are found");
    expect(mid.opened && (mid.unlocked & bit(8)) != 0 && (mid.unlocked & bit(9)) == 0, "BL-066 (ARTIFICIAL, a score poked to 5,002): 5,000 is found, 10,000 is not");
  }

  std::printf("%s (%d checks, %d failures)\n", failures == 0 ? "ALL TESTS PASSED" : "SOME TESTS FAILED", checks, failures);
  return failures == 0 ? 0 : 1;
}
