// Unit tests of the game modules (Stage H2): test_game_modules. No ROM and no stored recording: every expected value is worked out here from the rules
// written in the headers (docs F10, F11, F14, F15, F16), with made-up game states. The game's tables are part of the program.
#include <cstdio>
#include <cstring>

#include "../../src/game/chips.hpp"
#include "../../src/game/data/tables.hpp"
#include "../../src/game/logic/bottom.hpp"
#include "../../src/game/logic/flow.hpp"
#include "../../src/game/logic/ghosts.hpp"
#include "../../src/game/logic/maze.hpp"
#include "../../src/game/logic/objects.hpp"
#include "../../src/game/logic/players.hpp"
#include "../../src/game/logic/scoring.hpp"

using namespace game;

namespace {
int gChecks = 0, gFailures = 0;

void check(bool ok, const char* what) {
  ++gChecks;
  if (!ok) { ++gFailures; std::printf("FAIL  %s\n", what); }
}

class NullChip : public TiaChip {
 public:
  void write(uint8_t, uint8_t) override {}
};

int popcount16(const GameState& st, int player) {
  int n = 0;
  for (uint8_t b : st.player[player].maze) for (int i = 0; i < 8; ++i) n += (b >> i) & 1;
  return n;
}

// A score byte pair is two decimal digits each: 0x45 is forty-five.
int bcd(uint8_t b) { return (b >> 4) * 10 + (b & 15); }
int scoreOf(const GameState& st, int player) { return bcd(st.score[player]) + 100 * bcd(st.score[2 + player]) + 10000 * bcd(st.score[4 + player]); }

GameState fresh() {
  GameState st{};
  st.gameSelect = 6;
  return st;
}

void testScore() {
  GameState st = fresh();
  addScore(st, 4, 0);
  check(scoreOf(st, 0) == 1, "a wafer is 1 point");
  addScore(st, 5, 0);
  check(scoreOf(st, 0) == 6, "a power pill is 5 points");
  addScore(st, 6, 0);
  check(scoreOf(st, 0) == 106, "the bonus item is 100 points");
  st = fresh();
  const int ghosts[4] = {20, 40, 80, 160};
  int total = 0;
  for (int i = 0; i < 4; ++i) {
    addScore(st, static_cast<uint8_t>(i), 0);
    total += ghosts[i];
    check(scoreOf(st, 0) == total, "ghost points 20, 40, 80, 160");
  }
  st = fresh();
  st.score[0] = 0x99;
  addScore(st, 4, 0);
  check(st.score[0] == 0x00 && st.score[2] == 0x01, "the decimal carry goes from the low to the middle byte");
  st = fresh();
  st.score[0] = 0x99; st.score[2] = 0x99;
  addScore(st, 4, 0);
  check(st.score[0] == 0 && st.score[2] == 0 && st.score[4] == 0x01 && scoreOf(st, 0) == 10000, "the carry goes on to the high byte");
  st = fresh();
  st.playerMode = 0x80;   // player two's turn
  addScore(st, 5, 0);
  check(scoreOf(st, 1) == 5 && scoreOf(st, 0) == 0, "points go to the player in turn (player two)");
  st = fresh();
  st.score[1] = 0x37;
  addScore(st, 4, 0);
  check(st.score[1] == 0x37 && scoreOf(st, 0) == 1, "the other player's score is left alone");
}

void testLocate() {
  GameState st = fresh();
  st.objX[0] = 0x4C; st.objY[0] = 60;
  Location l = locateObject(st, 0);
  check(l.row == 6 && l.yOffset == 0 && l.xOffset == 0 && l.cellIndex == 19, "Pac-Man at his start place: row 6, cell 19");
  st.objY[0] = 61; st.objX[0] = 0x4D;
  l = locateObject(st, 0);
  check(l.row == 7 && l.yOffset == 0xF7 && l.xOffset == 1, "one pixel below and right: row 7, y offset -9, x offset 1");
  check(l.cellIndex == 19 + 40, "an odd row adds 40 to the cell index");
  st.objY[0] = 0;
  l = locateObject(st, 0);
  check(l.row == 0, "y = 0 is row 0");
  check(st.cellRow == l.row && st.cellIndex == l.cellIndex && st.yOffset == l.yOffset && st.xOffset == l.xOffset, "the result is also stored in the state");
}

void testMove() {
  GameState st = fresh();
  st.objX[0] = 80; st.objY[0] = 30; st.objState[0] = 0x00; st.speedAcc[0] = 0x9F;
  Move m = moveObject(st, 0);
  check(m.stepped && st.objX[0] == 81, "a carry of the speed accumulator moves one pixel right");
  st.objState[0] = 0x40; st.speedAcc[0] = 0x9F;
  moveObject(st, 0);
  check(st.objX[0] == 80, "direction 1 moves left");
  st.objState[0] = 0x80; st.speedAcc[0] = 0x9F;
  moveObject(st, 0);
  check(st.objY[0] == 31, "direction 2 moves down");
  st.objState[0] = 0xC0; st.speedAcc[0] = 0x9F;
  moveObject(st, 0);
  check(st.objY[0] == 30, "direction 3 moves up");
  st.objState[0] = 0x00; st.speedAcc[0] = 0x01;
  m = moveObject(st, 0);
  check(!m.stepped && st.objX[0] == 80, "no carry, no step");
  st.objX[0] = 3; st.objState[0] = 0x40; st.speedAcc[0] = 0x9F;
  moveObject(st, 0);
  check(st.objX[0] == 3, "no step to x = 2 (a step left from x = 3 is not made)");
  st.objX[0] = 0x96 - 1; st.objState[0] = 0x00; st.speedAcc[0] = 0x9F;
  moveObject(st, 0);
  check(st.objX[0] == 0x96 - 1, "no step to x = $96");
  st.objX[0] = 80; st.objY[0] = 0x47 - 1; st.objState[0] = 0x80; st.speedAcc[0] = 0x9F;
  moveObject(st, 0);
  check(st.objY[0] == 0x47 - 1, "no step to y = $47");
  // the step is made once in about 15 of 16 accumulator values: over 16 frames at seed 9 the object moves a fixed number of times
  st.objX[0] = 20; st.objY[0] = 30; st.objState[0] = 0x00; st.speedAcc[0] = 0x09;
  int steps = 0;
  for (int i = 0; i < 17; ++i) steps += moveObject(st, 0).stepped ? 1 : 0;
  check(steps > 0 && steps < 17 && st.objX[0] == 20 + steps, "Pac-Man at seed 9 steps on some frames, not on all");
}

void testDirectionAndWrap() {
  GameState st = fresh();
  for (uint8_t d = 0; d < 4; ++d) {
    st.objState[1] = 0x08; st.speedAcc[1] = 0;
    setDirection(st, 1, d);
    check((st.objState[1] >> 6) == d, "setDirection stores the direction in bits 7-6");
  }
  st.objState[2] = 0x0F; st.objX[2] = 70;
  wrapToBottom(st, 2);
  check((st.objState[2] & 0xF0) == 0x30 && (st.objState[2] & 0x0F) == 0x0F && st.objY[2] == 0x51, "wrapToBottom: state $30, low bits kept, y = $51");
  st.objState[3] = 0x05;
  wrapToTop(st, 3);
  check((st.objState[3] & 0xF0) == 0xF0 && (st.objState[3] & 0x0F) == 0x05 && st.objY[3] == 0x0B, "wrapToTop: state $F0, low bits kept, y = $0B");
}

void testTurns() {
  // A turn into the first row's wall is refused; a ghost may not turn straight back.
  GameState st = fresh();
  st.objX[1] = 0x4C; st.objY[1] = 30; st.objState[1] = 0x00;
  locateObject(st, 1);
  int allowed = 0;
  for (uint8_t d = 0; d < 4; ++d) {
    GameState copy = st;
    TryResult r = tryDirection(copy, 1, d);
    if (r.exit == TryExit::Taken) ++allowed;
    check(r.exit != TryExit::Taken || (copy.objState[1] >> 6) == d, "a taken turn sets the direction");
    if (d == 1) check(r.exit != TryExit::Taken, "a ghost moving right may not turn straight back (left)");
  }
  check(allowed >= 1, "a ghost on a cell of the maze can turn somewhere");
  // Pac-Man himself may turn back.
  GameState p = fresh();
  p.objX[0] = 0x4C; p.objY[0] = 60; p.objState[0] = 0x00;
  locateObject(p, 0);
  TryResult back = tryDirection(p, 0, 1);
  check(back.exit != TryExit::TurnBack, "Pac-Man is not stopped by the turn-back rule");
}

void testMazeAndWafers() {
  GameState st = fresh();
  newMaze(st);
  check(popcount16(st, 0) == 126, "a new map has 126 wafers");
  check((st.player[0].pillFlags & kPillsOnMap) == kPillsOnMap, "a new map has the four power pills");
  check(st.eventTimer == 0x3C && (st.gameFlags & 0x80), "a new map freezes the game for 60 steps");
  check(st.objX[0] == 0x4C && st.objY[0] == 0x3C && st.objState[0] == 0x80, "Pac-Man starts at x = $4C, y = $3C");
  for (int g = 1; g <= 4; ++g) check(st.objX[g] == 0x4C && st.objY[g] == 0x1E && st.objState[g] == 0, "each ghost starts in the pen");
  check(data::kMazeWalls.size() == 80, "the wall table has 80 bytes");
  // Pac-Man sweeps the whole maze: each wafer is eaten once, and the score is the number of wafers eaten.
  int eaten = 0;
  for (int pass = 0; pass < 2; ++pass) {
    int before = popcount16(st, 0);
    for (int y = 0; y <= 0x47; ++y) {
      for (int x = 3; x < 0x96; ++x) {
        st.objX[0] = static_cast<uint8_t>(x); st.objY[0] = static_cast<uint8_t>(y); st.objState[0] = 0x00;
        locateObject(st, 0);
        eatWafer(st);
      }
    }
    eaten += before - popcount16(st, 0);
    if (pass == 0) check(eaten > 0, "a sweep of the maze eats wafers");
  }
  check(scoreOf(st, 0) == eaten, "one point for every wafer eaten");
  int after = popcount16(st, 0);
  int again = 0;
  for (int x = 3; x < 0x96; ++x) {
    st.objX[0] = static_cast<uint8_t>(x); st.objY[0] = 60; locateObject(st, 0); eatWafer(st);
  }
  again = after - popcount16(st, 0);
  check(again == 0 && eaten + popcount16(st, 0) == 126, "a wafer is eaten only once; eaten plus left is 126");
}

void testNewMap() {
  GameState st = fresh();
  newMaze(st);
  st.player[0].lives = 3; st.player[0].level = 4;
  check(!newMapCheck(st), "with wafers left the map stays");
  std::memset(st.player[0].maze, 0, sizeof st.player[0].maze);
  st.player[0].pillFlags = 0;
  check(newMapCheck(st), "an empty map is refilled");
  check(st.player[0].lives == 4 && st.player[0].level == 5 && popcount16(st, 0) == 126, "one more life, the next level, a full map");
  std::memset(st.player[0].maze, 0, sizeof st.player[0].maze);
  st.player[0].pillFlags = 0; st.player[0].lives = 9; st.player[0].level = 0x0E;
  newMapCheck(st);
  check(st.player[0].lives == 9 && st.player[0].level == 0x0E, "lives stop at 9 and the level at $0E");
  GameState pills = fresh();
  newMaze(pills);
  std::memset(pills.player[0].maze, 0, sizeof pills.player[0].maze);
  check(!newMapCheck(pills), "power pills left also keep the map");
}

void testPlayers() {
  GameState st = fresh();
  st.score[3] = 0x12; st.player[0].lives = 2; st.player[1].lives = 1;
  clearScores(st);
  check(st.score[3] == 0 && st.player[0].lives == 0 && st.player[1].lives == 0, "clearScores clears the six score bytes and both lives");
  st = fresh();
  st.player[0].lives = 3; st.player[0].level = 5; st.player[1].lives = 2; st.player[1].level = 9;
  st.player[0].maze[3] = 0xAA; st.player[1].maze[3] = 0x55;
  swapPlayers(st);
  check(st.player[0].lives == 2 && st.player[0].level == 9 && st.player[0].maze[3] == 0x55 && st.player[1].lives == 3 && st.player[1].maze[3] == 0xAA,
        "swapPlayers exchanges the two blocks");
  const struct { uint8_t game, level; } levels[] = {{2, 4}, {3, 4}, {4, 6}, {6, 8}, {8, 10}, {9, 10}};
  for (auto& c : levels) {
    GameState g = fresh();
    g.gameSelect = c.game;
    newGameSetup(g);
    check(g.player[0].level == c.level && g.player[0].lives == 3, "the start level comes from the game number, with three lives");
  }
  GameState g = fresh();
  newGameSetup(g);
  check(g.player[0].scoreColour == 0x0A && g.player[1].scoreColour == 0x09, "the score colours of the two players");
}

void testSpeedSetup() {
  for (uint8_t level = 4; level <= 0x0E; ++level) {
    GameState st = fresh();
    st.player[0].level = level;
    NullChip chip;
    FrameInput in;
    BottomContext b{st, in, chip};
    frozenSpeedSetup(b);
    bool ok = true;
    for (int x = 1; x <= 4; ++x) {
      ok = ok && st.speedAcc[x] == ((x & 1) ? level - 1 : level) && st.ghostTimer[x - 1] == level - 1;
    }
    check(ok, "ghost speed seeds are L and L-1, the chase timers L-1");
  }
}

void testRandom() {
  // F14: from $FF the states are $FE, $FC, $F8, ... ; $FF is never reached again; after one lead-in step the sequence repeats every 127 steps.
  GameState st = fresh();
  st.rng = 0xFF;
  uint8_t value = rngStep(st);
  check(st.rng == 0xFE && value == 0x02, "first step from $FF: state $FE, value $02");
  value = rngStep(st);
  check(st.rng == 0xFC && value == 0x08, "second step: state $FC, value $08");
  st.rng = 0xFF;
  rngStep(st);
  const uint8_t start = st.rng;
  int steps = 0;
  bool zero = false, back = false;
  do {
    rngStep(st);
    ++steps;
    zero = zero || st.rng == 0;
    back = back || st.rng == 0xFF;
  } while (st.rng != start && steps < 1000);
  check(steps == 127, "after the lead-in step the random number repeats every 127 steps");
  check(!zero && !back, "the random number never reaches 0, and never returns to $FF");
  GameState a = fresh(), b = fresh();
  a.rng = b.rng = 0x5A;
  for (int i = 0; i < 50; ++i) { rngStep(a); rngStep(b); }
  check(a.rng == b.rng, "the random number depends only on its state");
}

void testStartUp() {
  GameState st;
  std::memset(&st, 0xAA, sizeof st);
  NullChip chip;
  startUp(st, chip);
  check(st.rng == 0xFF && st.bonusTimer == 12 && st.gameSelect == 6 && st.attractShift == 0xFF, "the start-up values");
  check(st.player[0].lives == 0 && st.gameFlags == 0, "no lives and no flags after the start-up");
  check(st.frameCounter == 0 && st.tick256 == 0, "counters start at 0");
}
}  // namespace

int main() {
  testScore();
  testLocate();
  testMove();
  testDirectionAndWrap();
  testTurns();
  testMazeAndWafers();
  testNewMap();
  testPlayers();
  testSpeedSetup();
  testRandom();
  testStartUp();
  std::printf("%d checks, %d failures\n", gChecks, gFailures);
  std::printf("result: %s\n", gFailures == 0 ? "OK" : "FAILED");
  return gFailures == 0 ? 0 : 1;
}
