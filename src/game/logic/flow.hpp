// How a game goes from one state to the next: the start-up, the start places, the event timer (scare, freeze, death delay) and the frozen start.
// (Stage G4.4: moved out of routines.*. The functions are the F21 rewrites of the original routines named in their comments.)
#pragma once
#include <cstdint>

#include "../chips.hpp"
#include "../data/cartridge.hpp"
#include "../state/state.hpp"
#include "bottom.hpp"

namespace game {

// F21.5  PLACE_OBJECTS ($FC21): freezes the game (GAME_FLAGS bit 7), sets the event timer to $3C (the freeze lasts 60 steps) and puts the four ghosts
// at x = $4C, y = $1E (state 0) and Pac-Man at x = $4C, y = $3C with the state $80.
void placeObjects(GameState& st);

// F21.25  The start-up ($F000-$F052), when the console is switched on: the whole page $00-$FF is cleared (the 128 chip registers, written twice over their
// mirrors, and the 128 memory bytes, the stack included); game number 6 (the power-on game), the bonus item's timer $0C, the title shift and the random
// number $FF; NEW_GAME_SETUP; the score shows $CB, $ED, $0F (the blank pattern of the title screen) and Pac-Man's y is $65; the four speed counters
// start at $0F;
// the bonus item's ball and missile are positioned (the chip writes HMM1, HMBL, RESBL, RESM1, HMOVE, HMCLR); the flags and the lives are 0.
// The code then goes on at the frame start ($F7D6).
void startUp(GameState& st, TiaChip& chip);

// Parts of the bottom logic (F21.23): the event timer step (true if the frame is over but for the sounds: game over) and the frozen set-up.
bool eventTimerStep(BottomContext& b);
void frozenSpeedSetup(BottomContext& b);

}  // namespace game
