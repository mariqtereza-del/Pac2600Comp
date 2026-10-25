"""Stage E7: makes the baseline entries about long play and checks them.

  python -I tools\\e7_entries.py <rom> <bot_run_dir>...  [--random <seed> <frames>]...

For each bot run folder (made by `stella_bot <rom> <dir> <frames> <seed> long`, Stage E7) it turns the route script into a recording
(baseline\\<ID>.inputs), traces it twice (byte-identical required) and writes observations computed from the trace: levels, deaths,
the power pills of each level, the speed of Pac-Man and of the ghosts in each level, and the bonus item. For each `--random` it
makes a seeded random-input game (a hold of one direction for 4-40 frames, now and then no direction; Game Reset on frame 240;
the fire button for 10 frames every 1,500 frames from frame 3000) and checks the movement rules of Stages E3/E4 on it.
Bytes (working labels, not confirmed): lives $98, pause flag $D2 bit 6, Pac-Man $B1/$B6, ghost i $B2+i/$B7+i, score $CC/$CE,
power pill $F2 bit 1, bonus item on the board $E7 bit 1, wafer map $88-$97.
"""
import collections
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import d14_side_by_side as d14  # noqa: E402
import e2_entries as e2  # noqa: E402
import e3_entries as e3  # noqa: E402
import e4_entries as e4  # noqa: E402
import e5_entries as e5  # noqa: E402
import e6_entries as e6  # noqa: E402

FIRE = 16


def item(t, f):
    return bool(t.b(f, 0xE7) & 2)


def pill(t, f):
    return bool(t.b(f, 0xF2) & 2)


def windows(flags):
    out, s = [], None
    for f, v in flags:
        if v and s is None:
            s = f
        if not v and s is not None:
            out.append((s, f - 1))
            s = None
    if s is not None:
        out.append((s, flags[-1][0]))
    return out


def gaps_text(c):
    tot = sum(c.values())
    return ', '.join('%d frames: %d' % kv for kv in sorted(c.items())) + (' (%d steps)' % tot)


def analyse(t, start):
    """Common observations of a long game; returns the facts and a dict of results for the checks."""
    facts = []
    res = {}
    n = t.n
    refills = [f for f in range(start + 1, n) if e5.wafers(t, f) > e5.wafers(t, f - 1) + 20 and t.lives(f - 1) > 0]
    ds = e6.deaths(t, start)
    over = t.first(start, lambda f: t.lives(f) == 0 and f > start + 10)
    facts.append('%s; %d deaths; the score at the end is %d (%d lives left); %d frames in all.' % (('The game ends (the lives counter reads 0) on frame %d' % over) if over else 'No game over within the entry', len(ds), e5.score(t, n - 1), t.lives(n - 1), n))
    bounds = [start] + refills + [over or n - 1]
    res['levels'] = len(refills) + 1
    facts.append('Levels: the wafer map refills (a level is cleared) on frames %s: %d level%s cleared, a %s level reached. Lives counter at each refill: %s.'
                 % (', '.join(map(str, refills)) or 'none', len(refills), '' if len(refills) == 1 else 's', len(refills) + 1, ', '.join(str(t.lives(f)) for f in refills) or '-'))
    for k in range(len(bounds) - 1):
        a, b = bounds[k], bounds[k + 1]
        lvl = k + 1
        # Pac-Man's steps
        pstep, prev = [], e4.ppos(t, a)
        pgaps = collections.Counter()
        last = None
        for f in range(a + 1, b):
            p = e4.ppos(t, f)
            if t.paused(f) or t.paused(f - 1):
                prev = p
                last = None
                continue
            if p != prev:
                if abs(p[0] - prev[0]) + abs(p[1] - prev[1]) == 1:
                    if last is not None and f - last <= 3:
                        pgaps[f - last] += 1
                    last = f
                prev = p
        ggaps = [collections.Counter() for _ in range(4)]
        gsize = [collections.Counter() for _ in range(4)]
        gall = collections.Counter()
        for i in range(4):
            prev, last = e4.gpos(t, i, a), None
            for f in range(a + 1, b):
                p = e4.gpos(t, i, f)
                if p != prev:
                    gsize[i][e4.dist(p, prev)] += 1
                    if not (t.paused(f) or t.paused(f - 1)):
                        gall[e4.dist(p, prev)] += 1
                    if last is not None and f - last <= 8:
                        ggaps[i][f - last] += 1
                    last = f
                prev = p
        pw = windows([(f, pill(t, f)) for f in range(a, b)])
        pws = [(s, e) for s, e in pw if e - s > 5]
        it = windows([(f, item(t, f)) for f in range(a, b)])
        facts.append('Level %d (frames %d-%d): Pac-Man frames between steps: %s. Ghost 0 frames between changes of place: %s; sizes of the changes of place of all four ghosts: %s (average %.2f pixels per change).'
                     % (lvl, a, b, gaps_text(pgaps), gaps_text(ggaps[0]), ', '.join('%d px: %d' % kv for kv in sorted(gall.items() if gall else [])), sum(k * v for k, v in gall.items()) / float(max(1, sum(gall.values())))))
        facts.append('Level %d: power pill windows %s; bonus item on the board %s.'
                     % (lvl, '; '.join('frames %d-%d (%d frames)' % (s, e, e - s + 1) for s, e in pws) or 'none',
                        '; '.join('frames %d-%d (%d frames, %d frames after the level began, %d wafers eaten by then)' % (s, e, e - s + 1, s - a, 126 - e5.wafers(t, s)) for s, e in it) or 'never'))
        res.setdefault('pgaps', []).append(pgaps)
        res.setdefault('ggaps', []).append(ggaps[0])
        res.setdefault('pw', []).append(pws)
        res.setdefault('item', []).append(it)
    # score rises other than wafer (+1), pill (+5), ghosts (+20,40,80,160)
    odd = collections.defaultdict(list)
    for f in range(start + 1, n):
        d = e5.score(t, f) - e5.score(t, f - 1)
        if d and d not in (1, 5, 20, 40, 80, 160):
            odd[d].append(f)
    for d, fl in sorted(odd.items()):
        facts.append('Score rises of %d (not a wafer, pill or ghost value): %d times, on frames %s; at the first, Pac-Man is at (%d,%d) and the bonus item flag ($E7 bit 1) goes from %s to %s.'
                     % (d, len(fl), ', '.join(map(str, fl[:8])) + (', ...' if len(fl) > 8 else ''), e4.ppos(t, fl[0])[0], e4.ppos(t, fl[0])[1], int(item(t, fl[0] - 1)), int(item(t, fl[0]))))
    bonus_rises = [(f, e5.score(t, f) - e5.score(t, f - 1)) for f in range(start + 1, n) if item(t, f - 1) and not item(t, f) and e5.score(t, f) - e5.score(t, f - 1) >= 2]
    res['bonus'] = bonus_rises
    facts.append('Bonus item eaten (the flag goes 1 to 0 on a frame where the score rises by 2 or more): %s.'
                 % ('; '.join('frame %d: +%d, Pac-Man at (%d,%d)' % (f, d, e4.ppos(t, f)[0], e4.ppos(t, f)[1]) for f, d in bonus_rises) or 'never in this game'))
    res['over'] = over
    res['deaths'] = len(ds)
    return facts, res


