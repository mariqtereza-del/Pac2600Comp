"""Stage F4: the data of the ROM, marked, sized and named by what reads it.

  python -I tools\\f4_tables.py <rom>

Input: reference\\disasm\\pacman_named.asm (Stage F3) and the instruction-level coverage files of Stage F1
(reference\\f1\\cov_*.csv: for each instruction the ROM bytes it read as data, over 29 recorded games).
For every data byte it finds which instructions read it (indexed table reads and reads through a RAM pointer), follows each read
forward to the chip register the value is stored into (GRP0/GRP1 = sprite shape, COLUPx = colour, PFx = playfield, AUDxx = sound),
cuts the data into segments at every old label and at the edges of what was read, and names each segment <KIND>_Fxxx:
  GFX   read through a pointer and stored into GRP0 or GRP1
  COLOR stored into COLUP0, COLUP1, COLUPF or COLUBK
  PF    stored into PF0, PF1 or PF2
  SND   stored into AUDC/AUDF/AUDV
  TBL   read by the program but not stored straight into a chip register (what it means is for later stages)
  UNREAD never read in the recorded games
  VECTORS the 6502's three vectors at $FFFA-$FFFF (read by the CPU itself; written as .word)
The kind is evidence about how the bytes are used, not a statement of what they mean in the game. Writes
reference\\disasm\\pacman_tables.asm (LOCAL ONLY), assembles it and requires the result to be byte-identical to the ROM.
"""
import collections
import glob
import hashlib
import pathlib
import re
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import f1_6502 as asm  # noqa: E402

ROOT = pathlib.Path(__file__).resolve().parent.parent
OUT = ROOT / 'reference' / 'disasm'
ORG = 0xF000
SINK_KIND = {'GRP0': 'GFX', 'GRP1': 'GFX', 'COLUP0': 'COLOR', 'COLUP1': 'COLOR', 'COLUPF': 'COLOR', 'COLUBK': 'COLOR', 'PF0': 'PF', 'PF1': 'PF', 'PF2': 'PF',
             'AUDC0': 'SND', 'AUDC1': 'SND', 'AUDF0': 'SND', 'AUDF1': 'SND', 'AUDV0': 'SND', 'AUDV1': 'SND'}
PRIORITY = ['GFX', 'COLOR', 'PF', 'SND', 'TBL']
ALLKINDS = PRIORITY + ['VECTORS', 'UNREAD']
DATA_LINE = re.compile(r'^\s+\.byte ([^;]+?)\s*; \$F([0-9A-F]{3})')
INSTR_LINE = re.compile(r'; \$F([0-9A-F]{3})  ')


def sink_of(lines, i):
    """The first chip register stored into after line i (within 14 lines, stopping at a return or jump)."""
    for j in range(i + 1, min(i + 15, len(lines))):
        m = re.match(r'\s+(STA|STX|STY) ([A-Z][A-Z0-9]*)', lines[j])
        if m and m.group(2) in SINK_KIND:
            return m.group(2)
        if re.match(r'\s+(RTS|JMP|JSR|RTI)', lines[j]):
            break
    return ''


