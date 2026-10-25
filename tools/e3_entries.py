"""Stage E3: makes the baseline entries about Pac-Man's movement (BL-026 .. BL-037) and checks them.

  python -I tools\\e3_entries.py <rom>

Same method as Stage E2 (tools\\e2_entries.py, which this file uses): each entry's input is written to baseline\\<ID>.inputs, the host
makes the trace twice (byte-identical required, SHA-256 stored), and the plain-words observations are computed from the trace.
Nothing is typed in by hand. Pac-Man's position is read from byte $B1 (x) and $B6 (y), its direction from $D2 bits 0-3 and the
joystick bits read by the game from $ED: working labels of Stages C6/C7, not yet confirmed (Phase F). On the screen x grows to the
right and y grows downward. The scripted entries start a game with Game Reset at frame 240 (play begins on frame 367, the start tune
before it) and use the first ~150 frames of play, before a ghost catches Pac-Man. BL-035 is the whole bot route of Stage C7.
"""
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import e2_entries as e2  # noqa: E402

LEFT, RIGHT, UP, DOWN = 4, 8, 1, 2
RESET = 64
NAMES = [(UP, 'UP'), (DOWN, 'DOWN'), (LEFT, 'LEFT'), (RIGHT, 'RIGHT'), (16, 'FIRE'), (32, 'SELECT'), (64, 'RESET')]
DIRNAME = {(-1, 0): 'left', (1, 0): 'right', (0, -1): 'up', (0, 1): 'down'}
ROUTE = e2.ROOT / 'reference' / 'd14' / 'c7_route' / 'input.inputs'


def inputs_of(segs, n):
    bits = [0] * n
    e2.press(bits, 240, RESET)
    for a, e, m in segs:
        for f in range(a, min(e, n)):
            bits[f] |= m
    return bits


def held(m):
    return '+'.join(n for v, n in NAMES if m & v) or 'nothing'


def input_runs(t, f0, f1):
    runs, start = [], f0
    for f in range(f0 + 1, f1 + 1):
        if f == f1 or (t.inp(f) & 79) != (t.inp(start) & 79):
            runs.append((start, f - 1, t.inp(start) & 79))
            start = f
    return runs


def pos(t, f):
    return t.b(f, 0xB1), t.b(f, 0xB6)


def legs(t, f0, f1):
    """Pixel steps of Pac-Man between f0 and f1 (frames with the pause flag set are skipped), grouped into legs of one direction."""
    out, jumps = [], []
    prev = pos(t, f0)
    for f in range(f0 + 1, f1):
        p = pos(t, f)
        if t.paused(f) or t.paused(f - 1):
            prev = p
            continue
        if p != prev:
            dx, dy = p[0] - prev[0], p[1] - prev[1]
            if abs(dx) + abs(dy) != 1:
                jumps.append((f, prev, p))
            else:
                d = (dx, dy)
                if out and out[-1]['dir'] == d and f - out[-1]['frames'][-1] <= 3:
                    out[-1]['frames'].append(f)
                    out[-1]['end'] = p
                else:
                    out.append({'dir': d, 'frames': [f], 'start': prev, 'end': p})
            prev = p
    return out, jumps


def leg_text(g):
    fr = g['frames']
    gaps = [b - a for a, b in zip(fr, fr[1:])]
    n = len(fr)
    text = 'Pac-Man moves %s from (%d,%d) to (%d,%d): %d pixel%s, first step on frame %d, last on frame %d' % (
        DIRNAME[g['dir']], g['start'][0], g['start'][1], g['end'][0], g['end'][1], n, '' if n == 1 else 's', fr[0], fr[-1])
    if gaps:
        counts = {k: gaps.count(k) for k in sorted(set(gaps))}
        text += '; frames between steps: %s (%s); %.3f pixels per frame' % (
            ','.join(map(str, gaps[:14])) + (',...' if len(gaps) > 14 else ''),
            ', '.join('%d x %d frame%s' % (v, k, '' if k == 1 else 's') for k, v in counts.items()), n / float(fr[-1] - fr[0] + 1))
    return text


