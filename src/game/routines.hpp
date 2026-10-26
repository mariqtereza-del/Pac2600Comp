// The reconstructed game logic, routine by routine (Stage F21). Each function is the C++ form of one routine of the original (the F8 list)
// and works on the game state exactly as the original works on its memory (the fields of game::GameState, state.hpp). The lockstep tool
// (src/lockstep) proves each function against the original: every recorded game stays identical, and a direct comparison of the routine's
// results (memory and registers) is made at every call.
// Rules for this file: no ROM data and no emulator in here; tables that the original reads from the cartridge come later through the
// data loader (F22). Names of the routines are the original's F8 names in camelCase.
#pragma once
#include <cstdint>

#include "chips.hpp"
#include "data/cartridge.hpp"
#include "state/cycles.hpp"
#include "state/state.hpp"
#include "inputs.hpp"
#include "logic/bottom.hpp"
#include "logic/collisions.hpp"
#include "logic/flow.hpp"
#include "logic/ghosts.hpp"
#include "logic/pacman.hpp"
#include "logic/players.hpp"
#include "logic/maze.hpp"
#include "logic/objects.hpp"
#include "logic/animation.hpp"
#include "logic/scoring.hpp"
#include "picture/status_bar.hpp"
#include "sound/sound_logic.hpp"

namespace game {

// F21.22  The top logic of the frame ($F87E-$F9A7), after the vertical sync has been switched off: the score colour of player 1; a new game (fire on the
// title screen, or Reset) or a step of the game number (Select: at once, then every 32 frames while held); the difficulty switch of the player in turn
// into GAME_FLAGS bit 5; the sprite colours; the ghost of this frame (object (FRAME_COUNTER and 3) + 1) updated three times unless the game is
// frozen or paused; the picture set-up values SPRITE_X and SPRITE0_Y; the opening animations of that ghost and of Pac-Man; the six colour values made
// from the colour table of the cartridge ($FF2B, or $FF36 for black and white), the title-screen shift and the colour mask; COLUP1 and COLUP0.
// The code then goes on with the drawing code at $F07F. Returns false if ghostDecide met the never-run path.
bool topLogic(GameState& st, const Cartridge& cart, const ConsoleInput& input, TiaChip& chip);

// F21.24  The start of the next frame ($F7D6-$F87A), after the overscan wait: the vertical blank and the vertical sync are switched on and the collision flags
// cleared (chip writes); the picture of Pac-Man and of the ghost sprite chosen from the cartridge tables ($FDAB: four ghost pictures by FRAME_COUNTER bits 4-5;
// $FDC2: four Pac-Man pictures by bits 3-4, or $FDFA by the event timer while he dies, or the first one while frozen); FRAME_COUNTER goes up; every 256
// frames TICK256 goes up and, unless it wraps, the bonus item's timer is counted (F15); on the title screen ATTRACT_SHIFT goes up whenever FRAME_COUNTER wraps
// (never to 0); the picture set-up values; the colour mask from the colour / black-and-white switch and the title shift; three colour indices.
// The code then goes on at $F87C (where the vertical sync is switched off: the frame boundary).
void frameStart(GameState& st, const Cartridge& cart, const ConsoleInput& input, TiaChip& chip);

}  // namespace game