def main():
    rom = pathlib.Path(sys.argv[1]).read_bytes()
    lines = (OUT / 'pacman_named.asm').read_text(encoding='utf-8').split('\n')
    where = {}  # offset of an instruction -> line index
    data_bytes = {}  # offset -> value for every data byte
    for i, l in enumerate(lines):
        m = DATA_LINE.match(l)
        if m:
            off = int(m.group(2), 16)
            for k, v in enumerate(m.group(1).split(',')):
                data_bytes[off + k] = int(v.strip().lstrip('$'), 16)
            continue
        m = INSTR_LINE.search(l)
        if m and not l.lstrip().startswith(';'):
            where[int(m.group(1), 16)] = i
    assert all(rom[o] == v for o, v in data_bytes.items())

    # which instructions read which data bytes (own operand bytes excluded)
    readers = collections.defaultdict(set)  # data offset -> instruction offsets
    for f in sorted(glob.glob(str(ROOT / 'reference' / 'f1' / 'cov_*.csv'))):
        for l in open(f):
            p = l.strip().split(',')
            if p[0] == 'r':
                ins, off = int(p[1]), int(p[2])
                n = asm.TABLE[rom[ins]][3]
                if off in data_bytes and not (ins <= off < ins + n):
                    readers[off].add(ins)
    ins_reads = collections.defaultdict(list)
    for off, ins in readers.items():
        for x in ins:
            ins_reads[x].append(off)
    ins_sink = {x: sink_of(lines, where[x]) for x in ins_reads}
    ins_kind = {x: SINK_KIND.get(s, 'TBL') for x, s in ins_sink.items()}

    kind_of = {}
    for off in data_bytes:
        ks = {ins_kind[x] for x in readers.get(off, ())}
        kind_of[off] = next((k for k in PRIORITY if k in ks), 'UNREAD')

    # segment boundaries: old labels, the edges of each reader's range, kind changes, the start of every data block
    cuts = set()
    for i, l in enumerate(lines):
        m = re.match(r'^D_F([0-9A-F]{3}):', l)
        if m:
            cuts.add(int(m.group(1), 16))
    # the ranges that readers touched, merged where they overlap or touch: one table (or one block of tables) each
    rngs = sorted((min(o), max(o)) for o in ins_reads.values())
    merged = []
    for a, b in rngs:
        if merged and a <= merged[-1][1] + 1:
            merged[-1][1] = max(merged[-1][1], b)
        else:
            merged.append([a, b])
    for a, b in merged:
        cuts.add(a)
        cuts.add(b + 1)
    cuts.add(0xFFA)  # the three 6502 vectors (NMI, RESET, IRQ/BRK) at $FFFA-$FFFF
    for o in data_bytes:
        if o >= 0xFFA:
            kind_of[o] = 'VECTORS'
    offs_sorted = sorted(data_bytes)
    segs, cur = [], None
    for o in offs_sorted:
        if cur and o == cur[-1] + 1 and o not in cuts and kind_of[o] == kind_of[cur[0]]:
            cur.append(o)
        else:
            if cur:
                segs.append(cur)
            cur = [o]
    segs.append(cur)
    names = {}
    for s in segs:
        names[s[0]] = '%s_F%03X' % (kind_of[s[0]], s[0])
    old2new = {}
    for i, l in enumerate(lines):
        m = re.match(r'^(D_F([0-9A-F]{3})):', l)
        if m:
            old2new[m.group(1)] = names[int(m.group(2), 16)]

    # new listing: instruction operands renamed, data regenerated
    out, i, seen_data = [], 0, False
    seg_at = {s[0]: s for s in segs}
    while i < len(lines):
        l = lines[i]
        if re.match(r'^D_F[0-9A-F]{3}:', l):
            i += 1
            continue
        if DATA_LINE.match(l):
            if not seen_data:
                seen_data = True
            i += 1
            continue
        for old, new in old2new.items():
            l = re.sub(r'\b%s\b' % old, new, l)
        out.append(l)
        i += 1
    # place each data segment where it was: data lines were removed in place, so rebuild from the F2 order
    # (simplest: walk the original lines again and emit segments at the first data line of each block)
    res, i = [], 0
    emitted = set()
    while i < len(lines):
        l = lines[i]
        m = DATA_LINE.match(l)
        lab = re.match(r'^D_F([0-9A-F]{3}):', l)
        if lab:
            i += 1
            continue
        if m:
            start = int(m.group(2), 16)
            # emit every segment that starts within this block of data lines
            j = i
            block = []
            while j < len(lines) and (DATA_LINE.match(lines[j]) or re.match(r'^D_F[0-9A-F]{3}:', lines[j])):
                mm = DATA_LINE.match(lines[j])
                if mm:
                    block.append(int(mm.group(2), 16))
                j += 1
            lo = block[0]
            hi = block[-1] + len(DATA_LINE.match(lines[j - 1] if DATA_LINE.match(lines[j - 1]) else lines[j - 2]).group(1).split(','))
            for s in segs:
                if lo <= s[0] < hi:
                    k = kind_of[s[0]]
                    users = sorted({x for o in s for x in readers.get(o, ())})
                    res.append('%s:' % names[s[0]])
                    if k == 'VECTORS':
                        note = 'read by the CPU itself when it starts (RESET) or is interrupted'
                    elif k == 'UNREAD':
                        note = 'never read in the recorded games'
                    else:
                        note = 'read by %s' % ', '.join('$F%03X (%s)' % (x, ins_sink[x] or 'no chip register') for x in users[:6]) + (' and %d more' % (len(users) - 6) if len(users) > 6 else '')
                    res.append('        ; %s, %d bytes, $F%03X-$F%03X: %s' % (k, len(s), s[0], s[-1], note))
                    if k == 'VECTORS':
                        for q, nm in zip(range(0, 6, 2), ('NMI', 'RESET', 'IRQ/BRK')):
                            res.append('        .word $%04X%s ; $F%03X  %s vector' % (rom[s[q]] | rom[s[q + 1]] << 8, ' ' * 33, s[q], nm))
                        continue
                    for q in range(0, len(s), 8):
                        chunk = s[q:q + 8]
                        res.append('        .byte %-39s ; $F%03X' % (','.join('$%02X' % rom[o] for o in chunk), chunk[0]))
            i = j
            continue
        for old, new in old2new.items():
            l = re.sub(r'\b%s\b' % old, new, l)
        res.append(l)
        i += 1
    text = '\n'.join(res) + '\n'
    (OUT / 'pacman_tables.asm').write_text(text, encoding='utf-8', newline='')
    back, labels = asm.assemble(text, ORG)
    same = back == rom
    (OUT / 'pacman_tables_rebuilt.bin').write_bytes(back)
    kinds = collections.Counter()
    nseg = collections.Counter()
    for s in segs:
        kinds[kind_of[s[0]]] += len(s)
        nseg[kind_of[s[0]]] += 1
    stats = ['data bytes: %d in %d segments' % (len(data_bytes), len(segs)),
             'by kind (bytes / segments): ' + ', '.join('%s %d / %d' % (k, kinds[k], nseg[k]) for k in ALLKINDS),
             'reading instructions found: %d (with a chip register as the end of the value: %d)' % (len(ins_reads), sum(1 for x in ins_sink.values() if x)),
             'ROM SHA-1:     %s' % hashlib.sha1(rom).hexdigest(), 'rebuilt SHA-1: %s' % hashlib.sha1(back).hexdigest(),
             'result: %s' % ('BYTE-IDENTICAL' if same else 'DIFFERENT')]
    segtab = ['segment table (kind, name, size, address range):'] + ['  %-6s %-12s %4d  $F%03X-$F%03X' % (kind_of[s[0]], names[s[0]], len(s), s[0], s[-1]) for s in segs]
    (OUT / 'tables_stats.txt').write_text('\n'.join(stats + segtab) + '\n', encoding='utf-8')
    print('\n'.join(stats + segtab))
    return 0 if same else 1


if __name__ == '__main__':
    sys.exit(main())
