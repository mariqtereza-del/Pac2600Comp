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
  const uint8_t blocked = cart.at(0xFFE9 + sound) & at(ram, ram::SOUND_ACTIVE);
  if (blocked != 0) return blocked;
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
    control = 4;
    pitch = at(ram, ram::SOUND_TIMER) & 0x1F;
    volume = pitch ^ 0x0F;
  } else if (active & 0x06) {                            // power pill (bit 1) and bonus item (bit 2): the steady tone
    control = pitch = volume = 0x0D;
  } else if (active & 0x08) {                            // wafer
    const uint8_t timer = at(ram, ram::SOUND_TIMER);
    if (timer != 0) { control = 9; pitch = 9; volume = timer; }
    else { control = pitch = volume = 0; }              // a timer of 0 cannot occur; the original then writes zeros
  } else {
    return;
  }
  at(ram, ram::TEMP_D3) = control;
  uint8_t& timer = at(ram, ram::SOUND_TIMER);
  timer = static_cast<uint8_t>(timer - 1);
  if (timer == 0) { at(ram, ram::SOUND_ACTIVE) = 0; return; }
  out.write(chip::AUDV0, volume);
  out.write(chip::AUDC0, control);
  out.write(chip::AUDF0, pitch);
}

// SOUND_MUSIC ($FF94).
void soundMusic(Ram& ram, const Cartridge& cart, SoundChip& out) {
  uint8_t& flags = at(ram, ram::GAME_FLAGS);
  const uint8_t timer = at(ram, ram::EVENT_TIMER);
  if (flags & 0x40) {                                   // dying: a falling tone
    out.write(chip::AUDV0, timer);
    out.write(chip::AUDF0, timer >> 1);
    out.write(chip::AUDC0, 4);
    return;
  }
  if (flags & 0x08) {                                   // the start tune
    uint8_t a = timer;
    if (timer == 0) { flags &= 0xF7; a = flags; }       // the tune ends; the note number is then taken from the flags themselves (as the original does)
    const uint8_t note = (a >> 3) & 7;
    if (note >= 4) return;
    out.write(chip::AUDF0, cart.at(0xFFF1 + note));
    out.write(chip::AUDF1, cart.at(0xFFF5 + note));
    out.write(chip::AUDC0, 4);
    out.write(chip::AUDC1, 4);
    out.write(chip::AUDV0, 8);
    out.write(chip::AUDV1, 8);
    return;
  }
  if (!((at(ram, ram::PILL_FLAGS) << 1) & 0x80)) return;   // scared (first part): the sweep on channel 1
  at(ram, ram::TEMP_D3) = 4;
  const uint8_t pitch = (at(ram, ram::FRAME_COUNTER) >> 1) & 0x0F;
  out.write(chip::AUDV1, (pitch ^ 0x0F) >> 1);
  out.write(chip::AUDC1, 4);
  out.write(chip::AUDF1, pitch);
}

