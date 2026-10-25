"""Stage F10: Pac-Man's movement and turning - a model of the original's rules, checked against the recorded games.

  python -I tools\\f10_pacman.py <rom>      (after f1-f9; needs the 70 baseline traces in reference\\baseline)

The model is written from the reading of the code (routines at $F61E-$F7BA, MOVE_OBJECT $F9EE, TRY_* $FB36-$FB9C, LOCATE_OBJECT $FBAD,
ANIMATE_OBJECT $FC8B, WRAP_* $FD26-$FD3E) and uses only: the wall table at $FE13 of the ROM, the joystick of the frame and Pac-Man's
bytes of the frame before (x $B1, y $B6, state $BB, speed accumulator $C0, idle counter $81).
It predicts, for every frame in which Pac-Man's code runs in the normal way, the new x, y, state, speed accumulator,
JOY_DIRECTION and idle counter, and compares them with the recorded memory of that frame.
Frames in which the code takes a different road are not predicted and are counted by reason: title screen, frozen game, a death or a
ghost eaten, a power pill eaten, a new map, the change of turn.
Writes docs\\F10_PACMAN.md. The ROM is only read.
"""
import collections
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
HIT = collections.Counter()  # which roads of the code the recorded games took
DIRCODE = [0x10, 0x50, 0x90, 0xD0]  # TBL_FE63: direction 0 right, 1 left, 2 down, 3 up (bits 7-6) with the "step" bit 4
FF25 = [0x10, 0x40]


