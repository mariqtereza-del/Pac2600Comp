// stella_bot: a search-based player for the Stella 7.0 core. At each decision point it saves the emulator
// state, tries two-step joystick plans ahead with Stella's own save/load, and plays the best one: eat wafers
// (RAM $88-$97 bit count goes down; the map refilling means the level ended), eat ghosts, do not die.
// $D2 = $40 pauses the game for a death, a ghost eaten ($F2 bit 1 = power pill in effect) and the level end;
// $98 (lives, counting the one in play) drops 126 frames after a death; the game is over once it is 0 and
// Pac-Man is parked at $B1/$B6 = 76/95, when attract mode starts (Stage C7 findings, working labels until Phase F). While searching it learns the maze
// (which Pac-Man positions $B1/$B6 follow each other) and where each wafer bit is eaten; when no plan eats a
// wafer it heads along the learned maze for the nearest known remaining wafer, or else for an unvisited place.
// After the level ends it lets go of the joystick so the ghosts end the game (death and game over captures).
// It writes its moves as an input script for stella_script.exe; only a replay of that script counts as
// evidence (DECISIONS.md T-C7-1). It only reads the ROM and writes into its output folder.
//
// Stage E7: with a fifth argument "long" it keeps steering after the level ends (later levels, until game over) and goes for
// the bonus item: byte $E7 bit 1 = the item is on the board (found by comparing frames of the C7 route), it sits near (76,50),
// and eating it is seen as that bit going 1 -> 0 on a frame where the score ($CC/$CE, decimal digits) rises by 2 or more.
// Without the fifth argument the behaviour is exactly the Stage C7 one (its route file is reproduced).
//
// Usage: stella_bot <rom> <out_dir> <max_frames> <seed> [long]

#include <bit>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <fstream>
#include <iterator>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "StellaLIBRETRO.hxx"
#include "OSystem.hxx"
#include "Console.hxx"
#include "System.hxx"
#include "M6532.hxx"
#include "Event.hxx"

namespace {

constexpr int kResetAt = 1800, kResetFrames = 10;  // same moment as C3-C6
constexpr int kDecide = 8;                         // frames between decisions
constexpr int kStep1 = 16, kStep2 = 48;            // plan: first direction, then second direction
constexpr int kAfterLevel = 600;                   // frames still steered after the level end
constexpr int kAfterGameOver = 900;                // frames recorded after game over

const Event::Type kDir[4] = {Event::LeftJoystickUp, Event::LeftJoystickDown,
                             Event::LeftJoystickLeft, Event::LeftJoystickRight};
const char* const kDirName[4] = {"UP", "DOWN", "LEFT", "RIGHT"};

StellaLIBRETRO stella;

using Pos = std::pair<int, int>;
std::map<Pos, std::set<Pos>> maze;  // learned: positions seen on consecutive frames
std::map<int, Pos> waferAt;         // learned: wafer bit (0..127) -> position where it was eaten

const uInt8* ram() { return stella.osystem().console().system().m6532().getRAM(); }
int wafers() { int n = 0; for(int a = 0x88; a <= 0x97; ++a) n += std::popcount(static_cast<unsigned>(ram()[a - 0x80])); return n; }
bool waferLeft(int bit) { return (ram()[0x88 - 0x80 + bit / 8] >> (bit % 8)) & 1; }
int lives() { return ram()[0x98 - 0x80]; }
bool paused() { return (ram()[0xD2 - 0x80] & 0x40) != 0; }
bool pill() { return (ram()[0xF2 - 0x80] & 0x02) != 0; }
int bcd(int x) { return (x >> 4) * 10 + (x & 15); }
int score() { return bcd(ram()[0xCC - 0x80]) + 100 * bcd(ram()[0xCE - 0x80]); }
bool itemOn() { return (ram()[0xE7 - 0x80] & 0x02) != 0; }
bool g_long = false;
bool g_bonusNow = false;  // the last frame run by step() ate the bonus item
int g_bonus = 0;  // bonus items eaten so far (long mode)
Pos pos() { return {ram()[0xB1 - 0x80], ram()[0xB6 - 0x80]}; }

// One frame with the joystick held in dir (-1: released); learns maze steps and eaten wafer positions.
void step(int dir, bool reset = false)
{
  uInt8 before[16];
  std::memcpy(before, ram() + 0x08, 16);
  const Pos p0 = pos();
  stella.setInputEvent(Event::ConsoleReset, reset ? 1 : 0);
  for(int d = 0; d < 4; ++d) stella.setInputEvent(kDir[d], d == dir ? 1 : 0);
  const bool itemBefore = itemOn();
  const int scoreBefore = score();
  stella.runFrame();
  g_bonusNow = g_long && itemBefore && !itemOn() && score() - scoreBefore >= 2;
  if(paused()) return;
  const Pos p1 = pos();
  if(p1 != p0 && std::abs(p1.first - p0.first) + std::abs(p1.second - p0.second) <= 4) {
    maze[p0].insert(p1);
    maze[p1].insert(p0);
  }
  for(int i = 0; i < 16; ++i)
    for(int b = 0; b < 8; ++b)
      if((before[i] >> b & 1) && !(ram()[0x08 + i] >> b & 1)) waferAt.emplace(i * 8 + b, p1);
}

std::map<Pos, int> distances(Pos from)
{
  std::map<Pos, int> dist{{from, 0}};
  std::deque<Pos> todo{from};
  while(!todo.empty()) {
    const Pos p = todo.front(); todo.pop_front();
    for(const Pos& q : maze[p])
      if(dist.emplace(q, dist[p] + 1).second) todo.push_back(q);
  }
  return dist;
}

struct Outcome { int eaten; int ghosts; int deathAt; Pos end; int bonus = 0; };

// Plays dir for the given frames. A pause with the power pill in effect is a ghost eaten (play goes on);
// any other pause is a death, unless the wafer map refilled (level end, counted as a huge meal).
Outcome simulate(int dir, int frames, int startWafers, int ghosts = 0, int bonus = 0)
{
  bool wasPaused = paused();
  for(int f = 0; f < frames; ++f) {
    const int w = wafers();
    step(dir);
    if(g_bonusNow) ++bonus;
    if(wafers() > w) return {1000, ghosts, -1, pos(), bonus};
    if(paused() && !wasPaused) {
      if(pill()) ++ghosts;
      else return {startWafers - wafers(), ghosts, f, pos(), bonus};
    }
    wasPaused = paused();
  }
  return {startWafers - wafers(), ghosts, -1, pos(), bonus};
}

}  // namespace

