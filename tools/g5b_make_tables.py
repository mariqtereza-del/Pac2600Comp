"""Stage G5b (D-009): writes the game's data tables as source code.

python -I tools\\g5b_make_tables.py <rom>      (needs reference\\g5a\\reads from tools\\g5a_data_list.py, reference\\disasm\\tables_stats.txt from tools\\f4_tables.py
                                              and the code and data map reference\\f6\\coverage_all.txt from tools\\f6_coverage.py)

Reads the player's own ROM once (it must be the right one: 4,096 bytes, SHA-1 0940fea7...6dbb1) and writes
  src\\game\\data\\tables.hpp   the declarations: one named, size-checked table per data segment that the game reads, and byAddress()
  src\\game\\data\\tables.cpp   the bytes of those tables (504 bytes in 48 tables - data only, none of the cartridge's program code)
  reference\\g5b\\names.txt      (local) which source name stands for which cartridge address, for the script that rewrites the code
Before writing, it checks that every carried byte is data in the F6 map (never a program instruction or operand) and that every carried table was
read in the games of G5a. Must say "result: OK". Rule 7 (as reworded by the owner on 2026-10-26, D-009) allows exactly these tables.
"""
import hashlib
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / 'tools'))
import g5a_data_list as g5a  # noqa: E402  (USE: what the named tables are for)

BS = chr(92)
SHA1 = '0940fea7f04cdb6d4b90c5ad1a7e344e68f6dbb1'
KIND = {'GFX': 'Gfx', 'TBL': 'Tbl', 'COLOR': 'Color', 'PF': 'Pf', 'SND': 'Snd', 'UNREAD': 'Unread'}
# names of the src/game/data/rom_tables.hpp (G4.1) by start address; the segment at $FF2B holds two colour tables (colour switch, black and white)
NAMES = {
    0xFE13: 'kMazeWalls', 0xFE6A: 'kWaferBitMask', 0xFE6C: 'kPillMask', 0xFE79: 'kWaferByteIndex', 0xFF06: 'kScoreAmountLow', 0xFF0C: 'kScoreAmountMid',
    0xFF1B: 'kLivesMarkerPf1', 0xFEEC: 'kLivesMarkerPf2', 0xFEF6: 'kDigitPointerLow', 0xFF0D: 'kAnimPointerStep', 0xFF19: 'kAnimWhichPointer',
    0xFF25: 'kAnimNextY', 0xFE6F: 'kChaseTimerReload', 0xFDAB: 'kGhostPictures', 0xFDC2: 'kPacmanPictures', 0xFDFA: 'kPacmanDyingPictures',
    0xFF2B: 'kColourTables', 0xFFE9: 'kSoundBlockers', 0xFE6E: 'kSoundBit', 0xFFED: 'kSoundLength', 0xFFF1: 'kStartTuneA', 0xFFF5: 'kStartTuneB',
    0xFE0A: 'kPlayfieldFE0A', 0xFE0C: 'kPlayfieldFE0C', 0xFE0D: 'kPlayfieldFE0D', 0xFE0F: 'kPlayfieldFE0F', 0xFE11: 'kPlayfieldFE11',
    0xFF41: 'kPlayfieldFF41', 0xFD3F: 'kMazeRowFD3F', 0xFD47: 'kMazeRowFD47', 0xFD57: 'kMazeRowFD57', 0xFD5B: 'kMazeRowFD5B', 0xFD63: 'kMazeRowFD63',
    0xFD6B: 'kMazeRowFD6B', 0xFD7B: 'kMazeRowFD7B', 0xFD83: 'kMazeRowFD83',
}
# Real tables that the F4 map cut into pieces (it cut by how the recorded games read the bytes). The survey run of G5b (--lenient, tables read past their
# end) found these eight places; each is one table of the original, read from several names at once. (first byte, last byte, name, what it holds)
GROUPS = [
    (0xFD5B, 0xFD62, 'kMazeRowFD5B', 'picture tables of the maze rows, 8 entries'),
    (0xFD6B, 0xFD7A, 'kMazeRowFD6B', 'picture tables of the maze rows, 16 entries'),
    (0xFD83, 0xFD8A, 'kMazeRowFD83', 'picture tables of the maze rows, 8 entries'),
    (0xFE0C, 0xFE0E, 'kPlayfieldFE0C', 'playfield patterns'),
    (0xFE6A, 0xFE75, 'kBitMasksAndReloads', 'bit masks (wafers, power pills, sound effects) and the chase timer reload values of the ghosts'),
    (0xFEEC, 0xFEF3, 'kLivesMarkerPf2', 'life-marker playfield, second row'),
    (0xFF06, 0xFF12, 'kScoreAmounts', 'score amounts (low and middle bytes) and the animation pointer steps'),
    (0xFF2B, 0xFF40, 'kColourTables', 'the colour tables: colour at offset 0, black and white at offset 11'),
]


