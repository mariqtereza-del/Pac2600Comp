// Whose turn it is and what each player owns: clearing the scores, swapping the two players, starting a game for the player in turn.
// (Stage G4.2: moved out of routines.hpp. The functions are the F21 rewrites of the original routines named in their comments.)
#pragma once
#include <cstdint>

#include "../state/state.hpp"

namespace game {

// F21.1  CLEAR_SCORES ($FC7D): both scores to 0 and both lives bytes to 0.
void clearScores(GameState& st);

// F21.2  SWAP_PLAYERS ($FB9F): exchanges the 20-byte block of the player in turn ($88-$9B: wafer map, lives, level, pill flags, score colour)
// with the block of the other player ($9C-$AF).
void swapPlayers(GameState& st);

// F21.6  NEW_GAME_SETUP ($FBF4): the start of a game for the player in turn: score colours ($0A and $09), three lives, the start level from the game
// number ((GAME_SELECT - 2) and 6) + 4 (F16), then NEW_MAZE (the original falls into it).
void newGameSetup(GameState& st);

}  // namespace game