// Stella's FSNodeLIBRETRO reads the ROM through this function; same definition as in libretro.cxx.
uInt32 libretro_read_rom(void* data)
{
  std::memcpy(data, stella.getROM(), stella.getROMSize());
  return stella.getROMSize();
}

int main(int argc, char* argv[])
{
  if(argc < 5) { std::fprintf(stderr, "usage: stella_bot <rom> <out_dir> <max_frames> <seed>\n"); return 2; }
  const std::string romPath = argv[1], outDir = argv[2];
  const int maxFrames = std::stoi(argv[3]);
  uint64_t lcg = std::stoull(argv[4]);
  g_long = argc > 5 && std::string(argv[5]) == "long";
  std::vector<int> levelEnds, bonusAt;
  auto rnd = [&]() { lcg = lcg * 6364136223846793005ULL + 1442695040888963407ULL; return static_cast<double>(lcg >> 11) / 9007199254740992.0; };

  std::ifstream romFile(romPath, std::ios::binary);
  const std::vector<char> rom((std::istreambuf_iterator<char>(romFile)), std::istreambuf_iterator<char>());
  if(rom.empty()) { std::fprintf(stderr, "cannot read ROM %s\n", romPath.c_str()); return 3; }
  stella.setROM(romPath.c_str(), rom.data(), rom.size());
  if(!stella.create(false)) { std::fprintf(stderr, "Stella could not create the console\n"); return 4; }

  std::vector<uInt8> here(stella.getStateSize()), mid(stella.getStateSize());
  std::vector<int> moves;  // the direction held on every frame from kResetAt + kResetFrames (-1: released)
  std::set<Pos> visited;
  std::ofstream log(outDir + "/bot_log.csv");
  log << "frame,dir,wafers,lives,paused,x,y,target_x,target_y,best_value\n";

  int frame = 0;
  for(; frame < kResetAt + kResetFrames; ++frame) step(-1, frame >= kResetAt);

  int dir = 2, levelEnd = -1, gameOver = -1;  // start towards the left (C6: up/down are blocked at the start)
  while(frame < maxFrames) {
    if(gameOver < 0 && lives() == 0 && pos() == Pos{76, 95}) gameOver = frame;
    if(gameOver >= 0 && frame >= gameOver + kAfterGameOver) break;
    const bool steering = gameOver < 0 && (g_long || levelEnd < 0 || frame < levelEnd + kAfterLevel);

    double best = -1e18;
    Pos target{-1, -1};
    if(!steering) dir = -1;
    else if(!paused()) {
      const int w0 = wafers();
      Outcome out[4][4];
      stella.saveState(here.data(), here.size());
      for(int d1 = 0; d1 < 4; ++d1) {
        stella.loadState(here.data(), here.size());
        const Outcome o1 = simulate(d1, kStep1, w0);
        if(o1.deathAt < 0) stella.saveState(mid.data(), mid.size());
        for(int d2 = 0; d2 < 4; ++d2) {
          out[d1][d2] = o1;
          if(o1.deathAt >= 0 || o1.eaten >= 1000) continue;  // every d2 gives the same outcome
          stella.loadState(mid.data(), mid.size());
          out[d1][d2] = simulate(d2, kStep2, w0, o1.ghosts, o1.bonus);
          if(out[d1][d2].deathAt >= 0) out[d1][d2].deathAt += kStep1;
        }
      }
      stella.loadState(here.data(), here.size());

      // Where to head when no plan eats: nearest known remaining wafer, else nearest unvisited place.
      const std::map<Pos, int> fromHere = distances(pos());
      int bestDist = 1 << 30;
      for(const auto& [bit, p] : waferAt)
        if(waferLeft(bit) && fromHere.count(p) && fromHere.at(p) < bestDist) { bestDist = fromHere.at(p); target = p; }
      if(g_long && itemOn()) {
        int itemDist = 1 << 30;
        for(const auto& [p, d] : fromHere)
          if(p.second == 50 && p.first >= 72 && p.first <= 80 && d < itemDist) { itemDist = d; target = p; }
      }
      if(target.first < 0)
        for(const auto& [p, d] : fromHere)
          if(!visited.count(p) && d < bestDist) { bestDist = d; target = p; }
      const std::map<Pos, int> toTarget = target.first < 0 ? std::map<Pos, int>{} : distances(target);

      for(int d1 = 0; d1 < 4; ++d1)
        for(int d2 = 0; d2 < 4; ++d2) {
          const Outcome& o = out[d1][d2];
          double v;
          if(o.deathAt >= 0) v = -10000.0 + o.deathAt;
          else {
            const auto it = toTarget.find(o.end);
            v = 1000.0 * o.eaten + 3000.0 * o.ghosts + 6000.0 * o.bonus - (it == toTarget.end() ? 500.0 : it->second);
          }
          v += (d1 == dir ? 0.5 : 0.0) + 0.01 * rnd();  // keep going; deterministic tie-break
          if(v > best) { best = v; dir = d1; }
        }
    }

    log << frame << ',' << (dir < 0 ? "-" : kDirName[dir]) << ',' << wafers() << ',' << lives() << ',' << paused()
        << ',' << pos().first << ',' << pos().second << ',' << target.first << ',' << target.second << ',' << best << '\n';
    for(int f = 0; f < kDecide && frame < maxFrames; ++f, ++frame) {
      const int w = wafers();
      step(dir);
      if(wafers() > w) { levelEnds.push_back(frame); if(levelEnd < 0) levelEnd = frame; }
      if(g_bonusNow) bonusAt.push_back(frame);
      moves.push_back(dir);
      visited.insert(pos());
    }
  }

  // The moves as an input script for stella_script.exe (consecutive equal directions merged).
  std::ofstream script(outDir + "/route.txt");
  script << "# stella_bot route (seed " << argv[4] << "): level end at frame " << levelEnd << ", game over at frame "
         << gameOver << "\n";
  script << kResetAt << ' ' << (kResetAt + kResetFrames - 1) << " RESET\n";
  for(size_t i = 0; i < moves.size(); ) {
    size_t j = i;
    while(j + 1 < moves.size() && moves[j + 1] == moves[i]) ++j;
    if(moves[i] >= 0)
      script << (kResetAt + kResetFrames + i) << ' ' << (kResetAt + kResetFrames + j) << ' ' << kDirName[moves[i]] << '\n';
    i = j + 1;
  }
  std::ofstream wmap(outDir + "/wafer_positions.csv");
  wmap << "bit,x,y\n";
  for(const auto& [bit, p] : waferAt) wmap << bit << ',' << p.first << ',' << p.second << '\n';
  if(g_long) {
    std::ofstream lv(outDir + "/long_summary.txt");
    lv << "level ends at frames:"; for(int f : levelEnds) lv << ' ' << f;
    lv << "\nbonus items eaten at frames:"; for(int f : bonusAt) lv << ' ' << f;
    lv << "\nfinal score " << score() << "\n";
  }
  std::printf("done: %d frames, level end at %d, game over at %d, wafers left %d, lives %d, maze %zu places, "
              "%zu wafer positions learned; level ends %zu, bonus items eaten %zu, score %d\n", frame, levelEnd, gameOver, wafers(), lives(), maze.size(), waferAt.size(), levelEnds.size(), bonusAt.size(), score());
  stella.destroy();
  return 0;
}