// PACMAN_KEEP_GOING ($F793).
KeepGoing pacmanKeepGoing(Ram& ram, const Cartridge& cart) {
  if ((at(ram, ram::Y_OFFSET) | at(ram, ram::X_OFFSET)) != 0) return KeepGoing::NotOnCell;
  const uint8_t direction = at(ram, ram::PACMAN_STATE) >> 6;      // 0 right, 1 left, 2 down, 3 up: the same code the TRY routines use
  return tryDirection(ram, cart, 0, direction).exit == TryExit::Taken ? KeepGoing::Moved : KeepGoing::Stands;
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

void clearTitleSelectStep(Ram& ram, TiaChip& chip) {        // L_F8C8: the game number goes up by one
  at(ram, ram::GAME_SELECT) = static_cast<uint8_t>(at(ram, ram::GAME_SELECT) + 1);
  at(ram, ram::PACMAN_Y) = 0x60;
  at(ram, ram::GAME_FLAGS) = 0x10;
  at(ram, ram::PLAYER_MODE) ^= 0x40;                        // one player / two players alternately
  clearScores(ram);
  chip.write(chip::AUDV0, 0);
  chip.write(chip::AUDV1, 0);
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
    if (at(ram, ram::SELECT_REPEAT) == 0) clearTitleSelectStep(ram, chip);
    for (;;) {                                               // L_F8FF: held down, a step every 32 frames
      at(ram, ram::SELECT_REPEAT) = static_cast<uint8_t>(at(ram, ram::SELECT_REPEAT) + 1);
      if ((at(ram, ram::SELECT_REPEAT) & 0x1F) != 0) break;
      clearTitleSelectStep(ram, chip);
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

// The bottom logic of the frame ($F4E7-$F7CE).
namespace {

struct Bottom {
  Ram& ram;
  const Cartridge& cart;
  const FrameInput& in;
  TiaChip& chip;

  uint8_t& flags() { return at(ram, ram::GAME_FLAGS); }
  uint8_t& pill() { return at(ram, ram::PILL_FLAGS); }
  uint8_t wall(unsigned i) { return cart.at(0xFE13 + (i & 0xFF)); }

  // L_F7C3: the sound at the end of every frame (the volumes are silenced first), and the mirror bit of Pac-Man's picture at L_F7BA
  void sounds() {
    chip.write(chip::AUDV0, 0);
    chip.write(chip::AUDV1, 0);
    soundEffects(ram, chip);
    soundMusic(ram, cart, chip);
  }
  void finish() {                                           // L_F7BA
    if (at(ram, ram::PACMAN_STATE) & 0x01) chip.write(chip::REFP1, 0x08);
    sounds();
  }

  // the pressed directions: joystick bits up 1, down 2, left 4, right 8 (the TEMP_D2 copy is what the original works on)
  enum class Where { Finish, KeepGoing };

  Where joystickAndMove() {                                 // L_F6CE
    const uint8_t joy = at(ram, ram::JOY_DIRECTION);
    at(ram, ram::TEMP_D2) = joy;
    if (joy == 0) return Where::KeepGoing;
    enum { Horizontal, Vertical } step = (at(ram, ram::PACMAN_STATE) & 0x80) ? Horizontal : Vertical;   // moving vertically: try sideways first
    for (;;) {
      if (step == Horizontal) {                             // L_F6DD
        const uint8_t pressed = (at(ram, ram::TEMP_D2) >> 2) & 3;      // bit 0 left, bit 1 right
        bool turn = false;
        uint8_t dir = 0;
        if (pressed != 0 && at(ram, ram::Y_OFFSET) == 0) {
          const bool left = (pressed & 1) != 0;
          const uint8_t cell = at(ram, ram::CELL_INDEX);
          if (at(ram, ram::X_OFFSET) != 0) {                // between two cell columns: a turn round is always allowed
            turn = true;
            dir = left ? 1 : 0;
          } else if (left) {
            if (at(ram, ram::POS_CODE) != 0xF9 && !(wall(static_cast<uint8_t>(cell - 1)) & 0x80)) { turn = true; dir = 1; }
          } else if (!(at(ram, ram::CELL_ROW) == 3 && cell == 0x38) && !(wall(static_cast<uint8_t>(cell + 2)) & 0x80)) {
            turn = true;
            dir = 0;
          }
        }
        if (turn) {                                         // L_F700 / L_F71B: the mirror bit, then SET_DIRECTION (which moves)
          if (dir == 1) at(ram, ram::PACMAN_STATE) |= 0x01;
          else at(ram, ram::PACMAN_STATE) &= 0xFE;
          setDirection(ram, 0, dir);
          return Where::Finish;
        }
        const uint8_t rest = at(ram, ram::TEMP_D2) & 0xF3;  // L_F725: only up / down left to try
        if (rest == 0) return Where::KeepGoing;
        at(ram, ram::TEMP_D2) = rest;
        step = Vertical;
        continue;
      }
      // L_F72D: vertical
      const uint8_t d2 = at(ram, ram::TEMP_D2);
      bool turn = false;
      uint8_t dir = 0;
      if ((d2 & 3) != 0 && at(ram, ram::X_OFFSET) == 0) {
        const bool up = (d2 & 1) != 0;
        bool done = false;
        if (at(ram, ram::PACMAN_X) == 0x4C) {               // the middle column: the openings
          const uint8_t y = at(ram, ram::PACMAN_Y);
          if (y == 0 && up) { wrapToTop(ram, 0); return Where::Finish; }
          if (y == 0x46 && (d2 & 2)) { wrapToBottom(ram, 0); return Where::Finish; }
        }
        if (at(ram, ram::Y_OFFSET) != 0) { turn = true; dir = up ? 3 : 2; done = true; }   // between two rows: always allowed
        else {
          const uint8_t t = wall(at(ram, ram::CELL_INDEX));
          if (up ? (t & 0x40) : (t & 0x20)) { turn = true; dir = up ? 3 : 2; }
          done = true;
        }
        (void)done;
      }
      if (turn) {
        setDirection(ram, 0, dir);
        return Where::Finish;
      }
      at(ram, ram::TEMP_D2) = at(ram, ram::TEMP_D2) & 0xFC;   // L_F781: nothing vertical; look at the sideways request again
      step = Horizontal;
    }
  }

  void eatWafer() {                                         // L_F666 (Pac-Man not in an opening)
    if (at(ram, ram::Y_OFFSET) != 0) return;
    uint8_t cell = at(ram, ram::CELL_INDEX);
    uint8_t t = wall(cell);
    if (at(ram, ram::PACMAN_X) != 0x4C && (at(ram, ram::PACMAN_STATE) & 0x80)) t |= wall(static_cast<uint8_t>(cell + 1));
    if (!(t & 1)) return;
    const uint8_t index = t >> 2;
    const uint8_t byte = cart.at(0xFE79 + ((at(ram, ram::CELL_ROW) << 1) | ((t >> 1) & 1)));
    const uint8_t mask = cart.at(0xFE6A + (index & 7));
    uint8_t& maze = at(ram, static_cast<uint8_t>(ram::MAZE_00 + byte));
    if (!(maze & mask)) return;
    maze &= static_cast<uint8_t>(~mask);
    addScore(ram, cart, 4, mask);                            // the original keeps the mask in X: TEMP_E1 gets it
    soundStart(ram, cart, 0);
  }

  // L_F6AA: the new map when nothing is left; returns true if the map was refilled (the original then goes on at L_F7BA)
  bool newMapCheck() {
    uint8_t left = pill() & 0x3C;
    for (uint8_t i = 0; i < 16; ++i) left |= at(ram, static_cast<uint8_t>(ram::MAZE_00 + i));
    if (left != 0) return false;
    if (at(ram, ram::LIVES) != 9) at(ram, ram::LIVES) = static_cast<uint8_t>(at(ram, ram::LIVES) + 1);
    if (at(ram, ram::LEVEL_SPEED) != 0x0E) at(ram, ram::LEVEL_SPEED) = static_cast<uint8_t>(at(ram, ram::LEVEL_SPEED) + 1);
    newMaze(ram);
    return true;
  }

  // L_F61E: the joystick, Pac-Man in an opening or in the maze
  void pacman() {
    uint8_t joy = in.swcha;
    if (!(at(ram, ram::PLAYER_MODE) & 0x80)) joy >>= 4;
    joy = static_cast<uint8_t>(~joy) & 0x0F;
    at(ram, ram::JOY_DIRECTION) = joy;
    if (joy != 0) at(ram, ram::TICK256) = 0;
    uint8_t& state = at(ram, ram::PACMAN_STATE);
    if (state & 0x20) {                                     // in an opening: only a turn round
      const bool up = (joy & 1) != 0, down = (joy & 2) != 0;
      if ((up && !(state & 0x80)) || (!up && down && (state & 0x80))) {
        if (state & 0x10) state ^= 0x80;
        else {
          at(ram, ram::PACMAN_Y) = static_cast<uint8_t>((0x3F ^ at(ram, ram::PACMAN_Y)) & 0x3F);
          state ^= 0xC0;
        }
      }
      return finish();
    }
    eatWafer();
    if (newMapCheck()) return finish();
    if (joystickAndMove() == Where::KeepGoing) {
      if (pacmanKeepGoing(ram, cart) != KeepGoing::Stands) moveObject(ram, 0);
    }
    finish();
  }

  void run() {
    if (at(ram, ram::ATTRACT_SHIFT) != 0) return sounds();             // the title screen: sound only
    locateObject(ram, 0);
    if (at(ram, ram::EAT_PAUSE) & 0x80) {                              // the pause after a ghost was eaten
      at(ram, ram::EAT_PAUSE) = static_cast<uint8_t>(at(ram, ram::EAT_PAUSE) - 1);
      return sounds();
    }
    uint8_t& timer = at(ram, ram::EVENT_TIMER);
    if (timer != 0 && (at(ram, ram::FRAME_COUNTER) & 1)) {            // L_F4F7: the event timer runs every second frame
      timer = static_cast<uint8_t>(timer - 1);
      if (timer == 0) {
        if (pill() & 0x80) {
          if (pill() & 0x40) { pill() &= 0xBF; timer = 0x20; goto after_timer; }     // flashing part of the scare
          pill() &= 0x7C;                                                            // the scare is over
        }
        if (flags() & 0x40) {                                                        // the death delay is over
          flags() &= 0xBF;
          placeObjects(ram);
          if ((at(ram, ram::PLAYER_MODE) & 0x40) && at(ram, ram::LIVES_P2) != 0) {  // two players: the turn passes
            swapPlayers(ram);
            at(ram, ram::PLAYER_MODE) ^= 0x80;
          }
          if ((at(ram, ram::LIVES) | at(ram, ram::LIVES_P2)) != 0) {
            at(ram, ram::LIVES) = static_cast<uint8_t>(at(ram, ram::LIVES) - 1);
            flags() |= 0x08;
          } else {                                                                   // game over
            flags() = 0;
            at(ram, ram::ATTRACT_SHIFT) = 0x5F;
            at(ram, ram::PACMAN_Y) = 0x5F;
            return sounds();
          }
        } else if (flags() & 0x80) {
          flags() &= 0x7F;                                                           // the freeze is over
        }
      }
    }
  after_timer:
    if (flags() & 0x80) {                                                            // frozen: the ghosts' speed set-up
      const uint8_t level = at(ram, ram::LEVEL_SPEED);
      const uint8_t below = static_cast<uint8_t>(level - 1);
      for (uint8_t x = 4; x >= 1; x = static_cast<uint8_t>(x - 1)) {
        at(ram, static_cast<uint8_t>(ram::SPEED_ACC0 + x)) = (x & 1) ? below : level;
        at(ram, static_cast<uint8_t>(ram::GHOST_TIMER0 - 1 + x)) = below;
      }
      return sounds();
    }
    // L_F573: the bonus item
    if (in.bonusTouched) {
      addScore(ram, cart, 6, 0);
      soundStart(ram, cart, 1);
      flags() ^= 0x02;
      at(ram, ram::BONUS_TIMER) = 0x12;
    } else {
      // L_F591: a power pill (only on the four places, with the pill still there)
      uint8_t x = 0;
      bool onPill = false;
      const uint8_t py = at(ram, ram::PACMAN_Y), px = at(ram, ram::PACMAN_X);
      if (py == 0x05 || py == 0x41) {
        if (py == 0x41) x = 1;
        if (px == 0x04 || px == 0x94) {
          if (px == 0x94) x = static_cast<uint8_t>(x + 2);
          onPill = true;
        }
      }
      if (onPill) {
        const uint8_t mask = cart.at(0xFE6C + x);
        if (pill() & mask) {
          pill() = static_cast<uint8_t>((pill() & ~mask) | 0xC0);
          timer = (flags() & 0x20) ? 0x50 : 0xA0;
          addScore(ram, cart, 5, x);
          soundStart(ram, cart, 2);
          if (newMapCheck()) return finish();
          return joystickPartAfterPill();
        }
      }
    }
    // L_F5D3: a ghost touched Pac-Man (checked in four frames out of eight)
    if (!(at(ram, ram::FRAME_COUNTER) & 4) && in.ghostTouched) {
      const uint8_t x = at(ram, ram::FRAME_COUNTER) & 3;
      uint8_t& ghost = at(ram, static_cast<uint8_t>(ram::GHOST_STATE0 + x));
      if (pill() & 0x80) {
        if (!(ghost & 0x08)) {
          ghost |= 0x08;
          const uint8_t y = pill() & 3;
          addScore(ram, cart, y, x);
          if (y != 3) pill() = static_cast<uint8_t>(pill() + 1);
          soundStart(ram, cart, 3);
          at(ram, ram::EAT_PAUSE) = 0x9F;
        }
      } else if (!(ghost & 0x08)) {                                                  // Pac-Man is caught
        flags() |= 0xC0;
        at(ram, ram::EVENT_TIMER) = 0x3F;
        return finish();
      }
    }
    pacman();
  }

  // after a power pill was eaten the original jumps to L_F6AA (the new map test) and carries on from there with the joystick part
  void joystickPartAfterPill() {
    if (joystickAndMove() == Where::KeepGoing) {
      if (pacmanKeepGoing(ram, cart) != KeepGoing::Stands) moveObject(ram, 0);
    }
    finish();
  }
};

}  // namespace

bool bottomLogic(Ram& ram, const Cartridge& cart, const FrameInput& input, TiaChip& chip) {
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
