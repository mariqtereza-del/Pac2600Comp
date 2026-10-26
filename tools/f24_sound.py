r"""Stage F24.2: the native game's sound, frame by frame.

  python -I tools\f24_sound.py <rom>      (after tools\build_lockstep.cmd)

Runs build\lockstep\lockstep_native.exe --pictures --sound over all 70 baseline games: the native game (logic, drawing code, video-chip model, collision
flags, sound-chip model; no 6502, no emulator, no original code) plays the recorded inputs; after every frame the 128 memory bytes (all but the stack bytes),
the fingerprint of the picture and the fingerprint of the sound handed over are compared with the golden trace. The sound writes of the game's bottom logic
are placed at the moment given by the processor cycles of the original code (counted along the path taken, src\game\logic\bottom.cpp). Control: every such write
one cycle late must change the sound. Writes docs\F24_SOUND.md. The ROM is only read.
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


def run(rom, item, extra=()):
    entry, pokes = item[0], item[1]
    cmd = [str(NATIVE), str(rom), str(ROOT / 'baseline' / (entry + '.inputs')), str(ROOT / 'reference' / 'baseline' / (entry + '.trace')), '--pictures', '--sound'] + list(extra)
    for p in pokes:
        cmd += ['--poke', p]
    t = time.time()
    r = subprocess.run(cmd, capture_output=True, text=True)
    return item[0], r.returncode, r.stdout.strip().split('\n'), time.time() - t


def main():
    rom = pathlib.Path(sys.argv[1]).resolve()
    items = f6.entries()
    with concurrent.futures.ThreadPoolExecutor(8) as ex:
        runs = list(ex.map(lambda it: run(rom, it), items))
        skew = list(ex.map(lambda it: run(rom, it, ['--skew', '1']), items))
    problems = []
    frames = 0
    secs = 0.0
    for e, rc, lines, t in runs:
        m = re.search(r'IDENTICAL \((\d+) frames\)', lines[-1])
        if rc != 0 or not m:
            problems.append('%s: %s' % (e, ' | '.join(lines[:4])))
        else:
            frames += int(m.group(1))
        secs += t
    n_ok = len(runs) - len(problems)
    print('native game with sound: %d of %d games identical (memory, picture and sound), %d frames, %.1f s of work' % (n_ok, len(runs), frames, secs))
    differ = 0
    wrong_frames = 0
    for e, rc, lines, t in skew:
        for l in lines:
            m = re.match(r'sound: DIFFERENT \((\d+) of', l)
            if m:
                differ += 1
                wrong_frames += int(m.group(1))
    print('control (every bottom-logic write one cycle late): the sound differs in %d of %d games (%d frames)' % (differ, len(skew), wrong_frames))
    if differ == 0:
        problems.append('control: the one-cycle skew changed nothing')
    print('problems:', problems)
    ok = not problems
    print('result: %s' % ('OK' if ok else 'MISMATCH'))
    d = ['# Stage F24.2: the native game\'s sound, frame by frame (2026-10-26)', '',
         ('The sound registers are written at the end of the bottom logic, whose length varies by several lines; the sound chip makes two samples a line, so the exact moment of those writes decides which samples change. The rewritten bottom logic (`src%sgame%sroutines.cpp`, `struct Bottom` and the routines it calls) now counts the processor cycles of the original instructions along the path it takes (a table of the cycles of every instruction of the listing: `python -I tools%sf24_cycles.py <from> <to>`), starting from the end of the drawing code (`game::PictureEnd`, the line and colour clock after its last write). `game::Renderer` turns the count into a scanline and colour clock for every write (3 colour clocks a cycle) and feeds them to the video-chip model and the sound-chip model (`game::TiaAudio`). The sound handed over after each frame is compared with the golden trace\'s.' % (BS, BS, BS)), '',
         '## Result: %s' % ('sound identical' if ok else 'MISMATCH'), '',
         '- **%d of %d recorded games, %d frames: memory (all but the stack bytes), the picture and the sound identical to the emulator\'s in every frame**, with no emulator, no 6502 and no recorded answers in the program. %.1f s of work.' % (n_ok, len(runs), frames, secs),
         '- Control: with every write of the bottom logic one processor cycle late, the sound differs in %d of %d games (%d frames in all), so the test can tell and the counted cycles are exact.' % (differ, len(skew), wrong_frames), '',
         '## Not claimed', '',
         '- The Select step of the title screen (it silences the sound in the top logic) is counted from the end of the vertical sync (`TiaChip::writeAtCycle`); only the paths that occur in the recordings are counted (first press and the repeat every 32 frames, with and without the title screen test).',
         '- The other writes of the top logic (the sprite colours) and of the frame start are placed on fixed lines: nothing visible or audible depends on them.',
         '- Only recorded inputs are played; random-input games come in F25.']
    (ROOT / 'docs' / 'F24_SOUND.md').write_text('\n'.join(d) + '\n', encoding='utf-8')
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