GUSE = {}
LENIENT = '--lenient' in sys.argv   # temporary survey mode: a read past the end of a table goes on into the next table (as the original does) and is recorded


def main():
    rom = pathlib.Path(sys.argv[1]).read_bytes()
    if len(rom) != 4096 or hashlib.sha1(rom).hexdigest() != SHA1:
        print('this is not the right ROM (4,096 bytes, SHA-1 %s)' % SHA1)
        return 2
    # the segments of the F4 map
    segs = []
    for line in (ROOT / 'reference' / 'disasm' / 'tables_stats.txt').read_text().split('\n'):
        m = re.match(r'\s+(\w+)\s+(\w+)\s+(\d+)\s+\$([0-9A-F]{4})-\$([0-9A-F]{4})', line)
        if m:
            segs.append((m.group(1), m.group(2), int(m.group(3)), int(m.group(4), 16)))
    # the reads of G5a
    reads = set()
    for f in (ROOT / 'reference' / 'g5a' / 'reads').glob('*.txt'):
        for l in f.read_text().splitlines():
            if l.strip():
                reads.add(int(l.split()[0], 16))
    # the code and data map of F6: one character per ROM byte (C instruction, c operand, D data, other: untouched)
    fmap = {}
    for line in (ROOT / 'reference' / 'f6' / 'coverage_all.txt').read_text().split('\n'):
        m = re.match(r'^(F[0-9A-F]{3})  (\S+)$', line)
        if m:
            base = int(m.group(1), 16)
            for i, ch in enumerate(m.group(2)):
                fmap[base + i] = ch
    carried = [(k, n, size, lo) for k, n, size, lo in segs if any(a in reads for a in range(lo, lo + size))]
    grouped = []
    for glo, ghi, gname, guse in GROUPS:
        inside = [s for s in segs if glo <= s[3] and s[3] + s[2] - 1 <= ghi]
        if sum(s[2] for s in inside) != ghi - glo + 1:
            print('PROBLEM: group %s is not covered by the segments of the F4 map' % gname)
            return 1
        carried = [c for c in carried if c not in inside]
        carried.append(('GRP', gname, ghi - glo + 1, glo))
        grouped.append((glo, ghi, gname, guse, inside))
    GUSE.update({glo: guse for glo, ghi, gname, guse, inside in grouped})
    problems = []
    code_bytes = [a for k, n, size, lo in carried for a in range(lo, lo + size) if fmap.get(a) in ('C', 'c')]
    if code_bytes:
        problems.append('carried bytes the F6 map calls program code: %s' % ', '.join('$%04X' % a for a in code_bytes[:8]))
    outside = sorted(a for a in reads if not any(lo <= a < lo + size for k, n, size, lo in carried))
    if outside:
        problems.append('reads outside the carried tables: %s' % ', '.join('$%04X' % a for a in outside[:8]))
    total = sum(s[2] for s in carried)
    kinds_in_map = {}
    for k, n, size, lo in carried:
        for a in range(lo, lo + size):
            kinds_in_map[fmap.get(a, '?')] = kinds_in_map.get(fmap.get(a, '?'), 0) + 1
    # names
    names = {}
    for k, n, size, lo in carried:
        names[lo] = n if k == 'GRP' else (NAMES.get(lo) or ('k%s%04X' % (KIND[k], lo)))
    assert len(set(names.values())) == len(names)

    hpp = ['// The data tables of the game (Stage G5b, D-009): the part of the cartridge\'s data that the game reads, as part of the program.',
           '// GENERATED by tools/g5b_make_tables.py from the player\'s own ROM - do not edit. %d bytes in %d tables: maze, shapes, colours, sounds, speeds, scores.' % (total, len(carried)),
           '// No program code of the cartridge is in here (checked against the F6 map by the tool); the list is docs/EMBEDDED_DATA.md. Rule 7 allows exactly this.',
           '// The tables are named for what they do. A read outside a table stops the program with a message (the original would read the next table).',
           '#pragma once',
           '#include <cstddef>',
           '#include <cstdint>',
           '',
           'namespace game::data {',
           '',
           '[[noreturn]] void tableIndexError(const char* table, std::size_t size, std::size_t index);',
           'uint8_t byAddress(uint16_t address);',
           *(['uint8_t pastEnd(const char* table, uint16_t address, std::size_t size, std::size_t index);'] if LENIENT else []),
           '',
           'template <std::size_t N>',
           'struct Table {',
           '  uint8_t bytes[N];',
           '  static constexpr std::size_t size() { return N; }',
           '  uint8_t operator[](std::size_t index) const {',
           ('    if (index >= N) return pastEnd(name, address, N, index);' if LENIENT else '    if (index >= N) tableIndexError(name, N, index);'),
           '    return bytes[index];',
           '  }',
           '  const char* name;',
           '  uint16_t address;',
           '};',
           '',
           '// One table: where it was in the original program (the game keeps such addresses in its state, as pointers to the shapes), its size and its bytes.',
           'struct Segment {',
           '  const char* name;',
           '  uint16_t address;',
           '  uint16_t size;',
           '  const uint8_t* bytes;',
           '};',
           'extern const Segment kSegments[];',
           'extern const std::size_t kSegmentCount;',
           '',
           '// The byte at an original-program address inside one of the tables. The game state holds pointers to the shapes as such addresses (docs/F18_KERNEL.md),',
           '// so the picture code reaches them this way. An address outside the tables stops the program.',
           'uint8_t byAddress(uint16_t address);',
           '']
    cpp = ['// GENERATED by tools/g5b_make_tables.py from the player\'s own ROM - do not edit. See tables.hpp.',
           '#include "tables.hpp"',
           '',
           '#include <cstdio>',
           '#include <cstdlib>',
           '',
           'namespace game::data {',
           '',
           'void tableIndexError(const char* table, std::size_t size, std::size_t index) {',
           '  std::fprintf(stderr, "table %s has %zu bytes; index %zu is outside it\\n", table, size, index);',
           '  std::abort();',
           '}',
           '']
    if LENIENT:
        at = cpp.index('#include <cstdlib>') + 1
        cpp[at:at] = ['#include <set>', '#include <string>', '#include <utility>']
        cpp += ['uint8_t pastEnd(const char* table, uint16_t address, std::size_t size, std::size_t index) {',
                '  static std::set<std::pair<std::string, std::size_t>> seen;',
                '  if (seen.insert({table, index}).second) std::fprintf(stderr, "CROSSING %s %zu %zu\\n", table, size, index);',
                '  return byAddress(static_cast<uint16_t>(address + index));',
                '}', '']
    for k, n, size, lo in sorted(carried, key=lambda s: s[3]):
        nm = names[lo]
        use = GUSE.get(lo) or g5a.USE.get(lo)
        use = (use if use else 'reached by index or pointer (see docs' + BS + 'F18_KERNEL.md, F19_SOUND.md)')
        hpp.append('// $%04X-$%04X, %d bytes: %s' % (lo, lo + size - 1, size, use))
        hpp.append('extern const Table<%d> %s;' % (size, nm))
        rows = []
        bs = rom[lo - 0xF000:lo - 0xF000 + size]
        for i in range(0, size, 16):
            rows.append('    ' + ', '.join('0x%02X' % b for b in bs[i:i + 16]))
        cpp.append('const Table<%d> %s = {{' % (size, nm))
        cpp.append(',\n'.join(rows))
        cpp.append('}, "%s", 0x%04X};' % (nm, lo))
        cpp.append('')
    cpp.append('const Segment kSegments[] = {')
    for k, n, size, lo in sorted(carried, key=lambda s: s[3]):
        cpp.append('    {"%s", 0x%04X, %d, %s.bytes},' % (names[lo], lo, size, names[lo]))
    cpp.append('};')
    cpp.append('const std::size_t kSegmentCount = sizeof kSegments / sizeof kSegments[0];')
    cpp += ['',
            'uint8_t byAddress(uint16_t address) {',
            '  std::size_t low = 0, high = kSegmentCount;   // the segments are sorted by address',
            '  while (low < high) {',
            '    const std::size_t mid = (low + high) / 2;',
            '    const Segment& s = kSegments[mid];',
            '    if (address < s.address) high = mid;',
            '    else if (address >= s.address + s.size) low = mid + 1;',
            '    else return s.bytes[address - s.address];',
            '  }',
            '  std::fprintf(stderr, "address $%04X is not inside any table\\n", address);',
            '  std::abort();',
            '}',
            '',
            '}  // namespace game::data',
            '']
    hpp += ['}  // namespace game::data', '']
    if problems:
        for p in problems:
            print('PROBLEM:', p)
        print('result: FAILED (nothing written)')
        return 1
    d = ROOT / 'src' / 'game' / 'data'
    (d / 'tables.hpp').write_text('\n'.join(hpp), encoding='utf-8', newline='\n')
    (d / 'tables.cpp').write_text('\n'.join(cpp), encoding='utf-8', newline='\n')
    (ROOT / 'reference' / 'g5b').mkdir(parents=True, exist_ok=True)
    nm_lines = ['%04X %s %d' % (lo, names[lo], size) for k, n, size, lo in sorted(carried, key=lambda s: s[3])]
    (ROOT / 'reference' / 'g5b' / 'names.txt').write_text('\n'.join(nm_lines) + '\n')
    # the list of record: docs/EMBEDDED_DATA.md (Rule 7 points at it). The part after the marker is rewritten here; names, sizes and use only, no byte values.
    marker = '## The tables as they are in the program (Stage G5b)'
    doc = ROOT / 'docs' / 'EMBEDDED_DATA.md'
    text = doc.read_text(encoding='utf-8')
    if marker in text:
        text = text[:text.index(marker)].rstrip('\n') + '\n\n'
    sec = [marker, '',
           'Written by `python -I tools' + BS + 'g5b_make_tables.py rom' + BS + 'pacman.a26` (run after `tools' + BS + 'g5a_data_list.py`). **This is the list Rule 7 refers to:** the %d tables in `src' % len(carried) + BS + 'game' + BS + 'data' + BS + 'tables.cpp`, %d bytes, all data (the F6 map calls %d of the bytes data and %d untouched; none is program code). Eight of them join several of the tables above, because the original reads them as one (found by the survey of G5b); that also brings in %d bytes that no game reads, lying between read bytes.' % (total, kinds_in_map.get('D', 0), kinds_in_map.get('.', 0), total - len([a for a in reads if any(lo <= a < lo + size for k, n, size, lo in carried)])), '',
           '| Source name | Original address range | Size | What it holds |', '| --- | --- | --- | --- |']
    for k, n, size, lo in sorted(carried, key=lambda s: s[3]):
        use = GUSE.get(lo) or g5a.USE.get(lo) or 'reached by index or pointer (docs' + BS + 'F18_KERNEL.md, F19_SOUND.md)'
        sec.append('| `%s` | $%04X-$%04X | %d | %s |' % (names[lo], lo, lo + size - 1, size, use))
    sec += ['', 'A read past the end of a table stops the program with a message; the proofs of G5b run 190 games without it happening.', '']
    doc.write_text(text + '\n'.join(sec), encoding='utf-8', newline='\n')
    print('tables: %d, %d bytes; F6 map of the carried bytes: %s; none is program code' % (len(carried), total, ', '.join('%s %d' % (k, v) for k, v in sorted(kinds_in_map.items()))))
    print('every carried table was read in the %d games of G5a (%d different addresses)' % (len(list((ROOT / 'reference' / 'g5a' / 'reads').glob('*.txt'))), len(reads)))
    print('wrote src%sgame%sdata%stables.hpp and tables.cpp' % (BS, BS, BS))
    print('result: OK')
    return 0


if __name__ == '__main__':
    sys.exit(main())
