"""Stage F13: the ghosts - movement and decisions, written as a model and checked against the recorded games.

  python -I tools\\f13_ghosts.py <rom>      (after f1-f12; needs the 70 baseline traces in reference\\baseline and tools\\f10_pacman.py)

The model follows the code of the top logic ($F92C-$F980), GHOST_UPDATE ($F9AA), GHOST_DECIDE ($FA26), the four TRY_* routines
($FB36-$FB9C), RNG_STEP ($FBE0), MOVE_OBJECT ($F9EE), ANIMATE_OBJECT ($FC8B) and the two WRAP routines ($FD26). From the memory of the frame
before (and the ROM tables) it predicts, for the ghost the frame updates, its x, y, state, speed accumulator and timer, and the random number
$CB, and compares with the recorded memory of every frame in which that code runs in the normal way. Frames on other roads are counted by reason.
Writes docs\\F13_GHOSTS.md. The ROM is only read.
"""
import collections
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / 'tools'))
import f10_pacman as f10  # noqa: E402

HIT = collections.Counter()
REV = [0x40, 0x00, 0xC0, 0x80]      # TBL_FE67: the state bits of the opposite direction (no turning back)
DIRCODE = [0x10, 0x50, 0x90, 0xD0]  # TBL_FE63


class Done(Exception):
    """A TRY_ routine took a direction: it dropped its caller's return (PLA PLA), so the decision routine ends."""


