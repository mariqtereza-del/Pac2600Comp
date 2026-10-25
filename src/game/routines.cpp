#include "routines.hpp"

namespace game {

// CLEAR_SCORES ($FC7D). The original loops X from 5 down to 0 storing 0 at SCORE_LO+X, i.e. the six score bytes $CC-$D1
// (player 1: low, player 2: low, player 1: middle, player 2: middle, player 1: high, player 2: high), then stores 0 in LIVES and LIVES_P2.
void clearScores(Ram& ram) {
  for (uint8_t i = 0; i < 6; ++i) at(ram, ram::SCORE_P1_LO + i) = 0;
  at(ram, ram::LIVES) = 0;
  at(ram, ram::LIVES_P2) = 0;
}

// SWAP_PLAYERS ($FB9F). The original loops X from $13 down to 0, exchanging the bytes MAZE_00+X and MAZE_P2_00+X.
void swapPlayers(Ram& ram) {
  for (uint8_t i = 0; i < 20; ++i) {
    const uint8_t mine = at(ram, ram::MAZE_00 + i);
    at(ram, ram::MAZE_00 + i) = at(ram, ram::MAZE_P2_00 + i);
    at(ram, ram::MAZE_P2_00 + i) = mine;
  }
}

// RNG_STEP ($FBE0). With r = RNG_STATE the original forms x = (r << 1) xor r (8 bits), shifts it left twice and rotates the last bit shifted out,
// which is bit 6 of x, into the low end of r: r = (r << 1) | bit 6 of x. The value returned is TBL_FE6A[r & 7] & r, where the table of the
// original holds the bit masks $80, $40, ... $01, i.e. 0x80 >> (r & 7) (the lockstep tool checks this against the cartridge bytes).
uint8_t rngStep(Ram& ram) {
  const uint8_t r = at(ram, ram::RNG_STATE);
  const uint8_t x = static_cast<uint8_t>((r << 1) ^ r);
  const uint8_t next = static_cast<uint8_t>((r << 1) | ((x >> 6) & 1));
  at(ram, ram::RNG_STATE) = next;
  return static_cast<uint8_t>((0x80 >> (next & 7)) & next);
}

namespace {
// WRAP_SET_Y ($FD3C): the shared end of both routines.
void wrapEnd(Ram& ram, uint8_t object, uint8_t directionBits, uint8_t y) {
  uint8_t& state = at(ram, ram::PACMAN_STATE + object);
  state = static_cast<uint8_t>((state & 0x0F) | directionBits);
  at(ram, ram::PACMAN_Y + object) = y;
}
}  // namespace

void wrapToBottom(Ram& ram, uint8_t object) { wrapEnd(ram, object, 0x30, 0x51); }
void wrapToTop(Ram& ram, uint8_t object) { wrapEnd(ram, object, 0xF0, 0x0B); }

// PLACE_OBJECTS ($FC21). Objects 0-4 are Pac-Man and the four ghosts (x at $B1+n, y at $B6+n, state at $BB+n).
void placeObjects(Ram& ram) {
  at(ram, ram::GAME_FLAGS) |= 0x80;
  at(ram, ram::EVENT_TIMER) = 0x3C;
  for (uint8_t n = 0; n <= 4; ++n) {
    at(ram, ram::PACMAN_X + n) = 0x4C;
    at(ram, ram::PACMAN_Y + n) = 0x1E;
    at(ram, ram::PACMAN_STATE + n) = 0;
  }
  at(ram, ram::PACMAN_Y) = 0x3C;
  at(ram, ram::PACMAN_STATE) = 0x80;
}

// NEW_MAZE ($FC0C). The 16 wafer bytes $88-$97 are set to $FF, except $8E = $7F and $8A = $FE (the two places that start empty, F11).
void newMaze(Ram& ram) {
  for (uint8_t i = 0; i < 16; ++i) at(ram, ram::MAZE_00 + i) = 0xFF;
  at(ram, ram::MAZE_06) = 0x7F;
  at(ram, ram::MAZE_02) = 0xFE;
  at(ram, ram::PILL_FLAGS) = 0x3C;
  placeObjects(ram);
}

// NEW_GAME_SETUP ($FBF4).
void newGameSetup(Ram& ram) {
  at(ram, ram::SCORE_COLOUR_P2) = 0x09;
  at(ram, ram::SCORE_COLOUR) = 0x0A;
  at(ram, ram::LIVES) = 3;
  at(ram, ram::LEVEL_SPEED) = static_cast<uint8_t>((((at(ram, ram::GAME_SELECT) - 2) & 6) + 4) & 0xFF);
  newMaze(ram);
}

// LOCATE_OBJECT ($FBAD). The row loop of the original subtracts 10 from y until the result is 0 or has bit 7 set, counting the passes in Y; so for
// y = 0 the row is 0 and for 0 < y < 128 the row is y / 10 rounded up and y_offset = y - 10 * row (zero or negative). (y of 128 and above never
// occurs: the largest y of an object is $5B; the loop is copied as it is.)
Location locateObject(Ram& ram, uint8_t object) {
  uint8_t y = at(ram, ram::PACMAN_Y + object);
  const uint8_t x = at(ram, ram::PACMAN_X + object);
  uint8_t row = 0;
  if (y != 0) {
    do {
      ++row;
      y = static_cast<uint8_t>(y - 10);
    } while (y != 0 && !(y & 0x80));
  }
  Location loc{};
  loc.row = row;
  loc.yOffset = y;
  loc.xOffset = x & 3;
  uint8_t cell = x >> 2;
  if (row & 1) cell = static_cast<uint8_t>(cell + 0x28);
  loc.cellIndex = cell;
  loc.cellBit6 = (cell >> 6) & 1;
  loc.posCode = static_cast<uint8_t>((cell << 2) | (loc.cellBit6 << 7) | (row >> 1));
  at(ram, ram::CELL_ROW) = loc.row;
  at(ram, ram::Y_OFFSET) = loc.yOffset;
  at(ram, ram::X_OFFSET) = loc.xOffset;
  at(ram, ram::CELL_INDEX) = loc.cellIndex;
  at(ram, ram::POS_CODE) = loc.posCode;
  return loc;
}

// MOVE_OBJECT ($F9EE).
Move moveObject(Ram& ram, uint8_t object) {
  Move mv{};
  uint8_t& acc = at(ram, ram::SPEED_ACC0 + object);
  mv.oldAcc = acc;
  const unsigned sum = static_cast<uint8_t>(acc << 4) + acc;
  acc = static_cast<uint8_t>(sum);
  if (sum <= 0xFF) return mv;
  mv.stepped = true;
  uint8_t& state = at(ram, ram::PACMAN_STATE + object);
  state &= 0xEF;
  mv.vertical = (state & 0x80) != 0;
  mv.stepValue = (state & 0x40) ? 0xFF : 0x01;      // left and up count down, right and down count up
  if (!mv.vertical) {
    uint8_t& x = at(ram, ram::PACMAN_X + object);
    mv.oldCoord = x;
    mv.newCoord = static_cast<uint8_t>(x + mv.stepValue);
    mv.stored = mv.newCoord != 0x02 && mv.newCoord != 0x96;
    if (mv.stored) x = mv.newCoord;
  } else {
    uint8_t& y = at(ram, ram::PACMAN_Y + object);
    mv.oldCoord = y;
    mv.newCoord = static_cast<uint8_t>(y + mv.stepValue);
    mv.stored = !(mv.newCoord & 0x80) && mv.newCoord != 0x47;
    if (mv.stored) y = mv.newCoord;
  }
  return mv;
}

// SET_DIRECTION ($F9E5).
Move setDirection(Ram& ram, uint8_t object, uint8_t direction) {
  uint8_t& state = at(ram, ram::PACMAN_STATE + object);
  state = static_cast<uint8_t>((state & 0x3F) | (direction << 6) | 0x10);
  return moveObject(ram, object);
}

namespace {
// The decimal-mode ADC of the 6502 (NMOS): two digits at a time.
Bcd bcdAdd(uint8_t a, uint8_t b, bool carryIn) {
  unsigned lo = (a & 0x0F) + (b & 0x0F) + (carryIn ? 1 : 0);
  if (lo > 9) lo += 6;
  unsigned hi = (a >> 4) + (b >> 4) + (lo > 0x0F ? 1 : 0);
  Bcd r{};
  r.overflow = ((~(a ^ b) & (a ^ (hi << 4)) & 0x80) != 0);
  if (hi > 9) hi += 6;
  r.carry = hi > 0x0F;
  r.value = static_cast<uint8_t>(((hi << 4) | (lo & 0x0F)) & 0xFF);
  return r;
}
}  // namespace

// ADD_SCORE ($FC45).
Bcd addScore(Ram& ram, const Cartridge& cart, uint8_t amount, uint8_t callerX) {
  at(ram, ram::TEMP_E1) = callerX;
  const uint8_t player = (at(ram, ram::PLAYER_MODE) & 0x80) ? 1 : 0;
  uint8_t& lo = at(ram, ram::SCORE_P1_LO + player);
  uint8_t& mid = at(ram, ram::SCORE_P1_MID + player);
  uint8_t& hi = at(ram, ram::SCORE_P1_HI + player);
  const Bcd a = bcdAdd(lo, cart.at(0xFF06 + amount), false);
  lo = a.value;
  const Bcd b = bcdAdd(mid, cart.at(0xFF0C + amount), a.carry);
  mid = b.value;
  const Bcd c = bcdAdd(hi, 0, b.carry);
  hi = c.value;
  return c;
}

// LIVES_MARKERS ($FC68).
uint8_t livesMarkers(Ram& ram, const Cartridge& cart, uint8_t lives, uint8_t offset) {
  at(ram, static_cast<uint8_t>(ram::TEMP_D2 + offset)) = lives == 0 ? 0 : 0x40;
  at(ram, static_cast<uint8_t>(ram::TEMP_D3 + offset)) = cart.at(0xFF1B + lives);
  const uint8_t last = cart.at(0xFEEC + lives);
  at(ram, static_cast<uint8_t>(ram::TEMP_D4 + offset)) = last;
  return last;
}

// TRY_UP / TRY_DOWN / TRY_LEFT / TRY_RIGHT and TRY_COMMIT.
TryResult tryDirection(Ram& ram, const Cartridge& cart, uint8_t object, uint8_t direction) {
  TryResult r{};
  r.row = at(ram, ram::CELL_ROW);
  r.cell = at(ram, ram::CELL_INDEX);
  switch (direction) {
    case 3:  // up: not from the first row; the cell must allow it (bit 6)
      if (r.row == 0) { r.exit = TryExit::AtEdge; return r; }
      r.tableByte = cart.at(0xFE13 + r.cell);
      if (!(r.tableByte & 0x40)) { r.exit = TryExit::Wall; return r; }
      break;
    case 2:  // down: not from the last row; bit 5
      if (r.row == 7) { r.exit = TryExit::AtEdge; return r; }
      r.tableByte = cart.at(0xFE13 + r.cell);
      if (!(r.tableByte & 0x20)) { r.exit = TryExit::Wall; return r; }
      break;
    case 1:  // left: the neighbour cell must have no wall (bit 7); the tunnel end of row 3 is closed
      if (r.row == 3 && r.cell == 0x3E) { r.exit = TryExit::AtEdge; return r; }
      r.tableByte = cart.at(0xFE13 + static_cast<uint8_t>(r.cell - 1));
      if (r.tableByte & 0x80) { r.exit = TryExit::Wall; return r; }
      break;
    default:  // right: the cell two further on (a cell is 4 pixels, an object 8 wide)
      if (r.row == 3 && r.cell == 0x38) { r.exit = TryExit::AtEdge; return r; }
      r.tableByte = cart.at(0xFE13 + static_cast<uint8_t>(r.cell + 2));
      if (r.tableByte & 0x80) { r.exit = TryExit::Wall; return r; }
      break;
  }
  uint8_t& state = at(ram, ram::PACMAN_STATE + object);
  r.stateBits = state & 0xC0;
  // TRY_COMMIT: a ghost does not turn straight back (the table TBL_FE67 is ((d xor 1) << 6)); Pac-Man (object 0) may
  if (object != 0 && r.stateBits == static_cast<uint8_t>((direction ^ 1) << 6)) { r.exit = TryExit::TurnBack; return r; }
  state = static_cast<uint8_t>((state & 0x3F) | (direction << 6) | 0x10);
  r.exit = TryExit::Taken;
  return r;
}

}  // namespace game
