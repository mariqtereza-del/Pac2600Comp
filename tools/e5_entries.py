"""Stage E5: makes the baseline entries about eating and scoring (BL-045 .. BL-047) and checks them.

  python -I tools\\e5_entries.py <rom>

Same method as Stages E2-E4 (computed observations, two byte-identical traces, SHA-256 stored). The score is read from memory as
two bytes of binary-coded decimal: $CC (tens and ones) and $CE (hundreds); a third byte for larger scores ($CD?) was never
nonzero in these entries and is not used; working labels found in Stage E5 by matching the score shown in the status bar (docs\\CAPTURES.md) with memory: not yet confirmed
(Phase F). The wafer map is bytes $88-$97 (Stage C7). BL-046 also reads the picture: it saves the picture at every moment the score
is steady and checks that the digits in the status bar are the digits of the memory score (the glyph of each digit is one-to-one).
"""
import collections
import concurrent.futures as cf
import os
import pathlib
import subprocess
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import e2_entries as e2  # noqa: E402
import e3_entries as e3  # noqa: E402

LEFT, RIGHT, UP, DOWN = e3.LEFT, e3.RIGHT, e3.UP, e3.DOWN
EXE = e2.ROOT / 'build' / 'host' / 'pacman2600.exe'


def bcd(x):
    return (x >> 4) * 10 + (x & 15)


def score(t, f):
    return bcd(t.b(f, 0xCC)) + 100 * bcd(t.b(f, 0xCE))


def wafers(t, f):
    return sum(bin(t.b(f, a)).count('1') for a in range(0x88, 0x98))


def pac(t, f):
    return t.b(f, 0xB1), t.b(f, 0xB6)


def cleared(t, f):
    """(address, bit) of every wafer-map bit that went from 1 to 0 on frame f."""
    out = []
    for a in range(0x88, 0x98):
        gone = t.b(f - 1, a) & ~t.b(f, a) & 255
        out += [(a, k) for k in range(8) if gone >> k & 1]
    return out


def route_bits(n):
    txt = e3.ROUTE.read_text().split('\n')
    bits = [0] * n
    ev = [tuple(map(int, l.split())) for l in txt[3:] if l]
    for i, (f, v) in enumerate(ev):
        end = ev[i + 1][0] if i + 1 < len(ev) else 10 ** 9
        for k in range(f, min(end, n)):
            bits[k] = v
    return bits


def events(t, f0, f1):
    out = []
    for f in range(f0 + 1, f1):
        if score(t, f) != score(t, f - 1) or t.lives(f) != t.lives(f - 1):
            out.append(f)
    return out


def screen_agreement(t, inputs, frames, work):
    """Save the picture at each frame, read the digits in the status bar (rows 184-190, 8-pixel cells from column 40) and check that each
    glyph stands for one digit only and the digits are those of the memory score. Returns (frames checked, glyph classes, problems)."""
    work.mkdir(parents=True, exist_ok=True)

    def run(f):
        subprocess.run([str(EXE), '--headless', str(work / 'x.png'), str(e2.ROM), '--replay', str(inputs), '--frames', str(f + 1), '--save-frame', str(f),
                        '--raw', str(work / ('f%d.raw' % f)), '--settings', str(work / ('s%d.ini' % f))], capture_output=True)
    with cf.ThreadPoolExecutor(8) as ex:
        list(ex.map(run, frames))
    glyph, problems = {}, []
    for f in frames:
        d = (work / ('f%d.raw' % f)).read_bytes()
        bg = d[184 * 160 + 10]
        cells = [tuple(tuple(1 if d[y * 160 + x] != bg else 0 for x in range(x0, x0 + 8)) for y in range(184, 191)) for x0 in range(40, 136, 8)]
        shown = [c for c in cells if any(any(r) for r in c)]
        digits = str(score(t, f))
        if len(shown) != len(digits):
            problems.append('frame %d: memory score %s has %d digits, the picture shows %d' % (f, digits, len(digits), len(shown)))
            continue
        for c, dg in zip(shown, digits):
            glyph.setdefault(c, set()).add(dg)
    wrong = [sorted(v) for v in glyph.values() if len(v) > 1]
    names = collections.defaultdict(set)
    for c, v in glyph.items():
        for dg in v:
            names[dg].add(c)
    split = [dg for dg, cs in names.items() if len(cs) > 1]
    if wrong:
        problems.append('a glyph stands for more than one digit: %s' % wrong)
    if split:
        problems.append('a digit is drawn with more than one glyph: %s' % split)
    return len(frames), len(glyph), problems


