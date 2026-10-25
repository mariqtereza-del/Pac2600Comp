r"""Stage F24.1: the sound-chip model.

  python -I tools\f24_audio.py <rom> [--dump]      (after tools\build_lockstep.cmd; --dump first makes the reference data with tools\f24_dump.py, about 2 minutes)

Runs build\lockstep\test_tia_audio.exe: for EVERY frame of all 70 baseline games (439,680 frames) the emulator's own list of writes to the six sound registers
is fed to the model (src\game\tia_audio.*) and the sound it hands over after the frame (524 mono samples, in whole fragments) must have the same fingerprint as
the golden trace's sound column. Controls: the effect time of every write moved by one colour clock must fail. Counts the frames that are not silent.
Writes docs\F24_AUDIO.md. The ROM is only read.
"""
import pathlib
import re
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
EXE = ROOT / 'build' / 'lockstep' / 'test_tia_audio.exe'
F24 = ROOT / 'reference' / 'f24'
BASE = ROOT / 'reference' / 'baseline'
BS = chr(92)


def run(*opts):
    r = subprocess.run([str(EXE), str(F24), str(BASE), '--show', '3'] + [str(o) for o in opts], capture_output=True, text=True)
    m = re.search(r'frames equal: (\d+) of (\d+)', r.stdout)
    return (int(m.group(1)), int(m.group(2))) if m else (-1, -1), r.stdout


def main():
    rom = pathlib.Path(sys.argv[1]).resolve()
    if '--dump' in sys.argv or not F24.exists():
        subprocess.run([sys.executable, '-I', str(ROOT / 'tools' / 'f24_dump.py'), str(rom)], check=True)
    problems = []
    (eq, total), out = run()
    print('model with the emulator\'s writes: %d of %d frames equal' % (eq, total))
    if eq != total or total < 439000:
        problems.append('normal run: %d of %d: %s' % (eq, total, out.strip().splitlines()[:3]))
    ctl = []
    for d in (-1, 1):
        (e, t), _ = run('--delta', d)
        ctl.append((d, e, t))
        print('control, every write %+d colour clock: %d of %d frames equal (must be fewer)' % (d, e, t))
        if e >= t:
            problems.append('control %+d did not fail' % d)
    silent = '%016x'
    h = 1469598103934665603
    for _ in range(524 * 4):
        h = ((h ^ 0) * 1099511628211) & (2 ** 64 - 1)
    quiet = '%016x' % h
    nonsilent = frames = 0
    for n in range(1, 71):
        lines = (BASE / ('BL-%03d.trace' % n)).read_text().split('\n')[3:]
        for l in lines:
            c = l.split(',')
            if len(c) > 4:
                frames += 1
                nonsilent += c[4] not in (quiet, '14650fb0739d0383')
    print('frames that are not silent: %d of %d' % (nonsilent, frames))
    print('problems:', problems)
    ok = not problems
    print('result: %s' % ('OK' if ok else 'MISMATCH'))
    d = ['# Stage F24.1: the sound-chip model (2026-10-26)', '',
         ('`src%sgame%stia_audio.hpp/.cpp`: `game::TiaAudio` is the chip\'s two sound channels as a small model. It is given the writes to `AUDC0/1`, `AUDF0/1`, `AUDV0/1` with the scanline and colour clock at which they happen and makes the sound samples: each channel has a divider (`AUDF`), a pulse and noise pattern generator (`AUDC`) and a volume (`AUDV`) that is added up on every colour clock; twice a scanline (at colour clocks 37 and 149) the average of the clocks since the last sample is taken for both channels and the two are added through the chip\'s mixing table (a resistor network: two volumes together are less than twice as loud). That is 524 samples for a frame of 262 lines, 31,440 a second. This game is mono: left and right are the same.' % (BS, BS)), '',
         '## Result: %s' % ('every frame equal' if ok else 'MISMATCH'), '',
         '- **%d of %d frames** of all 70 recorded games: the emulator\'s own list of writes to the sound registers fed to the model gives sound with the same fingerprint as the golden trace\'s, frame by frame. %d of the frames are not silent (tunes, wafers, power pills, ghosts, death), so the generators are really exercised.' % (eq, total, nonsilent),
         '- Controls: moving the effect time of every write by one colour clock (%s) leaves fewer frames equal, so the exact moment is pinned down: %s.' % ('-1 and +1', '; '.join('%+d: %d of %d' % c for c in ctl)), '',
         '## Facts found while comparing', '',
         '- **Packaging:** the emulator hands the sound over in whole fragments of 524 samples. The first frame (power-on to the first vertical sync, 43 lines = 86 samples) therefore gives nothing, and every later frame gives the 524 samples that finished in it: the last 86 of the frame before and the first 438 of its own. 43 lines was found by comparison (41, 42 and 44 to 49 fail).',
         '- **Timing:** the sound counter runs 60 colour clocks behind the clock the picture side reports for a write: a write has its effect 60 clocks later (59 and 61 fail in thousands of frames). Written as `kWriteDelay` in the model.', '',
         '## Not claimed', '',
         '- The writes come from the emulator here. Our own game logic writes the sound registers without exact timing (the bottom logic of the original takes a varying number of lines); getting the line of those writes right is the next stage (F24.2).',
         '- Stereo or other cartridges are not modelled (this game is mono).']
    (ROOT / 'docs' / 'F24_AUDIO.md').write_text('\n'.join(d) + '\n', encoding='utf-8')
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
