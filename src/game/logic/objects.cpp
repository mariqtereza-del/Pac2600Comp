#include "objects.hpp"
#include "../data/rom_tables.hpp"
#include "../state/cycles.hpp"

namespace game {

namespace {
// WRAP_SET_Y ($FD3C): the shared end of both routines.
void wrapEnd(GameState& st, uint8_t object, uint8_t directionBits, uint8_t y) {
  uint8_t& state = st.objState[object];
  state = static_cast<uint8_t>((state & 0x0F) | directionBits);
  st.objY[object] = y;
}
}  // namespace

void wrapToBottom(GameState& st, uint8_t object) { cy(4 + 2 + 2 + 4 + 2 + 3 + 4 + 6); wrapEnd(st, object, 0x30, 0x51); }

void wrapToTop(GameState& st, uint8_t object) { cy(4 + 2 + 2 + 4 + 2 + 4 + 6); wrapEnd(st, object, 0xF0, 0x0B); }

// LOCATE_OBJECT ($FBAD). The row loop of the original subtracts 10 from y until the result is 0 or has bit 7 set, counting the passes in Y; so for
// y = 0 the row is 0 and for 0 < y < 128 the row is y / 10 rounded up and y_offset = y - 10 * row (zero or negative). (y of 128 and above never
// occurs: the largest y of an object is $5B; the loop is copied as it is.)
Location locateObject(GameState& st, uint8_t object) {
  uint8_t y = st.objY[object];
  const uint8_t x = st.objX[object];
  uint8_t row = 0;
  cy(2 + 4);   // LDY #0, LDA OBJ_Y,X
  if (y == 0) cy(3);
  else {
    cy(2);
    do {
      ++row;
      y = static_cast<uint8_t>(y - 10);
      cy(2 + 2 + 2);   // INY, SEC, SBC
      if (y == 0) { cy(3); break; }
      cy(2);
      if (y & 0x80) { cy(2); break; }
      cy(3);
    } while (true);
  }
  Location loc{};
  loc.row = row;
  loc.yOffset = y;
  loc.xOffset = x & 3;
  uint8_t cell = x >> 2;
  cy(35 + ((row & 1) ? 6 : 3) + 23 + 6);   // the stores and shifts, the branch (odd rows add 40 to the cell), the position code, RTS
  if (row & 1) cell = static_cast<uint8_t>(cell + 0x28);
  loc.cellIndex = cell;
  loc.cellBit6 = (cell >> 6) & 1;
  loc.posCode = static_cast<uint8_t>((cell << 2) | (loc.cellBit6 << 7) | (row >> 1));
  st.cellRow = loc.row;
  st.yOffset = loc.yOffset;
  st.xOffset = loc.xOffset;
  st.cellIndex = loc.cellIndex;
  st.posCode = loc.posCode;
  return loc;
}

// MOVE_OBJECT ($F9EE).
Move moveObject(GameState& st, uint8_t object) {
  Move mv{};
  uint8_t& acc = st.speedAcc[object];
  mv.oldAcc = acc;
  const unsigned sum = static_cast<uint8_t>(acc << 4) + acc;
  acc = static_cast<uint8_t>(sum);
  cy(4 + 8 + 2 + 4 + 4);   // LDA, ASL x4, CLC, ADC, STA
  if (sum <= 0xFF) { cy(3 + 6); return mv; }
  cy(2);
  mv.stepped = true;
  uint8_t& state = st.objState[object];
  state &= 0xEF;
  mv.vertical = (state & 0x80) != 0;
  mv.stepValue = (state & 0x40) ? 0xFF : 0x01;      // left and up count down, right and down count up
  cy(2 + 4 + 2 + 4 + 2);   // LDY #0, LDA, AND, STA, ROL
  if (state & 0x40) cy(2 + 2 + 3); else cy(3 + 2);   // BPL / DEY, BMI ; INY
  cy(2);   // TYA
  if (!mv.vertical) {
    cy(2 + 4 + 2);   // BCS, ADC, CMP #2
    uint8_t& x = st.objX[object];
    mv.oldCoord = x;
    mv.newCoord = static_cast<uint8_t>(x + mv.stepValue);
    if (mv.newCoord == 0x02) cy(3 + 6);
    else {
      cy(2 + 2);   // BEQ, CMP #$96
      if (mv.newCoord == 0x96) cy(3 + 6); else cy(2 + 4 + 6);
    }
    mv.stored = mv.newCoord != 0x02 && mv.newCoord != 0x96;
    if (mv.stored) x = mv.newCoord;
  } else {
    cy(3 + 2 + 4);   // BCS taken, CLC, ADC
    uint8_t& y = st.objY[object];
    mv.oldCoord = y;
    mv.newCoord = static_cast<uint8_t>(y + mv.stepValue);
    if (mv.newCoord & 0x80) cy(3 + 6);
    else {
      cy(2 + 2);   // BMI, CMP #$47
      if (mv.newCoord == 0x47) cy(3 + 6); else cy(2 + 4 + 6);
    }
    mv.stored = !(mv.newCoord & 0x80) && mv.newCoord != 0x47;
    if (mv.stored) y = mv.newCoord;
  }
  return mv;
}

// SET_DIRECTION ($F9E5).
Move setDirection(GameState& st, uint8_t object, uint8_t direction) {
  cy(4 + 2 + 4 + 4);   // LDA, AND, ORA, STA, then MOVE_OBJECT follows
  uint8_t& state = st.objState[object];
  state = static_cast<uint8_t>((state & 0x3F) | (direction << 6) | 0x10);
  return moveObject(st, object);
}

// TRY_UP / TRY_DOWN / TRY_LEFT / TRY_RIGHT and TRY_COMMIT.
TryResult tryDirection(GameState& st, const Cartridge& cart, uint8_t object, uint8_t direction) {
  TryResult r{};
  r.row = st.cellRow;
  r.cell = st.cellIndex;
  switch (direction) {
    case 3:  // up: not from the first row; the cell must allow it (bit 6)
      cy(3);
      if (r.row == 0) { cy(3 + 6); r.exit = TryExit::AtEdge; return r; }
      cy(2 + 3 + 4 + 2);
      r.tableByte = cart.at(rom::kMazeWalls + r.cell);
      if (!(r.tableByte & 0x40)) { cy(3 + 6); r.exit = TryExit::Wall; return r; }
      cy(2 + 2);
      break;
    case 2:  // down: not from the last row; bit 5
      cy(3 + 2);
      if (r.row == 7) { cy(3 + 6); r.exit = TryExit::AtEdge; return r; }
      cy(2 + 3 + 4 + 2 + 2);
      r.tableByte = cart.at(rom::kMazeWalls + r.cell);
      if (!(r.tableByte & 0x20)) { cy(3 + 6); r.exit = TryExit::Wall; return r; }
      cy(2 + 2 + 3);
      break;
    case 1:  // left: the neighbour cell must have no wall (bit 7); the tunnel end of row 3 is closed
      cy(3 + 3 + 2);
      if (r.row != 3) cy(3);
      else {
        cy(2 + 2);
        if (r.cell == 0x3E) { cy(3 + 6); r.exit = TryExit::AtEdge; return r; }
        cy(2);
      }
      cy(2 + 4);
      r.tableByte = cart.at(rom::kMazeWalls + static_cast<uint8_t>(r.cell - 1));
      if (r.tableByte & 0x80) { cy(3 + 6); r.exit = TryExit::Wall; return r; }
      cy(2 + 2 + 3);
      break;
    default:  // right: the cell two further on (a cell is 4 pixels, an object 8 wide)
      cy(3 + 3 + 2);
      if (r.row != 3) cy(3);
      else {
        cy(2 + 2);
        if (r.cell == 0x38) { cy(3 + 6); r.exit = TryExit::AtEdge; return r; }
        cy(2);
      }
      cy(2 + 2 + 4);
      r.tableByte = cart.at(rom::kMazeWalls + static_cast<uint8_t>(r.cell + 2));
      if (r.tableByte & 0x80) { cy(3 + 6); r.exit = TryExit::Wall; return r; }
      cy(2 + 2 + 3);
      break;
  }
  uint8_t& state = st.objState[object];
  r.stateBits = state & 0xC0;
  // TRY_COMMIT: a ghost does not turn straight back (the table TBL_FE67 is ((d xor 1) << 6)); Pac-Man (object 0) may
  cy(2);   // TXA
  if (object == 0) cy(3);
  else {
    cy(2 + 4 + 2 + 4);
    if (r.stateBits == static_cast<uint8_t>((direction ^ 1) << 6)) { cy(3 + 6); r.exit = TryExit::TurnBack; return r; }
    cy(2);
  }
  cy(4 + 2 + 4 + 4 + 4 + 4 + 6);   // LDA, AND, ORA, STA, PLA, PLA, RTS
  state = static_cast<uint8_t>((state & 0x3F) | (direction << 6) | 0x10);
  r.exit = TryExit::Taken;
  return r;
}

// TRY_PAIR_UD / TRY_PAIR_LR (the choice of order is made from the random step's result by TRY_PAIR_1 / TRY_PAIR_2).
PairResult tryPair(GameState& st, const Cartridge& cart, uint8_t object, bool vertical, bool rngResultIsZero) {
  PairResult p{};
  const uint8_t low = vertical ? 3 : 1;   // up / left
  const uint8_t high = vertical ? 2 : 0;  // down / right
  p.dir[0] = rngResultIsZero ? low : high;
  p.dir[1] = rngResultIsZero ? high : low;
  p.res[0] = tryDirection(st, cart, object, p.dir[0]);
  p.count = 1;
  if (p.res[0].exit == TryExit::Taken) { p.taken = true; return p; }
  p.res[1] = tryDirection(st, cart, object, p.dir[1]);
  p.count = 2;
  p.taken = p.res[1].exit == TryExit::Taken;
  return p;
}

}  // namespace game