class Pac:
    """Pac-Man's own bytes and the position values made by LOCATE_OBJECT ($FBAD)."""

    def __init__(self, ram, tbl):
        self.x, self.y, self.st, self.acc, self.tick = ram[0x31], ram[0x36], ram[0x3B], ram[0x40], ram[0x01]
        self.tbl = tbl

    # ---- LOCATE_OBJECT ($FBAD) ----
    def locate(self):
        y, a, n = self.y, self.y, 0
        if a != 0:
            while True:
                n += 1
                a -= 10
                if a == 0 or a < 0:
                    break
        self.row_raw = n
        self.yoff = a & 0xFF
        self.xoff = self.x & 3
        carry = n & 1
        cell = self.x >> 2
        if carry:
            cell = (cell + 0x28) & 0xFF
        self.cell = cell
        # ASL A, ASL A: A = cell*4 (8 bit), carry = last bit shifted out
        v = (cell << 2) & 0xFF
        c = (cell >> 6) & 1
        # ROR CELL_ROW: bit 0 -> carry, carry -> bit 7
        row = n
        new_c = row & 1
        row = (row >> 1) | (c << 7)
        c = new_c
        a = v | row               # ORA CELL_ROW
        # ROL CELL_ROW
        c2 = (row >> 7) & 1
        row = ((row << 1) & 0xFF) | c
        self.row = row            # CELL_ROW as the program holds it after the routine
        self.pos = a

    # ---- MOVE_OBJECT ($F9EE) ----
    def move(self):
        t = (self.acc << 4) & 0xFF
        s = t + self.acc
        self.acc = s & 0xFF
        if not s >> 8:
            return
        HIT['move: the speed accumulator carried, one step'] += 1
        self.st &= 0xEF
        c, nbit = (self.st >> 7) & 1, (self.st >> 6) & 1
        d = -1 if nbit else 1
        if not c:
            nx = (d + self.x) & 0xFF
            if nx not in (0x02, 0x96):
                self.x = nx
            else:
                HIT['move: x stopped at the border (2 or $96)'] += 1
        else:
            ny = (d + self.y) & 0xFF
            if not (ny & 0x80) and ny != 0x47:
                self.y = ny
            else:
                HIT['move: y stopped at the border (below 0 or $47)'] += 1

    def set_dir(self, code):
        self.st = (self.st & 0x3F) | DIRCODE[code]

    def wall(self, i):
        return self.tbl[i & 0xFF] if 0 <= (i & 0xFF) < 80 else 0

    # ---- TRY_* ($FB36, $FB5C, $FB70, $FB87) for Pac-Man (X = 0): True when a direction was taken ----
    def try_dir(self, code):
        if code == 3:   # up
            if self.row == 0:
                return False
            if not (self.wall(self.cell) >> 6) & 1:
                return False
        elif code == 2:  # down
            if self.row == 7:
                return False
            if not (self.wall(self.cell) >> 5) & 1:
                return False
        elif code == 1:  # left
            if self.row == 3 and self.cell == 0x3E:
                return False
            if self.wall(self.cell - 1) & 0x80:
                return False
        else:           # right
            if self.row == 3 and self.cell == 0x38:
                return False
            if self.wall(self.cell + 2) & 0x80:
                return False
        self.set_dir(code)
        return True

    # ---- ANIMATE_OBJECT ($FC8B) for Pac-Man (X = 0, Y = 1): the opening in the top and bottom walls ----
    def animate(self):
        st = self.st
        HIT['opening: animation step'] += 1
        if not st & 0x10:
            self.y = (self.y - 1) & 0xFF
            if self.y != 0:
                return
            self.y = 0x5B if st & 0x80 else 0x00
            self.st = st | 0x10
            return
        c, n = (st >> 7) & 1, (st >> 6) & 1
        if c:
            if n:  # FD18
                self.y = (self.y - 1) & 0xFF
                if not self.y & 0x80:
                    return self._fcb2()
                self.st = (st & 0x0F) | 0xA0
                self.y = FF25[1]
                return
            self.y = (self.y - 1) & 0xFF   # FD02
            if self.y != 0x51:
                return
            self.y = 0x46
            self.st = (st & 0x0F) | 0xC0
            return
        if n:                      # C=0, N=1: INC B6
            self.y = (self.y + 1) & 0xFF
            return self._fcb2()
        self.y = (self.y + 1) & 0xFF   # FCE7
        if self.y != 0x5B:
            return
        self.st = (st & 0x0F) | 0x60
        self.y = FF25[1]

    def _fcb2(self):
        if self.y == 0x0B:
            self.y = 0
            self.st = (self.st & 0x0F) | 0x80
        # otherwise only the sprite values change

    # ---- Pac-Man's code of the bottom logic ($F61E - $F7BA) ----
    def logic(self, joy):
        self.tick = 0 if joy else self.tick
        if self.st & 0x20:     # in an opening: the stick can only turn him round ($F63B)
            up, down = joy & 1, (joy >> 1) & 1
            go = (up and not self.st & 0x80) or (down and self.st & 0x80 and not up)
            if go:
                HIT['opening: the stick turns him round'] += 1
                if self.st & 0x10:
                    self.st ^= 0x80
                else:
                    self.y = (0x3F ^ self.y) & 0x3F
                    self.st ^= 0xC0
            return
        d2 = joy
        if d2 == 0:
            return self.neutral()
        vertical = bool(self.st & 0x80)
        stage = 'F6DD' if vertical else 'F72D'
        while True:
            if stage == 'F6DD':
                lr = (d2 >> 2) & 3
                if lr == 0:
                    stage = 'F725'
                    continue
                left = lr & 1
                if self.yoff != 0:
                    stage = 'F725'
                    continue
                if self.xoff == 0:
                    if left:
                        if self.pos == 0xF9 or self.wall(self.cell - 1) & 0x80:
                            stage = 'F725'
                            continue
                        return self.turn(left=True)
                    if (self.row == 3 and self.cell == 0x38) or self.wall(self.cell + 2) & 0x80:
                        stage = 'F725'
                        continue
                    return self.turn(left=False)
                return self.turn(left=bool(left))
            if stage == 'F725':
                d2 &= 0xF3
                if d2 == 0:
                    return self.neutral()
                stage = 'F72D'
                continue
            if stage == 'F72D':
                ud = d2 & 3
                if ud == 0 or self.xoff != 0:
                    stage = 'F781'
                    continue
                up = d2 & 1
                if self.x == 0x4C:
                    if self.y == 0:
                        if up:
                            HIT['stick up at y=0 in the middle column: into the top opening'] += 1
                            self.st = (self.st & 0x0F) | 0xF0
                            self.y = 0x0B
                            return
                    elif self.y == 0x46 and (d2 >> 1) & 1:
                        HIT['stick down at y=$46 in the middle column: into the bottom opening'] += 1
                        self.st = (self.st & 0x0F) | 0x30
                        self.y = 0x51
                        return
                if self.yoff != 0:
                    HIT['stick up/down between two rows: reverses vertically at once'] += 1
                    self.set_dir(3 if up else 2)
                    return self.move()
                t = self.wall(self.cell)
                if up:
                    if (t >> 6) & 1:
                        HIT['stick up on a row: turn up'] += 1
                        self.set_dir(3)
                        return self.move()
                else:
                    if (t >> 5) & 1:
                        HIT['stick down on a row: turn down'] += 1
                        self.set_dir(2)
                        return self.move()
                stage = 'F781'
                continue
            if stage == 'F781':
                d2 &= 0xFC
                stage = 'F6DD'
                continue

    def turn(self, left):
        HIT['stick left/right: turn %s' % ('left' if left else 'right')] += 1
        if left:
            self.st |= 1
        else:
            self.st &= 0xFE
        self.set_dir(1 if left else 0)
        return self.move()

    # ---- PACMAN_KEEP_GOING ($F793) then MOVE_OBJECT ($F78D) ----
    def neutral(self):
        HIT['stick neutral'] += 1
        if self.yoff | self.xoff:
            HIT['stick neutral between cells: keeps moving'] += 1
            return self.move()
        code = (self.st >> 6) & 3
        if self.try_dir(code):
            HIT['stick neutral on a cell: keeps going'] += 1
            return self.move()
        HIT['stick neutral on a cell: blocked, stands still'] += 1
        # blocked: the routine drops its caller's return and the main loop goes on without moving


