r"""Stage F23.2: the drawing code as native code.

  python -I tools\f23_kernel.py <rom> [--dump]      (after tools\build_lockstep.cmd, tools\f20_lockstep.py (the oracle files) and tools\f23_tia.py;
                                                     --dump first makes the reference data with tools\f23_dump.py)

1. build\lockstep\test_kernel.exe: for 8,266 sampled frames of all 70 games the native drawing code (src\game\kernel.*) is given the 128 memory bytes
   the original drawing code starts with; the list of its writes to the video chip (scanline, colour clock, address of the writing instruction,
   register, value) must equal the emulator's list write for write (about 4.5 million writes), and the memory it leaves must equal the emulator's
   at the end of the original drawing code (all bytes except the stack bytes $F6-$FF).
2. Control: the same with every colour clock moved by 3: must fail in every frame.
3. The whole native game (lockstep_native, game::Game with the drawing code now inside) over all 70 recorded games, comparing all 128 bytes after
   every frame with NOTHING left out except the stack bytes (F21.27 had to leave out the 11 scratch bytes of the drawing code).
Writes docs\F23_KERNEL.md. The ROM is only read.
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

BS = chr(92)
F23 = ROOT / 'reference' / 'f23'
KERNEL = ROOT / 'build' / 'lockstep' / 'test_kernel.exe'
NATIVE = ROOT / 'build' / 'lockstep' / 'lockstep_native.exe'


def kernel(rom, *extra):
    r = subprocess.run([str(KERNEL), str(rom), str(F23), '--show', '3'] + list(extra), capture_output=True, text=True)
    m = re.search(r'sampled frames: (\d+); write lists equal: (\d+); memory equal: (\d+); writes checked: (\d+)', r.stdout)
    return (tuple(int(x) for x in m.groups()) if m else None), r.stdout


def native(rom, item):
    entry, pokes = item[0], item[1]
    cmd = [str(NATIVE), str(rom), str(ROOT / 'baseline' / (entry + '.inputs')), str(ROOT / 'reference' / 'baseline' / (entry + '.trace')),
           '--oracle', str(ROOT / 'reference' / 'f20' / ('oracle_%s.txt' % entry))]
    for p in pokes:
        cmd += ['--poke', p]
    t = time.time()
    r = subprocess.run(cmd, capture_output=True, text=True)
    return entry, r.returncode, r.stdout.strip().split('\n'), time.time() - t


def main():
    rom = pathlib.Path(sys.argv[1]).resolve()
    if '--dump' in sys.argv or not (F23 / 'e_BL-001.csv').exists():
        subprocess.run([sys.executable, '-I', str(ROOT / 'tools' / 'f23_dump.py'), str(rom)], check=True)
    problems = []
    res, out = kernel(rom)
    print('drawing code vs emulator: frames %s' % (res,))
    if not res or res[0] < 8000 or res[1] != res[0] or res[2] != res[0]:
        problems.append('drawing code: %s' % (out.strip().splitlines()[-3:],))
    frames, lists, mem, writes = res or (0, 0, 0, 0)
    ctl, _ = kernel(rom, '--shift', '3')
    print('control, every clock + 3: write lists equal in %s of %s frames (must be 0)' % (ctl[1] if ctl else '?', ctl[0] if ctl else '?'))
    if not ctl or ctl[1] != 0:
        problems.append('control did not fail')
    items = f6.entries()
    with concurrent.futures.ThreadPoolExecutor(8) as ex:
        runs = list(ex.map(lambda it: native(rom, it), items))
    total_frames = 0
    secs = 0.0
    bad = []
    for e, rc, lines, t in runs:
        m = re.search(r'IDENTICAL \((\d+) frames\)', lines[-1])
        if rc != 0 or not m:
            bad.append('%s: %s' % (e, ' | '.join(lines[:3])))
        else:
            total_frames += int(m.group(1))
        secs += t
    print('whole native game, nothing left out but the stack bytes: %d of %d games identical, %d frames, %.1f s of work' % (len(runs) - len(bad), len(runs), total_frames, secs))
    problems += bad
    print('problems:', problems)
    ok = not problems
    print('result: %s' % ('OK' if ok else 'MISMATCH'))
    d = ['# Stage F23.2: the drawing code as native code (2026-10-26)', '',
         ('`src%sgame%skernel.hpp/.cpp`: `game::drawPicture` does what the original does between the top logic and the bottom logic of a frame (`$F07F-$F4E6`): the positions of the sprites and of the power-pill missile, the 20-line maze rows (walls and wafers as playfield patterns, the ghost, Pac-Man, the power pills and the bonus item), the bottom wall, the score digits and the life markers. It works on the same 128 bytes, leaves the same scratch bytes in memory, and hands every write to the video chip to a `TimedChip` with the scanline and the colour clock at which the original makes it. The clocks are counted from the processor cycles of the original instructions (3 colour clocks per cycle, one more for a page crossing, a wait for the end of the line at each `WSYNC`); the video-chip model of F23.1 turns the writes into the picture.' % (BS, BS)), '',
         '## Result: %s' % ('identical' if ok else 'MISMATCH'), '',
         '- **%d sampled frames of all 70 games** (every 53rd frame): given the 128 bytes at `$F07F`, the native drawing code makes **%d of %d lists of writes to the video chip equal to the emulator\'s, write for write** (scanline, colour clock, address of the writing instruction, register, value; %d writes in all), and leaves **%d of %d memories equal** to the emulator\'s at `$F4E7` (all bytes except the stack bytes `$F6-$FF`).' % (frames, lists, frames, writes, mem, frames),
         '- Control: with every colour clock moved by 3 the same test finds %s of %s lists equal (it can tell).' % (ctl[1] if ctl else '?', ctl[0] if ctl else '?'),
         '- **Whole native game with the drawing code inside** (`game::Game`, `lockstep_native`): **%d of %d recorded games, %d frames, all 128 bytes identical after every frame with nothing left out except the stack bytes `$F6-$FF`** (F21.27 had to leave out 11 scratch bytes; the drawing code now makes them as well). %.1f s of work. The collision flags still come from the oracle until F23.4.' % (len(runs) - len(bad), len(runs), total_frames, secs), '',
         '## Not claimed', '',
         '- The picture itself is not compared here: that is F23.3 (these writes through the video-chip model, fingerprint of every frame against the golden traces).',
         '- The start line of the drawing code (the top logic ends on a line that varies by a few lines) is given by the test from the emulator; the game uses line 16. Only the set-up writes before line 34 depend on it, and nothing visible does.',
         '- Only every 53rd frame is compared write by write; F23.3 covers every frame by picture.']
    (ROOT / 'docs' / 'F23_KERNEL.md').write_text('\n'.join(d) + '\n', encoding='utf-8')
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
