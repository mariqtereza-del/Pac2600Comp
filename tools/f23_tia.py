r"""Stage F23.1: the video-chip model.

  python -I tools\f23_tia.py <rom> [--dump]      (after tools\build_lockstep.cmd; --dump first makes the reference data with tools\f23_dump.py, about 2 minutes)

Runs build\lockstep\test_tia_video.exe: for 8,266 sampled frames of all 70 baseline games the emulator's own list of chip writes is fed to the
model (src\game\tia_video.*) and the picture it makes must have the same fingerprint as the golden trace's (the emulator's picture). Controls: the same
test with one position constant changed by one must fail (so the test can tell). Scans how far each timing constant can move without changing any
picture. Writes docs\F23_VIDEO.md. The ROM is only read.
"""
import concurrent.futures
import collections
import glob
import pathlib
import re
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
EXE = ROOT / 'build' / 'lockstep' / 'test_tia_video.exe'
F23 = ROOT / 'reference' / 'f23'
BASE = ROOT / 'reference' / 'baseline'
BS = chr(92)


def run(*opts):
    r = subprocess.run([str(EXE), str(F23), str(BASE)] + [str(o) for o in opts] + ['--show', '0'], capture_output=True, text=True)
    m = re.search(r'equal: (\d+) of (\d+)', r.stdout)
    return (int(m.group(1)), int(m.group(2))) if m else (-1, -1)


def window(opt, lo, hi, step, total):
    """The values of the option for which every sampled frame is still equal."""
    vals = list(range(lo, hi + 1, step))
    with concurrent.futures.ThreadPoolExecutor(8) as ex:
        res = list(ex.map(lambda v: run(opt, v), vals))
    good = [v for v, r in zip(vals, res) if r[0] == total]
    return (min(good), max(good)) if good else None


def main():
    rom = pathlib.Path(sys.argv[1]).resolve()
    if '--dump' in sys.argv or not F23.exists():
        subprocess.run([sys.executable, '-I', str(ROOT / 'tools' / 'f23_dump.py'), str(rom)], check=True)
    problems = []
    eq, total = run()
    print('model, normal timing: %d of %d sampled frames equal' % (eq, total))
    if eq != total or total < 8000:
        problems.append('normal run: %d of %d' % (eq, total))
    controls = []
    for opt, v, name in (('--pp', 4, 'player position - 1'), ('--pp', 6, 'player position + 1'), ('--mp', 3, 'missile position - 1'), ('--mp', 5, 'missile position + 1'),
                         ('--bp', 3, 'ball position - 1'), ('--bp', 5, 'ball position + 1')):
        e, t = run(opt, v)
        controls.append((name, e, t))
        print('control, %s: %d of %d equal (must be fewer)' % (name, e, t))
        if e >= t:
            problems.append('control did not fail: ' + name)
    wins = {}
    for opt, lo, hi, name in (('--pf', -14, 14, 'playfield (PF0-PF2)'), ('--grp', -10, 12, 'player graphics (GRP0, GRP1)'),
                              ('--en', -14, 36, 'missile and ball enables'), ('--misc', -14, 40, 'colours, CTRLPF, NUSIZ, REFP, VDELP')):
        w = window(opt, lo, hi, 2, total)
        wins[name] = w
        print('timing window, %s: %s' % (name, w))
        if not w:
            problems.append('no timing works for ' + name)
    seen = collections.defaultdict(set)
    for fn in glob.glob(str(F23 / 't_BL-*.csv')):
        for l in open(fn):
            f, line, clk, pc, reg, val = l.split(',')
            reg = int(reg, 16)
            if reg in (0x04, 0x05, 0x0A, 0x0B, 0x0C, 0x25, 0x26):
                seen[reg].add(int(val, 16))
    names = {0x04: 'NUSIZ0', 0x05: 'NUSIZ1', 0x0A: 'CTRLPF', 0x0B: 'REFP0', 0x0C: 'REFP1', 0x25: 'VDELP0', 0x26: 'VDELP1'}
    used = {names[r]: sorted(v) for r, v in sorted(seen.items())}
    print('values the game writes:', used)
    print('problems:', problems)
    ok = not problems
    print('result: %s' % ('OK' if ok else 'MISMATCH'))
    d = ['# Stage F23.1: the video-chip model (2026-10-26)', '',
         ('`src%sgame%stia_video.hpp/.cpp`: `game::TiaVideo` is the video chip as a small model. It is given the writes the program makes to the chip, each with the scanline and colour clock at which it happened, and makes the 160 x 228 picture of the television window: playfield (left half and its repeat or mirror), two players with their copies and mirror, delayed graphics registers, two missiles, the ball, horizontal motion (`HMOVE` and the blank it makes at the left edge of a line), the priority and score-mode rules of `CTRLPF`, `VBLANK`, and the positions set by the `RESxx` strobes. It knows nothing about Pac-Man; what the game writes comes later (F23.3).' % (BS, BS)), '',
         '## Result: %s' % ('every picture equal' if ok else 'MISMATCH'), '',
         '- **%d of %d sampled frames** (every 53rd frame of all 70 recorded games, with the frame before each, so that settings left by the previous frame are right): the emulator\'s own list of chip writes fed to the model gives a picture whose fingerprint equals the golden trace\'s (the emulator\'s picture of 228 x 160 colour values). The only thing added to the writes is the power-on frame of the original (the places of missile 1 and the ball: 6 writes).' % (eq, total),
         '- Controls (the test can tell): ' + '; '.join('%s: %d of %d equal' % c for c in controls) + '. So the position constants (first pixel column of a player = clock at the end of the strobe - 68 + 5; missile and ball + 4) are pinned exactly by the pictures.',
         '- Timing windows: how far each write\'s effect time (in colour clocks after the end of its instruction) can move before any of the %d pictures changes: ' % total + '; '.join('%s: %s' % (k, ('%d to %d' % v) if v else 'none') for k, v in wins.items()) + '. The game writes these registers outside the places where the exact moment matters, so the pictures cannot pin them more closely; the model uses 0 for the playfield, 2 for the player graphics, 0 for the others (the middle of the windows).',
         '- Values the game writes to the registers whose many modes the model supports: ' + '; '.join('%s: %s' % (k, ', '.join('$%02X' % x for x in v)) for k, v in used.items()) + '.', '',
         '## Not claimed', '',
         '- The model code for other values (other `NUSIZ` copy patterns, a mirrored player with the other sizes, `RESMP`, mid-line `HMOVE`, the exact timing windows above) is written from the chip\'s documented behaviour but is not exercised by any recorded game, so it is not proved.',
         '- The collision flags are not made here (F23.4). The writes come from the emulator, not yet from our own drawing code (F23.3).']
    (ROOT / 'docs' / 'F23_VIDEO.md').write_text('\n'.join(d) + '\n', encoding='utf-8')
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
