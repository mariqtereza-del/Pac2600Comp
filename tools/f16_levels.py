"""Stage F16: levels, speed-ups and timers.

  python -I tools\\f16_levels.py <rom>      (after f1-f15; needs the 70 baseline traces and the flag logs reference\\f12 made by f12_collisions.py)

1. Static: every use of LEVEL_SPEED ($99), GAME_SELECT ($84) and EVENT_TIMER ($E4) in the listing: this is the complete list of what depends on the level.
2. Start level from the game number and +1 per cleared map: checked at every game start and new map of the 70 games.
3. Ghost speed per level: the speed accumulator's cycle (x17 mod 256) for each start value, with the measured steps per handled frame.
4. The event timer ($E4): one model for the scared time, the freeze after a new life or map, and the death delay; checked on every frame.
Writes docs\\F16_LEVELS.md. The ROM is only read.
"""
import collections
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / 'tools'))
import f12_collisions as f12  # noqa: E402

BS = chr(92)


def cycle(seed):
    """Steps (carries) in one full cycle of the accumulator and its length, for MOVE_OBJECT's acc = acc * 17 mod 256."""
    acc, steps, n = seed, 0, 0
    while True:
        t = ((acc << 4) & 0xFF) + acc
        acc = t & 0xFF
        steps += t >> 8
        n += 1
        if acc == seed:
            return steps, n


def uses(listing, names):
    out = collections.defaultdict(list)
    for line in listing.split('\n'):
        m = re.match(r'\s+([A-Z]{3})\s+(\w+)', line)
        if m and m.group(2) in names:
            pc = re.search(r'; \$([0-9A-F]{4})', line).group(1)
            out[m.group(2)].append((pc, m.group(1)))
    return out


