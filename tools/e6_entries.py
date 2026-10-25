"""Stage E6: makes the baseline entries about dying and game over (BL-048, BL-049) and checks them.

  python -I tools\\e6_entries.py <rom>

Same method as Stages E2-E5 (computed observations, two byte-identical traces, SHA-256 stored). Bytes used (working labels of
Stages C7 and E3-E5, not yet confirmed): lives $98, pause flag $D2 bit 6, Pac-Man $B1/$B6, ghost i $B2+i/$B7+i, score $CC/$CE,
wafer map $88-$97. The sound column of the trace is the fingerprint of each frame's sound; the most common one is silence.
"""
import collections
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import e2_entries as e2  # noqa: E402
import e3_entries as e3  # noqa: E402
import e4_entries as e4  # noqa: E402
import e5_entries as e5  # noqa: E402

FIRE = 16


def deaths(t, first):
    """(start, drop, end) of every death: the pause flag comes on with the lives counter unchanged, lives drop later, the flag clears."""
    out, f = [], first
    while f < t.n:
        if t.paused(f) and not t.paused(f - 1) and t.lives(f) > 0:
            end = t.first(f, lambda g: not t.paused(g))
            drop = t.first(f, lambda g: t.lives(g) != t.lives(f))
            if drop is not None and (end is None or drop <= (end or t.n)) and t.lives(drop) == t.lives(f) - 1:
                out.append((f, drop, end))
                f = (end or t.n)
                continue
        f += 1
    return out


def death_facts(t, d, start, drop, end, tag, silent, prev_play):
    facts = []
    near = sorted((e4.dist(e4.gpos(t, i, start), e4.ppos(t, start)), i) for i in range(4))
    facts.append('%s: the pause flag comes on at frame %d%s with the lives counter at %d; ghost distances from Pac-Man (x plus y): %s.'
                 % (tag, start, (' (%d frames after the last play began)' % (start - prev_play)) if prev_play is not None else '', t.lives(start),
                    ', '.join('ghost %d: %d' % (i, dd) for dd, i in sorted(near, key=lambda x: x[1]))))
    facts.append('%s: the lives counter goes from %d to %d on frame %d (%d frames after the flag came on); %s.'
                 % (tag, t.lives(start), t.lives(drop), drop, drop - start, ('the flag clears on frame %d (%d frames after it came on)' % (end, end - start)) if end else 'the flag is still set at the end of the entry'))
    if end:
        facts.append('%s: Pac-Man was at (%d,%d) when caught and is at (%d,%d) when the flag clears; the ghosts are at %s then; score %d before and %d after; wafers in the map %d before and %d after.'
                     % (tag, e4.ppos(t, start)[0], e4.ppos(t, start)[1], e4.ppos(t, end)[0], e4.ppos(t, end)[1],
                        ', '.join('(%d,%d)' % e4.gpos(t, i, end) for i in range(4)), e5.score(t, start), e5.score(t, end), e5.wafers(t, start), e5.wafers(t, end)))
    snd = [f for f in range(start, (end or t.n)) if t.aud(f) != silent]
    if snd:
        facts.append('%s: sound is on %d of the %d frames from the flag coming on to its clearing: first on frame %d (%d after), last on frame %d (%d after).'
                     % (tag, len(snd), (end or t.n) - start, snd[0], snd[0] - start, snd[-1], snd[-1] - start))
    else:
        facts.append('%s: no sound on any frame of the pause.' % tag)
    pics = len(set(t.pic(f) for f in range(start, (end or t.n))))
    facts.append('%s: %d different pictures are drawn during the pause (the death animation).' % (tag, pics))
    return facts


