// The start of the next frame: vertical blank and sync on, the pictures of Pac-Man and the ghost, the frame counter and the colours.
// (Stage G4.5: moved out of routines.*. The functions are the F21 rewrites of the original routines named in their comments.)
#pragma once
#include <cstdint>

#include "../chips.hpp"
#include "../data/cartridge.hpp"
#include "../inputs.hpp"
#include "../state/state.hpp"

namespace game {

// F21.24  The start of the next frame ($F7D6-$F87A), after the overscan wait: the vertical blank and the vertical sync are switched on and the collision flags
// cleared (chip writes); the picture of Pac-Man and of the ghost sprite chosen from the cartridge tables ($FDAB: four ghost pictures by FRAME_COUNTER bits 4-5;
// $FDC2: four Pac-Man pictures by bits 3-4, or $FDFA by the event timer while he dies, or the first one while frozen); FRAME_COUNTER goes up; every 256
// frames TICK256 goes up and, unless it wraps, the bonus item's timer is counted (F15); on the title screen ATTRACT_SHIFT goes up whenever FRAME_COUNTER wraps
// (never to 0); the picture set-up values; the colour mask from the colour / black-and-white switch and the title shift; three colour indices.
// The code then goes on at $F87C (where the vertical sync is switched off: the frame boundary).
void frameStart(GameState& st, const Cartridge& cart, const ConsoleInput& input, TiaChip& chip);

}  // namespace game
