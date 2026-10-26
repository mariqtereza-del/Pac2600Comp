#include "routines.hpp"
#include "data/rom_tables.hpp"

namespace game {

// The top logic of the frame ($F87E-$F9A7).
namespace {

// `lead` is the number of processor cycles from the end of the write that switched the vertical sync off to the first instruction of this step.
void clearTitleSelectStep(GameState& st, TiaChip& chip, long lead) {        // L_F8C8: the game number goes up by one
  st.gameSelect = static_cast<uint8_t>(st.gameSelect + 1);
  st.objY[0] = 0x60;
  st.gameFlags = 0x10;
  st.playerMode ^= 0x40;                        // one player / two players alternately
  clearScores(st);
  lead += 5 + 2 + 3 + 2 + 3 + 3 + 2 + 3 + 6 + (4 + 5 * 9 + 8 + 3 + 3 + 6);   // INC .. STA PLAYER_MODE, JSR, CLEAR_SCORES
  chip.writeAtCycle(lead + 3, chip::AUDV0, 0);
  chip.writeAtCycle(lead + 3 + 3, chip::AUDV1, 0);
  st.soundActive = 0;
  st.attractShift = 1;
  st.frameCounter = 1;
  uint8_t select = st.gameSelect;
  if (select == 0x12) select = 2;
  st.gameSelect = select;
  uint8_t shown = static_cast<uint8_t>(((select >> 1) - 2) & 7);   // the game number shown on the score line
  if (shown == 0) shown = 8;
  st.score[0] = shown;
  st.score[1] = shown;
}

void startGame(GameState& st) {            // F890: a new game
  st.rng = 0xFF;
  clearScores(st);
  st.player[1].lives = 0;
  st.attractShift = 0;
  st.gameFlags = 0x08;
  st.speedAcc[0] = (st.gameSelect >= 8) ? 0x0B : 0x09;    // Pac-Man's speed seed (F10)
  newGameSetup(st);
  st.playerMode &= 0x7F;
  if (st.playerMode & 0x40) {                   // two players: the second block is set up too
    swapPlayers(st);
    newGameSetup(st);
    st.player[1].lives = static_cast<uint8_t>(st.player[1].lives + 1);
  }
}

}  // namespace

bool topLogic(GameState& st, const Cartridge& cart, const ConsoleInput& in, TiaChip& chip) {
  st.colour[kColourScoreP1] = st.player[0].scoreColour;
  const bool start = (st.attractShift != 0 && in.fire) || in.reset;
  if (start) {
    startGame(st);
    st.selectRepeat = 0;
  } else if (!in.select) {
    st.selectRepeat = 0;
  } else {
    const long a0 = st.attractShift == 0 ? 12 : 16;   // the cycles from the end of the sync to the Select test (title screen: the fire test too)
    if (st.selectRepeat == 0) clearTitleSelectStep(st, chip, a0 + 18);
    for (;;) {                                               // L_F8FF: held down, a step every 32 frames
      st.selectRepeat = static_cast<uint8_t>(st.selectRepeat + 1);
      if ((st.selectRepeat & 0x1F) != 0) break;
      clearTitleSelectStep(st, chip, a0 + 35);
    }
  }
  // L_F90D: the difficulty switch of the player in turn becomes GAME_FLAGS bit 5
  st.gameFlags &= 0xDF;
  const bool playerTwo = (st.playerMode & 0x80) != 0;
  if (playerTwo ? in.rightDifficultyA : in.leftDifficultyA) st.gameFlags |= 0x20;
  st.colour[kColourSprite1] = 6;
  uint8_t object = st.frameCounter & 3;
  uint8_t colour0 = object;
  const uint8_t pill = st.player[0].pillFlags;
  if (pill & 0x80) colour0 = (pill & 0x40) ? 4 : 5;
  st.colour[kColourSprite0] = colour0;
  ++object;                                                  // 1-4: the ghost of this frame
  bool update;
  if (st.objState[object] & 0x08) {   // eyes: always updated, with another picture
    st.scratch[kSprite0PtrLo] = 0xC6;
    update = true;
  } else {
    update = !(st.eatPause & 0x80);
  }
  if (update && !(st.gameFlags & 0x80)) {
    for (int i = 0; i < 3; ++i)
      if (!ghostUpdate(st, cart, object)) return false;
  }
  st.spriteX = st.objX[object];
  st.scratch[kSprite0Y] = static_cast<uint8_t>(st.objY[object] + 0x0B);
  if (st.objState[object] & 0x20) animateObject(st, cart, object, 0);
  if (st.objState[0] & 0x20) animateObject(st, cart, 0, 1);
  // the six colour values: the colour table of the cartridge ($FF2B; $FF36 when the mask says black and white), the title shift, the mask
  const bool colourTv = (st.colourMask & 0x80) != 0;
  const uint16_t table = colourTv ? rom::kColourTableTv : rom::kColourTableBlackWhite;
  st.scratch[kTempD3] = 0xFF;
  st.scratch[kTempD2] = colourTv ? 0x2B : 0x36;
  for (int x = 5; x >= 0; --x) {
    uint8_t& entry = st.colour[kColourBg + x];
    entry = (cart.at(static_cast<uint16_t>(table + entry)) ^ st.attractShift) & st.colourMask;
  }
  chip.write(chip::COLUP1, st.colour[kColourSprite1]);
  chip.write(chip::COLUP0, st.colour[kColourSprite0]);
  return true;
}

// The start of the next frame ($F7D6-$F87A).
void frameStart(GameState& st, const Cartridge& cart, const ConsoleInput& in, TiaChip& chip) {
  chip.write(chip::VBLANK, 3);
  chip.write(chip::VSYNC, 3);
  chip.write(chip::CXCLR, 3);
  const uint8_t counter = st.frameCounter;
  const uint8_t eighths = counter >> 3;
  st.scratch[kSprite0PtrLo] = cart.at(rom::kGhostPictures + ((eighths >> 1) & 3));
  uint8_t sprite1 = 0;
  bool chosen = false;
  if (st.gameFlags & 0x40) {                         // dying: the picture follows the event timer
    sprite1 = cart.at(rom::kPacmanDyingPictures + ((st.eventTimer >> 2) & 0x0F));
    chosen = sprite1 != 0;
  }
  if (!chosen) {
    sprite1 = cart.at(rom::kPacmanPictures + (eighths & 3));
    if (st.gameFlags & 0x80) sprite1 = cart.at(rom::kPacmanPictures);        // frozen: the first picture
  }
  st.scratch[kSprite1PtrLo] = sprite1;
  uint8_t& frame = st.frameCounter;
  frame = static_cast<uint8_t>(frame + 1);
  if (frame == 0) {
    uint8_t& tick = st.tick256;
    tick = static_cast<uint8_t>(tick + 1);
    bool attractStep = true;
    if (tick != 0) {                                             // the bonus item's timer (F15)
      uint8_t& bonus = st.bonusTimer;
      bonus = static_cast<uint8_t>(bonus - 1);
      if (bonus == 0) {
        uint8_t& flags = st.gameFlags;
        flags ^= 0x02;
        uint8_t length = 0x12;
        if (flags & 0x02) length = (flags & 0x20) ? 2 : 3;
        bonus = length;
      }
      attractStep = st.attractShift != 0;
    }
    if (attractStep) {                                           // the title-screen shift, never 0
      uint8_t& shift = st.attractShift;
      do shift = static_cast<uint8_t>(shift + 1); while (shift == 0);
    }
  }
  st.scratch[kSprite0PtrHi] = 0xFD;
  st.scratch[kSprite1PtrHi] = 0xFD;
  st.scratch[kSprite1Y] = static_cast<uint8_t>(st.objY[0] + 0x0B);
  st.scratch[kSprite0Lines] = 7;
  st.scratch[kSprite1Lines] = 6;
  uint8_t mask = in.colour ? 0xFF : 0x0F;
  if (st.attractShift != 0) mask ^= 0x08;
  st.colourMask = mask;
  st.colour[kColourBg] = 7;
  st.colour[kColourMaze] = 8;
  st.colour[kColourScoreP2] = st.player[1].scoreColour;
}

}  // namespace game
