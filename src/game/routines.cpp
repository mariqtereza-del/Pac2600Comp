#include "routines.hpp"

namespace game {

// CLEAR_SCORES ($FC7D). The original loops X from 5 down to 0 storing 0 at SCORE_LO+X, i.e. the six score bytes $CC-$D1
// (player 1: low, player 2: low, player 1: middle, player 2: middle, player 1: high, player 2: high), then stores 0 in LIVES and LIVES_P2.
void clearScores(Ram& ram) {
  for (uint8_t i = 0; i < 6; ++i) at(ram, ram::SCORE_P1_LO + i) = 0;
  at(ram, ram::LIVES) = 0;
  at(ram, ram::LIVES_P2) = 0;
}

}  // namespace game
