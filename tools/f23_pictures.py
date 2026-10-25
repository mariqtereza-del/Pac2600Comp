r"""Stage F23.3: whole games, every picture.

  python -I tools\f23_pictures.py <rom>      (after tools\build_lockstep.cmd and tools\f20_lockstep.py (the collision-flag oracle files))

Runs build\lockstep\lockstep_native.exe --pictures over all 70 baseline games: the native game (game::Game: logic, drawing code, video-chip model; no 6502, no
emulator, no original code) plays the recorded inputs; after every frame the 128 memory bytes (all but the stack bytes) and the fingerprint of the
228 x 160 picture are compared with the golden trace. Control: a memory change that must change the picture (the life markers) must be found.
Writes docs\F23_PICTURES.md. The ROM is only read.
"""
import concurrent.futures
import pathlib
import re
import subprocess
import sys
import time

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / 'tools'))
import f6_coverage as f6  # noqa: E402

NATIVE = ROOT / 'build' / 'lockstep' / 'lockstep_native.exe'
BS = chr(92)


def run(rom, entry, pokes, extra=()):
    cmd = [str(NATIVE), str(rom), str(ROOT / 'baseline' / (entry + '.inputs')), str(ROOT / 'reference' / 'baseline' / (entry + '.trace')),
           '--oracle', str(ROOT / 'reference' / 'f20' / ('oracle_%s.txt' % entry)), '--pictures']
    for p in list(pokes) + list(extra):
        cmd += ['--poke', p]
    t = time.time()
    r = subprocess.run(cmd, capture_output=True, text=True)
    return entry, r.returncode, r.stdout.strip().split('\n'), time.time() - t


def main():
    rom = pathlib.Path(sys.argv[1]).resolve()
    items = f6.entries()
    with concurrent.futures.ThreadPoolExecutor(8) as ex:
        runs = list(ex.map(lambda it: run(rom, it[0], it[1]), items))
    problems = []
    frames = 0
    secs = 0.0
    for e, rc, lines, t in runs:
        m = re.search(r'IDENTICAL \((\d+) frames\)', lines[-1])
        if rc != 0 or not m:
            problems.append('%s: %s' % (e, ' | '.join(lines[:3])))
        else:
            frames += int(m.group(1))
        secs += t
    n_ok = len(runs) - len(problems)
    print('native game with pictures: %d of %d games identical (memory and picture), %d frames, %.1f s of work' % (n_ok, len(runs), frames, secs))
    e, rc, lines, _ = run(rom, 'BL-001', [], ['100:98:5'])
    ctl = rc != 0 and any(l.startswith('first picture difference: frame 100') for l in lines)
    print('control (5 lives poked into frame 100 of BL-001): picture difference found at frame 100: %s' % ctl)
    if not ctl:
        problems.append('control not found: %s' % lines[:3])
    print('problems:', problems)
    ok = not problems
    print('result: %s' % ('OK' if ok else 'MISMATCH'))
    d = ['# Stage F23.3: whole games, every picture (2026-10-26)', '',
         ('`lockstep_native --pictures` plays the recorded inputs of each game through `game::Game` with the drawing code (`src%sgame%skernel.*`) and the video-chip model (`tia_video.*`, joined by `renderer.*`). There is **no 6502, no emulator and no original program code** in it. After every frame it compares the 128 memory bytes (all but the stack bytes) and the **fingerprint of the whole 228 x 160 picture** with the golden trace, which holds the emulator\'s picture of every frame. The logic parts (top logic, bottom logic, frame start) have no exact timing; their few writes to the video chip (colours, `REFP1`, `VBLANK`) are placed on fixed scanlines (5, 240, 259: the drawing of a two-player picture runs to about line 236, so the bottom logic must come after it), which is early or late enough that nothing visible depends on it.' % (BS, BS)), '',
         '## Result: %s' % ('every picture identical' if ok else 'MISMATCH'), '',
         '- **%d of %d recorded games, %d frames: every picture and all memory identical to the emulator\'s**, the flicker of the ghosts included (%.1f s of work for all of it).' % (n_ok, len(runs), frames, secs),
         '- Control: poking five life markers into one frame of one game makes the picture difference appear at exactly that frame: %s.' % ctl, '',
         '## Not claimed', '',
         '- Frame 0 (the partial first picture after power-on, 45 lines) is not compared.',
         '- The two collision flags still come from the recorded answers (the oracle of F20); F23.4 makes them from the model.',
         '- Fingerprints prove equality of the whole picture (a 64-bit hash of all 36,480 colour values), not a pixel list; the earlier F23.1 test showed single-pixel differences are caught.']
    (ROOT / 'docs' / 'F23_PICTURES.md').write_text('\n'.join(d) + '\n', encoding='utf-8')
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
