// The wafer map of the player in turn: a fresh maze (then the objects go to their start places).
// (Stage G4.2: moved out of routines.hpp. The functions are the F21 rewrites of the original routines named in their comments.)
#pragma once
#include <cstdint>

#include "../data/cartridge.hpp"
#include "../state/state.hpp"

namespace game {

// F21.5  NEW_MAZE ($FC0C): fills the wafer map of the player in turn (all 126 wafers and the 4 power pills; two places stay empty), sets the pill
// flags to $3C, and then does PLACE_OBJECTS (the original falls into it).
void newMaze(GameState& st);

}  // namespace game
