#include "ghosts.hpp"
#include "../data/tables.hpp"
#include "../state/cycles.hpp"
#include "objects.hpp"

namespace game {

// RNG_STEP ($FBE0). With r = RNG_STATE the original forms x = (r << 1) xor r (8 bits), shifts it left twice and rotates the last bit shifted out,
// which is bit 6 of x, into the low end of r: r = (r << 1) | bit 6 of x. The value returned is TBL_FE6A[r & 7] & r, where the table of the
// original holds the bit masks $80, $40, ... $01, i.e. 0x80 >> (r & 7) (the lockstep tool checks this against the cartridge bytes).
uint8_t rngStep(GameState& st) {
  const uint8_t r = st.rng;
  const uint8_t x = static_cast<uint8_t>((r << 1) ^ r);
  const uint8_t next = static_cast<uint8_t>((r << 1) | ((x >> 6) & 1));
  st.rng = next;
  return static_cast<uint8_t>((0x80 >> (next & 7)) & next);
}

// GHOST_DECIDE ($FA26).
namespace {

struct Decider {
  GameState& st;
  uint8_t n;
  bool done = false;      // a turn was taken (or the opening entered): the decision is over
  bool unsupported = false;

  uint8_t& state() { return st.objState[n]; }
  uint8_t x() { return st.objX[n]; }
  uint8_t y() { return st.objY[n]; }

  void turn(uint8_t direction) {
    if (done) return;
    if (tryDirection(st, n, direction).exit == TryExit::Taken) done = true;
  }
  void pair(bool vertical) {
    if (done) return;
    const uint8_t r = rngStep(st);
    if (tryPair(st, n, vertical, r == 0).taken) done = true;
  }
  bool rngIsZero() { return rngStep(st) == 0; }

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
    // ORIGINAL: the last wander chain falls into code that no recording ever runs. Evidence: src\game\logic\ghosts.hpp (F21.15). Do not fix.
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

bool ghostDecide(GameState& st, uint8_t object) {
  Decider g{st, object};
  if ((st.xOffset | st.yOffset) != 0) return true;
  if (g.state() & 0x10) return true;
  if (st.attractShift != 0) { g.wander(); return !g.unsupported; }
  if (g.state() & 0x08) {                                          // eyes: back to the pen door
    st.scratch[kTempD3] = 0x58;
    st.scratch[kTempD2] = 0x1F;
    g.chase(0x58, 0x1F);
    return !g.unsupported;
  }
  if (g.x() == 0x4C && (st.rng & 0x0F) == 0) {    // the middle column: sometimes into the opening
    if (g.y() == 0x00) { wrapToTop(st, object); return true; }
    if (g.y() == 0x46) { wrapToBottom(st, object); return true; }
  }
  const uint8_t pacX = st.objX[0], pacY = st.objY[0];
  st.scratch[kTempD3] = pacX;
  st.scratch[kTempD2] = pacY;
  // ORIGINAL: a scared ghost aims at a mirrored x but at Pac-Man's own y. Evidence: docs\F13_GHOSTS.md (F13). Do not fix.
  if (st.player[0].pillFlags & 0x80) {                           // scared: x mirrored, y still Pac-Man's own
    const uint8_t mirrored = static_cast<uint8_t>(~static_cast<uint8_t>(pacX + 0x60));
    st.scratch[kTempD3] = mirrored;
    g.chase(mirrored, pacY);
    return !g.unsupported;
  }
  uint8_t& timer = st.ghostTimer[object - 1];
  timer = static_cast<uint8_t>(timer - 1);
  if (timer != 0) { g.chase(pacX, pacY); return !g.unsupported; }
  timer = data::kBitMasksAndReloads[5 + (st.player[0].level >> 1)];
  if ((object >> 1) & 1) timer = static_cast<uint8_t>(timer + 1);
  g.wander();
  return !g.unsupported;
}

// GHOST_UPDATE ($F9AA).
bool ghostUpdate(GameState& st, uint8_t object) {
  uint8_t& state = st.objState[object];
  if ((state & 0x08) && st.objY[object] == 0x1E) {
    const uint8_t x = st.objX[object];
    if (x == 0x58) { setDirection(st, object, 1); return true; }                 // at the pen door: left, and move
    if (x == 0x4C) {                                                              // above the pen
      if (st.player[0].pillFlags & 0x80) return true;                           // scared: wait
      state &= 0xF7;                                                              // a ghost again
      setDirection(st, object, 0);
      return true;
    }
  }
  if (state & 0x20) return true;                                                  // in an opening animation
  locateObject(st, object);
  if (!ghostDecide(st, object)) return false;
  if (!(state & 0x20)) moveObject(st, object);
  return true;
}

}  // namespace game