def main():
    rom = pathlib.Path(sys.argv[1]).resolve().read_bytes()
    listing = (ROOT / 'reference' / 'disasm' / 'pacman_routines.asm').read_text(encoding='utf-8')
    use = uses(listing, ('LEVEL_SPEED', 'GAME_SELECT', 'EVENT_TIMER'))
    stats = collections.Counter()
    bad = []
    seen_start = collections.Counter()
    obs = collections.defaultdict(lambda: [0, 0])      # (level, obj) -> [steps, handled frames]
    scare = []                                         # measured scared times
    for n in range(1, 71):
        entry = 'BL-%03d' % n
        if entry in ('BL-065', 'BL-066'):
            continue
        rows = f12.trace(entry)
        ev = {}
        for line in (ROOT / 'reference' / 'f12' / ('w_%s.csv' % entry)).read_text().split('\n'):
            if line:
                fr, pc, a = line.split(',')
                ev.setdefault(int(fr), {})[pc] = int(a)
        last_pill = None
        for fr in range(1, len(rows)):
            inp, post = rows[fr]
            pre = rows[fr - 1][1]
            d = ev.get(fr, {})
            # ---- 2. start level and +1 per map ----
            if pre[2] != 0 and post[2] == 0 and not inp & 0x40:
                want = ((post[0x04] - 2) & 6) + 4
                ok = post[0x19] == want
                stats['2: game starts: level = ((game number - 2) and 6) + 4, right' if ok else '2: game start level WRONG'] += 1
                seen_start[(post[0x04], post[0x19])] += 1
            if post[0x19] != pre[0x19] and not (pre[2] != 0 or post[2] != 0):
                newmap = (post[0x1A] & 0x3C) == 0x3C and (pre[0x1A] & 0x3C) != 0x3C
                ok = newmap and post[0x19] == min(pre[0x19] + 1, 0x0E)
                stats['2: level change = +1 at a new map (max $0E), right' if ok else '2: level change WRONG'] += 1
                if not ok and len(bad) < 5:
                    bad.append(('level', entry, fr, pre[0x19], post[0x19]))
            # ---- 3. measured ghost steps per handled frame (normal frames only) ----
            if (pre[2] == 0 and not inp & 0x60 and not pre[0x67] & 0x80 and not post[0x67] & 0x80 and not pre[0x65] & 0x80
                    and (pre[0x66] ^ post[0x66]) & 0x80 == 0):
                X = (pre[0] & 3) + 1
                if not (pre[0x3B + X] & 0x28) and not (post[0x3B + X] & 0x28):
                    steps = abs(post[0x31 + X] - pre[0x31 + X]) + abs(post[0x36 + X] - pre[0x36 + X])
                    obs[(pre[0x19], X)][0] += steps
                    obs[(pre[0x19], X)][1] += 1
            # ---- 4. the event timer ----
            et, e7, pill = pre[0x64], pre[0x67], pre[0x1A]
            fc = pre[0]
            setter = None
            if pre[2] != 0 or inp & 0x60:
                stats['4: skipped: title screen or Select/Reset'] += 1
                continue
            pill_eaten = any(pre[0x1A] & b and not post[0x1A] & b for b in (0x04, 0x08, 0x10, 0x20))
            death_trig = 'f5d9' in d and d['f5d9'] & 0x80 and not pre[0x1A] & 0x80 and not pre[0x3C + (fc & 3)] & 8
            newmap = (post[0x1A] & 0x3C) == 0x3C and (pre[0x1A] & 0x3C) != 0x3C
            if pill_eaten or death_trig or newmap:
                setter = 'pill' if pill_eaten else 'death' if death_trig else 'map'
                stats['4: skipped: the frame also sets the timer (%s)' % setter] += 1
                if pill_eaten:
                    want_t = 0x50 if pre[0x67] & 0x20 else 0xA0
                    stats['4:   pill eaten: timer set to $A0 ($50 with the difficulty bit), right' if post[0x64] == want_t and post[0x1A] & 0xC0 == 0xC0
                          else '4:   pill eaten timer WRONG'] += 1
                    last_pill = fr
                continue
            if pre[0x65] & 0x80:
                stats['4: eaten-ghost pause: timer frozen'] += 1
                ok = post[0x64] == et
                stats['4:   timer unchanged in the pause, right' if ok else '4:   pause timer WRONG'] += 1
                continue
            # predicted: decrement on frames where the frame counter seen by the logic is odd
            want_t, want_pill, want_e7 = et, pill, e7
            if et != 0 and fc & 1:
                want_t = et - 1
                if want_t == 0:
                    if pill & 0x80:
                        if pill & 0x40:
                            want_pill = pill & 0xBF
                            want_t = 0x20
                        else:
                            want_pill = pill & 0x7C
                            expiry = 'scare over'
                    if not (pill & 0x80 and pill & 0x40):
                        if e7 & 0x40:
                            stats['4: timer ran out in a death (F15/F17)'] += 1
                            ok = post[0x64] == 0x3C or post[0x67] == 0
                            stats['4:   death delay over: new try ($3C freeze) or game over, right' if ok else '4:   death end WRONG'] += 1
                            continue
                        want_e7 = e7 & 0x7F
            m = 0xC3
            if pre[0x1A] & 0x80 and (post[0x1A] & 3) != (pre[0x1A] & 3):
                m = 0xC0                # a ghost was eaten in this frame (F12): the ghost counter goes up
            got = (post[0x64], post[0x1A] & m, post[0x67] & 0x80)
            want = (want_t, want_pill & m, want_e7 & 0x80)
            # pill bits 1-0 (ghost counter) and 7,6 only; the other pill bits do not change here
            if got == want:
                stats['4: timer, scare bits and freeze right'] += 1
            else:
                stats['4: timer WRONG'] += 1
                if len(bad) < 8:
                    bad.append(('timer', entry, fr, 'pre', hex(et), hex(pill), hex(e7), 'fc', fc, 'got', [hex(v) for v in got], 'want', [hex(v) for v in want]))
            if pre[0x1A] & 0x80 and not post[0x1A] & 0x80 and last_pill is not None:
                scare.append(fr - last_pill)
                last_pill = None
    # ---- 3. the theory ----
    table = []
    for L in range(4, 0x0F):
        hi = cycle(L)
        lo = cycle(L - 1)
        table.append((L, hi, lo, rom[0xE6F + (L >> 1)]))
    ok = (not [k for k in stats if 'WRONG' in k] and not bad and stats['4: timer, scare bits and freeze right'] > 100000)
    for k, v in sorted(stats.items()):
        print('%8d  %s' % (v, k))
    for b in bad:
        print(b)
    print('measured scared times (frames from the last pill to the end of the scare):', collections.Counter(scare).most_common(8))
    print('result: %s' % ('OK' if ok else 'MISMATCH'))
    write_doc(stats, use, table, obs, scare, seen_start, ok)
    return 0 if ok else 1


