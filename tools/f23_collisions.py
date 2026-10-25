r"""Stage F23.4: the collision flags from the video-chip model.

  python -I tools\f23_collisions.py <rom>      (after tools\build_lockstep.cmd)

The game reads two collision flags of the video chip each frame (ghost touches Pac-Man; bonus item touches Pac-Man, F12). Until now the tests gave them
from the oracle files of F20 (answers recorded from the original). Now the video-chip model makes them from the picture it draws
(game::TiaVideo: the latches for player 0 - player 1, missile 1 - player 1 and ball - player 1, cleared by CXCLR).
1. lockstep_native --pictures WITHOUT any oracle over all 70 games: all 128 bytes (but the stack) and the picture of every frame identical.
2. Control: the same without the pictures and without the oracle (flags always off) must fail in games where a ghost catches Pac-Man.
3. Flags: how many frames per game ask for the flags and how often they are set is shown by the oracle files (counts only).
Writes docs\F23_COLLISIONS.md. The ROM is only read.
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


def run(rom, item, pictures):
    entry, pokes = item[0], item[1]
    cmd = [str(NATIVE), str(rom), str(ROOT / 'baseline' / (entry + '.inputs')), str(ROOT / 'reference' / 'baseline' / (entry + '.trace'))]
    if pictures:
        cmd.append('--pictures')
    for p in pokes:
        cmd += ['--poke', p]
    t = time.time()
    r = subprocess.run(cmd, capture_output=True, text=True)
    return entry, r.returncode, r.stdout.strip().split('\n'), time.time() - t


def main():
    rom = pathlib.Path(sys.argv[1]).resolve()
    items = f6.entries()
    with concurrent.futures.ThreadPoolExecutor(8) as ex:
        runs = list(ex.map(lambda it: run(rom, it, True), items))
        ctl = list(ex.map(lambda it: run(rom, it, False), items))
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
    print('no oracle, flags from the picture: %d of %d games identical (memory and picture), %d frames, %.1f s of work' % (n_ok, len(runs), frames, secs))
    failing = [e for e, rc, lines, t in ctl if rc != 0]
    print('control (flags always off, no pictures): %d of %d games fail' % (len(failing), len(ctl)))
    if not failing:
        problems.append('control: no game failed without flags')
    set_ghost = set_bonus = asked = 0
    for it in items:
        f = ROOT / 'reference' / 'f20' / ('oracle_%s.txt' % it[0])
        if f.exists():
            for l in open(f):
                p = l.split()
                if len(p) == 3:
                    asked += 1
                    set_bonus += p[1] != '0'
                    set_ghost += p[2] != '0'
    print('oracle files: %d frames listed, bonus flag set in %d, ghost flag set in %d' % (asked, set_bonus, set_ghost))
    print('problems:', problems)
    ok = not problems
    print('result: %s' % ('OK' if ok else 'MISMATCH'))
    d = ['# Stage F23.4: the collision flags from the video-chip model (2026-10-26)', '',
         'The game decides whether a ghost caught Pac-Man and whether he took the bonus item from two flags of the video chip (F12). The video-chip model (`game::TiaVideo`) now keeps the same latches while it draws: player 0 touched player 1 (the ghost), missile 1 touched player 1 and the ball touched player 1 (the bonus item), cleared by `CXCLR`. `game::Game` asks the picture for them after the drawing code and before the bottom logic (`TimedChip::collisions`). The recorded answers of F20 (the oracle) are no longer used.', '',
         '## Result: %s' % ('identical' if ok else 'MISMATCH'), '',
         '- **%d of %d recorded games, %d frames, with no oracle at all: all 128 memory bytes (but the stack) and the picture identical in every frame.** %.1f s of work.' % (n_ok, len(runs), frames, secs),
         '- Control: without the flags (and without pictures) %d of %d games fail, so the flags matter and the test can tell.' % (len(failing), len(ctl)),
         '- For scale, the oracle files list %d frames; the bonus flag is set in %d and the ghost flag in %d of them.' % (asked, set_bonus, set_ghost), '',
         '## What this ends', '',
         'The native game now needs nothing from the original or from recordings except the player\'s inputs and the cartridge data: its logic, its drawing code, its chip model and its collision flags are all our own code. What is still missing from a finished game: the sound generator (F24), the whole-game checks including random-input games (F25) and taking Stella out of the program (F26).', '',
         '## Not claimed', '',
         '- Sound is not made (F24).',
         '- Only recorded inputs are played; random-input games come in F25.']
    (ROOT / 'docs' / 'F23_COLLISIONS.md').write_text('\n'.join(d) + '\n', encoding='utf-8')
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
