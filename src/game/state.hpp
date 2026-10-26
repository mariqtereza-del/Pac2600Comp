// The state of the game (Stage G3): everything the original keeps in the console's 128 bytes of memory, as named fields.
// The fields come in the order of the original's memory, so the whole state is exactly 128 bytes and can be copied to and from the original's memory
// image with one copy. That copy (and the proof that every field sits where the F7 map says) lives in src/testsupport, not here: the game code never
// sees an address.
#pragma once
#include <cstdint>
#include <type_traits>

namespace game {

// What each player owns; the original keeps two copies and swaps them at a change of turn, so the player in turn is always player[0] (F7, F17).
struct PlayerBlock {
  uint8_t maze[16];      // the wafer map: one bit per wafer place, power pills included (F11)
  uint8_t lives;
  uint8_t level;         // changes only the ghosts' speed and chase timer (F16)
  uint8_t pillFlags;     // bits 2-5: the power pills still there; bits 6-7: the scared ghosts' sweep and state (F11, F16)
  uint8_t scoreColour;
};

// The scratch bytes $D2-$E3. The original reuses them for several jobs at different moments (the score digits' picture pointers overlay the sprite
// set-up bytes, for instance), so they stay one array; these are the names of the places in it.
enum Scratch : uint8_t {
  kTempD2, kTempD3, kTempD4, kTempD5, kTempD6, kTempD7,
  kSprite0Y, kSprite1Y, kTempDA, kSprite0Lines, kSprite1Lines,
  kSprite0PtrLo, kSprite0PtrHi, kSprite1PtrLo, kSprite1PtrHi,
  kTempE1, kTempE2, kTempE3,
  kScratchSize
};

// The six colour values the frame start works out for the picture, in the order of the original's memory.
enum Colour : uint8_t { kColourBg, kColourMaze, kColourSprite0, kColourSprite1, kColourScoreP2, kColourScoreP1, kColourCount };

// Object numbers: 0 is Pac-Man, 1-4 are the four ghosts (the arrays below are indexed by them).
struct GameState {
  uint8_t frameCounter;      // +1 every frame (the INC is at the end of the frame)
  uint8_t tick256;           // +1 every 256 frames
  uint8_t attractShift;      // not 0: the title screen
  uint8_t selectRepeat;
  uint8_t gameSelect;        // the game number chosen with Select
  uint8_t yOffset;           // where the located object stands inside its cell (LOCATE_OBJECT, F10)
  uint8_t xOffset;
  uint8_t cellIndex;

  PlayerBlock player[2];     // [0] = the player in turn

  uint8_t spriteX;           // the x of the sprite the picture code draws next
  uint8_t objX[5];
  uint8_t objY[5];
  uint8_t objState[5];       // direction bits 7-6, step due bit 4, eyes bit 3, opening animation bit 5 (F10, F13)
  uint8_t speedAcc[5];       // the speed accumulators (F10)
  uint8_t ghostTimer[4];     // ghost 1-4 (index object - 1): the chase timers (F13)

  uint8_t cellRow;
  uint8_t rowParity;
  uint8_t rng;               // the game's only random number, a shift register (F14)
  uint8_t score[6];          // index = 2 * part + player: low, middle, high byte of player 1 and 2, decimal (F15)

  uint8_t scratch[kScratchSize];

  uint8_t eventTimer;
  uint8_t eatPause;
  uint8_t playerMode;        // bit 6 two players, bit 7 player two's turn (E8)
  uint8_t gameFlags;
  uint8_t soundActive;
  uint8_t soundTimer;
  uint8_t unusedEA;
  uint8_t unusedEB;
  uint8_t bonusTimer;
  uint8_t joyDirection;
  uint8_t posCode;
  uint8_t colourMask;
  uint8_t colour[kColourCount];

  uint8_t stack[10];         // the 6502's stack: the native game does not use it
};

static_assert(sizeof(GameState) == 128, "the state is the 128 bytes of the original's memory");
static_assert(std::is_standard_layout_v<GameState> && std::is_trivially_copyable_v<GameState>);

}  // namespace game
