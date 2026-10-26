// The wafer map of the player in turn: a fresh maze (then the objects go to their start places).
// (Stage G4.2: moved out of routines.hpp. The functions are the F21 rewrites of the original routines named in their comments.)
#pragma once
#include <cstdint>

#include "../data/tables.hpp"
#include "../state/state.hpp"

namespace game {

// F21.5  NEW_MAZE ($FC0C): fills the wafer map of the player in turn (all 126 wafers and the 4 power pills; two places stay empty), sets the pill
// flags to $3C, and then does PLACE_OBJECTS (the original falls into it).
void newMaze(GameState& st);

// L_F666: Pac-Man eats the wafer under him (adds the score and starts the sound), if there is one.
void eatWafer(GameState& st);
// L_F6AA: when nothing is left on the map: one more life (at most 9), the next level (at most $0E), a fresh maze. True if the map was refilled.
bool newMapCheck(GameState& st);
// The wall table of the maze (one byte per cell: bit 7 side wall, bit 6 up allowed, bit 5 down allowed, F10) at cell index i.
inline uint8_t mazeWall(unsigned i) { return data::kMazeWalls[i & 0xFF]; }

}  // namespace game