class Ghost(f10.Pac):
    def __init__(self, world, X):
        self.w = world
        self.X = X
        r = world.ram
        self.x, self.y, self.st, self.acc, self.tm = r[0x31 + X], r[0x36 + X], r[0x3B + X], r[0x40 + X], r[0x44 + X]
        self.tick = 0

    def wall(self, i):
        return self.w.rom[0xE13 + (i & 0xFF)]

    # ---- the four TRY_ routines with the commit of TRY_COMMIT ($FB44) ----
    def try_(self, code):
        if code == 3:
            if self.row == 0 or not (self.wall(self.cell) >> 6) & 1:
                return
        elif code == 2:
            if self.row == 7 or not (self.wall(self.cell) >> 5) & 1:
                return
        elif code == 1:
            if self.row == 3 and self.cell == 0x3E:
                return
            if self.wall(self.cell - 1) & 0x80:
                return
        else:
            if self.row == 3 and self.cell == 0x38:
                return
            if self.wall(self.cell + 2) & 0x80:
                return
        if (self.st & 0xC0) == REV[code]:
            HIT['a way is allowed but would turn back: refused'] += 1
            return
        self.set_dir(code)
        raise Done

    def rng(self):
        r = self.w.rng
        # ASL A (carry = bit 7 of a), ASL A (carry = bit 7 of the result), ROL RNG_STATE takes that last carry
        a0 = ((r << 1) & 0xFF) ^ r
        a1 = (a0 << 1) & 0xFF
        c = (a1 >> 7) & 1
        self.w.rng = ((r << 1) | c) & 0xFF
        self.w.bump += 1
        return self.w.rom[0xE6A + (self.w.rng & 7)] & self.w.rng

    def pair_ud(self, flag):
        HIT['random pair up/down'] += 1
        if flag == 0:
            self.try_(3)
            self.try_(2)
        else:
            self.try_(2)
            self.try_(3)

    def pair_lr(self, flag):
        HIT['random pair left/right'] += 1
        if flag == 0:
            self.try_(1)
            self.try_(0)
        else:
            self.try_(0)
            self.try_(1)

    def pair1(self):
        self.pair_ud(self.rng())

    def pair2(self):
        self.pair_lr(self.rng())

    def wander(self, start):
        """L_FAB9 and the chain of random tries that follows it; the tails fall through into the next chain."""
        steps = ['up', 'right', 'left', 'down']
        i = steps.index(start)
        for name in steps[i:]:
            if name == 'up':
                if self.rng() == 0:
                    self.try_(3)
                self.pair2()
                HIT['NEVER RUN in the games: TRY_UP after the pair'] += 1
                self.try_(3)
            elif name == 'right':
                if self.rng() == 0:
                    self.try_(0)
                self.pair1()
                self.try_(0)
            elif name == 'left':
                if self.rng() == 0:
                    self.try_(1)
                self.pair1()
                self.try_(1)
            else:
                if self.rng() == 0:
                    self.try_(2)
                self.pair2()
                HIT['NEVER RUN in the games: TRY_DOWN after the pair'] += 1
                self.try_(2)
        HIT['NEVER RUN in the games: fell out of the last chain'] += 1

    def wander_from_state(self):
        d = self.st >> 6        # 0 right, 1 left, 2 down, 3 up
        HIT['random chain, direction %s' % ('right', 'left', 'down', 'up')[d]] += 1
        self.wander(('right', 'left', 'down', 'up')[d])

    # ---- GHOST_DECIDE ($FA26) ----
    def decide(self):
        w = self.w
        if self.xoff | (self.yoff & 0xFF):
            HIT['decide: not exactly on a cell'] += 1
            return
        if self.st & 0x10:
            HIT['decide: step already due'] += 1
            return
        try:
            if w.attract:
                HIT['decide: title screen, wander'] += 1
                self.wander_from_state()
                return
            if self.st & 8:
                HIT['decide: eyes, aim home'] += 1
                tx, ty = 0x58, 0x1F
                self.chase(tx, ty)
                return
            if self.x == 0x4C and (w.rng & 0x0F) == 0:
                if self.y == 0:
                    HIT['decide: enters the top opening'] += 1
                    self.wrap(0xF0, 0x0B)
                    return
                if self.y == 0x46:
                    HIT['decide: enters the bottom opening'] += 1
                    self.wrap(0x30, 0x51)
                    return
            tx, ty = w.pacx, w.pacy
            if w.pill & 0x80:
                HIT['decide: scared, aim away'] += 1
                tx = (~((w.pacx + 0x60) & 0xFF)) & 0xFF
                # the code works out a flipped y too but never stores it (no STA before JMP $FA99): the y aim stays Pac-Man's y
                self.chase(tx, ty)
                return
            self.tm = (self.tm - 1) & 0xFF
            if self.tm != 0:
                HIT['decide: chase'] += 1
                self.chase(tx, ty)
                return
            self.tm = w.rom[0xE6F + (w.level >> 1)]
            if (self.X >> 1) & 1:
                self.tm = (self.tm + 1) & 0xFF
            HIT['decide: timer ran out, reloaded, wander'] += 1
            self.wander_from_state()
        except Done:
            HIT['decide: a direction was taken'] += 1

    def wrap(self, bits, y):
        self.st = (self.st & 0x0F) | bits
        self.y = y

    def chase(self, tx, ty):
        if self.x < tx:
            self.try_(0)
        elif self.x > tx:
            self.try_(1)
        if self.y < ty:
            self.try_(2)
        else:
            self.try_(3)
        HIT['chase: no direction taken, wander'] += 1
        self.wander_from_state()

    # ---- ANIMATE_OBJECT ($FC8B) for a ghost: Y = 0 ----
    def animate(self):
        st = self.st
        HIT['ghost animation step (leaving the pen / opening)'] += 1
        if not st & 0x10:
            self.y = (self.y - 1) & 0xFF
            if self.y != 0:
                return
            self.y = 0x5B if st & 0x80 else 0x00
            self.st = st | 0x10
            return
        c, n = (st >> 7) & 1, (st >> 6) & 1
        if c:
            if n:
                self.y = (self.y - 1) & 0xFF
                if not self.y & 0x80:
                    return self._fcb2()
                self.st = (st & 0x0F) | 0xA0
                self.y = f10.FF25[0]
                return
            self.y = (self.y - 1) & 0xFF
            if self.y != 0x51:
                return
            self.y = 0x46
            self.st = (st & 0x0F) | 0xC0
            return
        if n:
            self.y = (self.y + 1) & 0xFF
            return self._fcb2()
        self.y = (self.y + 1) & 0xFF
        if self.y != 0x5B:
            return
        self.st = (st & 0x0F) | 0x60
        self.y = f10.FF25[0]

    # ---- GHOST_UPDATE ($F9AA) ----
    def update(self):
        w = self.w
        if self.st & 8 and self.y == 0x1E:
            if self.x == 0x58:
                HIT['update: eyes at the pen door x=$58, turn left'] += 1
                self.set_dir(1)
                self.move()
                return
            if self.x == 0x4C:
                if w.pill & 0x80:
                    HIT['update: eyes wait at the pen while scared'] += 1
                    return
                HIT['update: eyes home, ghost again, go right'] += 1
                self.st &= 0xF7
                self.set_dir(0)
                self.move()
                return
        if self.st & 0x20:
            return
        self.locate()
        self.decide()
        if self.st & 0x20 == 0:
            self.move()


