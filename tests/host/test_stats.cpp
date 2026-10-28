// Tests for src\host\stats (Stage I21): the totals found in the game's memory, the saved numbers and the hand-over between the threads.
// Usage: test_stats [<reference\baseline folder>]
//   With the folder of the local traces (reference\baseline) the totals are also worked out for recorded games: BL-001 (one game), BL-048 (two), BL-058 (a Game Reset in
//   the middle of a game: two games) and BL-046 (power pills and ghosts eaten).
#include <array>
#include <cstdio>
#include <fstream>
#include <string>

#include "../../src/host/settings.hpp"
#include "../../src/host/stats.hpp"
#include "../../src/host/trace.hpp"

static int failures = 0;
static int checks = 0;

static void expect(bool ok, const std::string& what)
{
  ++checks;
  if(!ok) { ++failures; std::printf("FAIL  %s\n", what.c_str()); }
}

using namespace stats;

// A memory image. $82 is the title-screen byte (0 = a game in play); the score as the game keeps it ($CC, $CE, $D0 for player 1; $CD, $CF, $D1 for player 2).
struct Memory {
  std::array<std::uint8_t, 128> ram{};
  std::uint8_t& at(int address) { return ram[static_cast<std::size_t>(address - 0x80)]; }
  void score(int v, int player = 0)
  {
    at(0xCC + player) = static_cast<std::uint8_t>(((v / 10 % 10) << 4) | (v % 10));
    at(0xCE + player) = static_cast<std::uint8_t>(((v / 1000 % 10) << 4) | (v / 100 % 10));
    at(0xD0 + player) = static_cast<std::uint8_t>(((v / 100000 % 10) << 4) | (v / 10000 % 10));
  }
};

static Totals runTrace(const std::string& path, bool& opened)
{
  std::ifstream in(path, std::ios::binary);
  opened = static_cast<bool>(in);
  Tracker tracker;
  std::string text;
  while(std::getline(in, text)) {
    trace::Row row;
    std::string error;
    if(text.empty() || text.rfind("PacMan2600-trace", 0) == 0 || text.rfind("rom_sha1=", 0) == 0 || text.rfind("columns=", 0) == 0) continue;
    if(!trace::parseLine(text, row, error)) break;
    tracker.update(row.ram.data());
  }
  return tracker.totals();
}