def main():
    e2.ROM = pathlib.Path(sys.argv[1]).resolve()
    checks = []

    # BL-048: an idle game from start to game over, then the fire button
    n = 3000
    bits = [0] * n
    e2.press(bits, 240, e3.RESET)
    e2.press(bits, 1900, FIRE)
    e2.press(bits, 2400, FIRE)
    t = e2.make('BL-048', 'An idle game: three lives lost, game over, the fire button starts a new game',
                'Game Reset on frame 240, then nobody plays and the fire button is pressed on frames 1900 and 2400: three deaths, the game over, the ghosts after it, and the new game.', bits)
    silent = collections.Counter(t.aud(f) for f in range(1, t.n)).most_common(1)[0][0]
    tune = t.first(300, lambda f: not t.paused(f))
    ds_all = deaths(t, tune)
    ds = [d for d in ds_all if d[0] < 2400]
    facts = ['Game Reset on frame 240 starts a game; play begins on frame %d. Nobody touches the joystick.' % tune]
    play = tune
    for k, (s, dr, en) in enumerate(ds):
        facts += death_facts(t, k, s, dr, en, 'Death %d' % (k + 1), silent, play)
        play = en
    over = t.first(tune, lambda f: t.lives(f) == 0)
    # game over: lives 0
    facts.append('Game over: the lives counter reads 0 from frame %d; the pause flag clears on frame %d (the third death took %d frames from the flag coming on to the clearing).'
                 % (over, t.first(over, lambda f: not t.paused(f)), t.first(over, lambda f: not t.paused(f)) - ds[-1][0] if len(ds) == 3 else -1))
    fp = t.first(over + 1, lambda f: not t.paused(f))
    gp = t.first(fp, lambda f: any(e4.gpos(t, i, f) != (76, 30) for i in range(4)))
    facts.append('At game over the ghosts stand at %s and Pac-Man at (%d,%d). The ghosts start to move again on frame %d (%d frames after the flag cleared) and keep moving; the score stays %d.'
                 % (', '.join('(%d,%d)' % e4.gpos(t, i, fp) for i in range(4)), e4.ppos(t, fp)[0], e4.ppos(t, fp)[1], gp, gp - fp, e5.score(t, t.n - 1) if e5.score(t, fp) == e5.score(t, t.n - 1) else e5.score(t, fp)))
    park = t.first(over, lambda f: e4.ppos(t, f) != e4.ppos(t, over))
    facts.append('The fire button pressed on frames 1900-1909 (%d frames after game over) does nothing: the lives counter stays 0 and no game starts. On frame %s Pac-Man moves from (%d,%d) to (%d,%d) (the place he keeps from then on).'
                 % (1900 - over, park, e4.ppos(t, over)[0], e4.ppos(t, over)[1], e4.ppos(t, park)[0], e4.ppos(t, park)[1]))
    st = t.first(2400, lambda f: t.lives(f) == 3)
    facts.append('The fire button pressed on frames 2400-2409 starts a new game: the lives counter reads 3 from frame %d (%d frame%s after the press began); the pause flag (start tune) is set until frame %d (%d frames); the score is %d and the map holds %d wafers when play begins.'
                 % (st, st - 2400, '' if st == 2400 else 's', t.first(st, lambda f: not t.paused(f)) - 1, t.first(st, lambda f: not t.paused(f)) - st, e5.score(t, t.first(st, lambda f: not t.paused(f))), e5.wafers(t, t.first(st, lambda f: not t.paused(f)))))
    for s2, dr2, en2 in ds_all[len(ds):]:
        facts.append('In the new game (play began on frame %d) the first death begins on frame %d, %d frames after play began, with the lives counter at %d; the lives counter drops %d frames later, as in the first game.' % (t.first(st, lambda f: not t.paused(f)), s2, s2 - t.first(st, lambda f: not t.paused(f)), t.lives(s2), dr2 - s2))
    e2.write_entry('BL-048', '', '', t.n, facts, 'start a game and do nothing: Pac-Man is caught three times; after the last one the board freezes, the ghosts go back to the box and walk again; the fire button starts a new game.')
    checks.append(('BL-048', len(ds) == 3 and over is not None and st is not None))

    # BL-049: every death of the bot route and its game over
    n = 9800
    bits = e5.route_bits(n)
    t = e2.make('BL-049', 'Deaths in a computer-played game and the game over',
                'The 9,800-frame bot route (as BL-035): the four deaths of that game, the lives dropping, the respawn, the sound and the game over.', bits)
    silent = collections.Counter(t.aud(f) for f in range(1, t.n)).most_common(1)[0][0]
    ds = deaths(t, 1930)
    facts = ['The input is the whole Stage C7 bot route (as BL-035). Deaths found (pause flag on, lives counter drops later): %d.' % len(ds)]
    prev = 1930
    for k, (s, dr, en) in enumerate(ds):
        facts += death_facts(t, k, s, dr, en, 'Death %d' % (k + 1), silent, None)
    lengths = sorted(set((en - s) for s, dr, en in ds if en))
    drops = sorted(set(dr - s for s, dr, en in ds))
    facts.append('Over all %d deaths the pause lasts %s frames and the lives counter drops %s frames after the flag came on.' % (len(ds), ', '.join(map(str, lengths)) or 'to the end of the entry', ', '.join(map(str, drops))))
    over = t.first(1930, lambda f: t.lives(f) == 0)
    if over is not None:
        facts.append('Game over: the lives counter reads 0 from frame %d; Pac-Man at (%d,%d); the ghosts at %s; score %d.' % (over, e4.ppos(t, over)[0], e4.ppos(t, over)[1], ', '.join('(%d,%d)' % e4.gpos(t, i, over) for i in range(4)), e5.score(t, over)))
        still = t.first(over, lambda f: e4.ppos(t, f) != e4.ppos(t, over))
        facts.append('From game over to the end of the entry (frame %d) Pac-Man %s.' % (t.n - 1, 'does not move' if still is None else 'first moves on frame %d' % still))
    e2.write_entry('BL-049', '', '', t.n, facts, 'replay the route of tools\\captures\\c7_captures.txt in Stella: Pac-Man is caught four times (frames given above) and loses a life 125 frames after each catch.')
    checks.append(('BL-049', len(ds) >= 4 and over is not None))

    bad = [x for x, ok in checks if not ok]
    print('entries made: %d; checks failed: %s' % (len(checks), bad or 'none'))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
