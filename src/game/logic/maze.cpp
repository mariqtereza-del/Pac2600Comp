// Reading tip: cy(n) counts processor cycles of the original (sound timing, state/cycles.hpp); the rules of the game are everything else.
#include "maze.hpp"
#include "../data/tables.hpp"
#include "../state/cycles.hpp"
#include "flow.hpp"
#include "scoring.hpp"
#include "../sound/sound_logic.hpp"

namespace game {

// NEW_MAZE ($FC0C). The 16 wafer bytes $88-$97 are set to $FF, except $8E = $7F and $8A = $FE (the two places that start empty, F11).
void newMaze(GameState& st) {
  cy(4 + 16 * 6 + 15 * 3 + 2 + 15);   // LDX, LDA, sixteen passes of the loop, the three pairs of loads and stores
  for (uint8_t i = 0; i < 16; ++i) st.player[0].maze[i] = 0xFF;
  st.player[0].maze[6] = 0x7F;
  st.player[0].maze[2] = 0xFE;
  st.player[0].pillFlags = kPillsOnMap;
  placeObjects(st);
}


// L_F666 (Pac-Man not in an opening): he eats the wafer under him, if there is one.
void eatWafer(GameState& st) {
  cy(3);
  if (st.yOffset != 0) { cy(3); return; }
  cy(2 + 3 + 4);
  uint8_t cell = st.cellIndex;
  uint8_t t = mazeWall(cell);
  cy(3 + 2);
  if (st.objX[0] == 0x4C) cy(3);
  else {
    cy(2 + 3);
    if (!(st.objState[0] & 0x80)) cy(3);
    else { cy(2 + 2 + 4); t |= mazeWall(static_cast<uint8_t>(cell + 1)); }
  }
  cy(2);   // LSR
  if (!(t & 1)) { cy(3); return; }
  cy(2);
  cy(2 + 2 + 3 + 2 + 2 + 4 + 2 + 2 + 2 + 2 + 4 + 2 + 4);   // LSR, TAX, LDA CELL_ROW, ROL, TAY, LDA, TAY, TXA, AND, TAX, LDA, TAX, AND
  const uint8_t index = t >> 2;
  const uint8_t byte = data::kWaferByteIndex[(st.cellRow << 1) | ((t >> 1) & 1)];
  const uint8_t mask = data::kBitMasksAndReloads[data::kWaferBitMaskAt + (index & 7)];
  uint8_t& maze = st.player[0].maze[byte];
  if (!(maze & mask)) { cy(3); return; }
  cy(2);
  cy(2 + 2 + 4 + 5 + 2 + 6);   // TXA, EOR, AND, STA, LDY #4, JSR ADD_SCORE
  maze &= static_cast<uint8_t>(~mask);
  addScore(st, 4, mask);                            // the original keeps the mask in X: TEMP_E1 gets it
  cy(2 + 6);
  soundStart(st, 0);
}

// L_F6AA: the new map when nothing is left; returns true if the map was refilled (the original then goes on at L_F7BA)
bool newMapCheck(GameState& st) {
  cy(2 + 3 + 2);
  uint8_t left = st.player[0].pillFlags & kPillsOnMap;
  for (uint8_t i = 0; i < 16; ++i) left |= st.player[0].maze[i];
  cy(16 * 6 + 15 * 3 + 2);   // sixteen passes: ORA, DEX, BPL (taken fifteen times)
  cy(2);   // TAX
  if (left != 0) { cy(3); return false; }
  cy(2);
  cy(3 + 2);
  // ORIGINAL: a cleared map gives one life (at most 9); no score ever does. Evidence: docs\F15_SCORING.md (F15). Do not fix.
  if (st.player[0].lives == 9) cy(3); else { cy(2 + 5); st.player[0].lives = static_cast<uint8_t>(st.player[0].lives + 1); }
  cy(3 + 2);
  if (st.player[0].level == 0x0E) cy(3); else { cy(2 + 5); st.player[0].level = static_cast<uint8_t>(st.player[0].level + 1); }
  cy(6);
  newMaze(st);
  cy(3);
  return true;
}

}  // namespace game
