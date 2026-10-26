#include "maze.hpp"
#include "../data/rom_tables.hpp"
#include "../state/cycles.hpp"
#include "../routines.hpp"

namespace game {

// NEW_MAZE ($FC0C). The 16 wafer bytes $88-$97 are set to $FF, except $8E = $7F and $8A = $FE (the two places that start empty, F11).
void newMaze(GameState& st) {
  cy(4 + 16 * 6 + 15 * 3 + 2 + 15);   // LDX, LDA, sixteen passes of the loop, the three pairs of loads and stores
  for (uint8_t i = 0; i < 16; ++i) st.player[0].maze[i] = 0xFF;
  st.player[0].maze[6] = 0x7F;
  st.player[0].maze[2] = 0xFE;
  st.player[0].pillFlags = 0x3C;
  placeObjects(st);
}

}  // namespace game
