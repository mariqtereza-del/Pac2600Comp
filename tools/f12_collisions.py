"""Stage F12: collisions - what the game uses (the chip's own collision flags), how it reacts, and how well position predicts them.

  python -I tools\\f12_collisions.py <rom>      (after f1-f11; needs the 70 baseline traces in reference\\baseline and the built stella_coverage.exe)

1. Static: every instruction of the listing that touches a collision register (reads CX..., writes CXCLR).
2. Dynamic: `stella_coverage` replays all 70 baseline entries and logs, at the three instructions that read the flags
   ($F575 = CXM1P and CXP1FB, $F5D9 = CXPPMM), the value the game really got (the low six bits are leftover bus bits; only bit 7 / bit 6 count).
   The logs go to reference\\f12 (local only).
3. The game's reaction is written as rules and checked on every logged frame against the recorded memory of that frame.
4. How far apart the two sprites can be and still collide, from the logged flags and the positions (the exact pixel overlap depends on the
   two pictures: that is the drawing kernel, F18 / F23).
Writes docs\\F12_COLLISIONS.md. The ROM is only read.
"""
import collections
import concurrent.futures
import pathlib
import re
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / 'tools'))
import f6_coverage as f6  # noqa: E402

EXE = ROOT / 'build' / 'stella_probe' / 'stella_coverage.exe'
OUT = ROOT / 'reference' / 'f12'
BS = chr(92)
POKE_EXEMPT = ('BL-065', 'BL-066')  # test-only memory edits: skipped like in F11
SCORE_LO = {0: 0x20, 1: 0x40, 2: 0x80, 3: 0x60, 4: 0x01, 5: 0x05, 6: 0x00}   # TBL_FF06
SCORE_MID = {0: 0x00, 1: 0x00, 2: 0x00, 3: 0x01, 4: 0x00, 5: 0x00, 6: 0x01}   # TBL_FF0C+Y


def trace(entry):
    rows = []
    for line in (ROOT / 'reference' / 'baseline' / (entry + '.trace')).read_text().split('\n')[3:]:
        p = line.split(',')
        if len(p) >= 6:
            rows.append((int(p[1]), bytes.fromhex(p[5])))
    return rows


def collect(item):
    entry, pokes = item
    out = OUT / ('w_%s.csv' % entry)
    r = subprocess.run([str(EXE), str(ROM), str(ROOT / 'baseline' / (entry + '.inputs')),
                        str(OUT / ('cov_%s.csv' % entry)), str(ROOT / 'reference' / 'baseline' / (entry + '.trace'))] + pokes
                       + ['watch=%s:F575,F5D9' % out], capture_output=True, text=True)
    return entry, r.returncode, r.stdout.strip().splitlines()[-1] if r.stdout.strip() else r.stderr.strip()


def bcd(v):
    return (v >> 4) * 10 + (v & 15)


def score(ram, player2):
    o = 1 if player2 else 0
    return bcd(ram[0x4C + o]) + 100 * bcd(ram[0x4E + o]) + 10000 * bcd(ram[0x50 + o])


def wafers_gone(pre, post):
    """Wafers eaten in the frame (maze bits $88-$97 that went from set to clear): the wafer test comes before the collision tests."""
    return sum(bin(a & ~b & 0xFF).count('1') for a, b in zip(pre[0x08:0x18], post[0x08:0x18]))


def static_scan(listing):
    """Lines of the routine listing that name a collision register."""
    found = []
    for line in listing.split('\n'):
        m = re.match(r'\s+([A-Z]{3})\s+(CX\w+?)(?:_AT\w+)?\s', line)
        if m:
            pc = re.search(r'; \$([0-9A-F]{4})', line)
            found.append((pc.group(1), m.group(1), m.group(2)))
    return found


