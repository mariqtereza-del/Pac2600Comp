"""Stage E8: makes the baseline entries about edge cases (BL-058 .. BL-070) and checks them.

  python -I tools\\e8_entries.py <rom>

Same method as Stages E2-E7 (computed observations, two byte-identical traces, SHA-256 stored). Entries BL-065 and BL-066 are
ARTIFICIAL: they change one memory byte on one frame (`pokes:` line in the entry text, `--poke` of the host, test only) to reach a
score that cannot be reached by playing in reasonable time; the plain-words text says so. Bytes (working labels, not confirmed):
lives $98, pause flag $D2 bit 6, Pac-Man $B1/$B6, ghost i $B2+i/$B7+i, score $CC/$CE, wafer map $88-$97, two players $E6 bit 6,
player in turn $E6 bit 7 (found in this stage), left difficulty A $E7 bit 5, black-and-white $EF high nibble.
"""
import collections
import pathlib
import subprocess
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import e2_entries as e2  # noqa: E402
import e3_entries as e3  # noqa: E402
import e4_entries as e4  # noqa: E402
import e5_entries as e5  # noqa: E402
import e6_entries as e6  # noqa: E402
import e7_entries as e7  # noqa: E402

SELECT, RESET, FIRE = 32, 64, 16
LDIFF, RDIFF, BW = 128, 256, 512
LEFT, RIGHT, UP, DOWN = e3.LEFT, e3.RIGHT, e3.UP, e3.DOWN


def bits_for(n, segs, presses=()):
    bits = [0] * n
    e2.press(bits, 240, RESET)
    for a, e, m in segs:
        for f in range(a, min(e, n)):
            bits[f] |= m
    for f, m in presses:
        e2.press(bits, f, m)
    return bits


def make(name, title, shows, bits, pokes=''):
    """Like e2.make, with an optional `pokes:` line (TEST ONLY memory edits, see tools\\baseline.py)."""
    e2.write_inputs(name, bits)
    head = 'id: %s\ntitle: %s\nshows: %s\ninputs: %s.inputs\nframes: %d\nrom_sha1: %s\n' % (name, title, shows, name, len(bits), e2.SHA)
    if pokes:
        head += 'pokes: %s\n' % pokes
    head += 'trace_sha256: (made by tools\\baseline.py make)\ntrace_bytes: (made by tools\\baseline.py make)\nobservations:\n'
    (e2.BASE / (name + '.entry.txt')).write_text(head, encoding='utf-8', newline='')
    r = subprocess.run([sys.executable, '-I', str(e2.ROOT / 'tools' / 'baseline.py'), 'make', str(e2.ROM), name], capture_output=True, text=True)
    print(r.stdout.strip())
    if r.returncode != 0:
        raise SystemExit(name + ': baseline.py make failed: ' + r.stdout + r.stderr)
    return e2.T(e2.read_trace(name))


def scratch_trace(bits, pokes, tag):
    """A trace that is not an entry (used for comparisons); written under reference\\baseline."""
    ins = e2.OUT / (tag + '.inputs')
    out = ['PacMan2600-inputs 1', 'rom_sha1=' + e2.SHA, 'frames=%d' % len(bits)]
    prev = None
    for f, b in enumerate(bits):
        if b != prev:
            out.append('%d %d' % (f, b))
            prev = b
    ins.write_text('\n'.join(out) + '\n')
    args = [str(e2.ROOT / 'build' / 'host' / 'pacman2600.exe'), '--headless', str(e2.OUT / (tag + '.png')), str(e2.ROM), '--replay', str(ins),
            '--trace', str(e2.OUT / (tag + '.trace')), '--settings', str(e2.OUT / (tag + '.ini'))]
    for p in pokes:
        args += ['--poke', p]
    r = subprocess.run(args, capture_output=True, text=True)
    assert r.returncode == 0, r.stderr
    return e2.T(e2.read_trace(tag))


