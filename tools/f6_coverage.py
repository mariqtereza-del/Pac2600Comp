"""Stage F6: what actually runs - every golden recording replayed with execution logging.

  python -I tools\f6_coverage.py <rom>

Runs build\stella_probe\stella_coverage.exe on EVERY baseline entry (baseline\BL-nnn.inputs, with the TEST-ONLY memory edits
of the entry's `pokes:` line), each checked frame by frame against its golden trace (picture and all 128 memory bytes), then
marks every one of the 4096 ROM bytes (reference\f6\, local only):
  C  first byte of an executed instruction     c  its other bytes
  D  read as data by some instruction (a real data fetch), never executed
  n  only touched by the processor's look-ahead at the byte after an instruction (not data use)
  .  never executed and never read
  Also listed: executed bytes that are ALSO read as data (code used as a table), and how often each instruction ran.
It compares with the F1 map (29 recordings) and checks the same things F1 checked.
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
OUT = ROOT / 'reference' / 'f6'
ROM = None
# Instructions of these modes really fetch a data byte. The others (implied, accumulator, relative, immediate) are recorded by the
# core when the processor looks at the byte after the instruction (the 6502 does this on every such instruction): not data use.
DATA_MODES = {m for m in set(v[1] for v in asm.TABLE if v) if m not in ('IMPLIED', 'ACCUMULATOR', 'RELATIVE', 'IMMEDIATE')}


def entries():
    ids = []
    for f in sorted((ROOT / 'baseline').glob('BL-*.entry.txt')):
        fields = {}
        for line in f.read_text(encoding='utf-8').split('\n'):
            m = re.match(r'(\w+): (.*)', line)
            if m and m.group(1) not in fields:
                fields[m.group(1)] = m.group(2).strip()
        ids.append((fields['id'], fields.get('pokes', '').split()))
    return ids


EXTRA = ['X-select-hold', 'X-left-A-idle', 'X-power-A']  # Stage F9: situations no baseline game reaches; no golden trace, only the instructions they run count


def run(item):
    entry, pokes = item
    out = OUT / ('cov_%s.csv' % entry)
    if entry.startswith('X-'):
        r = subprocess.run([str(EXE), str(ROM), str(ROOT / 'tools' / 'f9_inputs' / (entry + '.inputs')), str(out)], capture_output=True, text=True)
        return entry, r.returncode, r.stdout.strip().splitlines()[-1] if r.stdout.strip() else r.stderr.strip()
    r = subprocess.run([str(EXE), str(ROM), str(ROOT / 'baseline' / (entry + '.inputs')), str(out),
                        str(ROOT / 'reference' / 'baseline' / (entry + '.trace'))] + pokes, capture_output=True, text=True)
    return entry, r.returncode, r.stdout.strip().splitlines()[-1] if r.stdout.strip() else r.stderr.strip()


def classify(rom, executed, reads):
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
    also = []
    for off, srcs in sorted(reads.items()):
        real = [s for s in srcs if not (s <= off < s + asm.TABLE[rom[s]][3]) and asm.TABLE[rom[s]][1] in DATA_MODES]
        look = [s for s in srcs if not (s <= off < s + asm.TABLE[rom[s]][3]) and asm.TABLE[rom[s]][1] not in DATA_MODES]
        if real and kind[off] == '.':
            kind[off] = 'D'
        elif real and kind[off] in 'Cc':
            also.append(off)
        elif look and kind[off] == '.':
            kind[off] = 'n'
    return kind, problems, also


def ranges(kind, ch):
    out, i = [], 0
    while i < 4096:
        if kind[i] == ch:
            j = i
            while j + 1 < 4096 and kind[j + 1] == ch:
                j += 1
            out.append((i, j))
            i = j + 1
        else:
            i += 1
    return out


def main():
    global ROM
    ROM = pathlib.Path(sys.argv[1]).resolve()
    OUT.mkdir(parents=True, exist_ok=True)
    rom = ROM.read_bytes()
    items = entries() + [(e, []) for e in EXTRA]
    with cf.ThreadPoolExecutor(8) as ex:
        results = list(ex.map(run, items))
    for e, rc, line in results:
        print('%s: %s' % (e, line))
    bad = [r[0] for r in results if r[1] != 0]
    total = sum(int(m.group(1)) for r in results for m in [re.search(r'checked (\d+) frames', r[2])] if m)
    if bad:
        print('FAIL: a run differs from its golden trace or failed: %s' % bad)
        return 1
    executed, reads, outside = {}, {}, 0
    for entry, _ in items:
        for line in (OUT / ('cov_%s.csv' % entry)).read_text().split('\n'):
            p = line.split(',')
            if p[0] == 'x':
                executed[int(p[1])] = executed.get(int(p[1]), 0) + int(p[2])
            elif p[0] == 'r':
                reads.setdefault(int(p[2]), set()).add(int(p[1]))
            elif p[0] == 'm':
                outside += int(p[2])
    kind, problems, also = classify(rom, executed, reads)
    # compare with F1
    f1 = None
    f1file = ROOT / 'reference' / 'f1' / 'coverage_union.txt'
    if f1file.exists():
        f1 = []
        for line in f1file.read_text().split('\n'):
            if re.match(r'F[0-9A-F]{3}  ', line):
                f1.extend(line[6:])
    changed = [i for i in range(4096) if f1 and f1[i] != kind[i]]
    rows = ['F%03X  %s' % (b, ''.join(kind[b:b + 64])) for b in range(0, 4096, 64)]
    counts = {k: kind.count(k) for k in 'CcDn.'}
    lines = ['# stella_coverage over ALL %d baseline entries (%d frames, every one equal to its golden trace: picture and all 128 memory bytes) and %d extra runs without golden trace (Stage F9)' % (len(items) - len(EXTRA), total, len(EXTRA))]
    lines += rows
    lines += ['C first byte of an executed instruction, c its other bytes, D read as data only, n only the look-ahead byte after an instruction, . never executed or read',
              'counts: ' + ', '.join('%s %d' % (k, v) for k, v in counts.items()),
              'executed instruction starts outside the ROM: %d' % outside,
              'problems: %s' % (problems or 'none')]
    lines += ['', 'changes against the F1 map (29 recordings): %d bytes' % len(changed)]
    for i in changed:
        lines.append('  $F%03X  F1 %s -> F6 %s' % (i, f1[i], kind[i]))
    lines += ['', 'executed bytes that are also read as data by another instruction (code used as a table): %d' % len(also)]
    for i in also:
        lines.append('  $F%03X (%s)' % (i, kind[i]))
    lines += ['', 'only the look-ahead touched (n): %d bytes' % counts['n']]
    for a, b in ranges(kind, 'n'):
        lines.append('  $F%03X-$F%03X  (%d)' % (a, b, b - a + 1))
    lines += ['', 'never executed and never read (.): %d bytes in %d pieces' % (counts['.'], len(ranges(kind, '.')))]
    for a, b in ranges(kind, '.'):
        lines.append('  $F%03X-$F%03X  (%d)  %s' % (a, b, b - a + 1, ' '.join('%02X' % x for x in rom[a:b + 1])))
    lines += ['', 'read as data, never executed (D): %d bytes in %d pieces' % (counts['D'], len(ranges(kind, 'D')))]
    for a, b in ranges(kind, 'D'):
        lines.append('  $F%03X-$F%03X  (%d)' % (a, b, b - a + 1))
    (OUT / 'coverage_all.txt').write_text('\n'.join(lines) + '\n', encoding='utf-8')
    # run counts per instruction
    cl = ['offset,runs,instruction']
    for off in sorted(executed):
        mn, mode, cy, by, official = asm.TABLE[rom[off]]
        cl.append('F%03X,%d,%s' % (off, executed[off], mn))
    (OUT / 'run_counts.csv').write_text('\n'.join(cl) + '\n', encoding='utf-8')
    rare = sorted((n, o) for o, n in executed.items() if n <= 5)
    print('\n'.join(lines[:70]))
    print('rarely run instructions (5 times or fewer over all entries): %d' % len(rare))
    for n, o in rare[:40]:
        print('  $F%03X ran %d' % (o, n))
    return 1 if problems or outside else 0


if __name__ == '__main__':
    sys.exit(main())
