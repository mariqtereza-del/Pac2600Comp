// What happens when Pac-Man touches something: the bonus item, a power pill, a ghost.
// (Stage G4.4: moved out of routines.*. The functions are the F21 rewrites of the original routines named in their comments.)
#pragma once
#include <cstdint>

#include "../chips.hpp"
#include "../data/cartridge.hpp"
#include "../state/state.hpp"
#include "bottom.hpp"

namespace game {

bool bonusOrPill(BottomContext& b);
bool ghostTouch(BottomContext& b);

}  // namespace game