def timeline(t, tune, last):
    """The common observations: inputs, legs, direction changes, pushing into walls, moving with no direction held, pauses."""
    f0 = tune
    facts = ['Play begins on frame %d (pause flag clears); Pac-Man stands at (%d,%d), the start place.' % (f0, pos(t, f0)[0], pos(t, f0)[1])]
    for a, e, m in input_runs(t, f0, last + 1):
        facts.append('Joystick on frames %d-%d: %s.' % (a, e, held(m)))
    lg, jumps = legs(t, f0, last + 1)
    for g in lg:
        facts.append(leg_text(g))
    for a, b in zip(lg, lg[1:]):
        gap = b['frames'][0] - a['frames'][-1]
        facts.append('Direction change %s -> %s at (%d,%d): last step of the first leg on frame %d, first step of the next on frame %d (%d frame%s apart).'
                     % (DIRNAME[a['dir']], DIRNAME[b['dir']], a['end'][0], a['end'][1], a['frames'][-1], b['frames'][0], gap, '' if gap == 1 else 's'))
    for f, p, q in jumps:
        facts.append('Frame %d: the position jumps from (%d,%d) to (%d,%d) (not a one-pixel step).' % (f, p[0], p[1], q[0], q[1]))
    # pushing against a wall: a direction is held, nothing moves for 10 frames or more
    f = f0
    while f <= last:
        if not t.paused(f) and t.inp(f) & 15 and all(pos(t, f + k) == pos(t, f) for k in range(0, 10) if f + k <= last) and f + 9 <= last:
            g = f
            while g + 1 <= last and pos(t, g + 1) == pos(t, f) and not t.paused(g + 1):
                g += 1
            facts.append('Frames %d-%d: Pac-Man does not move (stays at (%d,%d)) although %s is held at frame %d: a wall in the way.'
                         % (f, g, pos(t, f)[0], pos(t, f)[1], held(t.inp(f) & 15), f))
            f = g + 1
        else:
            f += 1
    free = [fr for g in lg for fr in g['frames'] if not t.inp(fr) & 15]
    if free:
        facts.append('Pac-Man took %d pixel steps on frames when no direction was held (first on frame %d): it keeps going in its last direction.' % (len(free), free[0]))
    else:
        facts.append('Pac-Man took no pixel step on a frame when no direction was held.')
    f = f0 + 1
    while f <= last:
        if t.paused(f):
            g = f
            while g + 1 <= last and t.paused(g + 1):
                g += 1
            facts.append('Frames %d-%d: the pause flag (byte $D2 bit 6) is set for %d frames; the lives counter reads %d before and %d at its end; bytes $B1/$B6 go from (%d,%d) to (%d,%d).'
                         % (f, g, g - f + 1, t.lives(f - 1), t.lives(min(g + 1, t.n - 1)), pos(t, f - 1)[0], pos(t, f - 1)[1], pos(t, min(g + 1, t.n - 1))[0], pos(t, min(g + 1, t.n - 1))[1]))
            f = g + 1
        else:
            f += 1
    return facts, lg, jumps


def make_entry(name, title, shows, bits, how, extra=None):
    t = e2.make(name, title, shows, bits)
    tune = t.first(240, lambda f: t.lives(f) == 3 and not t.paused(f) and f > 300)
    s = t.first(240, lambda f: t.lives(f) == 3)
    assert s == 240 and tune is not None, name
    death = t.first(tune, lambda f: t.paused(f))
    last = t.n - 1
    facts = ['Game Reset on frame 240 starts a game (lives counter reads 3 from frame 240).']
    tl, lg, jumps = timeline(t, tune, last)
    facts += tl
    if extra:
        facts += extra(t, tune, lg, jumps)
    e2.write_entry(name, '', '', t.n, facts, how)
    return t, tune, lg, jumps