def invariants(t, start):
    """Movement rules of Stages E3/E4 over every frame of a long game (frames with the pause flag set are left out).
    Returns Pac-Man's one-pixel steps, his other changes of place, the ghosts' changes of place, the ghosts' jumps through
    the openings at the top and bottom of the maze (a jump of more than 3 pixels to or from y above 70, or to or from y 0, in column 72-80)
    and the ghosts' other jumps of more than 2 pixels."""
    big = steps = gsteps = 0
    passages, other, ppass = [], [], []
    for f in range(start + 1, t.n):
        if t.paused(f) or t.paused(f - 1) or t.lives(f) == 0 or t.lives(f) != t.lives(f - 1):
            continue
        p, q = e4.ppos(t, f), e4.ppos(t, f - 1)
        if p != q:
            if e4.dist(p, q) == 1:
                steps += 1
            elif 72 <= p[0] <= 80 and 72 <= q[0] <= 80 and (p[1] > 70 or q[1] > 70 or min(p[1], q[1]) <= 0):
                ppass.append((f, q, p))
            else:
                big += 1
        for i in range(4):
            a, b = e4.gpos(t, i, f), e4.gpos(t, i, f - 1)
            if a != b:
                gsteps += 1
                if e4.dist(a, b) > 3:
                    (passages if 72 <= a[0] <= 80 and 72 <= b[0] <= 80 and (a[1] > 70 or b[1] > 70 or min(a[1], b[1]) <= 0) else other).append((f, i, b, a))
    return steps, big, gsteps, passages, other, ppass


def make_from_bot(name, run_dir, e2_make=True):
    route = pathlib.Path(run_dir) / 'route.txt'
    head = route.read_text().split('\n')[0]
    bits = d14.read_script(route)
    over = int(head.rsplit(' ', 1)[1])
    n = len(bits) + 300
    bits = bits + [0] * 300
    return bits, head


