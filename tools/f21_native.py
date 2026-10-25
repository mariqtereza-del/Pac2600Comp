"""Stage F21.27: the whole game as native code - no 6502 core, no original program code.

  python -I tools\\f21_native.py <rom>      (after tools\\build_lockstep.cmd, tools\\f20_lockstep.py (the oracle files) and tools\\f21_routines.py (the scratch list))

Runs `build\\lockstep\\lockstep_native.exe` (the game of src\\game: start-up, top logic, bottom logic, frame start; no renderer) over all 70 baseline games and
compares all 128 memory bytes with the golden trace after every frame. The bytes left out: the stack bytes $F6-$FF (the native game has no stack) and the
11 scratch bytes of the drawing code (tools\\f21_scratch_bytes.txt, F21.26). Also runs it with nothing left out to show that only those bytes differ.
Writes docs\\F21_NATIVE.md. The ROM is only read (for the cartridge data by address).
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

EXE = ROOT / 'build' / 'lockstep' / 'lockstep_native.exe'
BS = chr(92)


def run(entry, pokes, extra):
    cmd = [str(EXE), str(ROM), str(ROOT / 'baseline' / (entry + '.inputs')), str(ROOT / 'reference' / 'baseline' / (entry + '.trace')),
           '--oracle', str(ROOT / 'reference' / 'f20' / ('oracle_%s.txt' % entry))]
    for p in pokes:
        cmd += ['--poke', p]
    cmd += list(extra)
    t = time.time()
    r = subprocess.run(cmd, capture_output=True, text=True)
    return entry, r.returncode, r.stdout.strip().split('\n'), time.time() - t


def main():
    global ROM
    ROM = pathlib.Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else ROOT / 'rom' / 'pacman.a26'
    items = f6.entries()
    scratch = [l.split()[0] for l in (ROOT / 'tools' / 'f21_scratch_bytes.txt').read_text().split('\n') if l and not l.startswith('#')]
    names = {l.split()[0]: l.split()[1] for l in (ROOT / 'tools' / 'f21_scratch_bytes.txt').read_text().split('\n') if l and not l.startswith('#')}
    problems = []
    with concurrent.futures.ThreadPoolExecutor(8) as ex:
        main_runs = list(ex.map(lambda it: run(it[0], it[1], ['--ignore', ','.join(scratch)]), items))
        bare = list(ex.map(lambda it: run(it[0], it[1], ['--all']), items))
    frames = 0
    secs = 0.0
    for e, rc, lines, t in main_runs:
        m = re.search(r'IDENTICAL \((\d+) frames\)', lines[-1])
        if rc != 0 or not m:
            problems.append('%s: %s' % (e, ' | '.join(lines[:3])))
        else:
            frames += int(m.group(1))
        secs += t
    print('native game, scratch and stack bytes left out: %d of %d games identical, %d frames, %.1f s of work' % (len(main_runs) - len(problems), len(main_runs), frames, secs))
    seen = set()
    for e, rc, lines, t in bare:
        for l in lines:
            if l.startswith('bytes that differ in some frame:'):
                seen |= {m.group(1) for m in re.finditer(r'\$([0-9A-F]{2}) x\d+', l)}
    extra = sorted(seen - set(scratch))
    print('with nothing left out (except the stack bytes) the bytes that differ in some frame: %s; not in the scratch list: %s' % (' '.join('$' + b for b in sorted(seen)), extra or 'none'))
    if extra:
        problems.append('bytes outside the scratch list differ: %s' % extra)
    print('problems:', problems)
    ok = not problems
    print('result: %s' % ('OK' if ok else 'MISMATCH'))
    d = ['# Stage F21.27: the whole game as native code (2026-10-26)', '',
         '`python -I tools%sf21_native.py rom%spacman.a26` runs the game of `src%sgame` with **no 6502 core and no original program code**: `game::Game::runFrame` (the start-up and the frame start on the first frame; later the top logic, the bottom logic and the frame start; the drawing is the renderer\'s job, F23). The ROM is read only to give the game its cartridge data by address. The inputs come from the recordings and the two collision flags from the oracle of F20 (until the renderer computes them).' % (BS, BS, BS), '',
         '## Result: %s' % ('identical' if ok else 'MISMATCH'), '',
         '- **%d of %d recorded games, %d frames: all 128 memory bytes identical after every frame**, except the bytes that are not game state: the stack bytes `$F6-$FF` (the native game has no stack) and the 11 scratch bytes of the drawing code (F21.26): %s.' % (len(main_runs) - len(problems), len(main_runs), frames, ', '.join('`$%s %s`' % (a, names[a]) for a in scratch)),
         '- With nothing left out but the stack bytes, the bytes that differ in some frame are exactly those scratch bytes: %s (outside the list: %s).' % (' '.join('`$%s`' % b for b in sorted(seen)), ', '.join(extra) if extra else 'none'),
         '- Running time: %.1f seconds of work for the %d frames (%d frames a second per core), so the game logic is far faster than needed.' % (secs, frames, int(frames / secs) if secs else 0), '',
         '## What this ends', '',
         'F21 (rewrite one routine per session) is complete: every routine and every part of the main loop of the original (F8, F5) is now written in C++ in `src%sgame` and proved against the original with the lockstep tool; the two together run a whole game without the original program. What the tests still lean on from outside the game code: the **oracle** for the collision flags (to be replaced by the renderer of F23, which computes them from the sprites: F18), the **cartridge data view** (to be filled by the loader of F22 from the player\'s ROM), and the **chip** interface (the renderer of F23 and the sound generator of F24 stand behind it).' % BS, '',
         '## Not claimed', '',
         '- The picture and the sound are not made here; nothing is drawn or heard (F23, F24).',
         '- The collision flags are still given; the 11 scratch bytes are left out of the comparison on the evidence of the recordings (F21.26).']
    (ROOT / 'docs' / 'F21_NATIVE.md').write_text('\n'.join(d) + '\n', encoding='utf-8')
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
