"""Stage F2: the raw disassembly of the ROM, in a form that assembles back to the original bytes.

  python -I tools\\f2_disasm.py <rom>

Reads the code/data map of Stage F1 (reference\\f1\\coverage_union.txt, made by tools\\f1_coverage.py), splits the 4,096 bytes into
instructions and data, gives a label to every address that is jumped to, branched to or read through an absolute address, writes
reference\\disasm\\pacman_raw.asm (LOCAL ONLY: it is the original program in readable form, Rule 7), assembles it with our own
assembler (tools\\f1_6502.py) and requires the result to be byte-identical to the ROM. The rebuilt bytes are written to
reference\\disasm\\pacman_rebuilt.bin and the statistics to reference\\disasm\\stats.txt (local) and printed.
The ROM is only read.
"""
import hashlib
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import f1_6502 as asm  # noqa: E402

ROOT = pathlib.Path(__file__).resolve().parent.parent
MAP = ROOT / 'reference' / 'f1' / 'coverage_union.txt'
OUT = ROOT / 'reference' / 'disasm'
ORG = 0xF000


def read_map():
    kind = []
    for line in MAP.read_text(encoding='utf-8').split('\n'):
        if len(line) > 6 and line[0] == 'F' and line[5:6] == ' ' and all(c in 'CcD.' for c in line[6:]):
            kind += list(line[6:])
    assert len(kind) == 4096, 'the map has %d bytes' % len(kind)
    return kind


def decode_run(rom, a, b):
    """Instructions that exactly fill rom[a:b], all official; or None."""
    out, i = [], a
    while i < b:
        mn, mode, cy, by, official = asm.TABLE[rom[i]]
        if not official or i + by > b:
            return None
        out.append((i, by))
        i += by
    return out


