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
#include "logic/players.hpp"
#include "logic/maze.hpp"
#include "logic/objects.hpp"
#include "logic/animation.hpp"
#include "logic/scoring.hpp"

namespace game {

// F21.3  RNG_STEP ($FBE0): steps the shift register RNG_STATE (the game's only random number, F14) and returns the value the original leaves in A:
// one bit of the new state, picked by its low three bits (zero or not is all the callers ask).
uint8_t rngStep(GameState& st);

// F21.5  PLACE_OBJECTS ($FC21): freezes the game (GAME_FLAGS bit 7), sets the event timer to $3C (the freeze lasts 60 steps) and puts the four ghosts
// at x = $4C, y = $1E (state 0) and Pac-Man at x = $4C, y = $3C with the state $80.
void placeObjects(GameState& st);
// F21.11  LIVES_MARKERS ($FC68): the three playfield values for the row of life markers of one player, for `lives` markers, stored in the scratch bytes
// TEMP_D2+offset .. TEMP_D4+offset (offset 0 for player 1, 3 for player 2): $40 if there is any life (else 0), then the cartridge bytes at $FF1B+lives
// and $FEEC+lives. Returns the last value stored (the original leaves it in A).
uint8_t livesMarkers(GameState& st, const Cartridge& cart, uint8_t lives, uint8_t offset);

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
