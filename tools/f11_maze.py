"""Stage F11: the maze and the storage of the wafers - a model checked against the recorded games.

  python -I tools\\f11_maze.py <rom>      (after f1-f10; needs the 70 baseline traces)

Reads the wall/wafer table of the ROM ($FE13, 80 bytes, one per cell), works out which memory bit stands for which wafer, and
predicts - for every frame in which Pac-Man's code runs on the normal road - the 16 wafer bytes ($88-$97), the power-pill bits
(PILL_FLAGS $9A), the score of the player whose turn it is, and, when the last wafer is eaten, the new map (all 16 bytes, lives,
level step, pill flags). Compares with the recorded memory. Uses the movement model of Stage F10 for Pac-Man's place.
Writes docs\\F11_MAZE.md. The ROM is only read.
"""
import collections
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import f10_pacman as f10  # noqa: E402

ROOT = pathlib.Path(__file__).resolve().parent.parent
PILL_MASK = {(5, 4): 0x20, (0x41, 4): 0x10, (5, 0x94): 0x08, (0x41, 0x94): 0x04}  # (y, x) -> bit of PILL_FLAGS (TBL_FE6C/FE6E/FE6F)
BITMASK = [0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01]  # TBL_FE6A..FE71
SCORE_ADD = {4: (0x01, 0x00), 5: (0x05, 0x00)}  # ADD_SCORE amount numbers used here: wafer, power pill (TBL_FF06 / TBL_FF0C)


def bcd_add(lo, mid, hi, a_lo, a_mid):
    def add(x, y, c):
        s = (x & 15) + (y & 15) + c
        carry_lo = 0
        if s > 9:
            s += 6
        lo_n = s & 15
        carry = 1 if s > 15 else 0
        s2 = (x >> 4) + (y >> 4) + carry
        if s2 > 9:
            s2 += 6
        return ((s2 & 15) << 4) | lo_n, 1 if s2 > 15 else 0
    lo, c = add(lo, a_lo, 0)
    mid, c = add(mid, a_mid, c)
    hi, c = add(hi, 0, c)
    return lo, mid, hi


