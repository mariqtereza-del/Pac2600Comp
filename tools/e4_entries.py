"""Stage E4: makes the baseline entries about the ghosts (BL-038 .. BL-044) and checks them.

  python -I tools\\e4_entries.py <rom>

Same method as Stages E2/E3 (tools\\e2_entries.py, tools\\e3_entries.py): inputs are written to baseline\\<ID>.inputs, the host makes
each trace twice (byte-identical required, SHA-256 stored) and the plain-words observations are computed from the trace.
Ghost i (0..3) is read from bytes $B2+i (x) and $B7+i (y) (working labels found in Stage E4 by watching which bytes move in an idle
game; not yet confirmed, Phase F). On the screen x grows to the right and y downward. Pac-Man is $B1/$B6 (Stage E3).
BL-038 to BL-043: scripted starts (Reset on frame 240, play on frame 367) that differ only in the timing of the input.
BL-044: the first 4,000 frames of the Stage C7 bot route (the first power pill and the first ghosts eaten).
"""
import collections
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import e2_entries as e2  # noqa: E402
import e3_entries as e3  # noqa: E402

LEFT, RIGHT, UP, DOWN = e3.LEFT, e3.RIGHT, e3.UP, e3.DOWN
HERE = e2.ROOT


def gpos(t, i, f):
    return t.b(f, 0xB2 + i), t.b(f, 0xB7 + i)


def ppos(t, f):
    return t.b(f, 0xB1), t.b(f, 0xB6)


def dist(a, b):
    return abs(a[0] - b[0]) + abs(a[1] - b[1])


def ghost_facts(t, tune, last):
    """What the four ghosts do from the start of play to `last`, while Pac-Man's pause flag is clear."""
    facts = []
    facts.append('Ghost start places at frame %d (play begins): %s.' % (tune, '; '.join('ghost %d (%d,%d)' % ((i,) + gpos(t, i, tune)) for i in range(4))))
    for i in range(4):
        ch, prev = [], gpos(t, i, tune)
        for f in range(tune + 1, last + 1):
            p = gpos(t, i, f)
            if p != prev:
                ch.append((f, dist(p, prev)))
            prev = p
        if not ch:
            facts.append('Ghost %d never changes place in this entry.' % i)
            continue
        res = collections.Counter(f % 4 for f, d in ch)
        dl = collections.Counter(d for f, d in ch)
        gaps = collections.Counter(b[0] - a[0] for a, b in zip(ch, ch[1:]))
        facts.append('Ghost %d: first change of place on frame %d (%d frames after play began); %d changes in all, on frames whose number mod 4 is %s; changes of %s pixels (count); frames between changes: %s.'
                     % (i, ch[0][0], ch[0][0] - tune, len(ch), ', '.join('%d (%d times)' % kv for kv in sorted(res.items())),
                        ', '.join('%d: %d' % kv for kv in sorted(dl.items())), ', '.join('%d: %d' % kv for kv in gaps.most_common(4))))
    for k in (60, 120, 180, 240):
        f = tune + k
        if f <= last and not t.paused(f):
            facts.append('%d frames after play began (frame %d): Pac-Man %s; ghosts %s.' % (k, f, '(%d,%d)' % ppos(t, f), ', '.join('%d:(%d,%d)' % ((i,) + gpos(t, i, f)) for i in range(4))))
    death = t.first(tune, lambda f: t.paused(f))
    if death is not None:
        near = sorted((dist(gpos(t, i, death), ppos(t, death)), i) for i in range(4))
        facts.append('Frame %d (%d frames after play began): the pause flag is set (Pac-Man is caught). Distance of each ghost from Pac-Man in pixels (x plus y): %s.'
                     % (death, death - tune, ', '.join('ghost %d: %d' % (i, d) for d, i in sorted(near, key=lambda x: x[1]))))
    else:
        facts.append('No ghost has caught Pac-Man by frame %d.' % last)
    return facts


def paths(t, tune, last):
    out = []
    for i in range(4):
        seq = []
        for f in range(tune, last + 1):
            p = gpos(t, i, f)
            if not seq or seq[-1] != p:
                seq.append(p)
        out.append(seq)
    return out