class World:
    pass


def predict(rom, pre, inp):
    w = World()
    w.rom, w.ram = rom, pre
    w.rng, w.attract, w.level, w.pill = pre[0x4B], pre[0x02], pre[0x19], pre[0x1A]
    w.pacx, w.pacy = pre[0x31], pre[0x36]
    w.bump = 0
    X = (pre[0] & 3) + 1
    g = Ghost(w, X)
    if g.st & 8:
        run = True
    else:
        run = not (pre[0x65] & 0x80)
    if run and pre[0x67] & 0x80:
        run = False
    if run:
        for _ in range(3):
            g.update()
    if g.st & 0x20:      # the animation ($F968-$F970) comes after the updates and is not skipped by the pause
        g.animate()
    return g, w


def main():
    rom = pathlib.Path(sys.argv[1]).resolve().read_bytes()
    stats = collections.Counter()
    miss = []
    for f in sorted((ROOT / 'reference' / 'baseline').glob('BL-???.trace')):
        rows = []
        for l in f.read_text().split('\n'):
            p = l.split(',')
            if len(p) >= 6 and p[0].isdigit():
                rows.append((int(p[1]), bytes.fromhex(p[5])))
        for i in range(1, len(rows)):
            inp, post = rows[i]
            pre = rows[i - 1][1]
            stats['frames'] += 1
            if f.stem in ('BL-065', 'BL-066'):
                stats['skipped: test-only memory edits (BL-065/066)'] += 1
                continue
            if inp & 0x60:
                stats['skipped: Select or Reset pressed'] += 1
                continue
            if pre[2] != 0 and inp & 16:
                stats['skipped: fire on the title screen (a game starts)'] += 1
                continue
            if pre[2] != 0 and (inp & 0x300 or inp & 0x80):
                stats['skipped: difficulty switch on the title screen'] += 1
                continue
            if (post[0x67] & 0x80) and not (pre[0x67] & 0x80):
                stats['skipped: death or new map starts in the bottom logic (ghosts are put back)'] += 1
                continue
            if (pre[0x66] ^ post[0x66]) & 0x80:
                stats['skipped: change of turn'] += 1
                continue
            if pre[0x67] & 0x80:
                stats['skipped: frozen game (the top logic does not move ghosts; the bottom logic sets their timers)'] += 1
                if post[0x67] & 0x80:
                    # $F55F-$F571: while frozen the bottom logic sets, for X = 4..1, accumulator and timer from LEVEL_SPEED (L): objects 4 and 2
                    # get accumulator L, objects 3 and 1 get L-1, all four timers L-1
                    lv = post[0x19]
                    y = (lv - 1) & 0xFF
                    good = all(post[0x44 + k] == y and post[0x40 + k] == (lv if k in (2, 4) else y) for k in (1, 2, 3, 4))
                    stats['  frozen frames: ghost accumulators and timers set from LEVEL_SPEED, right' if good else '  frozen frames: accumulators/timers WRONG'] += 1
                continue
            g, w = predict(rom, pre, inp)
            X = g.X
            want = (post[0x31 + X], post[0x36 + X], post[0x3B + X], post[0x40 + X], post[0x44 + X], post[0x4B])
            got = (g.x, g.y, g.st, g.acc, g.tm, w.rng)
            # bottom logic of the same frame may set bit 3 of the ghost state (ghost eaten, F12) or take a power pill's effect on nothing here
            others_same = all(pre[a] == post[a] for k in range(1, 5) if k != X for a in (0x31 + k, 0x36 + k, 0x3B + k, 0x40 + k, 0x44 + k))
            if got != want and got[2] | 8 == want[2] and got[:2] + got[3:] == want[:2] + want[3:] and not got[2] & 8 and want[2] & 8:
                stats['  ghost eaten in the bottom logic of the same frame (F12), rest right'] += 1
                got = want
            stats['predicted frames'] += 1
            if got == want and others_same:
                stats['correct'] += 1
            else:
                stats['wrong'] += 1
                if len(miss) < 15:
                    miss.append((f.stem, i, X, 'pre', tuple(pre[a] for a in (0x31 + X, 0x36 + X, 0x3B + X, 0x40 + X, 0x44 + X)), pre[0x4B], 'want', want, 'got', got, others_same))
            if w.bump:
                stats['  frames that stepped the random number'] += 1
    ok = stats['wrong'] == 0 and stats['correct'] > 100000 and not stats['  frozen frames: accumulators/timers WRONG']
    for k, v in sorted(stats.items()):
        print('%8d  %s' % (v, k))
    for m in miss:
        print(m)
    print('roads taken:')
    for k, v in sorted(HIT.items()):
        print('%8d  %s' % (v, k))
    print('result: %s' % ('OK' if ok else 'MISMATCH'))
    write_doc(stats, ok)
    return 0 if ok else 1


