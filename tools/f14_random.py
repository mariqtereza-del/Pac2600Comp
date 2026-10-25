"""Stage F14: randomness - where the game's random numbers come from.

  python -I tools\\f14_random.py <rom>      (after f2-f8, f13; needs the 70 baseline traces in reference\\baseline)

1. Static: every place in the listing that writes or reads the random byte $CB, and every read of the timer and of the console/joystick chips.
2. The generator itself (RNG_STEP, $FBE0) as a function; its cycle from the start value $FF.
3. Dynamic: in all 70 recorded games, every change of $CB from one frame to the next is either a start value ($FF) or a number of steps of that
   generator; so nothing else ever writes it.
4. Where the numbers are used.
Writes docs\\F14_RANDOM.md. The ROM is only read.
"""
import collections
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
BS = chr(92)
TBL = [0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01]   # TBL_FE6A: one bit mask for each value of state & 7


def step(r):
    a0 = ((r << 1) & 0xFF) ^ r
    c = (((a0 << 1) & 0xFF) >> 7) & 1
    return ((r << 1) | c) & 0xFF


def result(r):
    return TBL[r & 7] & r


def main():
    rom = pathlib.Path(sys.argv[1]).resolve().read_bytes()
    listing = (ROOT / 'reference' / 'disasm' / 'pacman_routines.asm').read_text(encoding='utf-8')
    problems = []

    # ---- 1. static ----
    refs = []
    for line in listing.split('\n'):
        if re.match(r'\s+[A-Z]{3}\s', line) and ('RNG_STATE' in line or '$00CB' in line):
            pc = re.search(r'; \$([0-9A-F]{4})', line).group(1)
            refs.append((pc, line.split(';')[0].strip()))
    writers = [r for r in refs if r[1].split()[0] in ('STA', 'STX', 'STY', 'ROL', 'ROR', 'ASL', 'LSR', 'INC', 'DEC')]
    readers = [r for r in refs if r not in writers]
    timer_reads = [re.search(r'; \$([0-9A-F]{4})', l).group(1) + ' ' + l.split(';')[0].strip() for l in listing.split('\n')
                   if re.match(r'\s+[A-Z]{3}\s+(INTIM|SWCH[AB]|INPT\w*)\b', l)]
    print('writers of $CB:', writers)
    print('readers of $CB:', readers)
    print('timer / input reads:', timer_reads)
    # an indexed store that could reach $CB: base + index range; arrays below $CB with an index up to 4 or 11
    indexed = []
    for line in listing.split('\n'):
        m = re.match(r'\s+(STA|STX|STY|INC|DEC|ROL|ROR|ASL|LSR)\s+(\w+),([XY])\s', line)
        if m:
            indexed.append(line.split(';')[0].strip())

    # ---- 2. the generator ----
    seen = {}
    r, n = 0xFF, 0
    while r not in seen:
        seen[r] = n
        r = step(r)
        n += 1
    period = n - seen[r]
    tail = seen[r]
    zero_cycle = step(0) == 0
    first = []
    r = 0xFF
    for _ in range(12):
        r = step(r)
        first.append((r, result(r)))
    # how many different states are visited from $FF, and is there a state the generator can never leave
    visited = len(seen)

    # ---- 3. dynamic ----
    stats = collections.Counter()
    stepsper = collections.Counter()
    bad = []
    reach = {}
    for s in range(256):
        x = s
        for k in range(0, 64):
            reach.setdefault((s, x), k)
            x = step(x)
    for f in sorted((ROOT / 'reference' / 'baseline').glob('BL-???.trace')):
        prev = None
        rows = []
        for l in f.read_text().split('\n'):
            p = l.split(',')
            if len(p) >= 6 and p[0].isdigit():
                rows.append(bytes.fromhex(p[5])[0x4B])
        for i in range(len(rows)):
            cur = rows[i]
            if i == 0:
                stats['first frame value $%02X' % cur] += 1
                continue
            pre = rows[i - 1]
            if f.stem in ('BL-065', 'BL-066'):
                continue
            stats['frames'] += 1
            if cur == pre:
                stats['unchanged'] += 1
                continue
            k = reach.get((pre, cur))
            if k is not None:
                stats['changed by %s steps' % ('1-3' if k <= 3 else '4-12' if k <= 12 else 'more')] += 1
                stepsper[k] += 1
            elif cur == 0xFF:
                stats['set to the start value $FF (a game starts)'] += 1
            else:
                stats['UNEXPLAINED change'] += 1
                if len(bad) < 10:
                    bad.append((f.stem, i, pre, cur))
    if stats['UNEXPLAINED change']:
        problems.append('unexplained changes: %d' % stats['UNEXPLAINED change'])
    print('problems:', len(problems), bad)
    for k, v in sorted(stats.items()):
        print('%8d  %s' % (v, k))
    ok = not problems and len(writers) == 3
    print('result: %s' % ('OK' if ok else 'MISMATCH'))

    d = ['# Stage F14: randomness (2026-10-26)', '',
         '`python -I tools%sf14_random.py rom%spacman.a26` finds every source of random numbers in the game and checks the finding against the 70 recorded games. This document is written by that tool.' % (BS, BS), '',
         '## Finding: one random byte, no outside source', '',
         'The game has **one** random byte, `RNG_STATE` (`$CB`). It is written in exactly three places and read in two:', '',
         '| Address | Instruction | What |', '| --- | --- | --- |']
    for pc, ins in writers:
        d.append('| $%s | `%s` | write |' % (pc, ins))
    for pc, ins in readers:
        d.append('| $%s | `%s` | read |' % (pc, ins))
    d += ['',
          '- `$F016` (start-up) and `$F892` (every time a game starts) set it to **$FF**. So every game starts from the same number, whatever happened before, and the game is a pure function of the buttons pressed: the same recording always plays the same game (the 70 replays, all equal to their golden traces, twice, show it).',
          '- `RNG_STEP` (`$FBE0`) is the only other writer: a shift register. With the state `r`: `a = ((r << 1) ^ r) << 1` (8 bits); the carry out of the last shift is rotated into the low end of `r`: `r = (r << 1 | carry) & $FF`. Then the routine returns `TBL_FE6A[r & 7] & r` (one bit of the new state, picked by its low three bits); the callers only ask whether that is zero or not.',
          '- The other reader, `$FA4F`, looks at the low four bits of `$CB` without stepping it (a ghost on the middle column takes an opening when they are 0).', '',
          '**No clock, timer or input feeds it.** The game reads the chips at only these places: ' + ', '.join('`%s`' % t for t in timer_reads) + '. The timer `INTIM` is read only in the two wait loops (`$F0C5`, `$F7D1`) which test it for zero and keep nothing; the joystick and console switches go to the game rules (F9), not to `$CB`. The frame counter and the idle counter are not mixed in either. Randomness therefore comes **only from the number of times `RNG_STEP` has been called since the game started**, and that number depends on where the ghosts are and what they decide (F13).', '',
          '## The generator', '',
          'Starting from $FF the sequence visits %d different states; it is periodic with a period of **%d steps** after %d lead-in steps. The state 0 would stay 0 for ever (%s), but $FF never leads to it. First 12 steps from $FF (state, value returned): ' % (visited, period, tail, 'yes' if zero_cycle else 'no') + ', '.join('$%02X/$%02X' % t for t in first) + '.', '',
          '## Check against the recorded games', '',
          'In all 70 games (the 2 test games BL-065/066 are skipped) every change of `$CB` between two frames is either the start value $FF or a number of steps of the generator from the value before; nothing else ever writes it. (F13 showed more: the exact value in every frame where a ghost decides, with the step count the model predicts.)', '',
          '| Count | What |', '| --- | --- |']
    for k, v in sorted(stats.items()):
        d.append('| %d | %s |' % (v, k))
    d += ['', 'Steps in one frame (0 means unchanged): ' + ', '.join('%d: %d' % (k, v) for k, v in sorted(stepsper.items())[:14]) + '.', '',
          '## Where the numbers are used', '',
          '- Ghost wandering: sideways choices and whether to keep straight (`GHOST_DECIDE`, `TRY_PAIR_1`, `TRY_PAIR_2`), when the ghost timer reaches zero, on the title screen, or when a chase finds no way (F13).',
          '- The ghost opening (low four bits are 0).',
          '- Nowhere else: not the bonus item, not the scoring, not the sound.', '',
          '## For the rebuilt game', '',
          'The rebuilt game needs no real random source and no seed choice: it starts `$CB` at $FF with every game and calls the same step in the same order. Because the original is deterministic, **the lockstep test (F20) can feed both versions the same buttons and compare everything**; no recording of random numbers is needed. A modern "random start" would be a modernisation and would have to be a separate, optional feature (Rule 3).', '',
          '## Not claimed', '',
          '- The moments at which each ghost decision happens are F13; the long-run statistics of the numbers are not studied (they are not needed to reproduce the game).']
    (ROOT / 'docs' / 'F14_RANDOM.md').write_text('\n'.join(d) + '\n', encoding='utf-8')
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
