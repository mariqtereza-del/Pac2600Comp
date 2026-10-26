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

namespace game {

// F21.1  CLEAR_SCORES ($FC7D): both scores to 0 and both lives bytes to 0.
void clearScores(GameState& st);

// F21.2  SWAP_PLAYERS ($FB9F): exchanges the 20-byte block of the player in turn ($88-$9B: wafer map, lives, level, pill flags, score colour)
// with the block of the other player ($9C-$AF).
void swapPlayers(GameState& st);

// F21.3  RNG_STEP ($FBE0): steps the shift register RNG_STATE (the game's only random number, F14) and returns the value the original leaves in A:
// one bit of the new state, picked by its low three bits (zero or not is all the callers ask).
uint8_t rngStep(GameState& st);

// F21.4  WRAP_TO_BOTTOM ($FD26) and WRAP_TO_TOP ($FD32), with their shared end WRAP_SET_Y ($FD3C): Pac-Man or a ghost (object 0-4) enters the opening
// in the bottom or top wall of the maze. The direction bits of its state are set (bottom: $30, top: $F0; the low four bits are kept) and its y
// is set to the start of the opening animation ($51 below the maze, $0B above it).
void wrapToBottom(GameState& st, uint8_t object);
void wrapToTop(GameState& st, uint8_t object);

// F21.5  PLACE_OBJECTS ($FC21): freezes the game (GAME_FLAGS bit 7), sets the event timer to $3C (the freeze lasts 60 steps) and puts the four ghosts
// at x = $4C, y = $1E (state 0) and Pac-Man at x = $4C, y = $3C with the state $80.
void placeObjects(GameState& st);
// F21.5  NEW_MAZE ($FC0C): fills the wafer map of the player in turn (all 126 wafers and the 4 power pills; two places stay empty), sets the pill
// flags to $3C, and then does PLACE_OBJECTS (the original falls into it).
void newMaze(GameState& st);

// F21.6  NEW_GAME_SETUP ($FBF4): the start of a game for the player in turn: score colours ($0A and $09), three lives, the start level from the game
// number ((GAME_SELECT - 2) and 6) + 4 (F16), then NEW_MAZE (the original falls into it).
void newGameSetup(GameState& st);

// F21.7  LOCATE_OBJECT ($FBAD): works out where object n (0 = Pac-Man, 1-4 = ghosts) stands in the maze grid (F10): the row, the distance from the
// row and cell column, and the cell index. Writes CELL_ROW, Y_OFFSET, X_OFFSET, CELL_INDEX and POS_CODE and returns them (the adapter needs them
// for the registers the original leaves).
struct Location {
  uint8_t row;        // CELL_ROW: y / 10 rounded up (0 when y = 0)
  uint8_t yOffset;    // Y_OFFSET: y - 10 * row (0 or negative, 8-bit)
  uint8_t xOffset;    // X_OFFSET: x and 3
  uint8_t cellIndex;  // CELL_INDEX: x / 4, plus 40 on odd rows
  uint8_t posCode;    // POS_CODE: (cell index << 2) or (bit 6 of the cell index << 7) or (row >> 1)
  uint8_t cellBit6;   // bit 6 of the cell index (the carry the original leaves)
};
Location locateObject(GameState& st, uint8_t object);

// F21.8  MOVE_OBJECT ($F9EE): the speed accumulator of object n (0 = Pac-Man, 1-4 = ghosts). It becomes (acc << 4) + acc (8 bits); a carry out is one
// step of one pixel in the direction in bits 7-6 of the state (0 right, 1 left, 2 down, 3 up), after clearing the "step due" bit 4 of the state.
// A step to x = 2 or x = $96, to a negative y or to y = $47 is not made (F10). The result describes what happened for the adapter.
struct Move {
  bool stepped;       // the accumulator carried
  bool vertical;      // the direction is down or up
  uint8_t oldAcc;     // the accumulator before
  uint8_t stepValue;  // 1 or $FF
  uint8_t oldCoord;   // x or y before the step
  uint8_t newCoord;   // x or y the step would give
  bool stored;        // the new coordinate was stored (not stopped by a border)
};
Move moveObject(GameState& st, uint8_t object);

// F21.9  SET_DIRECTION ($F9E5): stores direction code d (0 right, 1 left, 2 down, 3 up) in bits 7-6 of the state of object n and sets the "step due"
// bit 4 (the original takes both from the table TBL_FE63, whose four bytes are (d << 6) or $10), then falls into MOVE_OBJECT: the object moves at once.
// Returns what moveObject returned.
Move setDirection(GameState& st, uint8_t object, uint8_t direction);

// F21.10  ADD_SCORE ($FC45): adds the score amount number y (0-3: ghosts 20/40/80/160, 4: wafer 1, 5: power pill 5, 6: bonus item 100) to the score of the
// player in turn (PLAYER_MODE bit 7), in decimal: low byte + TBL_FF06[y], middle byte + TBL_FF0C[y] (cartridge tables), high byte + carry.
// The original keeps the caller's X in the scratch byte TEMP_E1 and gives it back; callerX is that X (the byte stays in the memory).
// Returns the last decimal addition (the high byte) for the adapter's flags.
struct Bcd {
  uint8_t value;
  bool carry;
  bool overflow;
};
Bcd addScore(GameState& st, const Cartridge& cart, uint8_t amount, uint8_t callerX);

// F21.11  LIVES_MARKERS ($FC68): the three playfield values for the row of life markers of one player, for `lives` markers, stored in the scratch bytes
// TEMP_D2+offset .. TEMP_D4+offset (offset 0 for player 1, 3 for player 2): $40 if there is any life (else 0), then the cartridge bytes at $FF1B+lives
// and $FEEC+lives. Returns the last value stored (the original leaves it in A).
uint8_t livesMarkers(GameState& st, const Cartridge& cart, uint8_t lives, uint8_t offset);

// F21.12  TRY_UP ($FB36), TRY_DOWN ($FB5C), TRY_LEFT ($FB70), TRY_RIGHT ($FB87) and their shared end TRY_COMMIT ($FB44): can object n turn into direction d
// (0 right, 1 left, 2 down, 3 up) from the cell it was located in (CELL_ROW, CELL_INDEX, F21.7)? The maze wall table of the cartridge (at $FE13, one
// byte per cell: bit 7 wall, bit 6 up allowed, bit 5 down allowed, F10) decides; the first and last row, and two cells of row 3 (the tunnel ends),
// are closed. A ghost (n > 0) may not turn straight back. If the turn is allowed the state is set (direction bits and the "step due" bit) and the
// ORIGINAL ROUTINE DROPS ITS CALLER'S RETURN (the caller is left, too: the caller's caller continues); otherwise it returns normally.
enum class TryExit { AtEdge, Wall, TurnBack, Taken };
struct TryResult {
  TryExit exit;
  uint8_t row, cell;
  uint8_t tableByte;     // the wall table byte that was read (not for AtEdge)
  uint8_t stateBits;     // bits 7-6 of the state before (for the turn-back test of ghosts)
};
TryResult tryDirection(GameState& st, const Cartridge& cart, uint8_t object, uint8_t direction);

// F21.13  TRY_PAIR_1 ($FAFC) with TRY_PAIR_UD ($FB05), and TRY_PAIR_2 ($FB19) with TRY_PAIR_LR ($FB22): after a random step, try two directions of one
// axis in an order the random number picks (vertical pair: up then down when the result is 0, else down then up; horizontal pair: left then right when
// 0, else right then left). The first turn that is allowed is taken (and, as in F21.12, ends the caller too); if neither is, the routine returns.
// `rngResultIsZero` is the Z flag the random step left (A = 0).
struct PairResult {
  uint8_t count;        // how many directions were tried (1 or 2)
  uint8_t dir[2];       // the directions, in the order tried
  TryResult res[2];     // their results
  bool taken;           // the last one tried was taken
};
PairResult tryPair(GameState& st, const Cartridge& cart, uint8_t object, bool vertical, bool rngResultIsZero);

// F21.14  ANIMATE_OBJECT ($FC8B): the animation of an object (n = 0 Pac-Man, 1-4 ghosts) whose state has bit 5 set: a ghost leaving the pen or going
// through an opening, Pac-Man going through an opening or dying (F10, F13). `sprite` is 0 for a ghost (player 0) and 1 for Pac-Man (player 1): it picks the
// sprite scratch bytes (SPRITE0_Y / SPRITE1_Y, SPRITE0_LINES / SPRITE1_LINES, the picture pointer) that the animation also sets. Depending on the
// "step due" bit 4 the object waits (its y counts down, then the animation starts) or moves along the opening path (direction bits 7-6 of the state:
// 0 down the outside to y $5B, 1 and 3 up the outside from the bottom or top, 2 back into the maze). The cartridge tables used: $FF0D (picture pointer
// steps), $FF19 (which pointer), $FF25 (start y of the next phase).
enum class AnimPath { Counting, CountDone, RightStep, RightEnd, DownStep, DownEnd, UpEnd, Reached0B, SpriteY, SpritePtr };
struct AnimResult {
  AnimPath path;
  uint8_t newY;        // the y after the step (before a table value replaces it)
  uint8_t stateAfter;  // the state after
  uint8_t tableByte;   // $FF0D byte added to the picture pointer (SpritePtr only)
  uint8_t ptrOld;      // the picture pointer byte before (SpritePtr only)
  uint8_t ptrIndex;    // which pointer (0 or 2: SPRITE_PTR + index) (SpritePtr only)
};
AnimResult animateObject(GameState& st, const Cartridge& cart, uint8_t object, uint8_t sprite);

// F21.15  GHOST_DECIDE ($FA26): the decision of the ghost (object n, 1-4) that has just been located (LOCATE_OBJECT, F21.7). Only on a cell exactly (both
// offsets 0) and with no step due. On the title screen the ghost wanders. Eyes (state bit 3) aim at the pen door (x $58, y $1F). Otherwise: on the
// middle column on the top or bottom row, once in 16 times (the low four bits of the random number are 0) it enters the opening (WRAP_TO_TOP /
// WRAP_TO_BOTTOM); scared ghosts (PILL_FLAGS bit 7) aim at a mirrored x ($ not x + $60 inverted) and at Pac-Man's own y (an error of the original, F13);
// others count their timer down and aim at Pac-Man until it reaches 0, then reload it from the cartridge table at $FE6F (level / 2, +1 for objects 2
// and 3) and wander. Aiming: x first (right if the target x is bigger, left if smaller), then y (down if smaller, else up); the first allowed turn
// ends the decision. Wandering: a chain of random steps, see the code. Writes TEMP_D2 / TEMP_D3 (target y / x) as the original does.
// Returns false only for the path the listing marks as never run (a ghost that falls out of the last wander chain into TRY_PAIR_1's code).
bool ghostDecide(GameState& st, const Cartridge& cart, uint8_t object);

// F21.16  GHOST_UPDATE ($F9AA): everything for one ghost (object n, 1-4) in one frame; the top logic calls it three times in a row for the same ghost
// (F13). Eyes (state bit 3) on the pen row (y $1E): at the pen door (x $58) they turn left and move on; above the pen (x $4C) they wait while the ghosts
// are scared, otherwise they become a ghost again (bit 3 cleared), turn right and move on. A ghost in an opening animation (bit 5) does nothing here.
// Otherwise: locate it, let it decide, and move it unless the decision put it into an opening. Returns false only for the never-run path of ghostDecide.
bool ghostUpdate(GameState& st, const Cartridge& cart, uint8_t object);

// F21.17  SOUND_START ($FF43): starts sound effect number y (0 wafer, 1 bonus item, 2 power pill, 3 ghost eaten, F19) unless one that outranks it is playing.
// The cartridge tables: $FFE9 (the sounds that block it, as bits of SOUND_ACTIVE), $FE6E (the bit it sets in SOUND_ACTIVE), $FFED (its length, into
// SOUND_TIMER). Returns the value the original leaves in A: the blocking bits if it was refused, else the length.
uint8_t soundStart(GameState& st, const Cartridge& cart, uint8_t sound);

// F21.18  SOUND_EFFECTS ($FF55) with its shared writer SOUND_WRITE ($FF8B): each frame, plays the sound effect that SOUND_START chose, on channel 0 (the
// caller has X = 0). Ghost eaten: control 4, pitch = timer and $1F, volume = pitch xor $0F; power pill and bonus item: the steady tone $0D; wafer:
// control 9, pitch 9, volume = timer. The timer goes down; when it reaches 0 nothing is written and the effect ends (SOUND_ACTIVE = 0). TEMP_D3 is the
// scratch byte that holds the control value.
void soundEffects(GameState& st, SoundChip& chip);

// F21.19  SOUND_MUSIC ($FF94): each frame, the long sounds, in this order: Pac-Man dying (GAME_FLAGS bit 6): channel 0, volume = pitch x 2 = EVENT_TIMER,
// control 4; the start tune (GAME_FLAGS bit 3): from bits 3-5 of EVENT_TIMER (or, when the timer is 0, of the flags with the tune bit cleared) a note 0-3
// for both channels from the cartridge tables $FFF1 / $FFF5, control 4, volume 8, silent for 4-7; the scared-ghost sweep (PILL_FLAGS bit 6): channel 1,
// control 4, pitch = (FRAME_COUNTER / 2) and $0F, volume = (pitch xor $0F) / 2. The caller has X = 1 (channel 1) for the sweep. TEMP_D3 holds the sweep's
// control value.
void soundMusic(GameState& st, const Cartridge& cart, SoundChip& chip);

// F21.20  PACMAN_KEEP_GOING ($F793): Pac-Man with the stick neutral. If he is not exactly on a cell nothing happens (NotOnCell: the routine returns and the
// main loop then moves him). Exactly on a cell he tries to go on in his own direction (TRY_* of F21.12): if that is allowed the direction is set again
// (Moved: the routine returns and the main loop moves him); if a wall is in the way (Stands) the routine drops its caller's return and the main loop
// goes on at $F7BA without moving him: he stands still.
enum class KeepGoing { NotOnCell, Moved, Stands };
KeepGoing pacmanKeepGoing(GameState& st, const Cartridge& cart);

// F21.21  SCORE_DIGITS ($F3EE) with the CLEAR_GRAPHICS ($F483) it falls into: turns the decimal digits of one player's score into the
// picture pointers of the score line and draws the line. `offset` is the byte of the score's high part (4 for player 1, 5 for player 2; the score is
// SCORE_P1_LO + offset, - 2, - 4). Only five digits are shown (the hundred-thousands digit never): the low nibble of the high byte and both nibbles of the
// middle and low bytes; leading zeros are blank (shape number 10), a score of 0 shows one 0. The digit pointers go to $D5-$DE (low byte from the cartridge
// table at $FEF6, high byte $FE), TEMP_E1 counts the digits that are not zero, TEMP_E2 is the slot being filled. The background colour is set to black
// after the third digit and to TEMP_D2 (the score colour) after the fifth. Then seven picture rows (row 6 down to 0), each: the graphics registers cleared,
// then the five digit shapes (cartridge bytes at $FE00 + shape pointer + row) written to GRP0 / GRP1 in the six-picture order; finally the graphics registers
// are cleared again (CLEAR_GRAPHICS).
void scoreDigits(GameState& st, const Cartridge& cart, TiaChip& chip, uint8_t offset);

// The console as the game logic sees it in one frame: the buttons and switches the top logic reads (F9).
struct ConsoleInput {
  bool fire = false;          // the fire button of the left joystick
  bool reset = false;         // the Reset switch pressed
  bool select = false;        // the Select switch pressed
  bool leftDifficultyA = false;   // the left difficulty switch in position A (SWCHB bit 6 set)
  bool rightDifficultyA = false;  // the right difficulty switch in position A (SWCHB bit 7 set)
  bool colour = true;             // the colour / black-and-white switch in position colour (SWCHB bit 3 set)
};

// F21.22  The top logic of the frame ($F87E-$F9A7), after the vertical sync has been switched off: the score colour of player 1; a new game (fire on the
// title screen, or Reset) or a step of the game number (Select: at once, then every 32 frames while held); the difficulty switch of the player in turn
// into GAME_FLAGS bit 5; the sprite colours; the ghost of this frame (object (FRAME_COUNTER and 3) + 1) updated three times unless the game is
// frozen or paused; the picture set-up values SPRITE_X and SPRITE0_Y; the opening animations of that ghost and of Pac-Man; the six colour values made
// from the colour table of the cartridge ($FF2B, or $FF36 for black and white), the title-screen shift and the colour mask; COLUP1 and COLUP0.
// The code then goes on with the drawing code at $F07F. Returns false if ghostDecide met the never-run path.
bool topLogic(GameState& st, const Cartridge& cart, const ConsoleInput& input, TiaChip& chip);

// What the chip tells the game in one frame, besides the console switches: the joysticks and the two collision flags (F12, F18).
struct FrameInput {
  uint8_t swcha = 0xFF;       // the joystick port: left joystick in the upper nibble, right joystick in the lower; 0 = pressed
  bool bonusTouched = false;  // missile 1 and the ball both overlapped Pac-Man (the bonus item)
  bool ghostTouched = false;  // player 0 (the ghost of the frame) overlapped player 1 (Pac-Man)
};

// F21.23  The bottom logic of the frame ($F4E7-$F7CE), after the picture is drawn: nothing on the title screen; otherwise the pause after a ghost was
// eaten (the pause counter only), the event timer (scare, freeze, death delay: F16, F17), the freeze set-up of the ghost speeds, the collisions (bonus
// item, power pill, ghost: F12), the joystick and Pac-Man's turns, his wafer, the new map, his move and stand-still rules (F10, F11), and at the end
// the sound routines (F19). Returns false if ghostUpdate's never-run path was met (it is not called from here, so this does not occur); kept for symmetry.
bool bottomLogic(GameState& st, const Cartridge& cart, const FrameInput& input, TiaChip& chip);

// F21.24  The start of the next frame ($F7D6-$F87A), after the overscan wait: the vertical blank and the vertical sync are switched on and the collision flags
// cleared (chip writes); the picture of Pac-Man and of the ghost sprite chosen from the cartridge tables ($FDAB: four ghost pictures by FRAME_COUNTER bits 4-5;
// $FDC2: four Pac-Man pictures by bits 3-4, or $FDFA by the event timer while he dies, or the first one while frozen); FRAME_COUNTER goes up; every 256
// frames TICK256 goes up and, unless it wraps, the bonus item's timer is counted (F15); on the title screen ATTRACT_SHIFT goes up whenever FRAME_COUNTER wraps
// (never to 0); the picture set-up values; the colour mask from the colour / black-and-white switch and the title shift; three colour indices.
// The code then goes on at $F87C (where the vertical sync is switched off: the frame boundary).
void frameStart(GameState& st, const Cartridge& cart, const ConsoleInput& input, TiaChip& chip);

// F21.25  The start-up ($F000-$F052), when the console is switched on: the whole page $00-$FF is cleared (the 128 chip registers, written twice over their
// mirrors, and the 128 memory bytes, the stack included); game number 6 (the power-on game), the bonus item's timer $0C, the title shift and the random
// number $FF; NEW_GAME_SETUP; the score shows $CB, $ED, $0F (the blank pattern of the title screen) and Pac-Man's y is $65; the four speed counters start at $0F;
// the bonus item's ball and missile are positioned (the chip writes HMM1, HMBL, RESBL, RESM1, HMOVE, HMCLR); the flags and the lives are 0.
// The code then goes on at the frame start ($F7D6).
void startUp(GameState& st, TiaChip& chip);

}  // namespace game
