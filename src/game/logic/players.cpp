#include "players.hpp"
#include <utility>
#include "../state/cycles.hpp"
#include "maze.hpp"

namespace game {

// CLEAR_SCORES ($FC7D). The original loops X from 5 down to 0 storing 0 at SCORE_LO+X, i.e. the six score bytes $CC-$D1
// (player 1: low, player 2: low, player 1: middle, player 2: middle, player 1: high, player 2: high), then stores 0 in LIVES and LIVES_P2.
void clearScores(GameState& st) {
  for (uint8_t i = 0; i < 6; ++i) st.score[i] = 0;
  st.player[0].lives = 0;
  st.player[1].lives = 0;
}

// SWAP_PLAYERS ($FB9F). The original loops X from $13 down to 0, exchanging the 20 bytes $88-$9B with $9C-$AF: the two PlayerBlocks.
void swapPlayers(GameState& st) {
  cy(2 + 19 * 21 + 20 + 6);   // LDX, 19 passes taking the branch and the last one not, RTS
  std::swap(st.player[0], st.player[1]);
}

// NEW_GAME_SETUP ($FBF4).
void newGameSetup(GameState& st) {
  st.player[1].scoreColour = 0x09;
  st.player[0].scoreColour = 0x0A;
  st.player[0].lives = 3;
  st.player[0].level = static_cast<uint8_t>((((st.gameSelect - 2) & 6) + 4) & 0xFF);
  newMaze(st);
}

}  // namespace game