def main():
    args = sys.argv[1:]
    e2.ROM = pathlib.Path(args[0]).resolve()
    jobs = []
    first_id = None
    i = 1
    while i < len(args):
        if args[i] == '--first-id':
            first_id = int(args[i + 1])
            i += 2
        elif args[i] == '--prefix':
            jobs.append(('prefix', args[i + 1], int(args[i + 2])))
            i += 3
        elif args[i] == '--random':
            jobs.append(('random', int(args[i + 1]), int(args[i + 2])))
            i += 3
        else:
            jobs.append(('bot', args[i], None))
            i += 1
    next_id = first_id or max(int(p.name[3:6]) for p in e2.BASE.glob('BL-*.entry.txt')) + 1
    checks = []
    for kind, a, b in jobs:
        name = 'BL-%03d' % next_id
        next_id += 1
        if kind in ('bot', 'prefix'):
            bits, head = make_from_bot(name, a)
            if kind == 'prefix':
                bits = bits[:b]
            title = ('A long game by the search bot (%s)' if kind == 'bot' else 'The first %d frames of a long game by the search bot, with the bonus item (%%s)' % (b or 0)) % pathlib.Path(a).name
            shows = 'The bot of Stage E7 playing on past the first level (%s): levels, deaths, the speed of Pac-Man and the ghosts in each level, the bonus item.' % head.lstrip('# ')
            seed = None
        else:
            seed, nframes = a, b
            lcg = seed
            bits = [0] * nframes
            e2.press(bits, 240, e3.RESET)
            f = 370
            while f < nframes:
                lcg = (lcg * 6364136223846793005 + 1442695040888963407) % (1 << 64)
                r = lcg >> 33
                length = 4 + r % 37
                d = (r >> 8) % 6
                m = [e3.UP, e3.DOWN, e3.LEFT, e3.RIGHT, 0, 0][d]
                for g in range(f, min(f + length, nframes)):
                    bits[g] |= m
                f += length
            for s in range(3000, nframes, 1500):
                e2.press(bits, s, FIRE)
            title = 'Random joystick moves, seed %d, %d frames' % (seed, nframes)
            shows = 'A seeded random game: a hold of one direction for 4 to 40 frames (sometimes none), Game Reset on frame 240, the fire button every 1,500 frames from frame 3000; used to check the movement rules over a very long game.'
        t = e2.make(name, title, shows, bits)
        f0 = t.first(1, lambda f: t.lives(f) == 3)
        start = t.first(f0, lambda f: not t.paused(f))
        facts = ['Game Reset on frame %d starts the game; play begins on frame %d. %s' % (f0, start, ('Input: ' + head.lstrip('# ') + ' (the route script of `stella_bot ... long`, turned into a recording; ' + ('300 idle frames added at the end).' if kind == 'bot' else 'cut after %d frames).' % b)) if kind in ('bot', 'prefix') else 'Input made from seed %d by the rule in tools\\e7_entries.py.' % seed)]
        if kind in ('bot', 'prefix'):
            f2, res = analyse(t, start)
            facts += f2
            checks.append((name, res['levels'] >= 1))
        else:
            starts = [f for f in range(240, t.n) if t.lives(f) == 3 and t.lives(f - 1) != 3]
            facts.append('Games started (the lives counter becomes 3): on frames %s.' % ', '.join(map(str, starts)))
            ds = e6.deaths(t, start)
            facts.append('Deaths seen (pause flag with the lives counter dropping later): %d; the lives counter reads 0 on frames %s.' % (len(ds), ', '.join(str(f) for f in range(1, t.n) if t.lives(f) == 0 and t.lives(f - 1) != 0) or 'never'))
            facts.append('Highest score %d; most wafers eaten from a full map %d; wafer-map refills (level cleared): %d.' % (max(e5.score(t, f) for f in range(start, t.n) if t.lives(f) > 0), 126 - min(e5.wafers(t, f) for f in range(start, t.n) if t.lives(f) > 0), len([f for f in range(start + 1, t.n) if e5.wafers(t, f) > e5.wafers(t, f - 1) + 20 and t.lives(f - 1) > 0 and t.lives(f) == t.lives(f - 1) + 1])))
        st, big, gst, passages, other, ppass = invariants(t, start)
        facts.append('Movement rules over every frame of play (pause frames left out): Pac-Man made %d one-pixel steps, %d jumps through the openings at the top or bottom of the maze (column 76, see BL-034) and %d other changes of place; the ghosts made %d changes of place, %d jumps through the same openings (%s) and %d other changes larger than 3 pixels (changes of 1 to 3 pixels are normal; the size grows with the level).'
                     % (st, len(ppass), big, gst, len(passages), '; '.join('frame %d ghost %d (%d,%d) to (%d,%d)' % (f, i, b[0], b[1], a[0], a[1]) for f, i, b, a in passages[:6]) or 'none', len(other)))
        e2.write_entry(name, '', '', t.n, facts, 'replay the recording in Stella (the host program with --replay) and compare with the numbers above.')
        checks.append((name, big == 0 and not other))
    bad = [x for x, ok in checks if not ok]
    print('entries made: %d; checks failed: %s' % (len(set(x for x, _ in checks)), bad or 'none'))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