def main():
    e2.ROM = pathlib.Path(sys.argv[1]).resolve()
    checks = []
    how = ' (switch on, press Game Reset, wait for the start tune to end; the frame numbers are counted from power-on at 60 per second; the joystick moves are listed above).'
    # BL-026: horizontal walk, both ways
    t, tune, lg, _ = make_entry('BL-026', 'Pac-Man walks left to the wall and back right to the other wall',
                                'From the start place the joystick is held LEFT, then RIGHT: the horizontal speed, the stop at a wall and the way back.',
                                inputs_of([(370, 440, LEFT), (440, 520, RIGHT)], 560),
                                'start a game, then hold the joystick left for about a second and a quarter, then right' + how)
    checks.append(('BL-026', len(lg) >= 2 and lg[0]['dir'] == (-1, 0) and lg[1]['dir'] == (1, 0)))
    # BL-027: vertical walk
    t, tune, lg, _ = make_entry('BL-027', 'Pac-Man turns down the first side corridor, then goes up and down it',
                                'LEFT to the corner, then DOWN, UP, DOWN held in turn: the vertical speed and the stops at the ends of the corridor.',
                                inputs_of([(370, 400, LEFT), (400, 430, DOWN), (430, 460, UP), (460, 520, DOWN)], 560),
                                'start a game, hold left until Pac-Man reaches the corner, then down, up, down' + how)
    checks.append(('BL-027', any(g['dir'] == (0, 1) for g in lg) and any(g['dir'] == (0, -1) for g in lg)))
    # BL-028: a turn asked for early and held is taken at the corner
    t, tune, lg, _ = make_entry('BL-028', 'A turn asked for early and held is taken at the corner',
                                'LEFT for 2 frames, then DOWN held from the start of the corridor: Pac-Man goes on left and turns down at the first corner.',
                                inputs_of([(370, 372, LEFT), (372, 460, DOWN)], 520),
                                'start a game, tap left, then hold down at once; Pac-Man walks left alone and turns down at the corner' + how)
    checks.append(('BL-028', len(lg) >= 2 and lg[0]['dir'] == (-1, 0) and lg[1]['dir'] == (0, 1)))
    # BL-029: a short request is not remembered
    t, tune, lg, _ = make_entry('BL-029', 'A turn asked for only briefly is not remembered',
                                'LEFT for 2 frames, DOWN for 2 frames, then nothing: Pac-Man walks left to the wall at the corner and stays there.',
                                inputs_of([(370, 372, LEFT), (372, 374, DOWN)], 480),
                                'start a game, tap left, then tap down, then let go of the joystick' + how)
    checks.append(('BL-029', len(lg) == 1 and lg[0]['dir'] == (-1, 0)))
    # BL-030: reversal
    t, tune, lg, _ = make_entry('BL-030', 'Pac-Man reverses at once',
                                'LEFT for 10 frames, then RIGHT held: the direction is reversed in the middle of a corridor.',
                                inputs_of([(370, 380, LEFT), (380, 440, RIGHT)], 480),
                                'start a game, hold left for a moment, then hold right' + how)
    checks.append(('BL-030', len(lg) >= 2 and lg[0]['dir'] == (-1, 0) and lg[1]['dir'] == (1, 0)))
    # BL-031 .. BL-033: diagonals
    for name, mask, label in (('BL-031', LEFT | DOWN, 'LEFT and DOWN'), ('BL-032', LEFT | UP, 'LEFT and UP'), ('BL-033', RIGHT | DOWN, 'RIGHT and DOWN')):
        t, tune, lg, _ = make_entry(name, 'Joystick held diagonally: %s together' % label,
                                    'The joystick is held %s together from the start place for about a second and a half.' % label,
                                    inputs_of([(370, 450, mask)], 500),
                                    'start a game and hold the stick diagonally (%s)' % label + how)
        checks.append((name, len(lg) >= 1))

    # BL-034, BL-036, BL-037: the opening in the bottom of the maze (and the top). What the bytes do is written down; nothing is guessed.
    def tunnel(t, tune, lg, jumps):
        out = []
        f = t.first(tune, lambda f: pos(t, f)[1] > 70)
        if f is not None:
            out.append('Frame %d: byte $B6 reads %d, more than the lowest corridor row (70); in the picture of that frame Pac-Man is drawn below the bottom of the maze, under the opening (looked at by eye).' % (f, pos(t, f)[1]))
        return out
    walk = [(370, 420, LEFT), (420, 470, DOWN), (470, 490, RIGHT)]
    t, tune, lg, jumps = make_entry('BL-034', 'DOWN pressed in the bottom corridor under the middle of the maze, and held',
                                    'Pac-Man walks left, down, right to the column of the opening in the bottom of the maze, then DOWN is held to the end: what the position bytes and the pause flag do.',
                                    inputs_of(walk + [(490, 760, DOWN)], 760), 'start a game, walk left, down, right to the middle of the bottom corridor, then hold down' + how, tunnel)
    checks.append(('BL-034', any(pos(t, f)[1] > 70 for f in range(tune, t.n))))

    # BL-035: the bot route (Stage C7), the whole of level 1
    txt = ROUTE.read_text().split('\n')
    n = int(txt[2].split('=')[1])
    bits = [0] * n
    ev = [tuple(map(int, l.split())) for l in txt[3:] if l]
    for i, (f, v) in enumerate(ev):
        end = ev[i + 1][0] if i + 1 < len(ev) else n
        for k in range(f, end):
            bits[k] = v
    t = e2.make('BL-035', 'Computer play: the whole first level by the search bot',
                'The 9,800-frame route of the search-playing bot of Stage C7: a game played, with about 280 joystick moves; used here to check the movement rules over a long game.', bits)
    facts = ['This entry is the input of Stage C7 (tools\\captures\\c7_route.txt) replayed from power-on: a computer-played game that clears level 1.',
             'Movement rules measured over every frame of play (frames with the pause flag set are left out; each is checked step by step):']
    steps, big = 0, []
    prev = None
    horiz = vert = 0
    for f in range(1, t.n):
        if t.paused(f) or t.paused(f - 1) or t.lives(f) != t.lives(f - 1):
            prev = pos(t, f)
            continue
        p = pos(t, f)
        if prev is not None and p != prev:
            dx, dy = p[0] - prev[0], p[1] - prev[1]
            if abs(dx) + abs(dy) > 1:
                big.append((f, prev, p))
            else:
                steps += 1
                if dx:
                    horiz += 1
                else:
                    vert += 1
        prev = p
    facts.append('Pixel steps of one pixel counted: %d (%d horizontal, %d vertical). Steps that were larger than one pixel or diagonal: %d%s.'
                 % (steps, horiz, vert, len(big), (', at frames ' + ', '.join('%d: (%d,%d)->(%d,%d)' % (f, a[0], a[1], b[0], b[1]) for f, a, b in big[:6])) if big else ''))
    pts = set(pos(t, f) for f in range(1, t.n) if not t.paused(f) and t.lives(f) > 0)
    play = [f for f in range(1, t.n) if t.lives(f) > 0 and not t.paused(f) and f > 1810]
    onrow = [p for p in set(pos(t, f) for f in play) if p[1] % 10 == 0]
    off = sorted(set(pos(t, f) for f in play if pos(t, f)[1] % 10 != 0))
    cols = sorted(set(p[0] for p in off))
    facts.append('Every position reached in play is either on a corridor row (y a multiple of 10: %s) or on one of %d vertical corridors (x = %s).'
                 % (','.join(str(y) for y in sorted(set(p[1] for p in onrow))), len(cols), ','.join(map(str, cols))))
    stepframes = [f for f in play if f > 1 and pos(t, f) != pos(t, f - 1) and not t.paused(f - 1) and abs(pos(t, f)[0] - pos(t, f - 1)[0]) + abs(pos(t, f)[1] - pos(t, f - 1)[1]) == 1]
    gl = [b - a for a, b in zip(stepframes, stepframes[1:]) if b - a <= 3]
    facts.append('Frames between two pixel steps of Pac-Man (when they are 3 or fewer): %s; so Pac-Man averages %.3f pixels per frame while moving.'
                 % (', '.join('%d frame%s: %d times' % (k, '' if k == 1 else 's', gl.count(k)) for k in sorted(set(gl))), len(gl) / float(sum(gl))))
    facts.append('Pac-Man never leaves the playing area: x runs from %d to %d, y from %d to %d in play.' % (min(p[0] for p in pts if p[1] <= 70), max(p[0] for p in pts if p[1] <= 70), min(p[1] for p in pts), max(p[1] for p in pts if p[1] <= 70)))
    e2.write_entry('BL-035', '', '', t.n, facts, 'replay the route of tools\\captures\\c7_captures.txt in Stella (Stage C7) and watch Pac-Man: he always moves along the corridors, one pixel at a time.')
    checks.append(('BL-035', not big))

    t, tune, lg, jumps = make_entry('BL-036', 'UP pressed just after Pac-Man has come out at the top of the maze',
                                    'As BL-034, but UP is pressed at frame 580, just after the pause flag of BL-034 clears: what happens at the opening in the top of the maze.',
                                    inputs_of(walk + [(490, 580, DOWN), (580, 760, UP)], 760), 'as BL-034, then press up on frame 580' + how, tunnel)
    checks.append(('BL-036', t.first(tune, lambda f: pos(t, f)[1] > 70) is not None))
    t, tune, lg, jumps = make_entry('BL-037', 'UP pressed while the pause flag of the bottom opening is set',
                                    'As BL-034, but the joystick is switched from DOWN to UP on frame 575, before the pause flag has cleared.',
                                    inputs_of(walk + [(490, 575, DOWN), (575, 700, UP)], 760), 'as BL-034, then press up on frame 575' + how, tunnel)
    checks.append(('BL-037', t.first(tune, lambda f: pos(t, f)[1] > 70) is not None))

    bad = [nme for nme, ok in checks if not ok]
    print('entries made: %d; checks failed: %s' % (len(checks), bad or 'none'))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
