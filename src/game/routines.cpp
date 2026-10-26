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
  cy(2 + 19 * 21 + 20 + 6);   // LDX, 19 passes taking the branch and the last one not, RTS
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

void wrapToBottom(Ram& ram, uint8_t object) { cy(4 + 2 + 2 + 4 + 2 + 3 + 4 + 6); wrapEnd(ram, object, 0x30, 0x51); }
void wrapToTop(Ram& ram, uint8_t object) { cy(4 + 2 + 2 + 4 + 2 + 4 + 6); wrapEnd(ram, object, 0xF0, 0x0B); }

// PLACE_OBJECTS ($FC21). Objects 0-4 are Pac-Man and the four ghosts (x at $B1+n, y at $B6+n, state at $BB+n).
void placeObjects(Ram& ram) {
  cy(17 + 5 * 18 + 4 * 3 + 2 + 10 + 6);   // the set-up, five passes of the loop (four take the branch), the last four stores, RTS
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
  cy(4 + 16 * 6 + 15 * 3 + 2 + 15);   // LDX, LDA, sixteen passes of the loop, the three pairs of loads and stores
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
  cy(4 + 8 + 2 + 4 + 4);   // LDA, ASL x4, CLC, ADC, STA
  if (sum <= 0xFF) { cy(3 + 6); return mv; }
  cy(2);
  mv.stepped = true;
  uint8_t& state = at(ram, ram::PACMAN_STATE + object);
  state &= 0xEF;
  mv.vertical = (state & 0x80) != 0;
  mv.stepValue = (state & 0x40) ? 0xFF : 0x01;      // left and up count down, right and down count up
  cy(2 + 4 + 2 + 4 + 2);   // LDY #0, LDA, AND, STA, ROL
  if (state & 0x40) cy(2 + 2 + 3); else cy(3 + 2);   // BPL / DEY, BMI ; INY
  cy(2);   // TYA
  if (!mv.vertical) {
    cy(2 + 4 + 2);   // BCS, ADC, CMP #2
    uint8_t& x = at(ram, ram::PACMAN_X + object);
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
    uint8_t& y = at(ram, ram::PACMAN_Y + object);
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
Move setDirection(Ram& ram, uint8_t object, uint8_t direction) {
  cy(4 + 2 + 4 + 4);   // LDA, AND, ORA, STA, then MOVE_OBJECT follows
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
  cy(3 + 2 + 3 + (player ? 2 + 2 : 3));   // STX, LDX #0, LDA, BPL (INX when player two)
  cy(2 + 2 + 4 + 4 + 4 + 4 + 4 + 4 + 4 + 2 + 4 + 2 + 3 + 6);   // SED, CLC, three load-add-store groups, CLD, LDX, RTS
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
      cy(3);
      if (r.row == 0) { cy(3 + 6); r.exit = TryExit::AtEdge; return r; }
      cy(2 + 3 + 4 + 2);
      r.tableByte = cart.at(0xFE13 + r.cell);
      if (!(r.tableByte & 0x40)) { cy(3 + 6); r.exit = TryExit::Wall; return r; }
      cy(2 + 2);
      break;
    case 2:  // down: not from the last row; bit 5
      cy(3 + 2);
      if (r.row == 7) { cy(3 + 6); r.exit = TryExit::AtEdge; return r; }
      cy(2 + 3 + 4 + 2 + 2);
      r.tableByte = cart.at(0xFE13 + r.cell);
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
      r.tableByte = cart.at(0xFE13 + static_cast<uint8_t>(r.cell - 1));
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
      r.tableByte = cart.at(0xFE13 + static_cast<uint8_t>(r.cell + 2));
      if (r.tableByte & 0x80) { cy(3 + 6); r.exit = TryExit::Wall; return r; }
      cy(2 + 2 + 3);
      break;
  }
  uint8_t& state = at(ram, ram::PACMAN_STATE + object);
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
PairResult tryPair(Ram& ram, const Cartridge& cart, uint8_t object, bool vertical, bool rngResultIsZero) {
  PairResult p{};
  const uint8_t low = vertical ? 3 : 1;   // up / left
  const uint8_t high = vertical ? 2 : 0;  // down / right
  p.dir[0] = rngResultIsZero ? low : high;
  p.dir[1] = rngResultIsZero ? high : low;
  p.res[0] = tryDirection(ram, cart, object, p.dir[0]);
  p.count = 1;
  if (p.res[0].exit == TryExit::Taken) { p.taken = true; return p; }
  p.res[1] = tryDirection(ram, cart, object, p.dir[1]);
  p.count = 2;
  p.taken = p.res[1].exit == TryExit::Taken;
  return p;
}

// ANIMATE_OBJECT ($FC8B).
AnimResult animateObject(Ram& ram, const Cartridge& cart, uint8_t object, uint8_t sprite) {
  AnimResult r{};
  uint8_t& state = at(ram, ram::PACMAN_STATE + object);
  uint8_t& y = at(ram, ram::PACMAN_Y + object);
  uint8_t& spriteY = at(ram, ram::SPRITE0_Y + sprite);
  if (!(state & 0x10)) {          // waiting: y counts down; at 0 the animation starts (y = $5B if bit 7 of the state is set, else 0)
    y = static_cast<uint8_t>(y - 1);
    if (y != 0) { r.path = AnimPath::Counting; spriteY = 0x70; r.newY = y; r.stateAfter = state; return r; }
    y = (state & 0x80) ? 0x5B : 0x00;
    state |= 0x10;
    r.path = AnimPath::CountDone;
    spriteY = 0x70;
    r.newY = y;
    r.stateAfter = state;
    return r;
  }
  const uint8_t direction = state >> 6;
  auto common = [&]() {           // L_FCB2: the part shared by the two upward cases
    r.newY = y;
    if (y == 0x0B) {
      y = 0;
      spriteY = 0x0B;
      state = static_cast<uint8_t>((state & 0x0F) | 0x80);
      r.path = AnimPath::Reached0B;
    } else if (!((y - 6) & 0x80)) {
      spriteY = y;
      r.path = AnimPath::SpriteY;
    } else {
      at(ram, ram::SPRITE0_LINES + sprite) = y;
      r.tableByte = cart.at(0xFF0D + y);
      r.ptrIndex = cart.at(0xFF19 + sprite);
      uint8_t& ptr = at(ram, ram::SPRITE0_PTR_LO + r.ptrIndex);
      r.ptrOld = ptr;
      ptr = static_cast<uint8_t>(ptr + r.tableByte);
      spriteY = 8;
      r.path = AnimPath::SpritePtr;
    }
    r.stateAfter = state;
  };
  switch (direction) {
    case 0:                       // down the outside, towards $5B
      y = static_cast<uint8_t>(y + 1);
      r.newY = y;
      if (y != 0x5B) { spriteY = y; r.path = AnimPath::RightStep; r.stateAfter = state; return r; }
      state = static_cast<uint8_t>((state & 0x0F) | 0x60);
      y = cart.at(0xFF25 + sprite);
      spriteY = 0x70;
      r.path = AnimPath::RightEnd;
      r.stateAfter = state;
      return r;
    case 1:                       // up from the bottom
      y = static_cast<uint8_t>(y + 1);
      common();
      return r;
    case 2:                       // back into the maze from below, towards $51
      y = static_cast<uint8_t>(y - 1);
      r.newY = y;
      if (y != 0x51) { spriteY = y; r.path = AnimPath::DownStep; r.stateAfter = state; return r; }
      y = 0x46;
      state = static_cast<uint8_t>((state & 0x0F) | 0xC0);
      spriteY = 0x70;
      r.path = AnimPath::DownEnd;
      r.stateAfter = state;
      return r;
    default:                      // 3: up the outside; below 0 the next phase starts
      y = static_cast<uint8_t>(y - 1);
      if (y & 0x80) {
        state = static_cast<uint8_t>((state & 0x0F) | 0xA0);
        y = cart.at(0xFF25 + sprite);
        spriteY = 0x70;
        r.path = AnimPath::UpEnd;
        r.newY = y;
        r.stateAfter = state;
        return r;
      }
      common();
      return r;
  }
}

// GHOST_DECIDE ($FA26).
namespace {

struct Decider {
  Ram& ram;
  const Cartridge& cart;
  uint8_t n;
  bool done = false;      // a turn was taken (or the opening entered): the decision is over
  bool unsupported = false;

  uint8_t& state() { return at(ram, ram::PACMAN_STATE + n); }
  uint8_t x() { return at(ram, ram::PACMAN_X + n); }
  uint8_t y() { return at(ram, ram::PACMAN_Y + n); }

  void turn(uint8_t direction) {
    if (done) return;
    if (tryDirection(ram, cart, n, direction).exit == TryExit::Taken) done = true;
  }
  void pair(bool vertical) {
    if (done) return;
    const uint8_t r = rngStep(ram);
    if (tryPair(ram, cart, n, vertical, r == 0).taken) done = true;
  }
  bool rngIsZero() { return rngStep(ram) == 0; }

  // The random chains. The code has one chain per direction, entered at the one for the direction the ghost is going and each falling through into the
  // next in the order up, right, left, down. A chain: a random step, if its result is 0 try to go straight on; then a random pair of the other axis
  // (vertical pair for the horizontal chains, horizontal pair for the vertical ones); then straight on again.
  void wander() {
    static const uint8_t order[4] = {3, 0, 1, 2};      // up, right, left, down
    const uint8_t d = state() >> 6;                    // 0 right, 1 left, 2 down, 3 up
    for (int k = (d == 3) ? 0 : d + 1; k < 4 && !done; ++k) {
      const uint8_t dir = order[k];
      if (rngIsZero()) turn(dir);
      pair(dir == 0 || dir == 1);                      // right and left chains use the vertical pair; up and down the horizontal pair
      turn(dir);                                       // the second straight-on try: marked "never run" for up and down in the listing
    }
    if (!done) unsupported = true;                     // the code then falls into TRY_PAIR_1 itself: never run in the recordings
  }

  void chase(uint8_t targetX, uint8_t targetY) {
    if (x() < targetX) turn(0);
    else if (x() > targetX) turn(1);
    if (done) return;
    if (y() < targetY) turn(2);
    else turn(3);
    if (done) return;
    wander();
  }
};

}  // namespace

bool ghostDecide(Ram& ram, const Cartridge& cart, uint8_t object) {
  Decider g{ram, cart, object};
  if ((at(ram, ram::X_OFFSET) | at(ram, ram::Y_OFFSET)) != 0) return true;
  if (g.state() & 0x10) return true;
  if (at(ram, ram::ATTRACT_SHIFT) != 0) { g.wander(); return !g.unsupported; }
  if (g.state() & 0x08) {                                          // eyes: back to the pen door
    at(ram, ram::TEMP_D3) = 0x58;
    at(ram, ram::TEMP_D2) = 0x1F;
    g.chase(0x58, 0x1F);
    return !g.unsupported;
  }
  if (g.x() == 0x4C && (at(ram, ram::RNG_STATE) & 0x0F) == 0) {    // the middle column: sometimes into the opening
    if (g.y() == 0x00) { wrapToTop(ram, object); return true; }
    if (g.y() == 0x46) { wrapToBottom(ram, object); return true; }
  }
  const uint8_t pacX = at(ram, ram::PACMAN_X), pacY = at(ram, ram::PACMAN_Y);
  at(ram, ram::TEMP_D3) = pacX;
  at(ram, ram::TEMP_D2) = pacY;
  if (at(ram, ram::PILL_FLAGS) & 0x80) {                           // scared: x mirrored, y still Pac-Man's own
    const uint8_t mirrored = static_cast<uint8_t>(~static_cast<uint8_t>(pacX + 0x60));
    at(ram, ram::TEMP_D3) = mirrored;
    g.chase(mirrored, pacY);
    return !g.unsupported;
  }
  uint8_t& timer = at(ram, static_cast<uint8_t>(ram::GHOST_TIMER0 - 1 + object));
  timer = static_cast<uint8_t>(timer - 1);
  if (timer != 0) { g.chase(pacX, pacY); return !g.unsupported; }
  timer = cart.at(0xFE6F + (at(ram, ram::LEVEL_SPEED) >> 1));
  if ((object >> 1) & 1) timer = static_cast<uint8_t>(timer + 1);
  g.wander();
  return !g.unsupported;
}

// GHOST_UPDATE ($F9AA).
bool ghostUpdate(Ram& ram, const Cartridge& cart, uint8_t object) {
  uint8_t& state = at(ram, ram::PACMAN_STATE + object);
  if ((state & 0x08) && at(ram, ram::PACMAN_Y + object) == 0x1E) {
    const uint8_t x = at(ram, ram::PACMAN_X + object);
    if (x == 0x58) { setDirection(ram, object, 1); return true; }                 // at the pen door: left, and move
    if (x == 0x4C) {                                                              // above the pen
      if (at(ram, ram::PILL_FLAGS) & 0x80) return true;                           // scared: wait
      state &= 0xF7;                                                              // a ghost again
      setDirection(ram, object, 0);
      return true;
    }
  }
  if (state & 0x20) return true;                                                  // in an opening animation
  locateObject(ram, object);
  if (!ghostDecide(ram, cart, object)) return false;
  if (!(state & 0x20)) moveObject(ram, object);
  return true;
}

// SOUND_START ($FF43).
uint8_t soundStart(Ram& ram, const Cartridge& cart, uint8_t sound) {
  cy(4 + 3);   // LDA, AND
  const uint8_t blocked = cart.at(0xFFE9 + sound) & at(ram, ram::SOUND_ACTIVE);
  if (blocked != 0) { cy(3 + 6); return blocked; }
  cy(2 + 4 + 3 + 4 + 3 + 6);
  at(ram, ram::SOUND_ACTIVE) = cart.at(0xFE6E + sound);
  const uint8_t length = cart.at(0xFFED + sound);
  at(ram, ram::SOUND_TIMER) = length;
  return length;
}

// SOUND_EFFECTS ($FF55) and SOUND_WRITE ($FF8B).
void soundEffects(Ram& ram, SoundChip& out) {
  const uint8_t active = at(ram, ram::SOUND_ACTIVE);
  uint8_t volume, pitch, control;
  if (active & 0x01) {                                   // ghost eaten: a sliding tone
    cy(24);
    control = 4;
    pitch = at(ram, ram::SOUND_TIMER) & 0x1F;
    volume = pitch ^ 0x0F;
  } else if (active & 0x06) {                            // power pill (bit 1) and bonus item (bit 2): the steady tone
    cy((active & 0x02) ? 22 : 30);
    control = pitch = volume = 0x0D;
  } else if (active & 0x08) {                            // wafer
    const uint8_t timer = at(ram, ram::SOUND_TIMER);
    cy(timer != 0 ? 33 : 37);
    if (timer != 0) { control = 9; pitch = 9; volume = timer; }
    else { control = pitch = volume = 0; }              // a timer of 0 cannot occur; the original then writes zeros
  } else {
    cy(29);
    return;
  }
  at(ram, ram::TEMP_D3) = control;
  uint8_t& timer = at(ram, ram::SOUND_TIMER);
  timer = static_cast<uint8_t>(timer - 1);
  cy(6);   // DEC SOUND_TIMER,X
  if (timer == 0) { cy(2 + 2 + 3 + 6); at(ram, ram::SOUND_ACTIVE) = 0; return; }
  cy(3);   // BNE
  cy(4); out.write(chip::AUDV0, volume);
  cy(3 + 4); out.write(chip::AUDC0, control);
  cy(4); out.write(chip::AUDF0, pitch);
  cy(6);
}

// SOUND_MUSIC ($FF94).
void soundMusic(Ram& ram, const Cartridge& cart, SoundChip& out) {
  uint8_t& flags = at(ram, ram::GAME_FLAGS);
  const uint8_t timer = at(ram, ram::EVENT_TIMER);
  cy(2 + 3);   // LDA #8, BIT GAME_FLAGS
  if (flags & 0x40) {                                   // dying: a falling tone
    cy(2 + 3 + 3);
    out.write(chip::AUDV0, timer);
    cy(2 + 3);
    out.write(chip::AUDF0, timer >> 1);
    cy(2 + 3);
    out.write(chip::AUDC0, 4);
    cy(6);
    return;
  }
  cy(3);   // BVC taken
  if (flags & 0x08) {                                   // the start tune
    cy(2 + 3);   // BEQ not taken, LDA EVENT_TIMER
    uint8_t a = timer;
    if (timer == 0) { cy(2 + 2 + 3 + 3); flags &= 0xF7; a = flags; }       // the tune ends; the note number is then taken from the flags themselves (as the original does)
    else cy(3);
    const uint8_t note = (a >> 3) & 7;
    cy(6 + 2 + 2 + 2);   // LSR x3, AND, TAX, CPX
    if (note >= 4) { cy(3 + 6); return; }
    cy(2);
    cy(4 + 3);
    out.write(chip::AUDF0, cart.at(0xFFF1 + note));
    cy(4 + 3);
    out.write(chip::AUDF1, cart.at(0xFFF5 + note));
    cy(2 + 3);
    out.write(chip::AUDC0, 4);
    cy(3);
    out.write(chip::AUDC1, 4);
    cy(2 + 3);
    out.write(chip::AUDV0, 8);
    cy(3);
    out.write(chip::AUDV1, 8);
    cy(6);
    return;
  }
  cy(3 + 3 + 2);   // BEQ taken, LDA PILL_FLAGS, ASL
  if (!((at(ram, ram::PILL_FLAGS) << 1) & 0x80)) { cy(3 + 6); return; }   // scared (first part): the sweep on channel 1
  cy(2);
  at(ram, ram::TEMP_D3) = 4;
  const uint8_t pitch = (at(ram, ram::FRAME_COUNTER) >> 1) & 0x0F;
  cy(2 + 3 + 3 + 2 + 2 + 2 + 2 + 2 + 3);   // LDA #4, STA, LDA, LSR, AND, TAY, EOR, LSR, JMP
  cy(4); out.write(chip::AUDV1, (pitch ^ 0x0F) >> 1);
  cy(3 + 4); out.write(chip::AUDC1, 4);
  cy(4); out.write(chip::AUDF1, pitch);
  cy(6);
}

// PACMAN_KEEP_GOING ($F793).
KeepGoing pacmanKeepGoing(Ram& ram, const Cartridge& cart) {
  cy(3 + 3);   // LDA, ORA
  if ((at(ram, ram::Y_OFFSET) | at(ram, ram::X_OFFSET)) != 0) { cy(2 + 6); return KeepGoing::NotOnCell; }
  cy(3 + 4 + 2);   // BEQ taken, LDA OBJ_STATE,X, ROL
  const uint8_t direction = at(ram, ram::PACMAN_STATE) >> 6;      // 0 right, 1 left, 2 down, 3 up: the same code the TRY routines use
  // BCS / BPL and the JSR of the TRY routine; the JMP to the PLA, PLA when the turn is refused
  switch (direction) {
    case 0: cy(2 + 3 + 6); break;
    case 1: cy(2 + 2 + 6); break;
    case 2: cy(3 + 3 + 6); break;
    default: cy(3 + 2 + 6); break;
  }
  const bool taken = tryDirection(ram, cart, 0, direction).exit == TryExit::Taken;
  if (!taken) cy((direction == 2 ? 0 : 3) + 4 + 4);   // JMP L_F7B8 (not after the last case), PLA, PLA: the caller is left too
  return taken ? KeepGoing::Moved : KeepGoing::Stands;
}

// SCORE_DIGITS ($F3EE) and CLEAR_GRAPHICS ($F483).
void scoreDigits(Ram& ram, const Cartridge& cart, TiaChip& chip, uint8_t offset) {
  auto buffer = [&](uint8_t slot) -> uint8_t& { return at(ram, static_cast<uint8_t>(ram::TEMP_D5 + slot)); };
  auto scoreByte = [&](uint8_t y) { return at(ram, static_cast<uint8_t>(ram::SCORE_P1_LO + y)); };
  uint8_t& seen = at(ram, ram::TEMP_E1);      // how many digits that are not zero were met (leading zeros are blank)
  uint8_t& slotMemory = at(ram, ram::TEMP_E2);
  seen = 0;
  uint8_t slot = 8;
  uint8_t y = offset;
  auto digitShape = [&](uint8_t digit) {
    if (digit != 0) ++seen;
    if (seen == 0) digit = 10;
    return cart.at(0xFEF6 + digit);
  };
  bool first = true;
  for (;;) {
    if (!first) {                                           // L_F3F6: the high nibble of the next lower byte
      buffer(slot) = digitShape(scoreByte(y) >> 4);
      slot = static_cast<uint8_t>(slot - 2);
      if (slot == 4) chip.write(chip::COLUBK, 0);
      else if (slot == 0) chip.write(chip::COLUBK, at(ram, ram::TEMP_D2));
    }
    first = false;
    slotMemory = slot;                                      // L_F422: the low nibble
    buffer(slot) = digitShape(scoreByte(y) & 0x0F);
    slot = static_cast<uint8_t>(slot - 2);
    slotMemory = slot;
    y = static_cast<uint8_t>(y - 2);
    if (y & 0x80) break;
  }
  if (seen == 0) buffer(0) = 0xAC;                          // a score of 0 shows one 0
  for (uint8_t i = 1; i <= 9; i += 2) buffer(i) = 0xFE;     // the high bytes of the five shape pointers
  auto shapeByte = [&](uint8_t base, uint8_t row) {
    const uint16_t pointer = static_cast<uint16_t>((buffer(static_cast<uint8_t>(base + 1)) << 8) | buffer(base));
    return cart.at(static_cast<uint16_t>(pointer + row));
  };
  seen = 6;
  for (;;) {
    const uint8_t row = seen;
    chip.write(chip::GRP0, 0);
    chip.write(chip::GRP0, 0);
    chip.write(chip::GRP1, shapeByte(8, row));              // pointer at $DD
    chip.write(chip::GRP0, shapeByte(6, row));              // $DB
    const uint8_t third = shapeByte(4, row);                // $D9
    slotMemory = third;
    const uint8_t fourth = shapeByte(2, row);               // $D7
    const uint8_t fifth = shapeByte(0, row);                // $D5
    chip.write(chip::GRP1, third);
    chip.write(chip::GRP0, fourth);
    chip.write(chip::GRP1, fifth);
    chip.write(chip::GRP0, fifth);
    seen = static_cast<uint8_t>(seen - 1);
    if (seen & 0x80) break;
  }
  chip.write(chip::GRP0, 0);                                // CLEAR_GRAPHICS
  chip.write(chip::GRP1, 0);
  chip.write(chip::GRP0, 0);
  chip.write(chip::GRP1, 0);
}

// The top logic of the frame ($F87E-$F9A7).
namespace {

// `lead` is the number of processor cycles from the end of the write that switched the vertical sync off to the first instruction of this step.
void clearTitleSelectStep(Ram& ram, TiaChip& chip, long lead) {        // L_F8C8: the game number goes up by one
  at(ram, ram::GAME_SELECT) = static_cast<uint8_t>(at(ram, ram::GAME_SELECT) + 1);
  at(ram, ram::PACMAN_Y) = 0x60;
  at(ram, ram::GAME_FLAGS) = 0x10;
  at(ram, ram::PLAYER_MODE) ^= 0x40;                        // one player / two players alternately
  clearScores(ram);
  lead += 5 + 2 + 3 + 2 + 3 + 3 + 2 + 3 + 6 + (4 + 5 * 9 + 8 + 3 + 3 + 6);   // INC .. STA PLAYER_MODE, JSR, CLEAR_SCORES
  chip.writeAtCycle(lead + 3, chip::AUDV0, 0);
  chip.writeAtCycle(lead + 3 + 3, chip::AUDV1, 0);
  at(ram, ram::SOUND_ACTIVE) = 0;
  at(ram, ram::ATTRACT_SHIFT) = 1;
  at(ram, ram::FRAME_COUNTER) = 1;
  uint8_t select = at(ram, ram::GAME_SELECT);
  if (select == 0x12) select = 2;
  at(ram, ram::GAME_SELECT) = select;
  uint8_t shown = static_cast<uint8_t>(((select >> 1) - 2) & 7);   // the game number shown on the score line
  if (shown == 0) shown = 8;
  at(ram, ram::SCORE_P1_LO) = shown;
  at(ram, ram::SCORE_P2_LO) = shown;
}

void startGame(Ram& ram) {            // F890: a new game
  at(ram, ram::RNG_STATE) = 0xFF;
  clearScores(ram);
  at(ram, ram::LIVES_P2) = 0;
  at(ram, ram::ATTRACT_SHIFT) = 0;
  at(ram, ram::GAME_FLAGS) = 0x08;
  at(ram, ram::SPEED_ACC0) = (at(ram, ram::GAME_SELECT) >= 8) ? 0x0B : 0x09;    // Pac-Man's speed seed (F10)
  newGameSetup(ram);
  at(ram, ram::PLAYER_MODE) &= 0x7F;
  if (at(ram, ram::PLAYER_MODE) & 0x40) {                   // two players: the second block is set up too
    swapPlayers(ram);
    newGameSetup(ram);
    at(ram, ram::LIVES_P2) = static_cast<uint8_t>(at(ram, ram::LIVES_P2) + 1);
  }
}

}  // namespace

bool topLogic(Ram& ram, const Cartridge& cart, const ConsoleInput& in, TiaChip& chip) {
  at(ram, ram::COLOUR_SCORE_P1) = at(ram, ram::SCORE_COLOUR);
  const bool start = (at(ram, ram::ATTRACT_SHIFT) != 0 && in.fire) || in.reset;
  if (start) {
    startGame(ram);
    at(ram, ram::SELECT_REPEAT) = 0;
  } else if (!in.select) {
    at(ram, ram::SELECT_REPEAT) = 0;
  } else {
    const long a0 = at(ram, ram::ATTRACT_SHIFT) == 0 ? 12 : 16;   // the cycles from the end of the sync to the Select test (title screen: the fire test too)
    if (at(ram, ram::SELECT_REPEAT) == 0) clearTitleSelectStep(ram, chip, a0 + 18);
    for (;;) {                                               // L_F8FF: held down, a step every 32 frames
      at(ram, ram::SELECT_REPEAT) = static_cast<uint8_t>(at(ram, ram::SELECT_REPEAT) + 1);
      if ((at(ram, ram::SELECT_REPEAT) & 0x1F) != 0) break;
      clearTitleSelectStep(ram, chip, a0 + 35);
    }
  }
  // L_F90D: the difficulty switch of the player in turn becomes GAME_FLAGS bit 5
  at(ram, ram::GAME_FLAGS) &= 0xDF;
  const bool playerTwo = (at(ram, ram::PLAYER_MODE) & 0x80) != 0;
  if (playerTwo ? in.rightDifficultyA : in.leftDifficultyA) at(ram, ram::GAME_FLAGS) |= 0x20;
  at(ram, ram::COLOUR_SPRITE1) = 6;
  uint8_t object = at(ram, ram::FRAME_COUNTER) & 3;
  uint8_t colour0 = object;
  const uint8_t pill = at(ram, ram::PILL_FLAGS);
  if (pill & 0x80) colour0 = (pill & 0x40) ? 4 : 5;
  at(ram, ram::COLOUR_SPRITE0) = colour0;
  ++object;                                                  // 1-4: the ghost of this frame
  bool update;
  if (at(ram, static_cast<uint8_t>(ram::PACMAN_STATE + object)) & 0x08) {   // eyes: always updated, with another picture
    at(ram, ram::SPRITE0_PTR_LO) = 0xC6;
    update = true;
  } else {
    update = !(at(ram, ram::EAT_PAUSE) & 0x80);
  }
  if (update && !(at(ram, ram::GAME_FLAGS) & 0x80)) {
    for (int i = 0; i < 3; ++i)
      if (!ghostUpdate(ram, cart, object)) return false;
  }
  at(ram, ram::SPRITE_X) = at(ram, static_cast<uint8_t>(ram::PACMAN_X + object));
  at(ram, ram::SPRITE0_Y) = static_cast<uint8_t>(at(ram, static_cast<uint8_t>(ram::PACMAN_Y + object)) + 0x0B);
  if (at(ram, static_cast<uint8_t>(ram::PACMAN_STATE + object)) & 0x20) animateObject(ram, cart, object, 0);
  if (at(ram, ram::PACMAN_STATE) & 0x20) animateObject(ram, cart, 0, 1);
  // the six colour values: the colour table of the cartridge ($FF2B; $FF36 when the mask says black and white), the title shift, the mask
  const bool colourTv = (at(ram, ram::COLOUR_MASK) & 0x80) != 0;
  const uint16_t table = colourTv ? 0xFF2B : 0xFF36;
  at(ram, ram::TEMP_D3) = 0xFF;
  at(ram, ram::TEMP_D2) = colourTv ? 0x2B : 0x36;
  for (int x = 5; x >= 0; --x) {
    uint8_t& entry = at(ram, static_cast<uint8_t>(ram::COLOUR_BG + x));
    entry = (cart.at(static_cast<uint16_t>(table + entry)) ^ at(ram, ram::ATTRACT_SHIFT)) & at(ram, ram::COLOUR_MASK);
  }
  chip.write(chip::COLUP1, at(ram, ram::COLOUR_SPRITE1));
  chip.write(chip::COLUP0, at(ram, ram::COLOUR_SPRITE0));
  return true;
}

// The bottom logic of the frame ($F4E7-$F7CE). It is written along the listing of the original, with the processor cycles of every instruction counted
// along the path taken (Stage F24.2): the sound registers are written at the end of it, and the exact moment decides which sound samples they change.
// The numbers in the cy() calls are the cycles of the original instructions (2 for a load of a constant, 3 for a zero-page access, 4 for an indexed or
// absolute one, a taken branch 3, a branch not taken 2, one more when a taken branch crosses a page, JSR and RTS 6) as listed by tools/f24_cycles.py.
namespace {

struct Bottom {
  Ram& ram;
  const Cartridge& cart;
  const FrameInput& in;
  TiaChip& chip;

  uint8_t& flags() { return at(ram, ram::GAME_FLAGS); }
  uint8_t& pill() { return at(ram, ram::PILL_FLAGS); }
  uint8_t& state() { return at(ram, ram::PACMAN_STATE); }
  uint8_t wall(unsigned i) { return cart.at(0xFE13 + (i & 0xFF)); }

  // L_F7C3: the volumes are silenced, then the sound effects (channel 0) and the long sounds (channel 1 for the sweep)
  void sounds() {
    cy(2 + 3);
    chip.write(chip::AUDV0, 0);
    cy(3);
    chip.write(chip::AUDV1, 0);
    cy(6);
    soundEffects(ram, chip);
    cy(2 + 6);
    soundMusic(ram, cart, chip);
  }
  void finish() {                                           // L_F7BA: the mirror bit of Pac-Man's picture
    cy(3 + 2);
    if (!(state() & 0x01)) cy(3);
    else { cy(2 + 2 + 3); chip.write(chip::REFP1, 0x08); }
    sounds();
  }

  enum class Flow { Finish, KeepGoing };

  // L_F6AA: the new map when nothing is left; returns true if the map was refilled (the original then goes on at L_F7BA)
  bool newMapCheck() {
    cy(2 + 3 + 2);
    uint8_t left = pill() & 0x3C;
    for (uint8_t i = 0; i < 16; ++i) left |= at(ram, static_cast<uint8_t>(ram::MAZE_00 + i));
    cy(16 * 6 + 15 * 3 + 2);   // sixteen passes: ORA, DEX, BPL (taken fifteen times)
    cy(2);   // TAX
    if (left != 0) { cy(3); return false; }
    cy(2);
    cy(3 + 2);
    if (at(ram, ram::LIVES) == 9) cy(3); else { cy(2 + 5); at(ram, ram::LIVES) = static_cast<uint8_t>(at(ram, ram::LIVES) + 1); }
    cy(3 + 2);
    if (at(ram, ram::LEVEL_SPEED) == 0x0E) cy(3); else { cy(2 + 5); at(ram, ram::LEVEL_SPEED) = static_cast<uint8_t>(at(ram, ram::LEVEL_SPEED) + 1); }
    cy(6);
    newMaze(ram);
    cy(3);
    return true;
  }

  void eatWafer() {                                         // L_F666 (Pac-Man not in an opening)
    cy(3);
    if (at(ram, ram::Y_OFFSET) != 0) { cy(3); return; }
    cy(2 + 3 + 4);
    uint8_t cell = at(ram, ram::CELL_INDEX);
    uint8_t t = wall(cell);
    cy(3 + 2);
    if (at(ram, ram::PACMAN_X) == 0x4C) cy(3);
    else {
      cy(2 + 3);
      if (!(state() & 0x80)) cy(3);
      else { cy(2 + 2 + 4); t |= wall(static_cast<uint8_t>(cell + 1)); }
    }
    cy(2);   // LSR
    if (!(t & 1)) { cy(3); return; }
    cy(2);
    cy(2 + 2 + 3 + 2 + 2 + 4 + 2 + 2 + 2 + 2 + 4 + 2 + 4);   // LSR, TAX, LDA CELL_ROW, ROL, TAY, LDA, TAY, TXA, AND, TAX, LDA, TAX, AND
    const uint8_t index = t >> 2;
    const uint8_t byte = cart.at(0xFE79 + ((at(ram, ram::CELL_ROW) << 1) | ((t >> 1) & 1)));
    const uint8_t mask = cart.at(0xFE6A + (index & 7));
    uint8_t& maze = at(ram, static_cast<uint8_t>(ram::MAZE_00 + byte));
    if (!(maze & mask)) { cy(3); return; }
    cy(2);
    cy(2 + 2 + 4 + 5 + 2 + 6);   // TXA, EOR, AND, STA, LDY #4, JSR ADD_SCORE
    maze &= static_cast<uint8_t>(~mask);
    addScore(ram, cart, 4, mask);                            // the original keeps the mask in X: TEMP_E1 gets it
    cy(2 + 6);
    soundStart(ram, cart, 0);
  }

  // L_F61E: the joystick, Pac-Man in an opening or in the maze
  void pacman() {
    cy(4 + 3);
    uint8_t joy = in.swcha;
    if (at(ram, ram::PLAYER_MODE) & 0x80) cy(3);
    else { cy(2 + 8); joy >>= 4; }
    joy = static_cast<uint8_t>(~joy) & 0x0F;
    cy(2 + 2 + 3);
    at(ram, ram::JOY_DIRECTION) = joy;
    if (joy == 0) cy(3);
    else { cy(2 + 2 + 3); at(ram, ram::TICK256) = 0; }
    cy(3 + 2);
    if (state() & 0x20) {                                   // in an opening: only a turn round
      cy(2);
      cy(3 + 2);
      const bool up = (joy & 1) != 0, down = (joy & 2) != 0;
      bool toL644 = false;
      if (!up) {
        cy(3 + 2);                                           // BCC taken, LSR
        if (!down) cy(3);
        else {
          cy(2 + 3);
          if (state() & 0x80) { cy(3); toL644 = true; } else cy(2 + 3);
        }
      } else {
        cy(2 + 3);
        if (state() & 0x80) cy(3); else { cy(2); toL644 = true; }
      }
      if (toL644) {                                          // L_F644
        cy(2);
        if (state() & 0x10) { cy(3 + 2 + 3); state() ^= 0x80; cy(3 + 3); }
        else {
          cy(2 + 2 + 3 + 2 + 3 + 2 + 3 + 3);
          at(ram, ram::PACMAN_Y) = static_cast<uint8_t>((0x3F ^ at(ram, ram::PACMAN_Y)) & 0x3F);
          state() ^= 0xC0;
        }
      }
      cy(3);   // JMP L_F7BA
      return finish();
    }
    cy(3);
    eatWafer();
    if (newMapCheck()) return finish();
    if (joystickAndMove() == Flow::KeepGoing) {
      cy(6);
      if (pacmanKeepGoing(ram, cart) != KeepGoing::Stands) { cy(6); moveObject(ram, 0); cy(3); }
    }
    finish();
  }

  // L_F6CE: the pressed directions (up 1, down 2, left 4, right 8; the TEMP_D2 copy is what the original works on)
  Flow joystickAndMove() {
    uint8_t a = at(ram, ram::JOY_DIRECTION), y = 0;
    bool carry = false;
    cy(2 + 3 + 3);
    at(ram, ram::TEMP_D2) = a;
    if (a == 0) { cy(2 + 3); return Flow::KeepGoing; }
    cy(3 + 3);
    y = state();
    enum { L_F6DD, L_F6F0, L_F700, L_F70A, L_F714, L_F71B, L_F725, L_F72D, L_F751, L_F75B, L_F766, L_F770, L_F775, L_F779, L_F77B, L_F781 } at_;
    if (!(y & 0x80)) { cy(3 + 1); at_ = L_F72D; } else { cy(2); at_ = L_F6DD; }
    for (;;) {
      switch (at_) {
        case L_F6DD:
          a = (at(ram, ram::TEMP_D2) >> 2) & 3;
          cy(2 + 2 + 2);
          if (a == 0) { cy(3 + 1); at_ = L_F725; break; }
          cy(2);
          carry = (a & 1) != 0;
          a >>= 1;
          cy(2 + 3);
          a = at(ram, ram::Y_OFFSET);
          if (a != 0) { cy(3 + 1); at_ = L_F725; break; }
          cy(2 + 3);
          a = at(ram, ram::X_OFFSET);
          if (a == 0) { cy(3); at_ = L_F6F0; break; }
          cy(2);
          if (!carry) { cy(3 + 1); at_ = L_F71B; break; }
          cy(2 + 3 + 1);
          at_ = L_F700;
          break;
        case L_F6F0:
          cy(3);
          y = at(ram, ram::CELL_INDEX);
          if (!carry) { cy(3 + 1); at_ = L_F70A; break; }
          cy(2 + 3 + 2);
          if (at(ram, ram::POS_CODE) == 0xF9) { cy(3 + 1); at_ = L_F725; break; }
          cy(2 + 2 + 4);
          --y;
          if (wall(y) & 0x80) { cy(3); at_ = L_F725; break; }
          cy(2);
          at_ = L_F700;
          break;
        case L_F700:
          cy(2 + 3 + 3 + 2 + 3);
          state() |= 0x01;
          y = 1;
          at_ = L_F77B;
          break;
        case L_F70A:
          cy(3 + 2);
          if (at(ram, ram::CELL_ROW) != 3) cy(3);
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
          if (wall(y) & 0x80) { cy(3); at_ = L_F725; break; }
          cy(2);
          at_ = L_F71B;
          break;
        case L_F71B:
          cy(2 + 3 + 3 + 2 + 3);
          state() &= 0xFE;
          y = 0;
          at_ = L_F77B;
          break;
        case L_F725:
          cy(3 + 2);
          a = at(ram, ram::TEMP_D2) & 0xF3;
          if (a == 0) { cy(3); return Flow::KeepGoing; }
          cy(2 + 3);
          at(ram, ram::TEMP_D2) = a;
          at_ = L_F72D;
          break;
        case L_F72D:
          cy(3 + 2);
          a = at(ram, ram::TEMP_D2) & 3;
          if (a == 0) { cy(3); at_ = L_F781; break; }
          cy(2 + 2);
          carry = (a & 1) != 0;
          cy(3);
          if (at(ram, ram::X_OFFSET) != 0) { cy(3); at_ = L_F781; break; }
          cy(2 + 3 + 2);
          if (at(ram, ram::PACMAN_X) != 0x4C) { cy(3); at_ = L_F75B; break; }
          cy(2 + 3);
          a = at(ram, ram::PACMAN_Y);
          if (a == 0) { cy(3); at_ = L_F751; break; }
          cy(2 + 2);
          if (a != 0x46) { cy(3); at_ = L_F75B; break; }
          cy(2 + 3 + 2 + 2);
          carry = (at(ram, ram::TEMP_D2) & 0x02) != 0;
          if (!carry) { cy(3); at_ = L_F75B; break; }
          cy(2 + 6);
          wrapToBottom(ram, 0);
          cy(3);
          return Flow::Finish;
        case L_F751:
          cy(3 + 2);
          carry = (at(ram, ram::TEMP_D2) & 0x01) != 0;
          if (!carry) { cy(3); at_ = L_F75B; break; }
          cy(2 + 6);
          wrapToTop(ram, 0);
          cy(3);
          return Flow::Finish;
        case L_F75B:
          cy(3 + 2 + 3);
          carry = (at(ram, ram::TEMP_D2) & 0x01) != 0;
          a = at(ram, ram::Y_OFFSET);
          if (a == 0) { cy(3); at_ = L_F766; break; }
          cy(2);
          if (!carry) { cy(3); at_ = L_F779; break; }
          cy(2 + 3);
          at_ = L_F770;
          break;
        case L_F766:
          cy(3 + 4);
          y = at(ram, ram::CELL_INDEX);
          a = wall(y);
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
          setDirection(ram, 0, y);
          cy(3);
          return Flow::Finish;
        case L_F781:
          cy(3 + 2 + 3 + 3);
          at(ram, ram::TEMP_D2) = at(ram, ram::TEMP_D2) & 0xFC;
          at_ = L_F6DD;
          break;
      }
    }
  }

  void run() {
    cy(3);                                                 // LDX ATTRACT_SHIFT
    if (at(ram, ram::ATTRACT_SHIFT) != 0) { cy(3 + 3); return sounds(); }   // the title screen: sound only
    cy(2 + 6);
    locateObject(ram, 0);
    cy(3);
    if (at(ram, ram::EAT_PAUSE) & 0x80) {                  // the pause after a ghost was eaten
      cy(2 + 5 + 3);
      at(ram, ram::EAT_PAUSE) = static_cast<uint8_t>(at(ram, ram::EAT_PAUSE) - 1);
      return sounds();
    }
    cy(3);
    uint8_t& timer = at(ram, ram::EVENT_TIMER);            // L_F4F7: the event timer runs every second frame
    cy(3);
    bool skipToF573 = false;
    if (timer == 0) cy(3 + 1);
    else {
      cy(2 + 3 + 2);
      if (!(at(ram, ram::FRAME_COUNTER) & 1)) cy(3);
      else {
        cy(2 + 5);
        timer = static_cast<uint8_t>(timer - 1);
        if (timer != 0) cy(3);
        else {
          cy(2 + 3 + 3);
          bool flagsPart = true;
          if (!(pill() & 0x80)) cy(3);
          else {
            cy(2);
            if (pill() & 0x40) { cy(2 + 2 + 3 + 2 + 3 + 3); pill() &= 0xBF; timer = 0x20; flagsPart = false; }   // flashing part of the scare
            else { cy(3 + 2 + 3); pill() &= 0x7C; }                                                            // the scare is over
          }
          if (flagsPart) {                                  // L_F51A
            cy(3 + 2);
            if (flags() & 0x40) {                           // the death delay is over
              cy(2 + 3 + 3);
              flags() &= 0xBF;
              cy(6);
              placeObjects(ram);
              cy(3);
              if (!(at(ram, ram::PLAYER_MODE) & 0x40)) cy(3);
              else {
                cy(2 + 3);
                if (at(ram, ram::LIVES_P2) == 0) cy(3);
                else {                                      // two players: the turn passes
                  cy(2 + 6);
                  swapPlayers(ram);
                  cy(3 + 2 + 3);
                  at(ram, ram::PLAYER_MODE) ^= 0x80;
                }
              }
              cy(3 + 3);
              if ((at(ram, ram::LIVES) | at(ram, ram::LIVES_P2)) != 0) {
                cy(2 + 5 + 2 + 3 + 3 + 3);
                at(ram, ram::LIVES) = static_cast<uint8_t>(at(ram, ram::LIVES) - 1);
                flags() |= 0x08;
              } else {                                      // game over
                cy(3 + 3 + 2 + 3 + 3 + 3);
                flags() = 0;
                at(ram, ram::ATTRACT_SHIFT) = 0x5F;
                at(ram, ram::PACMAN_Y) = 0x5F;
                return sounds();
              }
            } else {
              cy(3 + 3);
              if (!(flags() & 0x80)) cy(3);
              else { cy(2 + 2 + 3); flags() &= 0x7F; }      // the freeze is over
            }
          }
        }
      }
    }
    // L_F55B
    cy(3);
    if (flags() & 0x80) {                                   // frozen: the ghosts' speed set-up
      cy(2 + 3 + 2 + 2 + 2);
      const uint8_t level = at(ram, ram::LEVEL_SPEED);
      const uint8_t below = static_cast<uint8_t>(level - 1);
      for (uint8_t x = 4; x >= 1; x = static_cast<uint8_t>(x - 1)) {
        at(ram, static_cast<uint8_t>(ram::SPEED_ACC0 + x)) = (x & 1) ? below : level;
        at(ram, static_cast<uint8_t>(ram::GHOST_TIMER0 - 1 + x)) = below;
      }
      cy(45 + 4 + 3);
      return sounds();
    }
    cy(3);
    (void)skipToF573;
    // L_F573: the bonus item
    cy(3 + 3 + 2);
    if (in.bonusTouched) {
      cy(2 + 2 + 6);
      addScore(ram, cart, 6, 0);
      cy(2 + 6);
      soundStart(ram, cart, 1);
      cy(3 + 2 + 3 + 2 + 3 + 3);
      flags() ^= 0x02;
      at(ram, ram::BONUS_TIMER) = 0x12;
    } else {
      cy(3);
      // L_F591: a power pill (only on the four places, with the pill still there)
      cy(2 + 3 + 2);
      uint8_t x = 0;
      bool onPill = false;
      const uint8_t py = at(ram, ram::PACMAN_Y), px = at(ram, ram::PACMAN_X);
      bool toGhost = false;
      if (py == 0x05) cy(3);
      else {
        cy(2 + 2);
        if (py != 0x41) { cy(3); toGhost = true; }
        else { cy(2 + 2); x = 1; }
      }
      if (!toGhost) {
        cy(3 + 2);
        if (px == 0x04) cy(3);
        else {
          cy(2 + 2);
          if (px != 0x94) { cy(3); toGhost = true; }
          else { cy(2 + 2 + 2); x = static_cast<uint8_t>(x + 2); }
        }
        if (!toGhost) {
          cy(4 + 2 + 3);
          onPill = true;
          const uint8_t mask = cart.at(0xFE6C + x);
          if (!(pill() & mask)) { cy(3); onPill = false; }
          else {
            cy(2 + 2 + 2 + 3 + 2 + 3 + 2 + 3 + 2);
            pill() = static_cast<uint8_t>((pill() & ~mask) | 0xC0);
            const bool difficulty = (flags() & 0x20) != 0;
            if (!difficulty) cy(3); else cy(2 + 2);
            timer = difficulty ? 0x50 : 0xA0;
            cy(3 + 2 + 6);
            addScore(ram, cart, 5, x);
            cy(2 + 6);
            soundStart(ram, cart, 2);
            cy(3);
            if (newMapCheck()) return finish();
            if (joystickAndMove() == Flow::KeepGoing) {
              cy(6);
              if (pacmanKeepGoing(ram, cart) != KeepGoing::Stands) { cy(6); moveObject(ram, 0); cy(3); }
            }
            return finish();
          }
        }
      }
      (void)onPill;
    }
    // L_F5D3: a ghost touched Pac-Man (checked in four frames out of eight)
    cy(3 + 2);
    if (at(ram, ram::FRAME_COUNTER) & 4) cy(3 + 1);
    else {
      cy(2 + 3);
      if (!in.ghostTouched) cy(3 + 1);
      else {
        cy(2 + 3 + 2 + 2 + 3);
        const uint8_t x = at(ram, ram::FRAME_COUNTER) & 3;
        uint8_t& ghost = at(ram, static_cast<uint8_t>(ram::GHOST_STATE0 + x));
        if (pill() & 0x80) {
          cy(2 + 4 + 2);
          if (ghost & 0x08) cy(3 + 1);
          else {
            cy(2 + 4 + 2 + 4 + 3 + 2 + 2 + 6);
            ghost |= 0x08;
            const uint8_t y = pill() & 3;
            addScore(ram, cart, y, x);
            cy(2);
            if (y == 3) cy(3 + 1);
            else { cy(2 + 5); pill() = static_cast<uint8_t>(pill() + 1); }
            cy(2 + 6);
            soundStart(ram, cart, 3);
            cy(2 + 3 + 3);
            at(ram, ram::EAT_PAUSE) = 0x9F;
          }
        } else {
          cy(3 + 1 + 4 + 2);
          if (ghost & 0x08) cy(3);
          else {                                              // Pac-Man is caught
            cy(2 + 2 + 3 + 3 + 2 + 3 + 3);
            flags() |= 0xC0;
            at(ram, ram::EVENT_TIMER) = 0x3F;
            return finish();
          }
        }
      }
    }
    pacman();
  }
};

}  // namespace

bool bottomLogic(Ram& ram, const Cartridge& cart, const FrameInput& input, TiaChip& chip) {
  gCycles = 0;   // the writes to the chip are timed by the cycles counted from here (the end of the drawing code)
  Bottom b{ram, cart, input, chip};
  b.run();
  return true;
}

// The start of the next frame ($F7D6-$F87A).
void frameStart(Ram& ram, const Cartridge& cart, const ConsoleInput& in, TiaChip& chip) {
  chip.write(chip::VBLANK, 3);
  chip.write(chip::VSYNC, 3);
  chip.write(chip::CXCLR, 3);
  const uint8_t counter = at(ram, ram::FRAME_COUNTER);
  const uint8_t eighths = counter >> 3;
  at(ram, ram::SPRITE0_PTR_LO) = cart.at(0xFDAB + ((eighths >> 1) & 3));
  uint8_t sprite1 = 0;
  bool chosen = false;
  if (at(ram, ram::GAME_FLAGS) & 0x40) {                         // dying: the picture follows the event timer
    sprite1 = cart.at(0xFDFA + ((at(ram, ram::EVENT_TIMER) >> 2) & 0x0F));
    chosen = sprite1 != 0;
  }
  if (!chosen) {
    sprite1 = cart.at(0xFDC2 + (eighths & 3));
    if (at(ram, ram::GAME_FLAGS) & 0x80) sprite1 = cart.at(0xFDC2);        // frozen: the first picture
  }
  at(ram, ram::SPRITE1_PTR_LO) = sprite1;
  uint8_t& frame = at(ram, ram::FRAME_COUNTER);
  frame = static_cast<uint8_t>(frame + 1);
  if (frame == 0) {
    uint8_t& tick = at(ram, ram::TICK256);
    tick = static_cast<uint8_t>(tick + 1);
    bool attractStep = true;
    if (tick != 0) {                                             // the bonus item's timer (F15)
      uint8_t& bonus = at(ram, ram::BONUS_TIMER);
      bonus = static_cast<uint8_t>(bonus - 1);
      if (bonus == 0) {
        uint8_t& flags = at(ram, ram::GAME_FLAGS);
        flags ^= 0x02;
        uint8_t length = 0x12;
        if (flags & 0x02) length = (flags & 0x20) ? 2 : 3;
        bonus = length;
      }
      attractStep = at(ram, ram::ATTRACT_SHIFT) != 0;
    }
    if (attractStep) {                                           // the title-screen shift, never 0
      uint8_t& shift = at(ram, ram::ATTRACT_SHIFT);
      do shift = static_cast<uint8_t>(shift + 1); while (shift == 0);
    }
  }
  at(ram, ram::SPRITE0_PTR_HI) = 0xFD;
  at(ram, ram::SPRITE1_PTR_HI) = 0xFD;
  at(ram, ram::SPRITE1_Y) = static_cast<uint8_t>(at(ram, ram::PACMAN_Y) + 0x0B);
  at(ram, ram::SPRITE0_LINES) = 7;
  at(ram, ram::SPRITE1_LINES) = 6;
  uint8_t mask = in.colour ? 0xFF : 0x0F;
  if (at(ram, ram::ATTRACT_SHIFT) != 0) mask ^= 0x08;
  at(ram, ram::COLOUR_MASK) = mask;
  at(ram, ram::COLOUR_BG) = 7;
  at(ram, ram::COLOUR_MAZE) = 8;
  at(ram, ram::COLOUR_SCORE_P2) = at(ram, ram::SCORE_COLOUR_P2);
}

// The start-up ($F000-$F052).
void startUp(Ram& ram, TiaChip& chip) {
  for (int a = 0; a < 0x80; ++a) chip.write(static_cast<uint8_t>(a & 0x3F), 0);    // $00-$7F: the chip registers and their mirrors
  ram.fill(0);                                                                      // $80-$FF
  at(ram, ram::GAME_SELECT) = 6;
  at(ram, ram::BONUS_TIMER) = 12;
  at(ram, ram::ATTRACT_SHIFT) = 0xFF;
  at(ram, ram::RNG_STATE) = 0xFF;
  newGameSetup(ram);
  at(ram, ram::SCORE_P1_LO) = 0xCB;
  at(ram, ram::PACMAN_Y) = 0xCB >> 1;
  at(ram, ram::SCORE_P1_MID) = 0xED;
  at(ram, ram::SCORE_P1_HI) = 0x0F;
  for (uint8_t i = 1; i <= 4; ++i) at(ram, static_cast<uint8_t>(ram::SPEED_ACC0 + i)) = 0x0F;
  chip.write(chip::HMM1, 0xF0);
  chip.write(chip::HMBL, 0x80);
  chip.write(chip::RESBL, 0x80);
  chip.write(chip::RESM1, 0x80);
  chip.write(chip::HMOVE, 0x80);
  chip.write(chip::HMCLR, 0x80);
  at(ram, ram::GAME_FLAGS) = 0;
  at(ram, ram::LIVES) = 0;
}

}  // namespace game