def write_doc(stats, ok):
    d = ['# Stage F13: ghost movement and decisions (2026-10-26)', '']
    BS = chr(92)
    d += ['`python -I tools%sf13_ghosts.py rom%spacman.a26` holds a model of the ghost code and checks it against the recorded games. From the memory of the frame before, the ROM tables and the pressed buttons it predicts, for the ghost the frame updates, its x, y, state, speed accumulator and timer and the random number `$CB`, and compares them with the recorded memory. This document is written by that tool.' % (BS, BS), '',
          '## Result', '',
          '- **%d of %d predicted frames are exactly right (%d wrong)**, all other ghosts unchanged, in the 70 games (%d frames).' % (stats['correct'], stats['predicted frames'], stats['wrong'], stats['frames']),
          '- Frames not predicted, with the reason: ' + '; '.join('%s: %d' % (k.replace('skipped: ', ''), v) for k, v in sorted(stats.items()) if k.startswith('skipped')) + '.', '',
          'Roads of the code taken in the predicted frames:', '', '| Frames or times | Road |', '| --- | --- |']
    d += ['| %d | %s |' % (v, k) for k, v in sorted(HIT.items())]
    d += ['', RULES % stats['  frames that stepped the random number']]
    (ROOT / 'docs' / 'F13_GHOSTS.md').write_text('\n'.join(d) + '\n', encoding='utf-8')