def main():
    e2.ROM = pathlib.Path(sys.argv[1]).resolve()
    checks = []

    # BL-045: wafers along the first corridors
    bits = [0] * 600
    e2.press(bits, 240, e3.RESET)
    for a, e, m in ((370, 420, LEFT), (420, 470, DOWN), (470, 540, RIGHT)):
        for f in range(a, e):
            bits[f] |= m
    t = e2.make('BL-045', 'Eating wafers: left, down and right along the first corridors',
                'LEFT, DOWN, RIGHT held in turn from the start place: what each wafer eaten does to the score, the wafer map and the sound.', bits)
    tune = t.first(300, lambda f: not t.paused(f))
    facts = ['Game Reset on frame 240; play begins on frame %d; the score (bytes $CC/$CE) is %d and the wafer map ($88-$97) holds %d wafers.' % (tune, score(t, tune), wafers(t, tune))]
    ev = events(t, tune, t.n)
    for f in ev:
        c = cleared(t, f)
        facts.append('Frame %d: the score goes from %d to %d; Pac-Man is at (%d,%d); wafer-map bits cleared: %s.'
                     % (f, score(t, f - 1), score(t, f), pac(t, f)[0], pac(t, f)[1], ', '.join('$%02x bit %d' % x for x in c) or 'none'))
    first1 = t.first(1, lambda f: score(t, f) == 1)
    facts.insert(1, 'The score first reads 1 on frame %d, the first frame of play: the wafer at the start place is counted at once.' % first1)
    steps = [b - a for a, b in zip(ev, ev[1:])]
    facts.append('%d more wafers eaten after play began, each worth %s point; the wafer count goes from %d to %d. Frames between one wafer and the next: %s.'
                 % (len(ev), ','.join(str(d) for d in sorted(set(score(t, f) - score(t, f - 1) for f in ev))), wafers(t, tune), wafers(t, t.n - 1), ', '.join(map(str, steps[:20]))))
    e2.write_entry('BL-045', '', '', t.n, facts, 'start a game and walk left, down, right; the number at the right of the status bar goes up by one for every wafer eaten.')
    checks.append(('BL-045', len(ev) > 5 and all(score(t, f) - score(t, f - 1) == 1 for f in ev)))

    # BL-046: scores over the whole bot route, and the status bar
    n = 9800
    bits = route_bits(n)
    t = e2.make('BL-046', 'Every score value in a whole computer-played level, and the status bar',
                'The 9,800-frame route of the search bot (as BL-035), read for every change of the score: wafer, power pill, ghosts eaten in a row; and the digits in the status bar compared with the memory score.', bits)
    tune = 1930
    kinds = collections.defaultdict(list)
    for f in range(tune + 1, t.n):
        d = score(t, f) - score(t, f - 1)
        if d:
            w = wafers(t, f - 1) - wafers(t, f)
            kinds[(d, w, t.lives(f) - t.lives(f - 1))].append(f)
    facts = ['The input is the whole Stage C7 bot route (as BL-035). The score is read from bytes $CC and $CE as decimal digits; it is 0 at frame %d and %d at frame %d.' % (tune, score(t, t.n - 1), t.n - 1)]
    for (d, w, lv), fl in sorted(kinds.items()):
        facts.append('Score goes up by %d with %s and lives %s: %d times (first on frame %d, last on frame %d).' % (
            d, ('%d wafer-map bit%s cleared' % (w, '' if w == 1 else 's')) if w > 0 else ('%d bits set (the map refills)' % -w if w < 0 else 'no wafer-map bit cleared'),
            'unchanged' if lv == 0 else 'up by %d' % lv, len(fl), fl[0], fl[-1]))
    fives = [f for (d, w, lv), fl in kinds.items() if d == 5 for f in fl]
    facts.append('The %d rises of 5 (no wafer bit cleared) happen at Pac-Man places %s: four places near the corners of the maze, the one at (148,65) twice (before and after the refill of the map).' % (len(fives), ', '.join('(%d,%d) on frame %d' % (pac(t, f)[0], pac(t, f)[1], f) for f in sorted(fives))))
    chain = []
    for f in sorted(f for (d, w, lv), fl in kinds.items() if d in (20, 40, 80, 160) for f in fl):
        chain.append((f, score(t, f) - score(t, f - 1)))
    facts.append('Ghosts eaten (rises of 20, 40, 80, 160), in order: %s. Each run starts again at 20 after a new power pill: %s.' % (
        ', '.join('frame %d: +%d' % x for x in chain), 'yes' if all(chain[i][1] == 2 * chain[i - 1][1] or chain[i][1] == 20 for i in range(1, len(chain))) else 'NOT in this route'))
    singles = sorted(f for (d, w, lv), fl in kinds.items() if d == 1 and w == 1 for f in fl)
    refill = [f for (d, w, lv), fl in kinds.items() if w < 0 for f in fl][0]
    first1 = t.first(1, lambda f: score(t, f) == 1)
    nb = len([f for f in singles if f < refill])
    facts.append('Wafers: a full map holds %d wafers (frame %d, after the refill). The score first reads 1 on frame %d (the wafer at the start place, counted when play begins); %d single-wafer rises follow before the map refills on frame %d, and the refill itself is the last wafer (+1): %d + 1 + 1 = %d, the whole map. After the refill %d more single-wafer rises happen (the next level).'
                 % (wafers(t, refill), refill, first1, nb, refill, nb, nb + 2, len([f for f in singles if f > refill])))
    # the status bar
    stable = sorted(set([1900] + [f + 3 for f in range(tune + 1, t.n - 4) if score(t, f) != score(t, f - 1) and score(t, f + 3) == score(t, f)]))
    got = screen_agreement(t, e2.BASE / 'BL-046.inputs', stable, e2.OUT / 'BL-046.screens')
    facts.append('Status bar: the picture was saved on %d frames at which the score is steady (the 3rd frame after each change, and frame 1900). In every one the digits drawn at the right of the status bar are the digits of the memory score: %s (%d different digit shapes seen; each stands for one digit only).'
                 % (got[0], 'no disagreement' if not got[2] else 'PROBLEMS: ' + '; '.join(got[2]), got[1]))
    e2.write_entry('BL-046', '', '', t.n, facts, 'replay tools\\captures\\c7_captures.txt in Stella (Stage C7) and watch the number at the right of the status bar: +1 for a wafer, +5 for a power pill, +20, +40, +80, +160 for ghosts eaten in a row.')
    checks.append(('BL-046', not got[2] and got[1] == 10))

    # BL-047: the end of level 1
    n = 7500
    bits = route_bits(n)
    t = e2.make('BL-047', 'The end of the first level: the last wafer, the extra life, the refilled maze',
                'The first 7,500 frames of the bot route: the last wafer of the level is eaten on frame 7311.', bits)
    f = 7311
    facts = ['The input is the first 7,500 frames of the Stage C7 bot route.',
             'Frame %d: the score goes from %d to %d (the last wafer, +1); the wafer map goes from %d to %d wafers (a full map again); the lives counter ($98) goes from %d to %d.'
             % (f, score(t, f - 1), score(t, f), wafers(t, f - 1), wafers(t, f), t.lives(f - 1), t.lives(f))]
    stop = t.first(f - 5, lambda g: t.paused(g))
    end = t.first(stop, lambda g: not t.paused(g)) if stop is not None else None
    facts.append('Pause flag ($D2 bit 6) set from frame %s to %s.' % (stop, (end - 1) if end else 'the end'))
    facts.append('Wafers left in the map at frame %d, 1 frame before: %d. Pac-Man is at (%d,%d) on frame %d and at (%d,%d) on frame %d.' % (f - 1, wafers(t, f - 1), pac(t, f - 1)[0], pac(t, f - 1)[1], f - 1, pac(t, f + 1)[0], pac(t, f + 1)[1], f + 1))
    e2.write_entry('BL-047', '', '', t.n, facts, 'replay the route of tools\\captures\\c7_captures.txt up to the picture 07_level_end (frame 7310): one wafer is left; then the maze refills.')
    checks.append(('BL-047', wafers(t, f) > 120 and t.lives(f) == t.lives(f - 1) + 1))

    bad = [x for x, ok in checks if not ok]
    print('entries made: %d; checks failed: %s' % (len(checks), bad or 'none'))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
