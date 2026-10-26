#include "flow.hpp"
#include "../data/rom_tables.hpp"
#include "../state/cycles.hpp"
#include "players.hpp"

namespace game {

// PLACE_OBJECTS ($FC21). Objects 0-4 are Pac-Man and the four ghosts (x at $B1+n, y at $B6+n, state at $BB+n).
void placeObjects(GameState& st) {
  cy(17 + 5 * 18 + 4 * 3 + 2 + 10 + 6);   // the set-up, five passes of the loop (four take the branch), the last four stores, RTS
  st.gameFlags |= 0x80;
  st.eventTimer = 0x3C;
  for (uint8_t n = 0; n <= 4; ++n) {
    st.objX[n] = 0x4C;
    st.objY[n] = 0x1E;
    st.objState[n] = 0;
  }
  st.objY[0] = 0x3C;
  st.objState[0] = 0x80;
}

// The start-up ($F000-$F052).
void startUp(GameState& st, TiaChip& chip) {
  for (int a = 0; a < 0x80; ++a) chip.write(static_cast<uint8_t>(a & 0x3F), 0);    // $00-$7F: the chip registers and their mirrors
  st = GameState{};                                                                    // $80-$FF
  st.gameSelect = 6;
  st.bonusTimer = 12;
  st.attractShift = 0xFF;
  st.rng = 0xFF;
  newGameSetup(st);
  st.score[0] = 0xCB;
  st.objY[0] = 0xCB >> 1;
  st.score[2] = 0xED;
  st.score[4] = 0x0F;
  for (uint8_t i = 1; i <= 4; ++i) st.speedAcc[i] = 0x0F;
  chip.write(chip::HMM1, 0xF0);
  chip.write(chip::HMBL, 0x80);
  chip.write(chip::RESBL, 0x80);
  chip.write(chip::RESM1, 0x80);
  chip.write(chip::HMOVE, 0x80);
  chip.write(chip::HMCLR, 0x80);
  st.gameFlags = 0;
  st.player[0].lives = 0;
}

// L_F4F7: the event timer runs every second frame; when it ends, the scare, the death delay or the freeze is over. Returns true if the frame is
// over but for the sounds (game over).
bool eventTimerStep(BottomContext& b) {
  GameState& st = b.st;
  cy(3);
  uint8_t& timer = st.eventTimer;            // L_F4F7: the event timer runs every second frame
  cy(3);
  bool skipToF573 = false;
  if (timer == 0) cy(3 + 1);
  else {
    cy(2 + 3 + 2);
    if (!(st.frameCounter & 1)) cy(3);
    else {
      cy(2 + 5);
      timer = static_cast<uint8_t>(timer - 1);
      if (timer != 0) cy(3);
      else {
        cy(2 + 3 + 3);
        bool flagsPart = true;
        if (!(st.player[0].pillFlags & 0x80)) cy(3);
        else {
          cy(2);
          if (st.player[0].pillFlags & 0x40) { cy(2 + 2 + 3 + 2 + 3 + 3); st.player[0].pillFlags &= 0xBF; timer = 0x20; flagsPart = false; }   // flashing part of the scare
          else { cy(3 + 2 + 3); st.player[0].pillFlags &= 0x7C; }                                                            // the scare is over
        }
        if (flagsPart) {                                  // L_F51A
          cy(3 + 2);
          if (st.gameFlags & 0x40) {                           // the death delay is over
            cy(2 + 3 + 3);
            st.gameFlags &= 0xBF;
            cy(6);
            placeObjects(st);
            cy(3);
            if (!(st.playerMode & 0x40)) cy(3);
            else {
              cy(2 + 3);
              if (st.player[1].lives == 0) cy(3);
              else {                                      // two players: the turn passes
                cy(2 + 6);
                swapPlayers(st);
                cy(3 + 2 + 3);
                st.playerMode ^= 0x80;
              }
            }
            cy(3 + 3);
            if ((st.player[0].lives | st.player[1].lives) != 0) {
              cy(2 + 5 + 2 + 3 + 3 + 3);
              st.player[0].lives = static_cast<uint8_t>(st.player[0].lives - 1);
              st.gameFlags |= 0x08;
            } else {                                      // game over
              cy(3 + 3 + 2 + 3 + 3 + 3);
              st.gameFlags = 0;
              st.attractShift = 0x5F;
              st.objY[0] = 0x5F;
              return true;
            }
          } else {
            cy(3 + 3);
            if (!(st.gameFlags & 0x80)) cy(3);
            else { cy(2 + 2 + 3); st.gameFlags &= 0x7F; }      // the freeze is over
          }
        }
      }
    }
  }
  return false;
}

// L_F55B: while the game is frozen the ghosts' speed accumulators and chase timers are set up from the level.
void frozenSpeedSetup(BottomContext& b) {
  GameState& st = b.st;
    cy(2 + 3 + 2 + 2 + 2);
    const uint8_t level = st.player[0].level;
    const uint8_t below = static_cast<uint8_t>(level - 1);
    for (uint8_t x = 4; x >= 1; x = static_cast<uint8_t>(x - 1)) {
      st.speedAcc[x] = (x & 1) ? below : level;
      st.ghostTimer[x - 1] = below;
    }
    cy(45 + 4 + 3);
}

}  // namespace game