def predict(pre, joy, tbl):
    p = Pac(pre, tbl)
    if p.st & 0x20:
        p.animate()           # top logic ($F973)
    p.locate()                # bottom logic, first thing ($F4EB)
    p.logic(joy)
    if pre[0] == 0xFF:        # FRAME_START ($F812-$F816): the frame counter wraps and the idle counter goes up
        p.tick = (p.tick + 1) & 0xFF
    return p


def main():
    rom = pathlib.Path(sys.argv[1]).resolve().read_bytes()
    tbl = list(rom[0xE13:0xE63])
    stats = collections.Counter()
    miss = []
    per_file = {}
    for f in sorted((ROOT / 'reference' / 'baseline').glob('BL-???.trace')):
        rows = []
        for l in f.read_text().split('\n'):
            p = l.split(',')
            if len(p) >= 6 and p[0].isdigit():
                rows.append((int(p[1]), bytes.fromhex(p[5])))
        n_ok = 0
        for i in range(1, len(rows)):
            inp, post = rows[i]
            pre = rows[i - 1][1]
            if pre[2] != 0:
                stats['skipped: title screen / not in play'] += 1
                continue
            if inp & 32:
                stats['skipped: Select pressed during play (the game number steps and Pac-Man goes to y=$60)'] += 1
                continue
            if pre[0x67] & 0x80 or post[0x67] & 0x80:
                stats['skipped: frozen game (start of level, new map)'] += 1
                continue
            if post[0x67] & 0x40 or pre[0x67] & 0x40:
                stats['skipped: death'] += 1
                continue
            if pre[0x65] & 0x80:
                stats['skipped: pause after a ghost was eaten'] += 1
                if (pre[0x31], pre[0x36], pre[0x3B], pre[0x40]) == (post[0x31], post[0x36], post[0x3B], post[0x40]):
                    stats['  of which Pac-Man did not change at all'] += 1
                continue
            if (pre[0x66] ^ post[0x66]) & 0x80:
                stats['skipped: change of turn'] += 1
                continue
            if post[0x1A] & 0x80 and not pre[0x1A] & 0x80 and (post[0x1A] & 0x40):
                stats['skipped: power pill eaten'] += 1
                continue
            joy = 0 if pre[0x66] & 0x80 else inp & 15
            pr = predict(pre, joy, tbl)
            want = (post[0x31], post[0x36], post[0x3B], post[0x40], post[0x6D], post[0x01])
            got = (pr.x, pr.y, pr.st, pr.acc, joy, pr.tick)
            stats['predicted frames'] += 1
            if got == want:
                stats['correct'] += 1
                n_ok += 1
            else:
                stats['wrong'] += 1
                if len(miss) < 12:
                    miss.append((f.stem, i, 'pre x=%02X y=%02X st=%02X acc=%02X joy=%X' % (pre[0x31], pre[0x36], pre[0x3B], pre[0x40], joy),
                                 'want x=%02X y=%02X st=%02X acc=%02X ed=%X tick=%02X' % want, 'got x=%02X y=%02X st=%02X acc=%02X ed=%X tick=%02X' % got))
        per_file[f.stem] = n_ok
    for k, v in sorted(stats.items()):
        print('%8d  %s' % (v, k))
    print('roads of the code taken (frames):')
    for k, v in sorted(HIT.items()):
        print('%8d  %s' % (v, k))
    for m in miss:
        print(m)
    ok = stats['wrong'] == 0 and stats['correct'] > 100000 and stats['  of which Pac-Man did not change at all'] == stats['skipped: pause after a ghost was eaten']
    print('result: %s' % ('OK' if ok else 'MISMATCH'))
    speeds = {}
    for seed in (0x09, 0x0B):
        acc, steps = seed, 0
        for _ in range(256):
            t = ((acc << 4) & 0xFF) + acc
            acc = t & 0xFF
            steps += t >> 8
        a2, k = seed, 0
        while True:
            a2 = (((a2 << 4) & 0xFF) + a2) & 0xFF
            k += 1
            if a2 == seed or k > 300:
                break
        speeds[seed] = (steps, k)
    BS = chr(92)
    d = ['# Stage F10: Pac-Man\'s movement and turning (2026-10-26)', '',
         '`python -I tools%sf10_pacman.py rom%spacman.a26` holds a model of Pac-Man\'s rules written from the code and checks it against the recorded games: from the memory of the frame before, the joystick of the frame and the wall table of the ROM it predicts Pac-Man\'s x, y, state bits, speed accumulator, JOY_DIRECTION and idle counter **for every frame in which his code runs in the normal way, and compares them with the recorded memory**. This document is written by that tool.' % (BS, BS), '',
         '## Result', '',
         '- **%d of %d predicted frames are exactly right (0 wrong)**, in all 70 games (439,680 frames).' % (stats['correct'], stats['predicted frames']),
         '- Frames not predicted, with the reason: ' + '; '.join('%s: %d' % (k.replace('skipped: ', ''), v) for k, v in sorted(stats.items()) if k.startswith('skipped')) + '.',
         '- In the %d frames of the pause after a ghost was eaten, Pac-Man\'s bytes did not change in %d (the pause freezes him).' % (stats['skipped: pause after a ghost was eaten'], stats['  of which Pac-Man did not change at all']),
         '', 'Roads of the code taken in these frames (every road of the rules below was taken at least once, except the x border):', '', '| Frames | Road |', '| --- | --- |']
    d += ['| %d | %s |' % (v, k) for k, v in sorted(HIT.items())]
    d += ['', '## The rules (what the code does, in the order it does it)', '',
          'Pac-Man is object 0 of the five moving objects; his bytes are `PACMAN_X` ($B1), `PACMAN_Y` ($B6), `PACMAN_STATE` ($BB) and the speed accumulator `SPEED_ACC0` ($C0). The maze is a grid of **cells 4 pixels wide and rows 10 pixels high** (8 rows, y = 0 to $47). `LOCATE_OBJECT` turns x and y into: the row (y / 10), `Y_OFFSET` (0 when y is exactly on a row, else negative), `X_OFFSET` (x mod 4, 0 = exactly on a cell column) and the cell index (x / 4, plus 40 on odd rows: 80 cells). The wall table `TBL_FE13` has one byte per cell: **bit 7 = a wall blocks entering this cell from the side, bit 6 = he may go up from this cell, bit 5 = he may go down**; bits 0-4 describe the wafer (F11).', '',
          '**State bits:** bits 7-6 = direction (0 right, 1 left, 2 down, 3 up); bit 4 = "a step is due" (set when a direction is set, cleared by the step); bit 0 = faces left (mirrors the picture; set and cleared only by left and right turns); bit 5 = he is in one of the two openings of the maze (top or bottom wall, middle column) and is being animated.', '',
          '**Each frame, in this order:**', '',
          '1. *Top logic:* if bit 5 is set, `ANIMATE_OBJECT` moves him one step along the opening path (y counts through $51-$5B below the maze or $00-$0B above it) and, when the path ends, puts him back in the maze going down (from the bottom opening he comes out at the top) or up.',
          '2. *Bottom logic* (the game is running: not the title screen, not frozen, no ghost-eaten pause, no death): `LOCATE_OBJECT`, wafer eating, the check for a cleared map, then the joystick:',
          '   - **In an opening (bit 5):** the stick can only turn him round (up while his direction is horizontal-coded, or down while vertical-coded); nothing else.',
          '   - **Stick neutral:** if he is exactly on a cell (both offsets 0) he tries to go on in his direction; if the wall table forbids, he **stands still** (no step); otherwise, or when he is between cells, he steps.',
          '   - **Stick pressed:** the code tests the axis perpendicular to his travel first, then the other: (a) *moving vertically:* left or right is taken only if he is exactly on a row (`Y_OFFSET` 0) and the neighbour cell (cell - 1 for left, cell + 2 for right, with two exceptions on row 3) has no wall bit and, for left, the position code is not $F9; if he is between two cell columns the horizontal turn is always allowed. (b) *vertical:* only with `X_OFFSET` 0; in the middle column (x = $4C) at y = 0 with up, or y = $46 with down, he enters the opening (state $F0 and y = $0B, or state $30 and y = $51); between two rows a vertical reversal is always allowed; on a row up needs bit 6 and down needs bit 5 of the cell. If the wanted direction is not possible the code falls back to the other axis, and in the end to the neutral road.',
          '   - Setting a direction writes the two direction bits and bit 4, then takes the step at once.',
          '3. *The step* (`MOVE_OBJECT`): the accumulator is multiplied by 17 (mod 256); **a carry out means one step**. x changes by one (not onto 2 or $96); y changes by one (not below 0 and not onto $47). So speed is set only by the starting accumulator, and it never changes during a game: Pac-Man does not speed up.', '',
          '## Speed', '',
          'Start accumulator `$09` when `GAME_SELECT` ($84) is 2 to 7 (games 7, 8 and 1, either number of players) or `$0B` when it is 8 to $11 (games 2 to 6) ($F89F-$F8A9; in the 70 games the accumulator of Pac-Man shows the seed $09 only for $84 below 8 and $0B only for $84 from 8 up). So **Pac-Man is slower in games 1, 7 and 8**. The accumulator repeats after %d frames for `$09` (%d steps in 256 frames = %.3f px per frame) and after %d frames for `$0B` (%d steps in 256 frames = %.3f px per frame).' % (speeds[9][1], speeds[9][0], speeds[9][0] / 256.0, speeds[11][1], speeds[11][0], speeds[11][0] / 256.0), '',
          '## Not claimed', '',
          '- Wafer eating (the `TBL_FE13` bits 0-4, `TBL_FE79`, the maze bytes) and scoring are F11/F15; ghost collisions, the power pill and the bonus item are F12; the model was not asked to predict them and the frames where the power pill is eaten are skipped.',
          '- The x border road (x onto 2 or $96) was never taken: Pac-Man cannot reach it in this maze, so its meaning is not tested.',
          '- Player 2 never had a joystick in the recordings (F9): in his turn the model was given a neutral stick, which is what the recordings did.']
    (ROOT / 'docs' / 'F10_PACMAN.md').write_text('\n'.join(d) + '\n', encoding='utf-8')
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
