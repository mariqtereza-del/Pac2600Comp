// The status line under the maze: the life markers and the score digits.
// (Stage G4.3: moved out of routines.hpp. The functions are the F21 rewrites of the original routines named in their comments.)
#pragma once
#include <cstdint>

#include "../chips.hpp"
#include "../data/cartridge.hpp"
#include "../state/state.hpp"

namespace game {

// F21.11  LIVES_MARKERS ($FC68): the three playfield values for the row of life markers of one player, for `lives` markers, stored in the scratch bytes
// TEMP_D2+offset .. TEMP_D4+offset (offset 0 for player 1, 3 for player 2): $40 if there is any life (else 0), then the cartridge bytes at $FF1B+lives
// and $FEEC+lives. Returns the last value stored (the original leaves it in A).
uint8_t livesMarkers(GameState& st, const Cartridge& cart, uint8_t lives, uint8_t offset);

// F21.21  SCORE_DIGITS ($F3EE) with the CLEAR_GRAPHICS ($F483) it falls into: turns the decimal digits of one player's score into the
// picture pointers of the score line and draws the line. `offset` is the byte of the score's high part (4 for player 1, 5 for player 2; the score is
// SCORE_P1_LO + offset, - 2, - 4). Only five digits are shown (the hundred-thousands digit never): the low nibble of the high byte and both nibbles of the
// middle and low bytes; leading zeros are blank (shape number 10), a score of 0 shows one 0. The digit pointers go to $D5-$DE (low byte from the cartridge
// table at $FEF6, high byte $FE), TEMP_E1 counts the digits that are not zero, TEMP_E2 is the slot being filled. The background colour is set to black
// after the third digit and to TEMP_D2 (the score colour) after the fifth. Then seven picture rows (row 6 down to 0), each: the graphics registers cleared,
// then the five digit shapes (cartridge bytes at $FE00 + shape pointer + row) written to GRP0 / GRP1 in the six-picture order; finally the graphics registers
// are cleared again (CLEAR_GRAPHICS).
void scoreDigits(GameState& st, const Cartridge& cart, TiaChip& chip, uint8_t offset);

}  // namespace game
