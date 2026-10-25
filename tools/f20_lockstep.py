"""Stage F20: the lockstep harness - runs src\\lockstep (build\\lockstep\\lockstep.exe) over all 70 baseline games and tests the harness itself.

  python -I tools\\f20_lockstep.py <rom>      (after tools\\build_lockstep.cmd; needs the 70 baseline traces and the flag logs reference\\f12)

1. Makes the collision-flag oracle of every game from the F12 logs (reference\\f20\\oracle_BL-nnn.txt, local).
2. Runs the original program in the lockstep machine (no native routine) over every game and compares all 128 bytes after every frame: must be identical.
3. Runs it with a correct native RNG_STEP in place of the original: must stay identical.
4. Runs it with a deliberately broken native RNG_STEP: must stop at the first difference and name the byte and the routine.
5. Runs it without the flag oracle: must find a difference (shows that the harness is not blind).
Writes docs\\F20_LOCKSTEP.md. The ROM is only read.
"""
import collections
import concurrent.futures
import pathlib
import re
import subprocess
import sys
import time

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / 'tools'))
import f6_coverage as f6  # noqa: E402

EXE = ROOT / 'build' / 'lockstep' / 'lockstep.exe'
OUT = ROOT / 'reference' / 'f20'
BS = chr(92)


def make_oracle(entry):
    ev = collections.defaultdict(dict)
    for line in open(ROOT / 'reference' / 'f12' / ('w_%s.csv' % entry)):
        fr, pc, a = line.strip().split(',')
        ev[int(fr)][pc] = int(a)
    path = OUT / ('oracle_%s.txt' % entry)
    with open(path, 'w') as o:
        for fr in sorted(ev):
            d = ev[fr]
            o.write('%d %d %d\n' % (fr, int(bool(d.get('f575', 0) & 0x40)), int(bool(d.get('f5d9', 0) & 0x80))))
    return path


def run(entry, pokes, oracle=True, natives=()):
    cmd = [str(EXE), str(ROM), str(ROOT / 'baseline' / (entry + '.inputs')), str(ROOT / 'reference' / 'baseline' / (entry + '.trace'))]
    if oracle:
        cmd += ['--oracle', str(OUT / ('oracle_%s.txt' % entry))]
    for p in pokes:
        cmd += ['--poke', p]
    for n in natives:
        cmd += ['--native', n]
    t = time.time()
    r = subprocess.run(cmd, capture_output=True, text=True)
    return entry, r.returncode, r.stdout.strip().split('\n'), time.time() - t


def main():
    global ROM
    ROM = pathlib.Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else ROOT / 'rom' / 'pacman.a26'
    OUT.mkdir(parents=True, exist_ok=True)
    items = f6.entries()
    for e, _ in items:
        make_oracle(e)
    problems = []
    with concurrent.futures.ThreadPoolExecutor(8) as ex:
        base = list(ex.map(lambda it: run(it[0], it[1]), items))
        nat = list(ex.map(lambda it: run(it[0], it[1], natives=('rng',)), items))
        brk = list(ex.map(lambda it: run(it[0], it[1], natives=('rng-broken',)), items))
        blind = run('BL-035', [], oracle=False)
    frames = 0
    secs = 0.0
    for e, rc, lines, t in base:
        m = re.search(r'IDENTICAL \((\d+) frames\)', lines[-1])
        if rc != 0 or not m:
            problems.append('%s: %s' % (e, ' | '.join(lines[:3])))
        else:
            frames += int(m.group(1))
        secs += t
    print('1. original code in the lockstep machine: %d games identical of %d, %d frames, %.1f s of work' % (len(base) - len([1 for e, rc, l, t in base if rc]), len(base), frames, secs))
    bad_nat = [e for e, rc, lines, t in nat if rc != 0]
    print('2. correct native RNG_STEP: %d games identical of %d' % (len(nat) - len(bad_nat), len(nat)))
    if bad_nat:
        problems.append('correct native routine differs: %s' % bad_nat[:3])
    found = 0
    named = 0
    example = None
    for e, rc, lines, t in brk:
        if rc == 1 and lines[0].startswith('first difference'):
            found += 1
            if 'RNG_STATE' in lines[0] and 'RNG_STEP (native, broken on purpose)' in lines[0]:
                named += 1
                if example is None:
                    example = (e, lines[0])
    print('3. broken native RNG_STEP: found in %d games of %d, byte and routine named correctly in %d' % (found, len(brk), named))
    if found != named or found < 60:
        problems.append('broken routine not found everywhere (%d found, %d named)' % (found, named))
    print('4. without the flag oracle (BL-035):', blind[1], blind[2][0][:160])
    if blind[1] != 1:
        problems.append('missing oracle not detected')
    ok = not problems
    print('problems:', problems)
    print('result: %s' % ('OK' if ok else 'MISMATCH'))
    write_doc(len(base), frames, secs, len(nat), found, named, example, blind, ok)
    return 0 if ok else 1