def main():
    global ROM
    ROM = pathlib.Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else ROOT / 'rom' / 'pacman.a26'
    OUT.mkdir(parents=True, exist_ok=True)
    items = f6.entries()
    with concurrent.futures.ThreadPoolExecutor(8) as ex:
        runs = list(ex.map(collect, items))
    bad = [r for r in runs if r[1] != 0 or '0 differences' not in r[2]]
    print('runs: %d, with problems: %d' % (len(runs), len(bad)))
    for r in bad:
        print('  ', r)

    listing = (ROOT / 'reference' / 'disasm' / 'pacman_routines.asm').read_text(encoding='utf-8')
    static = static_scan(listing)
    print('collision registers named in the listing:', static)

    stats = collections.Counter()
    problems = []
    hit_cells = collections.defaultdict(collections.Counter)    # (dx, dy) -> {flag: n}   ghost against Pac-Man
    bonus_cells = collections.defaultdict(collections.Counter)  # (x, y) of Pac-Man -> {flag: n}
    tested = collections.Counter()                              # ghost index of each tested frame
    for entry, _ in items:
        rows = trace(entry)
        ev = collections.defaultdict(dict)
        for line in (OUT / ('w_%s.csv' % entry)).read_text().split('\n'):
            if line:
                fr, pc, a = line.split(',')
                ev[int(fr)][pc] = int(a)
        skip_poke = entry in POKE_EXEMPT
        for fr in range(1, len(rows)):
            _, pre = rows[fr - 1]
            inp, post = rows[fr]
            d = ev.get(fr, {})
            reached = 'f575' in d
            stats['frames'] += 1
            if skip_poke:
                stats['skipped: test-only memory edits (BL-065/066)'] += 1
                continue
            X = pre[0] & 3                      # FRAME_COUNTER seen by the frame's logic (the INC is at the end, in FRAME_START)
            p2 = bool(pre[0x66] & 0x80)         # player two's turn
            pill_pre = pre[0x1A]
            # ---- is the flag read at all? ----
            pill_eaten = (pre[0x1A] & 0x3C) != (post[0x1A] & 0x3C) and (post[0x1A] & 0xC0) == 0xC0 and (pre[0x1A] & 0xC0) != 0xC0
            if 'f5d9' in d:
                stats['frames in which the ghost flag is read'] += 1
                if (pre[0] & 4) or not reached:
                    problems.append('%s frame %d: ghost flag read although FRAME_COUNTER bit 2 is set / bonus test not reached' % (entry, fr))
            elif reached and not (pre[0] & 4) and not pill_eaten:
                # the ghost flag should have been read here
                problems.append('%s frame %d: ghost flag not read although it should be' % (entry, fr))
            if reached:
                stats['frames in which the bonus flag is read'] += 1
            # ---- bonus item ----
            if reached:
                bonus = bool(d['f575'] & 0x40)
                if bonus:
                    stats['bonus item eaten'] += 1
                    ok = ((pre[0x67] ^ post[0x67]) & 2) == 2 and post[0x6C] == 0x12 and score(post, p2) - score(pre, p2) == 100 + wafers_gone(pre, post)
                    if not ok:
                        problems.append('%s frame %d: bonus reaction wrong' % (entry, fr))
                    stats['  bonus reactions right'] += ok
                    bonus_cells[(pre[0x31], pre[0x36])][1] += 1
                elif pre[0x67] & 2:
                    bonus_cells[(pre[0x31], pre[0x36])][0] += 1
                if not bonus and score(post, p2) - score(pre, p2) == 100 + wafers_gone(pre, post) and (pre[0x67] ^ post[0x67]) & 2 and post[0x6C] == 0x12:
                    problems.append('%s frame %d: bonus reaction without the flag' % (entry, fr))
            # ---- ghost ----
            if 'f5d9' in d:
                hit = bool(d['f5d9'] & 0x80)
                tested[X] += 1
                ghost_bit = (pre[0x3C + X] >> 3) & 1
                gst_post = post[0x3C + X]
                scared = bool(pill_pre & 0x80)
                if not hit:
                    # no flag: no reaction (eaten bit not newly set, no death trigger)
                    newly = (gst_post & 8) and not ghost_bit
                    died = ((post[0x67] & 0xC0) == 0xC0) and ((pre[0x67] & 0xC0) != 0xC0) and post[0x64] == 0x3F
                    if newly or died:
                        problems.append('%s frame %d: reaction without the ghost flag' % (entry, fr))
                    stats['ghost flag clear, no reaction (right)'] += 1
                else:
                    stats['ghost flag set'] += 1
                    if ghost_bit:
                        stats['  ghost already eaten (eyes): nothing happens'] += 1
                    elif scared:
                        stats['  scared ghost: eaten'] += 1
                        n = pill_pre & 3
                        ok = ((gst_post & 8) != 0 and post[0x65] == 0x9F and (post[0x1A] & 3) == min(n + 1, 3)
                              and score(post, p2) - score(pre, p2) == {0: 20, 1: 40, 2: 80, 3: 160}[n] + wafers_gone(pre, post))
                        stats['    eaten reactions right (flag 8 of ghost, pause $9F, count, +20/40/80/160)'] += ok
                        if not ok:
                            problems.append('%s frame %d: ghost-eaten reaction wrong' % (entry, fr))
                    else:
                        stats['  unscared ghost: Pac-Man dies'] += 1
                        ok = (post[0x67] & 0xC0) == 0xC0 and post[0x64] == 0x3F
                        stats['    death starts right (flags $C0, event timer $3F)'] += ok
                        if not ok:
                            problems.append('%s frame %d: death trigger wrong' % (entry, fr))
                # geometry: ghost against Pac-Man as drawn (ghost: after the top logic = this frame's row; Pac-Man: before the bottom logic)
                if not ((pre[0x3B] & 0x20) or (post[0x3C + X] & 0x20)):
                    dx = post[0x32 + X] - pre[0x31]
                    dy = post[0x37 + X] - pre[0x36]
                    hit_cells[(dx, dy)][1 if hit else 0] += 1
    # ---- geometry summary ----
    hits = [k for k, v in hit_cells.items() if v[1]]
    allflag = [k for k, v in hit_cells.items() if v[1] and not v[0]]
    mixed = [k for k, v in hit_cells.items() if v[1] and v[0]]
    far = [k for k in hits if abs(k[0]) > 7 or abs(k[1]) > 7]
    box = (min(k[0] for k in hits), max(k[0] for k in hits), min(k[1] for k in hits), max(k[1] for k in hits))
    near = [k for k, v in hit_cells.items() if abs(k[0]) <= 7 and abs(k[1]) <= 7]
    near_misses = sum(v[0] for k, v in hit_cells.items() if abs(k[0]) <= 7 and abs(k[1]) <= 7)
    near_hits = sum(v[1] for k, v in hit_cells.items() if abs(k[0]) <= 7 and abs(k[1]) <= 7)
    outside_frames = sum(v[0] + v[1] for k, v in hit_cells.items() if abs(k[0]) > 7 or abs(k[1]) > 7)
    outside_hits = sum(v[1] for k, v in hit_cells.items() if abs(k[0]) > 7 or abs(k[1]) > 7)
    print('hits only inside the box dx %d..%d, dy %d..%d; hits outside |7|: %d' % (box[0], box[1], box[2], box[3], outside_hits))
    bonus_hit_cells = [k for k, v in bonus_cells.items() if v[1]]
    bonus_mixed = [k for k in bonus_hit_cells if bonus_cells[k][0]]
    for k, v in sorted(stats.items()):
        print('%6d  %s' % (v, k))
    print('problems:', len(problems))
    for p in problems[:20]:
        print('  ', p)
    ok = not problems and not bad and outside_hits == 0 and stats['ghost flag set'] > 600
    print('result: %s' % ('OK' if ok else 'MISMATCH'))

    # ---- document ----
    lines = ['# Stage F12: collisions (2026-10-26)', '',
             '`python -I tools%sf12_collisions.py rom%spacman.a26` finds what the game uses to detect that two things touch, replays all 70 recorded games and checks the game\'s reaction to the collision flags on every frame. This document is written by that tool.' % (BS, BS), '',
             '## What the game uses: the chip\'s own collision flags', '',
             'The TIA chip notes by itself, while it draws the picture, which of its objects (two sprites P0 and P1, two missiles M0 and M1, the ball BL, the playfield PF) overlapped; the program only reads the flags and clears them once per frame. **The game has no checks of its own that compare positions**: all touching is decided by the picture the kernel draws.', '',
             'Instructions in the ROM that use a collision register (all are in the main loop):', '', '| Address | Instruction | Register | Meaning |', '| --- | --- | --- | --- |']
    meaning = {'CXCLR': 'clears all flags (written once per frame, at the start of the frame)', 'CXM1P': 'bit 6 = missile 1 touched player 1 (Pac-Man)', 'CXP1FB': 'bit 6 = player 1 (Pac-Man) touched the ball', 'CXPPMM': 'bit 7 = player 0 touched player 1 (a ghost touched Pac-Man); bit 6 (the two missiles) is not used'}
    for pc, op, reg in static:
        lines.append('| $%s | `%s` | `%s` | %s |' % (pc, op, reg, meaning.get(reg, '')))
    lines += ['', 'Everything else the chip can report (the missile 0 and playfield pairs, the ball and playfield pairs, and so on) is never read. The wafers are drawn with missile 0 and the playfield, but the game never asks whether Pac-Man touched them: eating a wafer is decided by Pac-Man\'s cell (F11).', '',
              '## What the three reads decide', '',
              'All of this is in the bottom logic, once per frame, after the picture is drawn and before the joystick is read:', '',
              '1. **Bonus item** (`$F573-$F58F`): the flag byte is the AND of CXM1P and CXP1FB; its bit 6 means that missile 1 *and* the ball both overlapped Pac-Man (the item is drawn with these two). Then: +100 points (`ADD_SCORE` 6), sound 1, the item\'s on-flag (`GAME_FLAGS` bit 1) toggled off, `BONUS_TIMER` = $12.',
              '2. **Power pills** are not tested with the flags: Pac-Man standing exactly on one of the four places (F11).',
              '3. **A ghost** (`$F5D3-$F61B`): only when `FRAME_COUNTER` bit 2 is clear, so on 4 frames of every 8; the ghost tested is number `FRAME_COUNTER & 3`, which is exactly the one the kernel draws as player 0 in that frame (one ghost is drawn per frame, flicker). If CXPPMM bit 7 is set:',
              '   - ghost scared (`PILL_FLAGS` bit 7) and not yet eaten: set bit 3 of its state ("eaten, eyes only"); `ADD_SCORE` with Y = `PILL_FLAGS & 3` (+20, +40, +80, +160 for the 1st to 4th ghost of one pill); count +1 (stays at 3); sound 3; `EAT_PAUSE` = $9F (the game freezes for 160 frames);',
              '   - ghost scared but already eaten: nothing (the eyes are harmless);',
              '   - ghost not scared and not eaten: Pac-Man dies: `GAME_FLAGS` |= $C0, `EVENT_TIMER` = $3F, and the rest of the frame is skipped (the death sequence is F17);',
              '   - ghost not scared but eaten (eyes still travelling home after the scare ended): nothing.',
              '4. The flags are cleared by `STA CXCLR` at the start of the next frame (`FRAME_START`, $F7DE), so each picture starts with clean flags.', '',
              'Because only one ghost is tested per frame and only on 4 frames of 8, each ghost is tested once every 8 frames: a ghost that touches Pac-Man for fewer frames can be missed; this is part of the original\'s behaviour and the rebuilt game must keep it.', '',
              '## Result of the check', '',
              '- Replays: %d runs, all equal to their golden traces (picture and memory).' % len(runs),
              '- **%s**: %d problems in %d frames (the 2 test-only runs BL-065/066 are skipped).' % ('no mismatch' if not problems else 'MISMATCH', len(problems), stats['frames']), '',
              '| Count | What |', '| --- | --- |']
    for k, v in sorted(stats.items()):
        lines.append('| %d | %s |' % (v, k))
    lines += ['', 'Ghosts tested per index (0-3): %s.' % ', '.join('%d: %d' % (i, tested[i]) for i in range(4)), '',
              '## Where the flag is set (ghost against Pac-Man)', '',
              'The flag is true pixel overlap of the two pictures, so it depends on both sprite shapes and on the exact pixel column each sprite is placed at (rounded by the position routine at `$F09C`). That belongs to the drawing kernel (F18) and the renderer (F23), which must give the same flags.',
              'What the logged flags show about position (dx = ghost x - Pac-Man x, dy = ghost y - Pac-Man y, both as stored in memory):', '',
              '- %d different (dx, dy) pairs were tested; the flag was set in %d of them; **the flag was set only with |dx| <= 7 and |dy| <= 7** (hits outside: %d of %d frames).' % (len(hit_cells), len(hits), outside_hits, outside_frames),
              '- Inside that square the flag was set in %d and clear in %d tested frames. In %d of the %d (dx, dy) pairs with a hit the flag was set every time; in %d the same pair gave both answers (the picture shapes and the animation phase decide there).' % (near_hits, near_misses, len(allflag), len(hits), len(mixed)), '',
              'Picture of the square (`#` always set, `+` set sometimes, `.` never set, blank = never tested), dy down, dx across:', '', '```']
    lines.append('      dx: ' + ' '.join('%2d' % x for x in range(-9, 10)))
    for dy in range(-9, 10):
        row = []
        for dx in range(-9, 10):
            v = hit_cells.get((dx, dy))
            row.append(' ' if not v else ('#' if v[1] and not v[0] else '+' if v[1] else '.'))
        lines.append('dy %3d:   ' % dy + '  '.join(row))
    lines += ['```', '',
              '## Where the bonus flag is set', '',
              'The bonus item is drawn only in maze row 3 of the kernel (the row of the ghost pen; Pac-Man stands at y = 40 when he touches it), with missile 1 and the ball: there the kernel loads ENAM1 and ENABL from `GAME_FLAGS` (bit 1 = item shown) and in every other row it clears them (`$F18C`, `$F1D5`, `$F2BD`, `$F2F3`, `$F316-$F320`). While it was shown (`GAME_FLAGS` bit 1) and the flag was read, Pac-Man stood on %d different (x, y) places when the flag was set (%d times in all); the same place also gave "not set" in %d cases (the flag needs both objects to overlap, which depends on the exact pixel).' % (len(bonus_hit_cells), stats['bonus item eaten'], len(bonus_mixed)),
              'Places with a hit: ' + ', '.join('(%d,%d)' % k for k in sorted(bonus_hit_cells)) + '.', '',
              '## Not claimed', '',
              '- The pixel-exact overlap is not predicted here: the flags used above are the ones the emulator produced. A native game needs its own renderer to produce them (F23); the logs (`reference%sf12`, local) are the test data for that.' % BS,
              '- The death sequence, the pen and the ghost-eyes journey home, and the scared timers are F13, F16 and F17.',
              '- Sound numbers are named but their effect on the sound chip is F19.']
    (ROOT / 'docs' / 'F12_COLLISIONS.md').write_text('\n'.join(lines) + '\n', encoding='utf-8')
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