def state(t, f):
    return 'lives %d, score %d, %d wafers in the map, Pac-Man (%d,%d), ghosts %s' % (
        t.lives(f), e5.score(t, f), e5.wafers(t, f), e4.ppos(t, f)[0], e4.ppos(t, f)[1], ' '.join('(%d,%d)' % e4.gpos(t, i, f) for i in range(4)))


def first_diff(t, c, f0):
    for f in range(f0, min(t.n, c.n)):
        if t.ram[f] != c.ram[f] or t.pic(f) != c.pic(f):
            return f
    return None


def main():
    e2.ROM = pathlib.Path(sys.argv[1]).resolve()
    checks = []
    walk = [(370, 420, LEFT)]

    # BL-058: Game Reset in the middle of a game
    bits = bits_for(900, walk, [(450, RESET)])
    t = make('BL-058', 'Game Reset pressed in the middle of a game', 'A game is started, Pac-Man walks left and eats two wafers, then Game Reset is pressed on frame 450: the game starts again.', bits)
    tune = t.first(300, lambda f: not t.paused(f))
    again = t.first(451, lambda f: not t.paused(f))
    facts = ['Game Reset on frame 240 starts a game; play begins on frame %d.' % tune, 'Joystick: LEFT on frames 370-419. Frame 449: %s.' % state(t, 449),
             'Game Reset is pressed on frames 450-459 (the console switch register reads 3e).',
             'Frame 450 (the same frame): the pause flag comes on; %s. It stays on until frame %d (%d frames, the start tune again).' % (state(t, 450), again - 1, again - 450),
             'Frame %d (play begins again): %s.' % (again, state(t, again))]
    e2.write_entry('BL-058', '', '', t.n, facts, 'start a game, eat a few wafers, then press Game Reset: the game starts again from nothing, three lives, score 0.')
    checks.append(('BL-058', t.lives(450) == 3 and again - 450 > 100))

    # BL-059: Game Select pressed in the middle of a game, then Game Reset
    bits = bits_for(1200, walk, [(450, SELECT), (700, RESET)])
    t = make('BL-059', 'Game Select pressed in the middle of a game, then Game Reset', 'As BL-058 but Game Select is pressed on frame 450 and Game Reset on frame 700.', bits)
    facts = ['Game Reset on frame 240 starts a game (game 1, one player); Pac-Man walks left (LEFT on frames 370-419). Frame 449: %s; byte $E6 reads %02x.' % (state(t, 449), t.b(449, 0xE6)),
             'Game Select is pressed on frames 450-459 (switch register 3d).',
             'Frame 450 (same frame): %s; byte $E6 reads %02x (bit 6 set: the selection is now two players, the next in the cycle of Stage E2); the pause flag is %s.' % (state(t, 450), t.b(450, 0xE6), 'on' if t.paused(450) else 'off'),
             'Frame 699: %s; the lives counter is still 0, Pac-Man stays at (64,96) and the ghosts are walking (the game is no longer being played).' % state(t, 699),
             'Game Reset on frames 700-709 starts a game for %d players: frame 700 lives %d, byte $E6 %02x.' % (2 if t.b(700, 0xE6) & 0x40 else 1, t.lives(700), t.b(700, 0xE6))]
    e2.write_entry('BL-059', '', '', t.n, facts, 'start a game, then press Game Select: the game stops at once; Game Reset then starts the next selection.')
    checks.append(('BL-059', t.lives(450) == 0))

    # BL-060 .. BL-063: the switches during play, against a control
    n = 800
    control = make('BL-060', 'Control: the same walk with no switch touched', 'The walk of BL-061 to BL-063 with every console switch left alone: what they are compared with.', bits_for(n, walk))
    cf = e2.write_entry('BL-060', '', '', control.n, ['Game Reset on frame 240; Pac-Man walks left (LEFT on frames 370-419); no switch is touched. Frame 799: %s.' % state(control, 799)], 'start a game and walk left.')
    for name, mask, label, what in (('BL-061', LDIFF, 'left difficulty switch moved to A', 'left'), ('BL-062', RDIFF, 'right difficulty switch moved to A', 'right'), ('BL-063', BW, 'colour/black-and-white switch moved to black and white', 'bw')):
        t = make(name, 'During play: the %s' % label, 'As BL-060 but the %s on frame 450 and left there.' % label, bits_for(n, walk + [(450, n, mask)]))
        d = first_diff(t, control, 360)
        gh = next((f for f in range(min(t.n, control.n)) if any(e4.gpos(t, i, f) != e4.gpos(control, i, f) for i in range(4))), None)
        pm = next((f for f in range(min(t.n, control.n)) if e4.ppos(t, f) != e4.ppos(control, f)), None)
        facts = ['As BL-060 (Game Reset on frame 240, LEFT on frames 370-419) and, from frame 450 to the end, the %s (input bit %d).' % (label, mask)]
        if d is None:
            facts.append('Compared with BL-060 frame by frame (all 128 memory bytes and the picture): no difference on any of the %d frames.' % control.n)
        else:
            ch = [(0x80 + i, control.ram[d][i], t.ram[d][i]) for i in range(128) if control.ram[d][i] != t.ram[d][i]]
            facts.append('Compared with BL-060 frame by frame: the first difference is on frame %d (the switch register changes on frame 450); at that frame the memory bytes that differ are %s; the picture %s.'
                         % (d, ', '.join('$%02x: %02x in BL-060, %02x here' % c for c in ch) or 'none', 'differs' if t.pic(d) != control.pic(d) else 'is the same'))
            later = sorted(set(0x80 + i for f in range(d, min(t.n, control.n)) for i in range(128) if control.ram[f][i] != t.ram[f][i]))
            facts.append('Over the whole entry the memory bytes that ever differ from BL-060 are: %s.' % ', '.join('$%02x' % a for a in later))
        facts.append('First frame at which a ghost place differs from BL-060: %s; Pac-Man place: %s; the lives counter on frame 799 is %d (BL-060: %d).'
                     % (gh if gh is not None else 'none', pm if pm is not None else 'none', t.lives(799), control.lives(799)))
        e2.write_entry(name, '', '', t.n, facts, 'start a game, walk left, then move that switch on the console while the game runs.')
        checks.append((name, True))
    checks.append(('BL-060', True))

    # BL-064: a very long idle
    n = 40000
    t = make('BL-064', 'A very long idle: 40,000 frames at the title screen', 'The console is switched on and nothing is touched for 40,000 frames (11 minutes).', [0] * n)
    seen, rep = {}, None
    for f in range(t.n):
        if t.ram[f] in seen and rep is None:
            rep = (f, seen[t.ram[f]])
        seen.setdefault(t.ram[f], f)
    ch = collections.Counter()
    for f in range(1, t.n):
        for i in range(128):
            if t.ram[f][i] != t.ram[f - 1][i]:
                ch[0x80 + i] += 1
    facts = ['The switch register reads 3f on every frame: %s; the lives counter stays %d.' % (all(r[2] == '3f' for r in t.rows), t.lives(0)),
             '%d different pictures and %d different memory states in the 40,000 frames.' % (len(set(t.pic(f) for f in range(t.n))), len(seen)),
             'The whole 128-byte memory state %s.' % (('first repeats on frame %d (it equals frame %d): the attract mode is a loop of %d frames' % (rep[0], rep[1], rep[0] - rep[1])) if rep else 'never repeats within the entry'),
             'Memory bytes that change at all (changes counted): %s.' % ', '.join('$%02x: %d' % kv for kv in sorted(ch.items())),
             'Byte $81 runs from %d to %d and byte $EC from %d to %d; the lives counter stays %d; the score bytes ($CC/$CE) read %02x/%02x on the last frame.'
             % (min(t.b(f, 0x81) for f in range(t.n)), max(t.b(f, 0x81) for f in range(t.n)), min(t.b(f, 0xEC) for f in range(t.n)), max(t.b(f, 0xEC) for f in range(t.n)), t.lives(t.n - 1), t.b(t.n - 1, 0xCC), t.b(t.n - 1, 0xCE))]
    e2.write_entry('BL-064', '', '', t.n, facts, 'switch on and watch the title screen for eleven minutes without touching anything.')
    checks.append(('BL-064', True))

    # BL-065, BL-066: ARTIFICIAL score tests
    walkx = [(370, 420, LEFT), (420, 470, DOWN), (470, 540, RIGHT)]
    for name, cc, ce, label in (('BL-065', 0x95, 0x99, '9,995'), ('BL-066', 0x95, 0x49, '4,995')):
        pokes = '380:CC:%02x 380:CE:%02x' % (cc, ce)
        t = make(name, 'ARTIFICIAL: the score is set to %s by a memory edit' % label,
                 'NOT A REAL GAME SITUATION. On frame 380 two memory bytes are overwritten so that the score reads %s; Pac-Man then eats wafers (LEFT, DOWN, RIGHT) and the score crosses %s.' % (label, '10,000' if name == 'BL-065' else '5,000'),
                 bits_for(700, walkx), pokes)
        ev = [f for f in range(381, t.n) if e5.score(t, f) != e5.score(t, f - 1)]
        facts = ['ARTIFICIAL TEST: on frame 380 (before running that frame) byte $CC is set to %02x and byte $CE to %02x (a score of %s); in a real game this is not possible by playing. Nothing else is touched.' % (cc, ce, label),
                 'Pac-Man walks LEFT (frames 370-419), DOWN (420-469), RIGHT (470-539) and eats wafers as in BL-045.',
                 'Score changes (frame: score bytes $CE/$CC, decimal score, lives): %s.' % '; '.join('%d: %02x/%02x, %d, %d' % (f, t.b(f, 0xCE), t.b(f, 0xCC), e5.score(t, f), t.lives(f)) for f in [379, 380] + ev),
                 'The lives counter reads %s on every frame from 380 to the end: %s.' % (t.lives(380), 'no extra life' if all(t.lives(f) == t.lives(380) for f in range(380, t.n)) else 'it CHANGES (see above)'),
                 'Byte $CD reads %02x throughout (it never takes part).' % t.b(t.n - 1, 0xCD)]
        if name == 'BL-066':
            res = []
            for k in range(1, 11):
                ce2 = (((k - 1) % 10) << 4) | 9
                sc = scratch_trace(bits_for(700, walkx), ['380:CC:95', '380:CE:%02x' % ce2], 'E8_scratch_%d' % k)
                res.append((k * 1000, sorted(set(sc.lives(f) for f in range(380, sc.n)))))
            facts.append('Every thousand from 1,000 to 10,000 was crossed the same way (score set 5 below it): lives counter values seen: %s. So no extra life is given at any multiple of 1,000.'
                         % '; '.join('%d: %s' % (s, ','.join(map(str, l))) for s, l in res))
            gains = 0
            ok_games = []
            for g in ('BL-054', 'BL-055', 'BL-056'):
                tt = e2.T(e2.read_trace(g))
                start = tt.first(1, lambda f: tt.lives(f) == 3)
                rises = [f for f in range(start + 1, tt.n) if tt.lives(f) == tt.lives(f - 1) + 1 and tt.lives(f - 1) > 0]
                refills = [f for f in rises if e5.wafers(tt, f) > e5.wafers(tt, f - 1) + 20]
                ok_games.append('%s: %d rises of the lives counter, %d of them at a level clear, top score %d' % (g, len(rises), len(refills), max(e5.score(tt, f) for f in range(start, tt.n) if tt.lives(f) > 0)))
                gains += len(rises) - len(refills)
            facts.append('In the three long bot games (scores up to about 4,000) the lives counter rose %d times other than at a level clear (%s).' % (gains, '; '.join(ok_games)))
        e2.write_entry(name, '', '', t.n, facts, 'this cannot be checked on the real console without a cheat device; it is a memory edit in the emulator.')
        checks.append((name, True))

    # BL-067: two players
    n = 3800
    t = make('BL-067', 'Two players: an idle game for two players to the end', 'Game Select pressed once (game 1, two players), Game Reset on frame 240, nobody plays: the turns of the two players.', bits_for(n, walk, [(120, SELECT)]))
    facts = ['Game Select is pressed on frames 120-129 and Game Reset on frame 240 (a two-player game, byte $E6 bit 6 set); Pac-Man walks left (LEFT on frames 370-419); then nobody touches the joystick.']
    plays = [f for f in range(241, t.n) if not t.paused(f) and (t.paused(f - 1))]
    deaths = [f for f in range(241, t.n) if t.paused(f) and not t.paused(f - 1)]
    for f in plays:
        facts.append('Play begins on frame %d: byte $E6 reads %02x (bit 7: %d); lives counter %d; score bytes %02x/%02x; wafers in the map %d; byte $99 %02x; Pac-Man (%d,%d).'
                     % (f, t.b(f, 0xE6), t.b(f, 0xE6) >> 7, t.lives(f), t.b(f, 0xCE), t.b(f, 0xCC), e5.wafers(t, f), t.b(f, 0x99), e4.ppos(t, f)[0], e4.ppos(t, f)[1]))
    for f in deaths:
        end = t.first(f, lambda g: not t.paused(g))
        facts.append('The pause flag comes on at frame %d with $E6 = %02x and lives %d; it clears on frame %d with $E6 = %02x and lives %d.' % (f, t.b(f, 0xE6), t.lives(f), end, t.b(end, 0xE6) if end else -1, t.lives(end) if end else -1))
    over = t.first(241, lambda f: t.lives(f) == 0)
    facts.append('The lives counter reads 0 from frame %s.' % over)
    e2.write_entry('BL-067', '', '', t.n, facts, 'press Game Select once (two bars appear), Game Reset, and let both players be caught: the turn passes from one to the other after each death.')
    checks.append(('BL-067', len(plays) >= 3))

    # BL-068, BL-069: random joystick, switches, Select and Reset (a net for situations nobody thought of)
    for k, (name, seed) in enumerate((('BL-068', 31), ('BL-069', 32))):
        n = 24000
        lcg = seed
        bits = [0] * n
        e2.press(bits, 240, RESET)
        f = 370
        while f < n:
            lcg = (lcg * 6364136223846793005 + 1442695040888963407) % (1 << 64)
            r = lcg >> 33
            length = 4 + r % 37
            m = [UP, DOWN, LEFT, RIGHT, 0, 0][(r >> 8) % 6]
            for g in range(f, min(f + length, n)):
                bits[g] |= m
            f += length
        ev = []
        for s in range(1200, n, 1100):
            lcg = (lcg * 6364136223846793005 + 1442695040888963407) % (1 << 64)
            r = lcg >> 33
            kind = r % 6
            sw = [RESET, SELECT, FIRE, LDIFF, RDIFF, BW][kind]
            span = 10 if kind < 3 else 500
            for g in range(s, min(s + span, n)):
                bits[g] |= sw
            ev.append((s, ['Reset', 'Select', 'Fire', 'left difficulty A', 'right difficulty A', 'black and white'][kind], span))
        t = make(name, 'Random joystick moves and random console buttons, seed %d' % seed, 'A seeded random game of %d frames: joystick holds of 4-40 frames, and every 1,100 frames one console control (Reset, Select, Fire for 10 frames; a difficulty or colour switch held for 500 frames).' % n, bits)
        start = t.first(f0 := t.first(1, lambda g: t.lives(g) == 3), lambda g: not t.paused(g))
        facts = ['Game Reset on frame 240; random joystick moves by the rule of tools\\e8_entries.py. Console controls used: %s.' % '; '.join('frame %d %s%s' % (s, nme, '' if sp == 10 else ' (to frame %d)' % (s + sp - 1)) for s, nme, sp in ev)]
        st, big, gst, passages, other, ppass = e7.invariants(t, start)
        facts.append('Games started (the lives counter becomes 3): on frames %s.' % ', '.join(str(f) for f in range(1, t.n) if t.lives(f) == 3 and t.lives(f - 1) != 3))
        facts.append('Movement rules over every frame of play (pause frames left out): Pac-Man made %d one-pixel steps, %d jumps through the openings at the top or bottom and %d other changes of place; the ghosts made %d changes of place, %d jumps through the openings and %d other changes larger than 3 pixels.'
                     % (st, len(ppass), big, gst, len(passages), len(other)))
        e2.write_entry(name, '', '', t.n, facts, 'replay the recording with the host program (--replay).')
        checks.append((name, big == 0 and not other))

    # BL-070: eaten ghosts walk back to the box (the flag in bit 3 of $BC+i)
    bits, head = e7.make_from_bot('BL-070', e2.ROOT / 'reference' / 'e7' / 'long1')
    bits = bits[:12000]
    t = make('BL-070', 'Eaten ghosts walk back to the box', 'The first 12,000 frames of the first long bot game (Stage E7): every ghost eaten, and what happens to its place bytes and to bit 3 of its state byte until it is home.', bits)
    f0 = t.first(1, lambda f: t.lives(f) == 3)
    start = t.first(f0, lambda f: not t.paused(f))
    facts = ['The input is the first 12,000 frames of the bot game of seed 1 (%s); Game Reset on frame %d.' % (head.lstrip('# '), f0)]
    eaten = [f for f in range(start + 1, t.n) if e5.score(t, f) - e5.score(t, f - 1) in (20, 40, 80, 160)]
    facts.append('Ghosts eaten (score rises of 20, 40, 80 or 160): on frames %s.' % ', '.join('%d (+%d)' % (f, e5.score(t, f) - e5.score(t, f - 1)) for f in eaten))
    homes = []
    for i in range(4):
        on = None
        for f in range(start, t.n):
            v = t.b(f, 0xBC + i) & 8
            if v and on is None:
                on = f
            if (not v) and on is not None:
                sizes = collections.Counter()
                for g in range(on + 1, f):
                    d = e4.dist(e4.gpos(t, i, g), e4.gpos(t, i, g - 1))
                    if d and not t.paused(g):
                        sizes[d] += 1
                home = e4.gpos(t, i, f)
                near = min(eaten, key=lambda x: abs(x - on)) if eaten else None
                homes.append(72 <= home[0] <= 80 and 28 <= home[1] <= 32)
                facts.append('Ghost %d: bit 3 of byte $%02x sets on frame %d (%d frames from the nearest ghost-eaten frame %s) with the ghost at (%d,%d), and clears on frame %d (%d frames later) with the ghost at (%d,%d); its changes of place in between: %s.'
                             % (i, 0xBC + i, on, abs(on - near) if near is not None else -1, near, e4.gpos(t, i, on)[0], e4.gpos(t, i, on)[1], f, f - on, home[0], home[1], ', '.join('%d px: %d' % kv for kv in sorted(sizes.items())) or 'none'))
                on = None
    facts.append('In %d of %d episodes the flag clears with the ghost back at the start place (x 72-80, y 28-32). The scared state during a power pill is not this flag: in the first power pill of BL-044 the ghosts that were not eaten (ghosts 1 and 3) never set it. Which byte holds the scared state is still not identified.' % (sum(homes), len(homes)))
    e2.write_entry('BL-070', '', '', t.n, facts, 'eat a ghost during a power pill: its eyes go back to the box in the middle; nothing is visible on the status bar.')
    checks.append(('BL-070', len(homes) >= 3 and all(homes)))

    bad = [x for x, ok in checks if not ok]
    print('entries made: %d; checks failed: %s' % (len(checks), bad or 'none'))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
