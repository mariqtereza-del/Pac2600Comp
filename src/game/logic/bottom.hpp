// The bottom logic of the frame: the order in which the parts of the second half of a frame run.
// (Stage G4.4: moved out of routines.*. The functions are the F21 rewrites of the original routines named in their comments.)
#pragma once
#include <cstdint>

#include "../chips.hpp"
#include "../data/cartridge.hpp"
#include "../state/state.hpp"
#include "../inputs.hpp"

namespace game {

// F21.23  The bottom logic of the frame ($F4E7-$F7CE), after the picture is drawn: nothing on the title screen; otherwise the pause after a ghost was
// eaten (the pause counter only), the event timer (scare, freeze, death delay: F16, F17), the freeze set-up of the ghost speeds, the collisions (bonus
// item, power pill, ghost: F12), the joystick and Pac-Man's turns, his wafer, the new map, his move and stand-still rules (F10, F11), and at the end
// the sound routines (F19). Returns false if ghostUpdate's never-run path was met (it is not called from here, so this does not occur); kept for symmetry.
bool bottomLogic(GameState& st, const Cartridge& cart, const FrameInput& input, TiaChip& chip);

// What the parts of the bottom logic work on together. Each part is a function in another module of logic/; this file names the shared pieces.
struct BottomContext {
  GameState& st;
  const Cartridge& cart;
  const FrameInput& in;
  TiaChip& chip;
};

// L_F7BA / L_F7C3 of the original: the end of every frame of play. finishFrame sets the mirror bit of Pac-Man's picture and then calls playSounds,
// which silences the volumes and runs the sound routines.
void playSounds(BottomContext& b);
void finishFrame(BottomContext& b);

}  // namespace game
