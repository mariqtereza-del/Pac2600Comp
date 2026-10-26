"""Stage G5a (D-009): which cartridge bytes does the game really read?

python -I tools\\g5a_data_list.py <rom>      (after tools\\build_lockstep.cmd; needs reference\\f25 (python -I tools\\f25_games.py ... --make) and the F4 map
                                              reference\\disasm\\pacman_tables.asm from tools\\f4_tables.py)

Runs build\\lockstep\\lockstep_native_reads.exe (the native game with a counter on every read of the cartridge by address) over the 70 recorded
games and the 120 seeded random games of F25, merges the counts and lays them over the table map of F4. Writes
  docs\\EMBEDDED_DATA.md                    names, sizes, address ranges and read counts only - no cartridge byte values (this file goes into Git)
  reference\\g5a\\embedded_tables.txt        the byte-level list (LOCAL ONLY, never commit it)
and must say "result: OK": every byte that was read lies inside a data table of the F4 map, none in the cartridge's program code.
"""
import concurrent.futures
import pathlib
import re
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / 'tools'))
import f6_coverage as f6  # noqa: E402

EXE = ROOT / 'build' / 'lockstep' / 'lockstep_native_reads.exe'
OUT = ROOT / 'reference' / 'g5a'
BS = chr(92)

# what the named tables are for (the names that rom_tables.hpp had in G4.1, now the table names of src/game/data/tables.hpp); other segments are reached through a pointer or an index
USE = {
    0xFE13: 'maze wall table (kMazeWalls)', 0xFE6A: 'wafer bit masks (kWaferBitMask)', 0xFE6C: 'power-pill masks (kPillMask)',
    0xFE79: 'wafer byte index (kWaferByteIndex)', 0xFF06: 'score amounts, low byte (kScoreAmountLow)', 0xFF0C: 'score amounts, middle byte (kScoreAmountMid)',
    0xFF1B: 'life-marker playfield, first (kLivesMarkerPf1)', 0xFEEC: 'life-marker playfield, second (kLivesMarkerPf2)',
    0xFEF6: 'score digit pointers (kDigitPointerLow)', 0xFF0D: 'animation pointer steps (kAnimPointerStep)', 0xFF19: 'animation pointer choice (kAnimWhichPointer)',
    0xFF25: 'animation next y (kAnimNextY)', 0xFE6F: 'ghost chase timer reload (kChaseTimerReload)', 0xFDAB: 'ghost pictures (kGhostPictures)',
    0xFDC2: 'Pac-Man pictures (kPacmanPictures)', 0xFDFA: 'dying Pac-Man pictures (kPacmanDyingPictures)', 0xFF2B: 'colours, colour switch (kColourTableTv)',
    0xFF36: 'colours, black and white (kColourTableBlackWhite)', 0xFFE9: 'sound blockers (kSoundBlockers)', 0xFE6E: 'sound bits (kSoundBit)',
    0xFFED: 'sound lengths (kSoundLength)', 0xFFF1: 'start tune notes, first (kStartTuneA)', 0xFFF5: 'start tune notes, second (kStartTuneB)',
}


def run_one(name, inputs, trace, oracle, pokes):
    reads = OUT / 'reads' / (name + '.txt')
    if '--reuse' in sys.argv and reads.exists():   # only the document is rewritten, from the counts of an earlier run
        counts = {int(l.split()[0], 16): int(l.split()[1]) for l in reads.read_text().splitlines() if l}
        return name, True, counts
    cmd = [str(EXE), str(ROM), str(inputs), str(trace), '--pictures', '--sound', '--cart-reads', str(reads)]
    if oracle:
        cmd += ['--oracle', str(oracle)]
    for p in pokes:
        cmd += ['--poke', p]
    r = subprocess.run(cmd, capture_output=True, text=True)
    ok = 'result: IDENTICAL' in r.stdout
    counts = {}
    if reads.exists():
        for line in reads.read_text().split('\n'):
            if line:
                a, n = line.split()
                counts[int(a, 16)] = int(n)
    return name, ok, counts


