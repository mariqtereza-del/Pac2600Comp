"""Stage F1: which bytes of the ROM are code, which are data, which are never touched, by running the baseline games.

  python -I tools\\f1_coverage.py <rom>

Runs build\\stella_probe\\stella_coverage.exe on a set of baseline recordings (each checked frame by frame against its golden trace:
the picture and all 128 memory bytes must be equal), writes reference\\f1\\cov_<ID>.csv (local), unions them and writes
reference\\f1\\coverage_union.txt: for every ROM offset one letter:
  C  first byte of an instruction the CPU executed
  c  a later byte of an executed instruction (its operand)
  D  read by an instruction as data (and not part of an executed instruction)
  .  never executed and never read in these games
and the checks: every executed first byte is an official opcode, no executed instruction starts inside another one.
The ROM is only read.
"""
import concurrent.futures as cf
import pathlib
import re
import subprocess
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import f1_6502 as asm  # noqa: E402

ROOT = pathlib.Path(__file__).resolve().parent.parent
EXE = ROOT / 'build' / 'stella_probe' / 'stella_coverage.exe'
OUT = ROOT / 'reference' / 'f1'
# the start-up, the 16 selections (a few), movement, ghosts, scoring, dying, long games, edge cases, two players, random play
SET = ['BL-002', 'BL-003', 'BL-005', 'BL-019', 'BL-020', 'BL-025', 'BL-026', 'BL-034', 'BL-036', 'BL-038', 'BL-044', 'BL-046', 'BL-048', 'BL-049',
       'BL-050', 'BL-051', 'BL-052', 'BL-053', 'BL-054', 'BL-055', 'BL-056', 'BL-058', 'BL-059', 'BL-061', 'BL-063', 'BL-064', 'BL-067', 'BL-068', 'BL-069']


# Stage F9: situations no baseline game reaches, found while reading the input code (the Select switch held for a while, the
# difficulty switch at A with the bonus item or a power pill). They have no golden trace: only the instructions they run are taken.
EXTRA = ['X-select-hold', 'X-left-A-idle', 'X-power-A']


def run(entry):
    out = OUT / ('cov_%s.csv' % entry)
    if entry.startswith('X-'):
        r = subprocess.run([str(EXE), str(ROM), str(ROOT / 'tools' / 'f9_inputs' / (entry + '.inputs')), str(out)], capture_output=True, text=True)
        return entry, r.returncode, r.stdout.strip().splitlines()[-1] if r.stdout.strip() else r.stderr.strip()
    r = subprocess.run([str(EXE), str(ROM), str(ROOT / 'baseline' / (entry + '.inputs')), str(out), str(ROOT / 'reference' / 'baseline' / (entry + '.trace'))],
                       capture_output=True, text=True)
    return entry, r.returncode, r.stdout.strip().splitlines()[-1] if r.stdout.strip() else r.stderr.strip()


def main():
    global ROM
    ROM = pathlib.Path(sys.argv[1]).resolve()
    OUT.mkdir(parents=True, exist_ok=True)
    rom = ROM.read_bytes()
    with cf.ThreadPoolExecutor(8) as ex:
        results = list(ex.map(run, SET + EXTRA))
    bad = [r for r in results if r[1] != 0]
    total = sum(int(re.search(r'checked (\d+) frames', r[2]).group(1)) for r in results if re.search(r'checked (\d+) frames', r[2]))
    for e, rc, line in results:
        print('%s: %s' % (e, line))
    if bad:
        print('FAIL: a run differs from its golden trace or failed: %s' % [b[0] for b in bad])
        return 1
    executed, reads, outside = {}, {}, 0
    for entry in SET + EXTRA:
        for line in (OUT / ('cov_%s.csv' % entry)).read_text().split('\n'):
            p = line.split(',')
            if p[0] == 'x':
                executed[int(p[1])] = executed.get(int(p[1]), 0) + int(p[2])
            elif p[0] == 'r':
                reads.setdefault(int(p[2]), set()).add(int(p[1]))
            elif p[0] == 'm':
                outside += int(p[2])
    kind = ['.'] * 4096
    problems = []
    for off in sorted(executed):
        mn, mode, cy, by, official = asm.TABLE[rom[off]]
        if not official:
            problems.append('executed first byte $F%03X is %02X (%s), not an official opcode' % (off, rom[off], mn))
        for k in range(by):
            if off + k < 4096:
                if k and (off + k) in executed:
                    problems.append('instruction at $F%03X overlaps the instruction start $F%03X' % (off, off + k))
                if kind[off + k] == '.' or k == 0:
                    kind[off + k] = 'C' if k == 0 else 'c'
    for off, srcs in sorted(reads.items()):
        # a read counts as data only when it is not one of the bytes of the instruction that made it
        real = [s for s in srcs if not (s <= off < s + asm.TABLE[rom[s]][3])]
        if real and kind[off] == '.':
            kind[off] = 'D'
    rows = []
    for base in range(0, 4096, 64):
        rows.append('F%03X  %s' % (base, ''.join(kind[base:base + 64])))
    counts = {k: kind.count(k) for k in 'CcD.'}
    text = ('# stella_coverage union over %d baseline recordings (%d frames, every one checked equal to its golden trace: picture and all 128 memory bytes) plus extra runs without golden trace (Stage F9)\n' % (len(SET), total)
            + '\n'.join(rows) + '\n' + 'C first byte of an executed instruction, c its other bytes, D read as data only, . never executed or read\n'
            + 'counts: ' + ', '.join('%s %d' % (k, v) for k, v in counts.items()) + '\n'
            + 'executed instruction starts outside the ROM: %d\n' % outside
            + 'problems: %s\n' % (problems or 'none'))
    (OUT / 'coverage_union.txt').write_text(text, encoding='utf-8')
    print(text)
    return 1 if problems or outside else 0


if __name__ == '__main__':
    sys.exit(main())