def main():
    rom = pathlib.Path(sys.argv[1]).read_bytes()
    assert len(rom) == 4096
    kind = read_map()
    OUT.mkdir(parents=True, exist_ok=True)

    # 1. items: ('code', offset, length, executed) or ('data', offset, length)
    items, i = [], 0
    while i < 4096:
        if kind[i] == 'C':
            by = asm.TABLE[rom[i]][3]
            assert all(kind[i + k] == 'c' for k in range(1, by)), 'inconsistent map at $F%03X' % i
            items.append(['code', i, by, True])
            i += by
        else:
            j = i
            while j < 4096 and kind[j] != 'C':
                j += 1
            run = (i, j)
            seg = ''.join(kind[i:j])
            if set(seg) == {'.'} and j - i <= 8 and j <= 0xFF0 and decode_run(rom, i, j):  # never-run bytes in the code area that are whole instructions
                for off, by in decode_run(rom, i, j):
                    items.append(['code', off, by, False])
            else:
                items.append(['data', i, j - i])
            i = j

    # 2. labels: every branch, jump, call and absolute reference into the ROM
    labels = {}
    mirror_refs = 0
    notes = []
    starts = set(it[1] for it in items if it[0] == 'code')
    for it in items:
        if it[0] != 'code':
            continue
        off = it[1]
        mn, mode, cy, by, _ = asm.TABLE[rom[off]]
        if mode == 'RELATIVE':
            d = rom[off + 1] - 256 if rom[off + 1] > 127 else rom[off + 1]
            tgt = (ORG + off + 2 + d) & 0xFFFF
        elif mode in ('ABSOLUTE', 'ABSOLUTE_X', 'ABSOLUTE_Y'):
            tgt = rom[off + 1] | rom[off + 2] << 8
        else:
            continue
        if (tgt & 0xF000) == 0xF000:
            labels[tgt - ORG] = None
        elif (tgt & 0x1000) == 0x1000 and tgt < 0x2000:
            mirror_refs += 1  # a reference through the mirror $1000-$1FFF: written as a number
    # code labels at instruction starts, data labels elsewhere
    for off in list(labels):
        if off in starts:
            labels[off] = 'L_F%03X' % off
        else:
            inside = [it for it in items if it[0] == 'data' and it[1] <= off < it[1] + it[2]]
            labels[off] = 'D_F%03X' % off
            if not inside:
                notes.append('reference to $F%03X is not the start of an instruction and not in data' % off)
    # split data items at labels
    new = []
    for it in items:
        if it[0] == 'data':
            cuts = sorted(o for o in labels if it[1] < o < it[1] + it[2])
            pts = [it[1]] + cuts + [it[1] + it[2]]
            for a, b in zip(pts, pts[1:]):
                new.append(['data', a, b - a])
        else:
            new.append(it)
    items = new

    # 3. the listing
    lines = ['; Pac-Man (Atari 2600), raw disassembly (Stage F2). LOCAL ONLY: this is the original program in readable form.',
             '; Made by tools\\f2_disasm.py from the ROM and the code/data map of Stage F1. No names or comments yet (F3 onwards).',
             '        .org $F000']
    n_instr = n_unexec = data_bytes = 0
    for it in items:
        off = it[1]
        if off in labels:
            lines.append('%s:' % labels[off])
        if it[0] == 'code':
            mn, mode, cy, by, _ = asm.TABLE[rom[off]]
            n, text = asm.decode(rom, off, ORG + off)
            if mode == 'RELATIVE':
                d = rom[off + 1] - 256 if rom[off + 1] > 127 else rom[off + 1]
                tgt = (ORG + off + 2 + d) & 0xFFFF
                if tgt - ORG in labels and (tgt & 0xF000) == 0xF000:
                    text = '%s %s' % (mn.upper(), labels[tgt - ORG])
            elif mode in ('ABSOLUTE', 'ABSOLUTE_X', 'ABSOLUTE_Y'):
                tgt = rom[off + 1] | rom[off + 2] << 8
                if (tgt & 0xF000) == 0xF000:
                    suffix = {'ABSOLUTE': '', 'ABSOLUTE_X': ',X', 'ABSOLUTE_Y': ',Y'}[mode]
                    text = '%s %s%s' % (mn.upper(), labels[tgt - ORG], suffix)
            lines.append('        %-18s ; $F%03X  %s%s' % (text, off, ' '.join('%02X' % rom[off + k] for k in range(by)), '' if it[3] else '   (never run in the recorded games)'))
            n_instr += 1
            n_unexec += 0 if it[3] else 1
        else:
            data_bytes += it[2]
            for k in range(0, it[2], 8):
                chunk = rom[off + k:off + min(k + 8, it[2])]
                lines.append('        .byte %-39s ; $F%03X' % (','.join('$%02X' % b for b in chunk), off + k))
    text = '\n'.join(lines) + '\n'
    (OUT / 'pacman_raw.asm').write_text(text, encoding='utf-8', newline='')

    # 4. rebuild and compare
    back, labs = asm.assemble(text, ORG)
    (OUT / 'pacman_rebuilt.bin').write_bytes(back)
    same = back == rom
    stats = ['listing lines: %d' % len(lines), 'instructions: %d (of which never run in the recorded games: %d)' % (n_instr, n_unexec), 'data bytes: %d' % data_bytes,
             'labels: %d (code %d, data %d)' % (len(labels), sum(1 for v in labels.values() if v.startswith('L_')), sum(1 for v in labels.values() if v.startswith('D_'))),
             'references through the mirror $1000-$1FFF written as numbers: %d' % mirror_refs, 'notes: %s' % (notes or 'none'),
             'ROM bytes: %d, rebuilt bytes: %d' % (len(rom), len(back)),
             'ROM SHA-1:     %s' % hashlib.sha1(rom).hexdigest(), 'rebuilt SHA-1: %s' % hashlib.sha1(back).hexdigest(),
             'ROM SHA-256:     %s' % hashlib.sha256(rom).hexdigest(), 'rebuilt SHA-256: %s' % hashlib.sha256(back).hexdigest(),
             'result: %s' % ('BYTE-IDENTICAL' if same else 'DIFFERENT')]
    (OUT / 'stats.txt').write_text('\n'.join(stats) + '\n', encoding='utf-8')
    print('\n'.join(stats))
    return 0 if same else 1


if __name__ == '__main__':
    sys.exit(main())
