// The top logic of the frame: the first half of a frame, from the end of the vertical sync to the start of the drawing.
// (Stage G4.5: moved out of routines.*. The functions are the F21 rewrites of the original routines named in their comments.)
#pragma once
#include <cstdint>

#include "../chips.hpp"
#include "../inputs.hpp"
#include "../state/state.hpp"

namespace game {

// F21.22  The top logic of the frame ($F87E-$F9A7), after the vertical sync has been switched off: the score colour of player 1; a new game (fire on the
// title screen, or Reset) or a step of the game number (Select: at once, then every 32 frames while held); the difficulty switch of the player in turn
// into GAME_FLAGS bit 5; the sprite colours; the ghost of this frame (object (FRAME_COUNTER and 3) + 1) updated three times unless the game is
// frozen or paused; the picture set-up values SPRITE_X and SPRITE0_Y; the opening animations of that ghost and of Pac-Man; the six colour values made
// from the colour table of the cartridge ($FF2B, or $FF36 for black and white), the title-screen shift and the colour mask; COLUP1 and COLUP0.
// The code then goes on with the drawing code at $F07F. Returns false if ghostDecide met the never-run path.
bool topLogic(GameState& st, const ConsoleInput& input, TiaChip& chip);

}  // namespace game