int main(int argc, char** argv)
{
  // The set: nine, each with an id, a label and an explanation; the ids differ.
  expect(kCount == 9, "nine totals");
  for(int i = 0; i < kCount; ++i) {
    expect(std::string(definition(i).label).size() > 3 && std::string(definition(i).explanation).size() > 10 && std::string(definition(i).id).size() > 2, "a label, an id and an explanation for " + std::string(definition(i).id));
    for(int j = 0; j < i; ++j) expect(std::string(definition(i).id) != definition(j).id && std::string(definition(i).label) != definition(j).label, "ids and labels differ");
  }

  // The text: thousands separators and times.
  expect(groupedText(0) == "0" && groupedText(999) == "999" && groupedText(1000) == "1,000" && groupedText(1234567) == "1,234,567" && groupedText(999999) == "999,999", "numbers with separators");
  expect(timeText(0) == "0 s" && timeText(59) == "0 s" && timeText(60) == "1 s", "a second is 59.9227 frames");
  expect(timeText(60 * 61) == "1 min 01 s", "minutes and seconds: " + timeText(60 * 61));
  expect(timeText(60 * 3725) == "1 h 02 min", "hours and minutes: " + timeText(60 * 3725));

  // Not a game: the power-on zeros (before the title screen has ever been seen), then the title screen.
  {
    Tracker t;
    Memory m;   // $82 = 0, all zero: the memory at power-on
    expect(!t.update(m.ram.data()) && t.totals() == Totals(), "the power-on zeros are not a game");
    m.at(0x82) = 0x5F;
    expect(!t.update(m.ram.data()) && t.totals() == Totals(), "the title screen counts nothing");
  }

  // A game: it starts when $82 falls to 0; frames, the score, the lives and the events are counted.
  {
    Tracker t;
    Memory m;
    m.at(0x82) = 0x5F;
    t.update(m.ram.data());
    m.at(0x82) = 0;
    m.at(0x98) = 3;
    t.update(m.ram.data());
    expect(t.totals().games == 1 && t.totals().playFrames == 1 && t.totals().mostLives == 3, "a game starts when the title screen goes");
    for(int i = 0; i < 9; ++i) t.update(m.ram.data());
    expect(t.totals().games == 1 && t.totals().playFrames == 10 && t.totals().longestGameFrames == 10, "ten frames of one game");
    int score = 0;
    for(int i = 0; i < 3; ++i) { score += 1; m.score(score); t.update(m.ram.data()); }
    expect(t.totals().wafers == 3 && t.totals().highScore == 3, "three wafers");
    score += 5; m.score(score); t.update(m.ram.data());
    expect(t.totals().powerPills == 1 && t.totals().wafers == 3 && t.totals().highScore == 8, "a power pill");
    for(int rise : {20, 40, 80, 160}) { score += rise; m.score(score); t.update(m.ram.data()); }
    expect(t.totals().ghostsEaten == 4 && t.totals().highScore == 308, "four ghosts");
    m.at(0xE7) = 0x40; t.update(m.ram.data()); t.update(m.ram.data());
    expect(t.totals().deaths == 1, "Pac-Man starting to die counts once, however long it takes");
    m.at(0xE7) = 0;
    m.at(0x98) = 7; t.update(m.ram.data());
    m.at(0x98) = 2; t.update(m.ram.data());
    expect(t.totals().mostLives == 7, "the most extra lives held at once");
    const std::uint64_t frames = t.totals().playFrames;
    m.at(0x82) = 0x5F; t.update(m.ram.data());   // game over: back to the title screen
    expect(t.totals().playFrames == frames && t.totals().games == 1 && t.totals().longestGameFrames == frames, "the title screen after the game adds nothing");
    m.at(0x82) = 0; m.score(0); m.at(0x98) = 3;
    t.update(m.ram.data());
    expect(t.totals().games == 2 && t.totals().highScore == 308 && t.totals().longestGameFrames == frames, "a second game; the high score and the longest game are kept");
  }

  // Game Reset in the middle of a game (BL-058): the score falls, a new game starts; a Reset before anything was eaten is not seen.
  {
    Tracker t;
    Memory m;
    m.at(0x82) = 0x5F; t.update(m.ram.data());
    m.at(0x82) = 0; t.update(m.ram.data());
    m.score(64); t.update(m.ram.data());
    m.score(0); t.update(m.ram.data());
    expect(t.totals().games == 2, "a score that falls in the middle of a game is a new game");
    t.update(m.ram.data());
    expect(t.totals().games == 2, "and it is counted once");
  }

  // Two players: the score of the one not to play is not mistaken for a fall; the better of the two is the high score.
  {
    Tracker t;
    Memory m;
    m.at(0x82) = 0x5F; t.update(m.ram.data());
    m.at(0x82) = 0; m.at(0xE6) = 0x40;
    m.score(500, 0); t.update(m.ram.data());
    m.at(0xE6) = 0xC0;   // player 2's turn: its own score, nothing eaten yet
    t.update(m.ram.data());
    m.score(30, 1); t.update(m.ram.data());
    expect(t.totals().games == 1, "a change of turn is not a new game");
    m.score(900, 1); t.update(m.ram.data());
    expect(t.totals().highScore == 900, "the better score of the two players is the high score");
  }

  // The saved numbers: one key each, the other lines kept, only changed lines written, read back; damage counts as 0.
  {
    const std::string file = "test_stats_scratch.ini";
    std::remove(file.c_str());
    settings::set(file, "volume", "80");
    expect(loadTotals(file) == Totals(), "nothing is counted in a new file");
    Totals a;
    a.games = 3; a.playFrames = 123456789012ull; a.highScore = 12345; a.wafers = 800; a.powerPills = 17; a.ghostsEaten = 9; a.deaths = 11; a.mostLives = 6; a.longestGameFrames = 40000;
    expect(saveTotals(file, a, Totals()), "the totals are saved");
    expect(loadTotals(file) == a, "and read back, a number as large as 123,456,789,012 as well");
    expect(settings::get(file, "volume") == "80" && settings::get(file, "stat_games") == "3" && settings::get(file, "stat_high_score") == "12345", "the other lines are kept, the key is stat_<id>");
    Totals b = a;
    b.deaths = 12;
    settings::set(file, "stat_games", "99");   // a line that the second save must not touch: it is equal in `b` and `a`
    expect(saveTotals(file, b, a) && settings::get(file, "stat_deaths") == "12" && settings::get(file, "stat_games") == "99", "only the lines that changed are written");
    settings::set(file, "stat_wafers", "lots");
    settings::set(file, "stat_deaths", "-4");
    const Totals damaged = loadTotals(file);
    expect(damaged.wafers == 0 && damaged.deaths == 0 && damaged.powerPills == 17, "a damaged line counts as 0 and the others are read");
    Tracker again(a);
    Memory m;
    m.at(0x82) = 0x5F; again.update(m.ram.data());
    m.at(0x82) = 0; again.update(m.ram.data());
    expect(again.totals().games == 4 && again.totals().playFrames == a.playFrames + 1, "a new run goes on from the saved totals");
    std::remove(file.c_str());
  }

  // The hand-over between the threads.
  {
    Watch w;
    Memory m;
    m.at(0x82) = 0x5F; w.gameFrame(m.ram.data());
    m.at(0x82) = 0; w.gameFrame(m.ram.data()); w.gameFrame(m.ram.data());
    expect(w.snapshot().games == 1 && w.snapshot().playFrames == 2, "the window thread sees what the game thread counted");
    Totals start;
    start.games = 5;
    Watch resumed(start);
    expect(resumed.snapshot().games == 5, "a watch starts from the saved totals");
  }

  // Recorded games, when the folder of traces is given.
  if(argc >= 2) {
    const std::string dir = argv[1];
    bool opened = false;
    const Totals one = runTrace(dir + "/BL-001.trace", opened);
    expect(opened && one.games == 1 && one.playFrames > 100, "BL-001 is one game");
    const Totals two = runTrace(dir + "/BL-048.trace", opened);
    expect(opened && two.games == 2 && two.deaths >= 1, "BL-048 is two games with deaths");
    const Totals reset = runTrace(dir + "/BL-058.trace", opened);
    expect(opened && reset.games == 2, "BL-058 (Game Reset in the middle of a game) is two games");
    const Totals eating = runTrace(dir + "/BL-046.trace", opened);
    expect(opened && eating.powerPills >= 5 && eating.ghostsEaten >= 8 && eating.wafers > 20, "BL-046 eats power pills and ghosts");
    std::printf("BL-001: %llu games, %llu frames; BL-048: %llu games, %llu deaths; BL-058: %llu games; BL-046: %llu wafers, %llu pills, %llu ghosts\n",
                static_cast<unsigned long long>(one.games), static_cast<unsigned long long>(one.playFrames), static_cast<unsigned long long>(two.games), static_cast<unsigned long long>(two.deaths),
                static_cast<unsigned long long>(reset.games), static_cast<unsigned long long>(eating.wafers), static_cast<unsigned long long>(eating.powerPills), static_cast<unsigned long long>(eating.ghostsEaten));
  }

  std::printf("%d checks, %d failures\n", checks, failures);
  if(failures == 0) std::printf("ALL TESTS PASSED\n");
  return failures == 0 ? 0 : 1;
}