def write_doc(stats, use, table, obs, scare, seen_start, ok):
    d = ['# Stage F16: levels, speed-ups and timers (2026-10-26)', '',
         '`python -I tools%sf16_levels.py rom%spacman.a26` lists everything in the code that depends on the level, works out the speeds and timers from the ROM and checks the rules on every frame of the 70 recorded games (the 2 test games BL-065/066 are skipped). This document is written by that tool.' % (BS, BS), '',
         '## What the level changes (complete list)', '',
         'The level is the byte `LEVEL_SPEED` (`$99`). The code that reads or writes it, and the game number `GAME_SELECT` (`$84`), is all there is:', '',
         '| Address | Instruction | What |', '| --- | --- | --- |']
    what = {'$F55F': 'while the game is frozen: ghost speed accumulators and timers are set from the level',
            '$F6C0': 'new map: level + 1 ...', '$F6C6': '... up to $0E', '$FA86': 'the ghost timer is reloaded from the table by level / 2',
            '$FC00': 'game number -> start level', '$FC0A': 'game start: level stored',
            '$F8A1': 'game start: Pac-Man speed seed from the game number', '$F00E': 'power-on: game number 6'}
    for name in ('LEVEL_SPEED', 'GAME_SELECT'):
        for pc, op in use[name]:
            d.append('| $%s | `%s %s` | %s |' % (pc, op, name, what.get('$' + pc, 'game number: Select steps it, show it on the score line')))
    d += ['', '**So the level changes only two things: how fast the ghosts are and how long their chase timer lasts.** It does not change the scared time, Pac-Man\'s speed (that comes from the game number only, F10), the bonus item, the points or the number of lives. Nothing is looked up in a table of "levels" with fruit or other pictures: the maze and everything shown are the same on every level.', '',
          '## Start level and level steps', '',
          '- A game starts at level `((game number - 2) and 6) + 4`: 4, 6, 8 or 10 (game numbers 2-3 -> 4, 4-5 -> 6, 6-7 -> 8, 8-9 -> 10, then it repeats, so the eight variations use four starting levels). Each cleared map adds 1, up to `$0E` (14).',
          '- Checked: ' + '; '.join('%s: %d' % (k.replace('2: ', ''), v) for k, v in sorted(stats.items()) if k.startswith('2:')) + '.',
          '- Game starts seen (game number, level): ' + ', '.join('($%02X, %d) x%d' % (k[0], k[1], v) for k, v in sorted(seen_start.items())) + '.', '',
          '## Ghost speed', '',
          'Every ghost has a speed accumulator that is multiplied by 17 (mod 256) each time the ghost is updated; a carry out is one step (F10). The start value is set while the game is frozen (new life, new map, new game): **objects 2 and 4 get the level L, objects 1 and 3 get L - 1**. The accumulator then cycles, so the speed is fixed by the start value:', '',
          '| Level L | Start L: steps per cycle | Start L-1: steps per cycle | Chase timer reload (table `$FE6F`, +1 for objects 2 and 3) |', '| --- | --- | --- | --- |']
    for L, hi, lo, tm in table:
        d.append('| %d | %d of %d updates = %.3f | %d of %d = %.3f | %d |' % (L, hi[0], hi[1], hi[0] / hi[1], lo[0], lo[1], lo[0] / lo[1], tm))
    d += ['', 'A ghost is updated 3 times in the frame that handles it, once every 4 frames, so the speed in pixels per frame is 0.75 x the step share. For comparison Pac-Man (seed $09 or $0B) moves 0.562 or 0.688 pixels a frame. The measured average steps per handled frame (normal play, ghost not eyes and not in an opening) by level, next to 3 x the theoretical share of the two start values:', '',
          '| Level | Object | Measured steps per handled frame | Handled frames | 3 x theory |', '| --- | --- | --- | --- | --- |']
    for (L, X), (st, n) in sorted(obs.items()):
        if n < 200:
            continue
        seed = L if X in (2, 4) else L - 1
        c = cycle(seed)
        d.append('| %d | %d | %.3f | %d | %.3f |' % (L, X, st / n, n, 3.0 * c[0] / c[1]))
    d += ['', 'The measured values scatter around the theory because the accumulator restarts from its start value at every freeze, and a ghost that turns round or stops at a wall does not always gain the step. The exact step-by-step behaviour was already proved in F13 (348,146 frames).', '',
          '## The event timer `$E4` (scared time, freeze, death delay)', '',
          'One byte times three things. It goes down by 1 only in frames where the frame counter seen by the logic is odd (every second frame) and not during the eaten-ghost pause; when it reaches 0:',
          '', '- **Scared, first part** (bits 7 and 6 of `PILL_FLAGS` set): bit 6 is cleared and the timer set to `$20`: the ghosts flash (the picture changes colour) for 32 steps = 64 frames.',
          '- **Scared, second part** (bit 7 only): the scare ends (`PILL_FLAGS` AND `$7C`: the ghost counter goes back to 0).',
          '- **Frozen** (`GAME_FLAGS` bit 7): the freeze ends (the bit is cleared). A freeze starts with the timer at `$3C` (60 steps = 120 frames) after a new map or a new try.',
          '- **A death** (`GAME_FLAGS` bit 6): the timer started at `$3F`; at 0 the next try is set up or the game ends (F15).',
          '', 'A power pill sets bits 7 and 6 and the timer to **`$A0` (160 steps = 320 frames), or `$50` (160 frames) when `GAME_FLAGS` bit 5 is set** (the difficulty switch of the player in turn), then the flashing part of 64 frames. The eaten-ghost pause is 32 frames (`EAT_PAUSE` counts from `$9F` down to `$7F`) and the timer stands still in it, so every ghost eaten lengthens the scare by 32 frames. **The scared time does not depend on the level.** Eating a second pill during a scare starts the time again.', '',
          'Results of the frame-by-frame check:', '', '| Count | What |', '| --- | --- |']
    d += ['| %d | %s |' % (v, k) for k, v in sorted(stats.items()) if k.startswith('4:')]
    import collections as c2
    sc = c2.Counter(scare)
    d += ['', 'Measured: frames from the last power pill to the end of the scare: ' + ', '.join('%d frames x%d' % (k, v) for k, v in sc.most_common(8)) + ' (the spread of 1 frame is the odd/even frame counter; shorter values follow a pill eaten while ghosts were already flashing).', '',
          '## Not claimed', '',
          '- Levels above 14 (the level cannot go further); the difficulty-A scared time ($50) is from the code and was seen only if the table above shows a 160-step pill; two-player level handling (no recording).',
          '- The picture colours of the scared and flashing ghosts are the kernel (F18).']
    (ROOT / 'docs' / 'F16_LEVELS.md').write_text('\n'.join(d) + '\n', encoding='utf-8')


if __name__ == '__main__':
    sys.exit(main())
