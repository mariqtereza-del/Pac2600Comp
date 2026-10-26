r"""Stage F22: the data loader.

  python -I tools\f22_loader.py <rom>      (after tools\build_lockstep.cmd, tools\f20_lockstep.py and tools\f21_routines.py)

1. Runs build\lockstep\test_rom_check.exe (SHA-1 test values; the right ROM is accepted, one changed bit / wrong size / missing file refused; nothing is kept).
2. Runs the whole native game of F21.27 (tools\f21_native.py): must say result: OK. Since G5c it reads no ROM.
3. G5c (D-009): lockstep_native is given a ROM path that does not exist and must still end IDENTICAL; build\lockstep\test_no_rom.exe (a program
   with no ROM code in it) must reproduce the recorded pictures and sounds of tests\game\data; the window program must refuse a wrong ROM file
   (exit 4, no picture) and accept the right one.
4. Looks through every file of src\ for Atari data: any run of 6 or more consecutive numbers in the source that equals 8 consecutive cartridge bytes
   (with at least 4 different values among them) is reported. Allowed only in src\game\data\tables.cpp and only inside the listed tables.
Writes docs\F22_LOADER.md. The ROM is only read.
"""
import pathlib
import re
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
BS = chr(92)
RUN = 8


def main():
    rom_path = pathlib.Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else ROOT / 'rom' / 'pacman.a26'
    rom = rom_path.read_bytes()
    problems = []

    t = subprocess.run([str(ROOT / 'build' / 'lockstep' / 'test_rom_check.exe'), str(rom_path)], capture_output=True, text=True)
    lines = t.stdout.strip().split('\n')
    n_ok = sum(1 for l in lines if l.startswith('ok'))
    n_fail = sum(1 for l in lines if l.startswith('FAIL'))
    print('ROM check tests: %d ok, %d failed, exit %d' % (n_ok, n_fail, t.returncode))
    if t.returncode != 0 or n_fail or not lines[-1].endswith('OK'):
        problems.append('ROM check tests: ' + ' | '.join(l for l in lines if l.startswith('FAIL')))

    n = subprocess.run([sys.executable, '-I', str(ROOT / 'tools' / 'f21_native.py'), str(rom_path)], capture_output=True, text=True)
    nl = n.stdout.strip().split('\n')
    print('native game:', nl[0])
    if n.returncode != 0 or not nl[-1].endswith('OK'):
        problems.append('f21_native: ' + nl[-1])

    # G5c: no ROM is read by the game
    r = subprocess.run([str(ROOT / 'build' / 'lockstep' / 'lockstep_native.exe'), str(ROOT / 'no' / 'such' / 'rom.a26'), str(ROOT / 'baseline' / 'BL-001.inputs'),
                        str(ROOT / 'reference' / 'baseline' / 'BL-001.trace'), '--oracle', str(ROOT / 'reference' / 'f20' / 'oracle_BL-001.txt')], capture_output=True, text=True)
    no_path = r.returncode == 0 and 'result: IDENTICAL' in r.stdout
    print('lockstep_native given a ROM path that does not exist: exit %d, %s' % (r.returncode, r.stdout.strip().split('\n')[-1]))
    if not no_path:
        problems.append('the game needed the ROM path')
    norom = []
    for name, frames in (('BL-001', 700), ('BL-035', 3000)):
        x = subprocess.run([str(ROOT / 'build' / 'lockstep' / 'test_no_rom.exe'), str(ROOT / 'baseline' / (name + '.inputs')),
                            str(ROOT / 'tests' / 'game' / 'data' / (name + '.fingerprints.txt'))], capture_output=True, text=True)
        last = x.stdout.strip().split('\n')[-1]
        norom.append('%s: %s' % (name, last))
        if x.returncode != 0 or 'result: IDENTICAL (%d frames)' % frames not in last:
            problems.append('test_no_rom %s: %s' % (name, last))
    print('test_no_rom (a program without any ROM code):', '; '.join(norom))
    exe = ROOT / 'build' / 'host' / 'pacman2600.exe'
    scratch = ROOT / 'build' / 'host' / 'g5c_scratch'
    scratch.mkdir(exist_ok=True)
    wrong = subprocess.run([str(exe), '--headless', str(scratch / 'wrong.png'), str(ROOT / 'baseline' / 'BL-001.inputs'), '--frames', '5', '--save-frame', '3', '--settings', str(scratch / 'a.ini')], capture_output=True, text=True)
    right = subprocess.run([str(exe), '--headless', str(scratch / 'right.png'), str(rom_path), '--frames', '5', '--save-frame', '3', '--settings', str(scratch / 'b.ini')], capture_output=True, text=True)
    refuses = wrong.returncode == 4 and not (scratch / 'wrong.png').exists()
    accepts = right.returncode == 0 and (scratch / 'right.png').exists()
    print('window program given a wrong file: exit %d, no picture: %s; given the right ROM: exit %d, picture: %s' % (wrong.returncode, refuses, right.returncode, accepts))
    if not (refuses and accepts):
        problems.append('the window program did not refuse a wrong ROM or did not accept the right one')

    grams = {}
    for i in range(len(rom) - RUN + 1):
        w = tuple(rom[i:i + RUN])
        if len(set(w)) >= 4:
            grams.setdefault(w, i)
    def scan(text, name):
        found = []
        nums = []
        for m in re.finditer(r'0[xX][0-9a-fA-F]+|\$[0-9a-fA-F]{2}\b|\b\d+\b', text):
            s = m.group(0)
            try:
                nums.append(int(s[2:], 16) if s[:2] in ('0x', '0X') else int(s[1:], 16) if s[0] == '$' else int(s))
            except ValueError:
                nums.append(-1)
        for i in range(len(nums) - RUN + 1):
            w = tuple(nums[i:i + RUN])
            if w in grams:
                found.append('%s: numbers %s = cartridge bytes at $%04X' % (name, ' '.join('%02X' % x for x in w), 0xF000 + grams[w]))
        return found

    # control: the scan must catch data written out in the three usual styles
    pos = next(i for i in range(0xE00, len(rom) - 8) if len(set(rom[i:i + 8])) >= 4)
    control_ok = all(scan(f(rom[pos:pos + 8]), 'control') for f in (
        lambda b: ', '.join('0x%02X' % x for x in b), lambda b: ', '.join(str(x) for x in b), lambda b: ' '.join('$%02X' % x for x in b)))
    print('scan control (a copy of 8 cartridge bytes written as hex, decimal and $-hex is found): %s' % control_ok)
    if not control_ok:
        problems.append('the scan did not find its own control data')
    hits = []
    table_hits = []
    scanned = 0
    for p in sorted((ROOT / 'src').rglob('*')):
        if not p.is_file() or p.suffix not in ('.cpp', '.hpp', '.h', '.c', '.txt', '.md', '.cmake'):
            continue
        scanned += 1
        rel = str(p.relative_to(ROOT)).replace(chr(92), '/')
        found = scan(p.read_text(encoding='utf-8', errors='replace'), rel)
        if rel == 'src/game/data/tables.cpp':   # since G5b (D-009) the game's data tables are allowed here, and only here
            table_hits += found
        else:
            hits += found
    print('source scan: %d files in src, runs of %d cartridge bytes found in the source outside src/game/data/tables.cpp: %s' % (scanned, RUN, len(hits) or 'none'))
    tables_text = (ROOT / 'src' / 'game' / 'data' / 'tables.cpp').read_text(encoding='utf-8')
    ranges = [(int(a, 16), int(a, 16) + int(n)) for a, n in re.findall(r'\{"\w+", 0x([0-9A-F]{4}), (\d+),', tables_text)]
    def inside(a):
        return any(lo <= a < hi for lo, hi in ranges)
    stray = [h for h in table_hits if not (inside(int(h.rsplit('$', 1)[1], 16)) and inside(int(h.rsplit('$', 1)[1], 16) + RUN - 1))]
    print('tables file: %d tables listed, %d bytes; %d runs of cartridge bytes found in it, %d of them not inside a listed table' % (len(ranges), sum(hi - lo for lo, hi in ranges), len(table_hits), len(stray)))
    if stray or not ranges:
        problems.append('cartridge bytes in tables.cpp outside the listed tables: %d' % len(stray))
    for h in hits[:10]:
        print('  ', h)
    if hits:
        problems.append('cartridge data found in the source: %d runs' % len(hits))

    print('problems:', problems)
    ok = not problems
    print('result: %s' % ('OK' if ok else 'MISMATCH'))
    d = ['# Stage F22, updated in G5c: the ROM check (2026-10-26)', '',
         '`src%sgame%sdata%srom_check.hpp/.cpp`: `game::checkRom` and `checkRomFile` take the player\'s own ROM and say yes only if it is exactly 4096 bytes **and** its SHA-1 fingerprint is the expected one (`0940fea7f04cdb6d4b90c5ad1a7e344e68f6dbb1`). The check keeps nothing and hands nothing to the game: since G5c (D-009) the game has all its data in `tables.cpp` and reads no ROM. The check is the indirect copy protection of the project: the window program does not start the game without the right file, so nobody without a legal dump can play with it. The check has its own small SHA-1 so the game code does not depend on the window program; the file is only opened for reading.' % (BS, BS, BS), '',
         '## Result: %s' % ('OK' if ok else 'MISMATCH'), '',
         '- ROM check tests (`tests%sgame%stest_rom_check.cpp`): %d ok, %d failed. They check three standard SHA-1 test values, that the right ROM is accepted, that one changed bit is refused (fingerprint), one byte short / long / empty is refused (size), a missing file and a foreign file are refused.' % (BS, BS, n_ok, n_fail),
         '- The native game of F21.27 (memory, picture and sound of every recorded game): %s.' % nl[0],
         '- **The game needs no ROM:** `lockstep_native` given a ROM path that does not exist ends IDENTICAL (%s). `test_no_rom`, a program with no ROM code in it, reproduces the recorded pictures and sounds of the fingerprint files in `tests%sgame%sdata` (%s).' % (no_path, BS, BS, '; '.join(norom)),
         '- **The window program is the gate:** a wrong file is refused (exit 4, no picture): %s; the right ROM is accepted (exit 0, picture): %s.' % (refuses, accepts),
         '- Source scan: %d files under `src`; runs of %d consecutive numbers that equal %d consecutive cartridge bytes (at least 4 different values): **%s** outside `src/game/data/tables.cpp`. Since G5b (D-009, Rule 7 as reworded) that one file holds the game\'s data tables (%d tables, %d bytes), and every run found in it lies inside a listed table (%d runs, %d stray). Everywhere else the source holds addresses and logic, not Atari\'s data. The scan was first shown to work on a control (8 cartridge bytes written as 0x.., decimal and $.. are all found: %s).' % (scanned, RUN, RUN, len(hits) or 'none', len(ranges), sum(hi - lo for lo, hi in ranges), len(table_hits), len(stray), control_ok), '',
         '## Not claimed', '',
         '- The check is a simple one (size and fingerprint of a file). It keeps honest users honest; it is not strong protection against someone who wants to defeat it, and it was not meant to be (D-009).',
         '- The scan finds copied runs written as numbers; it cannot prove that no data is hidden in some other disguise. The rule of the project (T-F21-1) is that tables are read by address only.']
    (ROOT / 'docs' / 'F22_LOADER.md').write_text('\n'.join(d) + '\n', encoding='utf-8')
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