RULES = '''## The rules (what the code does, in the order it does it)

All of this is in the top logic of a frame, after the joystick, the pill-colour and sprite-pointer lines (`$F92C-$F980`). The ghost handled in a frame is
object `(FRAME_COUNTER & 3) + 1`, where FRAME_COUNTER is the value of the frame before (the counter goes up at the end of a frame, in `FRAME_START`): **each ghost is handled once every 4 frames, but three times in that frame** (`GHOST_UPDATE` is called three times in a row), so its speed accumulator is stepped three times and it can move up to three pixels.

1. *Does it run?* A ghost whose state bit 3 is set (eaten, only eyes) always runs; others run only if `EAT_PAUSE` is not negative (not in the 160-frame pause after a ghost was eaten). Nothing runs while `GAME_FLAGS` bit 7 is set (frozen: death, new life, new map); in that time the bottom logic sets the accumulators and timers of the four ghosts from `LEVEL_SPEED` (objects 4 and 2: accumulator = LEVEL_SPEED; objects 3 and 1: LEVEL_SPEED - 1; all four timers LEVEL_SPEED - 1).
2. *`GHOST_UPDATE`*:
   - eyes at y = $1E: at x = $58 (the pen door) turn left and move; at x = $4C (above the pen) wait there while the ghosts are scared, otherwise become a ghost again (bit 3 cleared), go right and move;
   - a ghost in an opening animation (bit 5) does nothing here;
   - otherwise `LOCATE_OBJECT`, `GHOST_DECIDE`, and then `MOVE_OBJECT` unless the decision put the ghost into an opening (bit 5).
3. *`GHOST_DECIDE`* only decides when the ghost stands exactly on a cell (both offsets 0) and no step is pending (state bit 4 clear). Then:
   - on the title screen (`ATTRACT_SHIFT` not 0): wander (below);
   - eyes: aim at the pen door (x = $58, y = $1F) and chase;
   - at x = $4C, y = 0 or y = $46 and the low four bits of the random number 0 (1 time in 16): enter the top or bottom opening (`WRAP_TO_TOP` / `WRAP_TO_BOTTOM`: state and y set, animation follows);
   - scared (`PILL_FLAGS` bit 7): aim at x = NOT(Pac-Man x + $60) (a mirror image) and **at Pac-Man's y** (the original works out a mirrored y as well but never stores it, so scared ghosts run away horizontally and still go towards Pac-Man's row), and chase;
   - otherwise the ghost's timer goes down by one; while it is not zero: aim at Pac-Man and chase. When it reaches zero the timer is reloaded from the table `$FE6F` indexed by LEVEL_SPEED / 2 (plus 1 for objects 2 and 3) and the ghost wanders for this decision.
4. *Chase*: if the target x is bigger try right, if smaller try left; then if the ghost's y is smaller than the target y try down, otherwise (also equal) try up. The first allowed direction wins and ends the decision (`TRY_COMMIT`). A direction that would turn the ghost straight back is refused, so a ghost never reverses on its own. If nothing was taken, wander.
5. *Wander* (a chain that depends on the direction the ghost is going): the random number is stepped (`RNG_STEP`: a shift register, state `$CB`); if the result is zero try to keep going straight; then step again and try the two sideways directions in an order chosen by the result; then straight again. The chains for up, right, left and down fall through into each other in that order, but a ghost never gets past the first chain because a corridor always offers some direction.
6. *Allowed* (`TRY_*`): up needs bit 6 and down bit 5 of the wall-table byte of the cell (and not row 0 / row 7); left and right need no wall bit in the neighbour cell (cell - 1, cell + 2) and have two exceptions on row 3 (cells $3E and $38).
7. *`MOVE_OBJECT`* as for Pac-Man (F10): multiply the accumulator by 17, a carry is one step. Ghost speeds come from the accumulator start values (level); the other speeds are not claimed beyond what the traces show.
8. After the updates, a ghost in an opening animation (bit 5) is animated by `ANIMATE_OBJECT` (y counts through $51-$5B below the maze or 0-$0B above it); this also runs during the eaten-ghost pause.

## Not claimed

- The random number `$CB` is predicted exactly (it is stepped in %d frames of the checked ones) but where it starts is F14 (it is set to $FF when a game starts, `$F890`).
- Frozen frames, the death sequence, the change of turn and new-map frames are skipped here (counted above); F16 and F17 follow them.
- Roads the games never took (a ghost running out of the last wander chain; going up or down out of the pair) are marked "NEVER RUN" in the code listing.
- The ghost speed table and what raises it with the level is F16.
'''


if __name__ == '__main__':
    sys.exit(main())
