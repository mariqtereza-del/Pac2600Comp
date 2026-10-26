// Pac-Man in the bottom logic: the stick, his turns and his move, and what he does when the stick is neutral.
// (Stage G4.4: moved out of routines.*. The functions are the F21 rewrites of the original routines named in their comments.)
#pragma once
#include <cstdint>

#include "../chips.hpp"
#include "../state/state.hpp"
#include "bottom.hpp"

namespace game {

// F21.20  PACMAN_KEEP_GOING ($F793): Pac-Man with the stick neutral. If he is not exactly on a cell nothing happens (NotOnCell: the routine returns and the
// main loop then moves him). Exactly on a cell he tries to go on in his own direction (TRY_* of F21.12): if that is allowed the direction is set again
// (Moved: the routine returns and the main loop moves him); if a wall is in the way (Stands) the routine drops its caller's return and the main loop
// goes on at $F7BA without moving him: he stands still.
enum class KeepGoing { NotOnCell, Moved, Stands };
KeepGoing pacmanKeepGoing(GameState& st);

// L_F61E (F21.23): Pac-Man's part of the frame; pacmanMove is the end it shares with the power pill.
void pacmanBottom(BottomContext& b);
void pacmanMove(BottomContext& b);

}  // namespace game