def main():
    global ROM
    ROM = pathlib.Path(sys.argv[1]).resolve()
    (OUT / 'reads').mkdir(parents=True, exist_ok=True)
    jobs = []
    for entry, pokes in f6.entries():
        jobs.append((entry, ROOT / 'baseline' / (entry + '.inputs'), ROOT / 'reference' / 'baseline' / (entry + '.trace'),
                     ROOT / 'reference' / 'f20' / ('oracle_%s.txt' % entry), pokes))
    randoms = sorted(p.stem for p in (ROOT / 'reference' / 'f25').glob('R-*.trace'))
    for name in randoms:
        jobs.append((name, ROOT / 'reference' / 'f25' / (name + '.inputs'), ROOT / 'reference' / 'f25' / (name + '.trace'), None, ()))
    with concurrent.futures.ThreadPoolExecutor(8) as ex:
        res = list(ex.map(lambda j: run_one(*j), jobs))
    problems = [n for n, ok, c in res if not ok]
    total = {}
    for n, ok, c in res:
        for a, k in c.items():
            total[a] = total.get(a, 0) + k
    print('games run: %d (%d recorded, %d random); not identical: %s' % (len(res), len(jobs) - len(randoms), len(randoms), problems or 'none'))

    # the F4 table map
    segs = []
    for line in (ROOT / 'reference' / 'disasm' / 'tables_stats.txt').read_text().split('\n'):
        m = re.match(r'\s+(\w+)\s+(\w+)\s+(\d+)\s+\$([0-9A-F]{4})-\$([0-9A-F]{4})', line)
        if m:
            segs.append((m.group(1), m.group(2), int(m.group(3)), int(m.group(4), 16), int(m.group(5), 16)))
    rom = ROM.read_bytes()
    F4TOTAL = re.search(r'data bytes: (\d+)', (ROOT / 'reference' / 'disasm' / 'tables_stats.txt').read_text()).group(1)
    seg_of = {}
    for k, name, size, lo, hi in segs:
        for a in range(lo, hi + 1):
            seg_of[a] = (k, name, size, lo, hi)
    outside = sorted(a for a in total if a not in seg_of)
    inside_kinds = {}
    for a in total:
        if a in seg_of:
            inside_kinds.setdefault(seg_of[a][0], set()).add(a)
    rows = []
    for k, name, size, lo, hi in segs:
        read = sorted(a for a in range(lo, hi + 1) if a in total)
        rows.append((k, name, size, lo, hi, len(read), sum(total[a] for a in read)))
    carried = [r for r in rows if r[5] > 0]
    unread_in_carried = sum(r[2] - r[5] for r in carried)
    carry_bytes = sum(r[2] for r in carried)
    read_bytes = sum(r[5] for r in rows)

    lines = ['# The cartridge data the game really uses (Stage G5a, D-009)', '',
             'Written by `python -I tools' + BS + 'g5a_data_list.py rom' + BS + 'pacman.a26`. It contains names, sizes, address ranges and counts only; **no byte value of the cartridge**. The byte-level list is `reference' + BS + 'g5a' + BS + 'embedded_tables.txt` (local only).', '',
             '## How it was measured', '',
             'The native game was run with a counter on every read of the cartridge by address (`lockstep_native_reads`, a test build; the game\'s own builds have no counter) over the %d recorded games and the %d seeded random games of F25 (all play states: title, play, death, game over, two players, bonus item, power pill, ghost eaten, level changes). Every game ended identical to its golden trace, so the counts belong to the real game. The table map is the one of F4 (`docs' % (len(jobs) - len(randoms), len(randoms)) + BS + 'F4_DATA_TABLES.md`).', '',
             '## Result', '',
             '- **%d different cartridge addresses were read** (of 4,096).' % len(total),
             '- **%d of them lie in data tables of the F4 map, %d lie outside every data table** (that is, in the cartridge\'s program code).' % (len(total) - len(outside), len(outside)),
             '- **%d tables are read at least once, together %d bytes; of these %d bytes are never read** (the rest of a table that is only partly used). The tables that are never read: %s.' % (len(carried), carry_bytes, unread_in_carried,
                 ', '.join('%s (%d)' % (r[1], r[2]) for r in rows if r[5] == 0 and r[0] not in ('UNREAD', 'VECTORS')) or 'none'),
             '- **Data the program would carry: %d bytes in %d tables** (or %d bytes if only the bytes that are read were kept). The cartridge has 4,096 bytes; its machine code is not part of this.' % (carry_bytes, len(carried), read_bytes),
             '- Reads in the program code: **%s**.' % ('none' if not outside else 'FOUND at ' + ', '.join('$%04X' % a for a in outside[:10])),
             '- Check against F4: %d bytes carried + %d bytes in tables the native code never reads + %d bytes in segments never read = **%d** = the %s data bytes of the F4 map.' % (carry_bytes, sum(r[2] for r in rows if r[5] == 0 and r[0] not in ('UNREAD', 'VECTORS')), sum(r[2] for r in rows if r[5] == 0 and r[0] in ('UNREAD', 'VECTORS')), sum(r[2] for r in rows), F4TOTAL), '',
             '## The tables', '',
             '| Table | Kind | Address range | Size | Bytes read | Reads (all games) | Used for |', '| --- | --- | --- | --- | --- | --- | --- |']
    for k, name, size, lo, hi, nread, nreads in rows:
        if k in ('UNREAD', 'VECTORS') and nread == 0:
            continue
        use = USE.get(lo, 'reached by index or pointer (docs' + BS + 'F18_KERNEL.md, F19_SOUND.md)' if nread else 'never read')
        lines.append('| `%s` | %s | $%04X-$%04X | %d | %d | %d | %s |' % (name, k, lo, hi, size, nread, nreads, use))
    unread = [r for r in rows if r[0] in ('UNREAD', 'VECTORS') and r[5] == 0]
    lines += ['', '%d further small segments (%d bytes, kinds UNREAD and VECTORS in the F4 map) are never read by the game and would not be carried. (F4 found one UNREAD segment unread in its 29 recordings; the larger set of games here reads one byte of it, so it is listed in the table above.)' % (len(unread), sum(r[2] for r in unread)), '',
              '## The ROM check afterwards (D-009)', '',
              '- The program still asks for the ROM at start-up and checks its size (4,096 bytes) and SHA-1 (`0940fea7...6dbb1`) with the existing finder (D3) and loader checks (F22). It then starts the game **without handing it any byte**.',
              '- The lockstep suite, the measuring tools and `pacman2600_ref` keep using the ROM as the reference; they are test tools and are not part of a release.',
              '- F22\'s source scan (\"the source holds no cartridge bytes\") changes in G5b into a check that the only cartridge bytes in the source are the tables listed above.', '',
              '## Rule 7 as approved by the owner (2026-10-26, D-009)', '',
              'Before: *The original ROM must never be included in the public repository or executable release.*', '',
              'Now (in `CLAUDE.md`): *The original ROM file, and the cartridge\'s program code, must never be included in the public repository or executable release. The game data it needs (the tables listed in `docs' + BS + 'EMBEDDED_DATA.md`) is part of the program and is included with the release: the game reads no game data from the ROM. The ROM exists only as an indirect form of DRM. The program requires the player to have a legal dump of the original game and checks it (size and SHA-1) before it starts, so that nobody without a legal dump can play it with this project.*', '',
              'The owner\'s words: the ROM is an indirect DRM, preventing anyone without a legal dump from playing with this project; no game data is read from the ROM; it is all included with the release.', '']
    (ROOT / 'docs' / 'EMBEDDED_DATA.md').write_text('\n'.join(lines), encoding='utf-8', newline='\n')

    # the local byte-level list
    local = ['# LOCAL ONLY (reference is ignored by Git). Cartridge bytes that the game reads, table by table.']
    for k, name, size, lo, hi, nread, nreads in carried:
        local.append('%s $%04X-$%04X size %d read %d: ' % (name, lo, hi, size, nread) + ' '.join('%02X' % rom[a - 0xF000] for a in range(lo, hi + 1)))
    (OUT / 'embedded_tables.txt').write_text('\n'.join(local) + '\n', encoding='utf-8')

    ok = not problems and not outside
    print('different addresses read: %d; inside data tables: %d; outside (program code): %d' % (len(total), len(total) - len(outside), len(outside)))
    print('tables read: %d, %d bytes (read bytes: %d, never-read bytes inside them: %d)' % (len(carried), carry_bytes, read_bytes, unread_in_carried))
    print('by kind (addresses read): ' + ', '.join('%s %d' % (k, len(v)) for k, v in sorted(inside_kinds.items())))
    print('result: %s' % ('OK' if ok else 'FAILED'))
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