def write_doc(n, frames, secs, nnat, found, named, example, blind, ok):
    d = ['# Stage F20: the lockstep harness (2026-10-26)', '',
         '`python -I tools%sf20_lockstep.py rom%spacman.a26` runs the harness over all %d recorded games and tests the harness itself. This document is written by that tool.' % (BS, BS, n), '',
         '## Result: %s' % ('the harness works' if ok else 'MISMATCH'), '',
         '| Check | Result |', '| --- | --- |',
         '| The original program run by the harness\'s own 6502 core, compared with the golden memory after every frame | **%d games, %d frames, all 128 bytes identical in every frame** (%.1f s of work in all) |' % (n, frames, secs),
         '| The same with a natively written `RNG_STEP` in place of the original routine | identical in all %d games |' % nnat,
         '| The same with a deliberately broken `RNG_STEP` | stopped at the first difference in %d games; byte and routine named correctly in %d. Example: `%s: %s` |' % (found, named, example[0] if example else '-', example[1] if example else '-'),
         '| The same without the collision-flag oracle (BL-035) | found a difference: `%s` |' % blind[2][0][:150], '',
         '## What the harness is', '',
         'The goal of Phase F is a game written in our own code that behaves exactly like the original. The harness lets us get there **one routine at a time** and know at once when a rewrite is wrong:', '',
         '- `src%slockstep%smachine.*` is a small 6502 core that runs the original program bytes (read from the player\'s ROM; never copied into the repository) over the 128 bytes of console memory. It models the **inputs** (joysticks, switches, fire), the **collision flags** and the **frame boundary** (the write that switches the vertical sync off); it does not model time (the timer always reads 0, `WSYNC` does nothing) and does not make the picture or the sound. The picture kernel still runs, because it writes scratch bytes of the memory that are part of the comparison.' % (BS, BS),
         '- Any routine of the F8 list can be **replaced by a native C++ routine**: `Machine::registerNative(entry, name, function)`. The function reads and writes the memory with `read`/`write` and the registers `a, x, y, p`, and ends with `rts()`. From then on the harness runs the native routine whenever the original program calls that address.',
         '- After every frame all 128 bytes are compared with the golden trace. **On the first different frame it stops and prints the frame, the byte (with its F7 name), both values, and the routine that wrote the byte last** (a routine of the F8 list, a part of the main loop, or the native routine) with the address of the instruction.',
         '- `tools%sbuild_lockstep.cmd` builds `build%slockstep%slockstep.exe` (no Stella and no third-party code). Run it on one game: `lockstep.exe rom%spacman.a26 baseline%sBL-035.inputs reference%sbaseline%sBL-035.trace --oracle reference%sf20%soracle_BL-035.txt [--native rng] [--poke frame:addr:value] [--all]`.' % ((BS,) * 9),
         '', '## Facts the harness needed', '',
         '- **The frame boundary is the write that switches the vertical sync off (`STX VSYNC` at `$F87C`).** A golden row is the memory at that moment; the inputs of frame f are in place from the start of frame f. With this rule row 0 (from the power-on reset with zeroed memory) and all later rows match.',
         '- **Collision flags.** The game reads them from the chip, which here would need the picture. Until the native renderer exists (F23), the harness takes them from an **oracle**: the values the game read in the emulator (F12 logs, `reference%sf20` is made from them by this tool). F18 proved that the flags equal the overlap of the two sprites, so F23 replaces the oracle by a computation.' % BS,
         '- **Decimal mode** (the score) is done in the core; the 6502 stack lives in the memory bytes `$F6-$FF` and is compared too.',
         '- The core is only as big as this program needs: no interrupts, no unofficial opcodes (the harness stops with a message if one is met).', '',
         '## How the next stages use it (F21)', '',
         'For every routine of the F8 list, smallest first: write the native version, register it under its entry address, run `tools%sf20_lockstep.py`-style checks on all 70 games; the routine is accepted when every game stays identical. When a routine is wrong, the first difference points at the byte and, through the last-writer record, at the routine. Routines that drop the caller\'s return (the TRY_ helpers) must pop the extra address in their native version (`rts()` twice).' % BS, '',
         '## Not claimed', '',
         '- Picture and sound are not compared here (F23 and F24 have their own comparisons); nor is timing: the cycle-exact reference of the picture code is `tools%sf18_cpu.py`.' % BS,
         '- Player 2\'s joystick is not an input of the recordings (F9), so it is not exercised.']
    (ROOT / 'docs' / 'F20_LOCKSTEP.md').write_text('\n'.join(d) + '\n', encoding='utf-8')


if __name__ == '__main__':
    sys.exit(main())
