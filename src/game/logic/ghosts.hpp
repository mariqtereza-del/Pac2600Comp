// The four ghosts: where they turn, their update each frame, and the game's one random number.
// (Stage G4.4: moved out of routines.*. The functions are the F21 rewrites of the original routines named in their comments.)
#pragma once
#include <cstdint>

#include "../chips.hpp"
#include "../data/cartridge.hpp"
#include "../state/state.hpp"

namespace game {

// F21.3  RNG_STEP ($FBE0): steps the shift register RNG_STATE (the game's only random number, F14) and returns the value the original leaves in A:
// one bit of the new state, picked by its low three bits (zero or not is all the callers ask).
uint8_t rngStep(GameState& st);

// F21.15  GHOST_DECIDE ($FA26): the decision of the ghost (object n, 1-4) that has just been located (LOCATE_OBJECT, F21.7). Only on a cell exactly (both
// offsets 0) and with no step due. On the title screen the ghost wanders. Eyes (state bit 3) aim at the pen door (x $58, y $1F). Otherwise: on the
// middle column on the top or bottom row, once in 16 times (the low four bits of the random number are 0) it enters the opening (WRAP_TO_TOP /
// WRAP_TO_BOTTOM); scared ghosts (PILL_FLAGS bit 7) aim at a mirrored x ($ not x + $60 inverted) and at Pac-Man's own y (an error of the original, F13);
// others count their timer down and aim at Pac-Man until it reaches 0, then reload it from the cartridge table at $FE6F (level / 2, +1 for objects 2
// and 3) and wander. Aiming: x first (right if the target x is bigger, left if smaller), then y (down if smaller, else up); the first allowed turn
// ends the decision. Wandering: a chain of random steps, see the code. Writes TEMP_D2 / TEMP_D3 (target y / x) as the original does.
// Returns false only for the path the listing marks as never run (a ghost that falls out of the last wander chain into TRY_PAIR_1's code).
bool ghostDecide(GameState& st, const Cartridge& cart, uint8_t object);

// F21.16  GHOST_UPDATE ($F9AA): everything for one ghost (object n, 1-4) in one frame; the top logic calls it three times in a row for the same ghost
// (F13). Eyes (state bit 3) on the pen row (y $1E): at the pen door (x $58) they turn left and move on; above the pen (x $4C) they wait while the ghosts
// are scared, otherwise they become a ghost again (bit 3 cleared), turn right and move on. A ghost in an opening animation (bit 5) does nothing here.
// Otherwise: locate it, let it decide, and move it unless the decision put it into an opening. Returns false only for the never-run path of ghostDecide.
bool ghostUpdate(GameState& st, const Cartridge& cart, uint8_t object);

}  // namespace game