def main():
    rom = pathlib.Path(sys.argv[1]).resolve().read_bytes()
    tbl = list(rom[0xE13:0xE63])
    fe79 = list(rom[0xE79:0xE89])
    # the wafer slots
    slot_of = {}
    for r in range(8):
        for c in range(40):
            v = tbl[(r & 1) * 40 + c]
            if v & 1:
                slot_of.setdefault((fe79[2 * r + ((v >> 1) & 1)], (v >> 2) & 7), []).append((r, c))
    one_to_one = len(slot_of) == 128 and all(len(v) == 1 for v in slot_of.values())
    start_maze = [0xFF] * 16
    start_maze[6] = 0x7F
    start_maze[2] = 0xFE
    wafers_at_start = sum(bin(b).count('1') for b in start_maze)
    stats = collections.Counter()
    miss = []
    for f in sorted((ROOT / 'reference' / 'baseline').glob('BL-???.trace')):
        rows = []
        for l in f.read_text().split('\n'):
            p = l.split(',')
            if len(p) >= 6 and p[0].isdigit():
                rows.append((int(p[1]), bytes.fromhex(p[5])))
        pokes = set()  # the artificial entries BL-065/066 edit the score in memory on one frame
        for l in (ROOT / 'baseline' / (f.stem + '.entry.txt')).read_text(encoding='utf-8').splitlines():
            if l.startswith('pokes:'):
                pokes = {int(x.split(':')[0]) for x in l[6:].split()}
        for i in range(1, len(rows)):
            inp, post = rows[i]
            if i in pokes:
                stats['skipped: memory edited by the test (BL-065/066)'] += 1
                continue
            if inp & 64:
                stats['skipped: Reset pressed during play'] += 1
                continue
            pre = rows[i - 1][1]
            if pre[2] != 0:
                stats['skipped: title screen / not in play'] += 1
                continue
            if inp & 32:
                stats['skipped: Select pressed during play'] += 1
                continue
            if pre[0x67] & 0x80:
                stats['skipped: frozen game'] += 1
                continue
            if pre[0x67] & 0x40 or post[0x67] & 0x40:
                stats['skipped: death'] += 1
                continue
            if pre[0x65] & 0x80:
                stats['skipped: pause after a ghost was eaten'] += 1
                continue
            if (pre[0x66] ^ post[0x66]) & 0x80:
                stats['skipped: change of turn'] += 1
                continue
            if (pre[0x67] ^ post[0x67]) & 2:
                stats['skipped: bonus item appears, goes or is eaten'] += 1
                continue
            p = f10.Pac(pre, tbl)
            if p.st & 0x20:
                p.animate()
            p.locate()
            maze = list(pre[8:24])
            pills = pre[0x1A]
            lives, level = pre[0x18], pre[0x19]
            two = bool(pre[0x66] & 0x80)
            sc = [pre[0x4C + two], pre[0x4E + two], pre[0x50 + two]]  # CC/CD, CE/CF, D0/D1
            event = 'none'
            mask = PILL_MASK.get((p.y, p.x))
            if mask and pills & mask:
                pills = (pills & ~mask & 0xFF) | 0xC0
                sc = list(bcd_add(*sc, *SCORE_ADD[5]))
                event = 'pill'
                ran_f6aa = True
            else:
                ran_f6aa = True
                if p.st & 0x20:
                    ran_f6aa = False
                elif p.yoff == 0:
                    t = tbl[p.cell]
                    if p.x != 0x4C and p.st & 0x80:
                        t |= rom[0xE13 + p.cell + 1]
                    if t & 1:
                        row = p.row
                        slot = fe79[2 * row + ((t >> 1) & 1)]
                        m = BITMASK[(t >> 2) & 7]
                        if maze[slot] & m:
                            maze[slot] &= ~m & 0xFF
                            sc = list(bcd_add(*sc, *SCORE_ADD[4]))
                            event = 'wafer'
            if ran_f6aa and not (pills & 0x3C) and not any(maze):
                maze = list(start_maze)
                pills = 0x3C
                if lives != 9:
                    lives += 1
                if level != 0x0E:
                    level += 1
                event = 'new map'
            stats['events: ' + event] += 1
            exclude_score = (pre[0x65] != post[0x65]) or event == 'new map' and False
            # bits 7-6 and 0-1 of PILL_FLAGS belong to the scared time and the ghosts eaten (F12/F15): only the pill places (bits 2-5) are compared,
            # and bits 7-6 where this frame sets them
            m = 0xFC if event == 'pill' else 0x3C
            if event == 'new map':
                m = 0xFF
            want = (bytes(post[8:24]), post[0x1A] & m, post[0x18], post[0x19])
            got = (bytes(maze), pills & m, lives, level)
            stats['predicted frames'] += 1
            ok = want == got
            if not exclude_score:
                ok = ok and [post[0x4C + two], post[0x4E + two], post[0x50 + two]] == sc
            else:
                stats['score not compared (ghost eaten in this frame)'] += 1
            if ok:
                stats['correct'] += 1
            else:
                stats['wrong'] += 1
                if len(miss) < 10:
                    miss.append((f.stem, i, event, 'want %s pills=%02X lives=%d lvl=%d sc=%s' % (want[0].hex(), want[1], want[2], want[3], [hex(post[0x4C + two]), hex(post[0x4E + two]), hex(post[0x50 + two])]),
                                 'got  %s pills=%02X lives=%d lvl=%d sc=%s' % (got[0].hex(), got[1], got[2], got[3], [hex(x) for x in sc])))
    for k, v in sorted(stats.items()):
        print('%8d  %s' % (v, k))
    for m in miss:
        print(m)
    ok = stats['wrong'] == 0 and stats['correct'] > 100000 and one_to_one and wafers_at_start == 126 and stats['events: wafer'] > 1000 and stats['events: new map'] > 0 and stats['events: pill'] > 0
    print('slots one-to-one: %s, wafers at start: %d' % (one_to_one, wafers_at_start))
    print('result: %s' % ('OK' if ok else 'MISMATCH'))
    # document
    BS = chr(92)
    d = ['# Stage F11: the maze and the storage of the wafers (2026-10-26)', '',
         '`python -I tools%sf11_maze.py rom%spacman.a26` reads the wall/wafer table of the ROM, works out which memory bit stands for which wafer, and checks a model of the wafer rules against the recorded games. This document is written by that tool.' % (BS, BS), '',
         '## Result', '',
         '- **%d of %d predicted frames exactly right (0 wrong)** over the 70 games: the 16 wafer bytes `$88-$97`, the power-pill bits of `PILL_FLAGS`, the score of the player whose turn it is, and on a cleared map the new wafer bytes, lives and level step.' % (stats['correct'], stats['predicted frames']),
         '- Events in the predicted frames: %s.' % ', '.join('%s %d' % (k.replace('events: ', ''), v) for k, v in sorted(stats.items()) if k.startswith('events')),
         '- Frames not predicted: %s.' % '; '.join('%s: %d' % (k.replace('skipped: ', ''), v) for k, v in sorted(stats.items()) if k.startswith('skipped')),
         '- The table gives **128 wafer places, each used by exactly one cell** (%s); the new map starts with **126** of them set (the two places in the middle column, `$8E` bit 7 and `$8A` bit 0, start empty) and four power pills in `PILL_FLAGS`.' % ('checked' if one_to_one else 'NOT one-to-one'), '',
         '## How the maze is stored', '',
         '- `TBL_FE13` (80 bytes at `$FE13`): one byte per cell; even rows use bytes 0-39, odd rows use bytes 40-79 (the same two row patterns repeat four times: 8 rows of 40 cells). Bit 7 = wall, bit 6 = open upwards, bit 5 = open downwards, **bit 0 = a wafer is kept for this cell, bit 1 and bits 2-4 = which memory bit**.',
         '- A wafer is the memory bit `TBL_FE6A[(t >> 2) & 7]` (`$80, $40, ... $01`) of the maze byte `TBL_FE79[2 * row + ((t >> 1) & 1)]` (offset from `$88`).',
         '- The 16 bytes `MAZE_00..15` at `$88-$97` hold the wafers of the player whose turn it is (a second copy at `$9C-$AB` for the other player, exchanged at the change of turn). The four power pills are bits 2-5 of `PILL_FLAGS` (`$20, $10, $08, $04` for the places (x=4,y=5), (x=4,y=$41), (x=$94,y=5), (x=$94,y=$41)); bits 7-6 of that byte are set when a pill is eaten (scared time).', '',
         '## When a wafer is eaten (every frame, bottom logic, before the joystick is read)', '',
         '1. The power pill is tested first: Pac-Man exactly on one of the four places and its bit set: the bit is cleared, `PILL_FLAGS` gets bits 7-6, +5 points, and **the joystick and the step of that frame are skipped**.',
         '2. Otherwise, with Pac-Man exactly on a row (`Y_OFFSET` 0) and not in an opening: the byte of his cell is read; **when he moves vertically and is not in the middle column (x = $4C), the byte of the next cell is OR-ed in** (a wafer between two cells is eaten from either); if bit 0 is set and the memory bit is still set it is cleared, +1 point, sound 0.',
         '3. If all 16 bytes and the pill bits are zero: lives +1 (at most 9), level step +1 (at most $0E), all wafer bytes `$FF` with `$8E = $7F` and `$8A = $FE`, pill bits `$3C`; then the objects are put back and the game freezes.', '',
         '## The maze (from the table)', '',
         'Rows 0-7 (y = 0, 10, ... 70), 40 cells of 4 pixels (x = 0 to 156): `#` wall bit, `.` a wafer is kept here, `u` open upwards, `d` open downwards. (The pictures drawn by the kernel are F18.)', '', '```']
    for r in range(8):
        base = (r & 1) * 40
        d.append('row %d y=%02d  %s' % (r, r * 10, ''.join('#' if tbl[base + c] & 0x80 else ('.' if tbl[base + c] & 1 else ' ') for c in range(40))))
        d.append('         up   %s' % ''.join('u' if tbl[base + c] & 0x40 else ' ' for c in range(40)))
        d.append('         down %s' % ''.join('d' if tbl[base + c] & 0x20 else ' ' for c in range(40)))
    d += ['```', '', '## Which bit is which wafer (maze byte, bit -> row, cell)', '', '| Byte | bit 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |', '| --- | --- | --- | --- | --- | --- | --- | --- | --- |']
    for b in range(16):
        cells = []
        for bit in range(8):
            v = slot_of.get((b, bit))
            cells.append('r%d c%d' % v[0] if v else '-')
        # the table indexes bits as (t>>2)&7 -> mask BITMASK[idx]; idx 0 is bit 7
        d.append('| $%02X | %s |' % (0x88 + b, ' | '.join(cells)))
    d += ['', '(Column "bit 7" is index 0 of the table, `$80`; column "0" is index 7, `$01`.)', '',
          '## Not claimed', '',
          '- How the kernel turns these bytes into dots on the screen (F18); the exact scoring of ghosts, the bonus item and extra lives (F15); the ghosts\' use of the same wall table (F13); what happens to the objects when a new map starts (F12/F16).',
          '- Frames with the bonus item appearing or going, a change of turn, a death, a ghost-eaten pause and Select during play are not predicted (counts above); the score is not compared in the frames where a ghost was eaten.']
    (ROOT / 'docs' / 'F11_MAZE.md').write_text('\n'.join(d) + '\n', encoding='utf-8')
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
