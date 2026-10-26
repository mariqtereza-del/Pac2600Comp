// Reading tip: cy(n) counts processor cycles of the original (sound timing, state/cycles.hpp); the rules of the game are everything else.
#include "pacman.hpp"
#include "../state/cycles.hpp"
#include "maze.hpp"
#include "objects.hpp"

namespace game {

namespace {

enum class Flow { Finish, KeepGoing };

// L_F6CE: the pressed directions (up 1, down 2, left 4, right 8; the TEMP_D2 copy is what the original works on)
Flow joystickAndMove(BottomContext& b) {
  GameState& st = b.st;
  uint8_t a = st.joyDirection, y = 0;
  bool carry = false;
  cy(2 + 3 + 3);
  st.scratch[kTempD2] = a;
  if (a == 0) { cy(2 + 3); return Flow::KeepGoing; }
  cy(3 + 3);
  y = st.objState[0];
  enum { L_F6DD, L_F6F0, L_F700, L_F70A, L_F714, L_F71B, L_F725, L_F72D, L_F751, L_F75B, L_F766, L_F770, L_F775, L_F779, L_F77B, L_F781 } at_;
  if (!(y & 0x80)) { cy(3 + 1); at_ = L_F72D; } else { cy(2); at_ = L_F6DD; }
  for (;;) {
    switch (at_) {
      case L_F6DD:
        a = (st.scratch[kTempD2] >> 2) & 3;
        cy(2 + 2 + 2);
        if (a == 0) { cy(3 + 1); at_ = L_F725; break; }
        cy(2);
        carry = (a & 1) != 0;
        a >>= 1;
        cy(2 + 3);
        a = st.yOffset;
        if (a != 0) { cy(3 + 1); at_ = L_F725; break; }
        cy(2 + 3);
        a = st.xOffset;
        if (a == 0) { cy(3); at_ = L_F6F0; break; }
        cy(2);
        if (!carry) { cy(3 + 1); at_ = L_F71B; break; }
        cy(2 + 3 + 1);
        at_ = L_F700;
        break;
      case L_F6F0:
        cy(3);
        y = st.cellIndex;
        if (!carry) { cy(3 + 1); at_ = L_F70A; break; }
        cy(2 + 3 + 2);
        if (st.posCode == 0xF9) { cy(3 + 1); at_ = L_F725; break; }
        cy(2 + 2 + 4);
        --y;
        if (mazeWall(y) & 0x80) { cy(3); at_ = L_F725; break; }
        cy(2);
        at_ = L_F700;
        break;
      case L_F700:
        cy(2 + 3 + 3 + 2 + 3);
        st.objState[0] |= kObjMirrored;
        y = 1;
        at_ = L_F77B;
        break;
      case L_F70A:
        cy(3 + 2);
        if (st.cellRow != 3) cy(3);
        else {
          cy(2 + 2);
          if (y == 0x38) { cy(3); at_ = L_F725; break; }
          cy(2);
        }
        at_ = L_F714;
        break;
      case L_F714:
        cy(2 + 2 + 4);
        y = static_cast<uint8_t>(y + 2);
        if (mazeWall(y) & 0x80) { cy(3); at_ = L_F725; break; }
        cy(2);
        at_ = L_F71B;
        break;
      case L_F71B:
        cy(2 + 3 + 3 + 2 + 3);
        st.objState[0] &= static_cast<uint8_t>(~kObjMirrored);
        y = 0;
        at_ = L_F77B;
        break;
      case L_F725:
        cy(3 + 2);
        a = st.scratch[kTempD2] & 0xF3;
        if (a == 0) { cy(3); return Flow::KeepGoing; }
        cy(2 + 3);
        st.scratch[kTempD2] = a;
        at_ = L_F72D;
        break;
      case L_F72D:
        cy(3 + 2);
        a = st.scratch[kTempD2] & 3;
        if (a == 0) { cy(3); at_ = L_F781; break; }
        cy(2 + 2);
        carry = (a & 1) != 0;
        cy(3);
        if (st.xOffset != 0) { cy(3); at_ = L_F781; break; }
        cy(2 + 3 + 2);
        if (st.objX[0] != 0x4C) { cy(3); at_ = L_F75B; break; }
        cy(2 + 3);
        a = st.objY[0];
        if (a == 0) { cy(3); at_ = L_F751; break; }
        cy(2 + 2);
        if (a != 0x46) { cy(3); at_ = L_F75B; break; }
        cy(2 + 3 + 2 + 2);
        carry = (st.scratch[kTempD2] & 0x02) != 0;
        if (!carry) { cy(3); at_ = L_F75B; break; }
        cy(2 + 6);
        wrapToBottom(st, 0);
        cy(3);
        return Flow::Finish;
      case L_F751:
        cy(3 + 2);
        carry = (st.scratch[kTempD2] & 0x01) != 0;
        if (!carry) { cy(3); at_ = L_F75B; break; }
        cy(2 + 6);
        wrapToTop(st, 0);
        cy(3);
        return Flow::Finish;
      case L_F75B:
        cy(3 + 2 + 3);
        carry = (st.scratch[kTempD2] & 0x01) != 0;
        a = st.yOffset;
        if (a == 0) { cy(3); at_ = L_F766; break; }
        cy(2);
        if (!carry) { cy(3); at_ = L_F779; break; }
        cy(2 + 3);
        at_ = L_F770;
        break;
      case L_F766:
        cy(3 + 4);
        y = st.cellIndex;
        a = mazeWall(y);
        if (!carry) { cy(3); at_ = L_F775; break; }
        cy(2 + 2);
        if (!(a & 0x40)) { cy(3); at_ = L_F781; break; }
        cy(2);
        at_ = L_F770;
        break;
      case L_F770:
        cy(2 + 3);
        y = 3;
        at_ = L_F77B;
        break;
      case L_F775:
        cy(2 + 2);
        if (!(a & 0x20)) { cy(3); at_ = L_F781; break; }
        cy(2);
        at_ = L_F779;
        break;
      case L_F779:
        cy(2);
        y = 2;
        at_ = L_F77B;
        break;
      case L_F77B:
        cy(6);
        setDirection(st, 0, y);
        cy(3);
        return Flow::Finish;
      case L_F781:
        cy(3 + 2 + 3 + 3);
        st.scratch[kTempD2] = st.scratch[kTempD2] & 0xFC;
        at_ = L_F6DD;
        break;
    }
  }
}

}  // namespace

// PACMAN_KEEP_GOING ($F793).
KeepGoing pacmanKeepGoing(GameState& st) {
  cy(3 + 3);   // LDA, ORA
  if ((st.yOffset | st.xOffset) != 0) { cy(2 + 6); return KeepGoing::NotOnCell; }
  cy(3 + 4 + 2);   // BEQ taken, LDA OBJ_STATE,X, ROL
  const uint8_t direction = st.objState[0] >> 6;      // 0 right, 1 left, 2 down, 3 up: the same code the TRY routines use
  // BCS / BPL and the JSR of the TRY routine; the JMP to the PLA, PLA when the turn is refused
  switch (direction) {
    case 0: cy(2 + 3 + 6); break;
    case 1: cy(2 + 2 + 6); break;
    case 2: cy(3 + 3 + 6); break;
    default: cy(3 + 2 + 6); break;
  }
  const bool taken = tryDirection(st, 0, direction).exit == TryExit::Taken;
  if (!taken) cy((direction == 2 ? 0 : 3) + 4 + 4);   // JMP L_F7B8 (not after the last case), PLA, PLA: the caller is left too
  return taken ? KeepGoing::Moved : KeepGoing::Stands;
}

// The end shared by Pac-Man in the maze and by the power pill: the new map when nothing is left, the stick, his move; then the end of the frame.
void pacmanMove(BottomContext& b) {
  GameState& st = b.st;
  if (newMapCheck(st)) return finishFrame(b);
  if (joystickAndMove(b) == Flow::KeepGoing) {
    cy(6);
    if (pacmanKeepGoing(st) != KeepGoing::Stands) { cy(6); moveObject(st, 0); cy(3); }
  }
  finishFrame(b);
}

// L_F61E: the joystick, Pac-Man in an opening or in the maze
void pacmanBottom(BottomContext& b) {
  GameState& st = b.st;
  const FrameInput& in = b.in;
  cy(4 + 3);
  uint8_t joy = in.swcha;
  if (st.playerMode & 0x80) cy(3);
  else { cy(2 + 8); joy >>= 4; }
  joy = static_cast<uint8_t>(~joy) & 0x0F;
  cy(2 + 2 + 3);
  st.joyDirection = joy;
  if (joy == 0) cy(3);
  else { cy(2 + 2 + 3); st.tick256 = 0; }
  cy(3 + 2);
  if (st.objState[0] & kObjInOpening) {                                   // in an opening: only a turn round
    cy(2);
    cy(3 + 2);
    const bool up = (joy & 1) != 0, down = (joy & 2) != 0;
    bool toL644 = false;
    if (!up) {
      cy(3 + 2);                                           // BCC taken, LSR
      if (!down) cy(3);
      else {
        cy(2 + 3);
        if (st.objState[0] & 0x80) { cy(3); toL644 = true; } else cy(2 + 3);
      }
    } else {
      cy(2 + 3);
      if (st.objState[0] & 0x80) cy(3); else { cy(2); toL644 = true; }
    }
    if (toL644) {                                          // L_F644
      cy(2);
      if (st.objState[0] & kObjStepDue) { cy(3 + 2 + 3); st.objState[0] ^= 0x80; cy(3 + 3); }
      else {
        cy(2 + 2 + 3 + 2 + 3 + 2 + 3 + 3);
        st.objY[0] = static_cast<uint8_t>((0x3F ^ st.objY[0]) & 0x3F);
        st.objState[0] ^= 0xC0;
      }
    }
    cy(3);   // JMP L_F7BA
    return finishFrame(b);
  }
  cy(3);
  eatWafer(st);
  pacmanMove(b);
}

}  // namespace game