def compare(t, tune, tlast, ref, rtune, rlast, name):
    """Two comparisons with the first entry, both only up to the earlier of the two catches: frame by frame from the start of
    play, and by the sequence of places each ghost visits (timing ignored)."""
    n = min(tlast - tune, rlast - rtune) + 1
    first = None
    for k in range(n):
        for i in range(4):
            if gpos(t, i, tune + k) != gpos(ref, i, rtune + k):
                first = (k, i)
                break
        if first:
            break
    if first:
        k, i = first
        text = ('Compared with %s frame by frame from the start of play (%d frames, up to the earlier catch): the first difference in a ghost place is %d frames after play began (ghost %d: (%d,%d) here, (%d,%d) in %s).'
                % (name, n, k, i, gpos(t, i, tune + k)[0], gpos(t, i, tune + k)[1], gpos(ref, i, rtune + k)[0], gpos(ref, i, rtune + k)[1], name))
    else:
        text = 'Compared with %s frame by frame from the start of play: no ghost place differs in the %d frames compared (up to the earlier catch).' % (name, n)
    pa, pb = paths(t, tune, tlast), paths(ref, rtune, rlast)
    res = []
    for i in range(4):
        m = min(len(pa[i]), len(pb[i]))
        d = next((j for j in range(m) if pa[i][j] != pb[i][j]), None)
        res.append('ghost %d: %s' % (i, ('the same %d places in the same order' % m) if d is None else 'first different place is number %d' % d))
    # the last place of the shorter list may be unfinished, so the final one is not compared
    text2 = 'Ghost routes compared with %s by the places each ghost visits, timing ignored (up to the earlier catch): %s.' % (name, '; '.join(res))
    return [text, text2], first[0] if first else None


def main():
    e2.ROM = pathlib.Path(sys.argv[1]).resolve()
    checks = []
    results = {}
    how = ' (switch on, press Game Reset, wait for the start tune; frames are counted from power-on at 60 per second).'
    specs = [
        ('BL-038', 'Ghosts from the start, Pac-Man stands still', 'Game Reset on frame 240, then nobody touches the joystick: where the four ghosts go until one catches Pac-Man.', 240, [], 800),
        ('BL-039', 'The same start, Game Reset one frame later', 'As BL-038 but Game Reset is pressed on frame 241: do the ghosts do the same thing, counted from the start of play?', 241, [], 800),
        ('BL-040', 'The same start, Game Reset 60 frames later', 'As BL-038 but Game Reset is pressed on frame 300 (a different place in the attract-mode timer).', 300, [], 800),
        ('BL-041', 'Pac-Man walks left 12 pixels and stands still', 'As BL-038 but LEFT is held on frames 370-391: does the ghosts\' way change when Pac-Man moves?', 240, [(370, 392, LEFT)], 800),
        ('BL-042', 'The same walk left, one frame later', 'As BL-041 but LEFT is held on frames 371-392 (the input is one frame later).', 240, [(371, 393, LEFT)], 800),
        ('BL-043', 'Pac-Man walks left, down and right along the bottom corridor', 'LEFT, DOWN, RIGHT held in turn from the start place: how the ghosts follow.', 240, [(370, 420, LEFT), (420, 470, DOWN), (470, 520, RIGHT)], 800),
    ]
    ref = None
    for name, title, shows, reset, segs, n in specs:
        bits = [0] * n
        e2.press(bits, reset, e3.RESET)
        for a, e, m in segs:
            for f in range(a, min(e, n)):
                bits[f] |= m
        t = e2.make(name, title, shows, bits)
        tune = t.first(reset, lambda f: t.lives(f) == 3 and not t.paused(f) and f > reset + 60)
        death = t.first(tune, lambda f: t.paused(f))
        last = (death - 1) if death else t.n - 1
        facts = ['Game Reset on frame %d starts a game; play begins on frame %d (the start tune takes %d frames).' % (reset, tune, tune - reset)]
        facts += ['Joystick on frames %d-%d: %s.' % (a, e, e3.held(m)) for a, e, m in e3.input_runs(t, tune, t.n) if m]
        facts.append('Pac-Man starts at (%d,%d).' % ppos(t, tune))
        facts += ghost_facts(t, tune, last)
        rest = 'frames of the entry up to the catch (frame %d)' % last
        if ref is None:
            ref = (t, tune, name, last)
            results[name] = None
        else:
            texts, k = compare(t, tune, last, ref[0], ref[1], ref[3], ref[2])
            facts += texts
            results[name] = k
        e2.write_entry(name, '', '', t.n, facts, 'switch on, press Game Reset, then do what the list of joystick moves says; watch the ghosts leave the box and come at Pac-Man.')
        checks.append((name, tune is not None))
    print('first ghost difference from BL-038 (frames after play began):', results)

    # BL-044: the first 4,000 frames of the bot route
    txt = e3.ROUTE.read_text().split('\n')
    n = 4000
    bits = [0] * n
    ev = [tuple(map(int, l.split())) for l in txt[3:] if l]
    for i, (f, v) in enumerate(ev):
        end = ev[i + 1][0] if i + 1 < len(ev) else 10 ** 9
        for k in range(f, min(end, n)):
            bits[k] = v
    t = e2.make('BL-044', 'Computer play: the first power pill and the first ghosts eaten',
                'The first 4,000 frames of the Stage C7 bot route: the first power pill (frames 2226-2673) and the first ghosts eaten.', bits)
    facts = ['The input is the first 4,000 frames of the Stage C7 bot route (tools\\captures\\c7_route.txt): a computer-played game.']
    pw = [f for f in range(1, t.n) if t.b(f, 0xF2) & 2 and t.lives(f) == 3 and f > 1900]
    wins, s = [], None
    for f in pw:
        if s is None:
            s = p = f
        elif f != p + 1:
            wins.append((s, p))
            s = p = f
        else:
            p = f
    if s is not None:
        wins.append((s, p))
    facts.append('Power pill in effect (byte $F2 bit 1 set during play): %s.' % '; '.join('frames %d-%d (%d frames)' % (a, b, b - a + 1) for a, b in wins))
    for a, b in wins:
        inside = [f for f in range(a, b) if t.b(f, 0xD2) & 0x40 and not t.b(f - 1, 0xD2) & 0x40]
        facts.append('During frames %d-%d Pac-Man is stopped (pause flag $D2 bit 6 set) %d time%s, starting on frame%s %s.' % (a, b, len(inside), '' if len(inside) == 1 else 's', '' if len(inside) == 1 else 's', ', '.join(map(str, inside)) or '-'))
    # normal versus power-pill step sizes and rhythm of each ghost
    inpow = lambda f: any(a <= f <= b for a, b in wins)
    for i in range(4):
        d_in, d_out, res = collections.Counter(), collections.Counter(), collections.Counter()
        prev = gpos(t, i, 1930)
        for f in range(1931, t.n):
            p = gpos(t, i, f)
            if p != prev:
                (d_in if inpow(f) else d_out)[dist(p, prev)] += 1
                res[f % 4] += 1
            prev = p
        facts.append('Ghost %d, frames 1931-3999: changes of place by size outside the power pill: %s; inside it: %s; frame numbers mod 4 of all changes: %s.'
                     % (i, ', '.join('%d px: %d' % kv for kv in sorted(d_out.items())), ', '.join('%d px: %d' % kv for kv in sorted(d_in.items())) or 'none', ', '.join('%d: %d' % kv for kv in sorted(res.items()))))
    # the ghosts that were eaten: the ghost nearest Pac-Man when the pause flag came on during a power pill
    for f in range(1931, t.n):
        if t.b(f, 0xD2) & 0x40 and not t.b(f - 1, 0xD2) & 0x40 and inpow(f):
            near = min((dist(gpos(t, i, f), ppos(t, f)), i) for i in range(4))
            if near[0] > 6:
                facts.append('Frame %d: Pac-Man stops (pause flag) during a power pill but no ghost is within 6 pixels (the nearest, ghost %d, is %d pixels away; this stop is not a ghost eaten by distance).' % (f, near[1], near[0]))
                continue
            i = near[1]
            home = t.first(f + 1, lambda g: 64 <= gpos(t, i, g)[0] <= 88 and gpos(t, i, g)[1] <= 40)
            end = t.paused(f)
            stop = t.first(f, lambda g: not t.paused(g))
            facts.append('Frame %d: ghost %d is %d pixels from Pac-Man and Pac-Man stops (pause flag) until frame %d (%d frames). Ghost %d is at (%d,%d) then and %s.'
                         % (f, i, near[0], stop, stop - f, i, gpos(t, i, f)[0], gpos(t, i, f)[1],
                            ('is next inside the start area (x 64-88, y 40 or less) on frame %d (%d frames later), at (%d,%d)' % (home, home - f, gpos(t, i, home)[0], gpos(t, i, home)[1])) if home is not None else 'does not get back to the start area before frame %d' % (t.n - 1)))
    e2.write_entry('BL-044', '', '', t.n, facts, 'replay tools\\captures\\c7_captures.txt in Stella (Stage C7) and watch the first power pill: the ghosts turn lilac and Pac-Man can eat them.')
    checks.append(('BL-044', bool(wins)))
    bad = [n for n, ok in checks if not ok]
    print('entries made: %d; checks failed: %s' % (len(checks), bad or 'none'))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
