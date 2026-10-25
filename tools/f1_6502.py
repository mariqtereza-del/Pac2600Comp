"""Stage F1: a 6502 disassembler and assembler of our own (no download, DECISIONS.md T-F1-1).

The opcode table is read from Stella's own disassembler source (third_party\\stella\\src\\debugger\\DiStella.cxx, vendored and never
edited): mnemonic, addressing mode, cycles and length of all 256 opcodes. Only the 151 official opcodes are assembled; the others
are written as `.byte`.

  python -I tools\\f1_6502.py dis <rom> [org]      linear disassembly of the whole file to stdout
  python -I tools\\f1_6502.py selftest <rom>       the tests of Stage F1 (docs\\F1_TOOLS.md)

Syntax (one instruction per line; `;` starts a comment):
  LDA #$10   LDA $80   LDA $80,X   LDA $0080 (four hex digits: absolute even below $100)   LDA $1234,Y
  LDA ($80,X)   LDA ($80),Y   JMP ($1234)   ASL A   BNE $F012 (the target address)   BNE label
  label:   .org $F000   .byte $01,$02   .word $F000
"""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
DISTELLA = ROOT / 'third_party' / 'stella' / 'src' / 'debugger' / 'DiStella.cxx'

MODES = ['IMPLIED', 'ACCUMULATOR', 'IMMEDIATE', 'ZERO_PAGE', 'ZERO_PAGE_X', 'ZERO_PAGE_Y', 'ABSOLUTE', 'ABSOLUTE_X', 'ABSOLUTE_Y',
         'ABS_INDIRECT', 'INDIRECT_X', 'INDIRECT_Y', 'RELATIVE']
LENGTH = {'IMPLIED': 1, 'ACCUMULATOR': 1, 'IMMEDIATE': 2, 'ZERO_PAGE': 2, 'ZERO_PAGE_X': 2, 'ZERO_PAGE_Y': 2, 'ABSOLUTE': 3, 'ABSOLUTE_X': 3,
          'ABSOLUTE_Y': 3, 'ABS_INDIRECT': 3, 'INDIRECT_X': 2, 'INDIRECT_Y': 2, 'RELATIVE': 2}


def load_table():
    """[(mnemonic, mode, cycles, bytes, official)] for opcodes 0..255, from Stella's DiStella.cxx."""
    text = DISTELLA.read_text(encoding='utf-8')
    start = text.index('DiStella::ourLookup = {')
    rx = re.compile(r'/\*\s*([0-9a-f]{2})\s*\*/\s*\{\s*"([^"]+)"\s*,\s*AddressingMode::(\w+)\s*,\s*AccessMode::\w+\s*,\s*RWMode::\w+\s*,\s*(\d+)\s*,\s*(\d+)\s*\}')
    table = {}
    for m in rx.finditer(text, start):
        op = int(m.group(1), 16)
        mn = m.group(2)
        table[op] = (mn.lower(), m.group(3), int(m.group(4)), int(m.group(5)), mn.islower() and not mn.startswith('.'))
        if len(table) == 256:
            break
    assert sorted(table) == list(range(256)), 'the opcode table in DiStella.cxx was not read completely (%d)' % len(table)
    return [table[i] for i in range(256)]


TABLE = load_table()
ENCODE = {}  # (mnemonic, mode) -> opcode, official only
for _op, (_mn, _mode, _cy, _by, _off) in enumerate(TABLE):
    if _off:
        assert (_mn, _mode) not in ENCODE
        ENCODE[(_mn, _mode)] = _op
assert all(LENGTH[m] == by for (mn, m, cy, by, off) in TABLE if off), 'a length in the table disagrees with its addressing mode'


def operand_text(mode, data, pc):
    if mode in ('IMPLIED',):
        return ''
    if mode == 'ACCUMULATOR':
        return 'A'
    if mode == 'IMMEDIATE':
        return '#$%02X' % data[0]
    if mode == 'ZERO_PAGE':
        return '$%02X' % data[0]
    if mode == 'ZERO_PAGE_X':
        return '$%02X,X' % data[0]
    if mode == 'ZERO_PAGE_Y':
        return '$%02X,Y' % data[0]
    v = data[0] | (data[1] << 8) if len(data) > 1 else 0
    if mode == 'ABSOLUTE':
        return '$%04X' % v
    if mode == 'ABSOLUTE_X':
        return '$%04X,X' % v
    if mode == 'ABSOLUTE_Y':
        return '$%04X,Y' % v
    if mode == 'ABS_INDIRECT':
        return '($%04X)' % v
    if mode == 'INDIRECT_X':
        return '($%02X,X)' % data[0]
    if mode == 'INDIRECT_Y':
        return '($%02X),Y' % data[0]
    if mode == 'RELATIVE':
        off = data[0] - 256 if data[0] > 127 else data[0]
        return '$%04X' % ((pc + 2 + off) & 0xFFFF)
    raise ValueError(mode)


def decode(data, i, pc):
    """(length, text) of the item at data[i]: an official instruction, or one `.byte` (illegal opcode or a cut-off instruction)."""
    mn, mode, cy, by, official = TABLE[data[i]]
    if not official or i + by > len(data):
        return 1, '.byte $%02X' % data[i]
    ops = bytes(data[i + 1:i + by])
    text = mn.upper() + (' ' + operand_text(mode, ops, pc) if mode != 'IMPLIED' else '')
    return by, text


def disassemble(data, org):
    """Linear disassembly: [(address, bytes, text)]."""
    out, i = [], 0
    while i < len(data):
        n, text = decode(data, i, org + i)
        out.append((org + i, bytes(data[i:i + n]), text))
        i += n
    return out


def listing(items):
    return '\n'.join('%s' % t for a, b, t in items) + '\n'


# ---- the assembler ----

NUM = r'(?:\$[0-9A-Fa-f]+|\d+)'


def parse_number(tok):
    return int(tok[1:], 16) if tok.startswith('$') else int(tok)


class AsmError(Exception):
    pass


def assemble(source, org=0):
    """Two passes. Returns (bytes, labels)."""
    lines = []
    for raw in source.split('\n'):
        line = raw.split(';')[0].strip()
        if line:
            lines.append(line)
    labels = {}
    for pas in (1, 2):
        out = bytearray()
        pc = org
        for line in lines:
            while True:
                m = re.match(r'^([A-Za-z_][\w]*):\s*(.*)$', line)
                if not m:
                    break
                if pas == 1:
                    if m.group(1) in labels:
                        raise AsmError('label twice: ' + m.group(1))
                    labels[m.group(1)] = pc
                line = m.group(2)
            if not line:
                continue
            low = line.lower()
            if low.startswith('.org'):
                new = value(line[4:].strip(), labels, pas)
                if out and new < pc:
                    raise AsmError('.org goes backwards')
                out += bytes(new - pc) if out else b''
                pc = new
                if not out:
                    org_here = new
                continue
            if low.startswith('.byte'):
                vals = [value(v.strip(), labels, pas) for v in line[5:].split(',')]
                for v in vals:
                    if not 0 <= v <= 255:
                        raise AsmError('byte out of range: ' + line)
                    out.append(v)
                    pc += 1
                continue
            if low.startswith('.word'):
                for v in [value(v.strip(), labels, pas) for v in line[5:].split(',')]:
                    out += bytes([v & 255, (v >> 8) & 255])
                    pc += 2
                continue
            parts = line.split(None, 1)
            mn = parts[0].lower()
            opnd = parts[1].strip() if len(parts) > 1 else ''
            op, operand_bytes = encode(mn, opnd, pc, labels, pas)
            out.append(op)
            out += operand_bytes
            pc += 1 + len(operand_bytes)
    return bytes(out), labels


def value(tok, labels, pas):
    tok = tok.strip()
    m = re.match(r'^([A-Za-z_]\w*)\s*([+-]\s*%s)?$' % NUM, tok)
    if m:
        base = labels.get(m.group(1))
        if base is None:
            if pas == 1:
                return 0x1000
            raise AsmError('unknown label: ' + m.group(1))
        off = parse_number(m.group(2).replace(' ', '')[1:]) * (-1 if m.group(2).replace(' ', '')[0] == '-' else 1) if m.group(2) else 0
        return base + off
    if re.match('^%s$' % NUM, tok):
        return parse_number(tok)
    raise AsmError('cannot read the value: ' + tok)


def wide(tok):
    """A written number with four or more hex digits means an absolute address."""
    return tok.startswith('$') and len(tok) - 1 >= 4


def encode(mn, opnd, pc, labels, pas):
    def need(mode):
        if (mn, mode) not in ENCODE:
            raise AsmError('%s has no %s form: %s %s' % (mn.upper(), mode, mn, opnd))
        return ENCODE[(mn, mode)]
    if opnd == '' :
        if (mn, 'IMPLIED') in ENCODE:
            return ENCODE[(mn, 'IMPLIED')], b''
        return need('ACCUMULATOR'), b''
    if opnd.upper() == 'A':
        return need('ACCUMULATOR'), b''
    if opnd.startswith('#'):
        v = value(opnd[1:], labels, pas)
        if not 0 <= v <= 255:
            raise AsmError('immediate out of range: ' + opnd)
        return need('IMMEDIATE'), bytes([v])
    if (mn, 'RELATIVE') in ENCODE:
        target = value(opnd, labels, pas)
        off = ((target - (pc + 2) + 0x8000) & 0xFFFF) - 0x8000  # the 6502 address space wraps at $FFFF
        if pas == 2 and not -128 <= off <= 127:
            raise AsmError('branch too far: %s %s' % (mn, opnd))
        return need('RELATIVE'), bytes([off & 255])
    m = re.match(r'^\((.+),\s*[Xx]\)$', opnd)
    if m:
        return need('INDIRECT_X'), bytes([value(m.group(1), labels, pas) & 255])
    m = re.match(r'^\((.+)\)\s*,\s*[Yy]$', opnd)
    if m:
        return need('INDIRECT_Y'), bytes([value(m.group(1), labels, pas) & 255])
    m = re.match(r'^\((.+)\)$', opnd)
    if m:
        v = value(m.group(1), labels, pas)
        return need('ABS_INDIRECT'), bytes([v & 255, v >> 8])
    idx = ''
    m = re.match(r'^(.+),\s*([XxYy])$', opnd)
    if m:
        opnd, idx = m.group(1).strip(), m.group(2).upper()
    v = value(opnd, labels, 2 if pas == 2 else 1)
    zp = v < 256 and not wide(opnd) and (re.match(r'^%s$' % NUM, opnd) is not None or v < 256)
    suffix = {'': '', 'X': '_X', 'Y': '_Y'}[idx]
    if zp and (mn, 'ZERO_PAGE' + suffix) in ENCODE:
        return ENCODE[(mn, 'ZERO_PAGE' + suffix)], bytes([v])
    return need('ABSOLUTE' + suffix), bytes([v & 255, v >> 8])


# ---- the tests of Stage F1 ----

def selftest(rom_path):
    rom = pathlib.Path(rom_path).read_bytes()
    results = []

    def check(name, ok, detail=''):
        results.append((name, ok, detail))
        print('%-4s %s%s' % ('PASS' if ok else 'FAIL', name, (': ' + detail) if detail else ''))

    official = [op for op in range(256) if TABLE[op][4]]
    check('the opcode table has 256 entries, 151 of them official', len(TABLE) == 256 and len(official) == 151, '%d official' % len(official))
    known = {0xA9: ('lda', 'IMMEDIATE', 2, 2), 0x4C: ('jmp', 'ABSOLUTE', 3, 3), 0x6C: ('jmp', 'ABS_INDIRECT', 5, 3), 0x20: ('jsr', 'ABSOLUTE', 6, 3),
             0x60: ('rts', 'IMPLIED', 6, 1), 0xD0: ('bne', 'RELATIVE', 2, 2), 0x85: ('sta', 'ZERO_PAGE', 3, 2), 0x91: ('sta', 'INDIRECT_Y', 6, 2),
             0xEA: ('nop', 'IMPLIED', 2, 1), 0x00: ('brk', 'IMPLIED', 7, 1), 0x0A: ('asl', 'ACCUMULATOR', 2, 1)}
    bad = [hex(op) for op, v in known.items() if TABLE[op][:4] != v]
    check('eleven opcodes known from the 6502 data sheet (mnemonic, mode, cycles, length) agree with the table', not bad, ', '.join(bad))
    # every official opcode: build an instruction, disassemble, assemble, same bytes
    bad = []
    for op in official:
        mn, mode, cy, by, _ = TABLE[op]
        raw = bytes([op, 0x12, 0xF3][:by]) if mode != 'RELATIVE' else bytes([op, 0x10])
        n, text = decode(raw + b'\x00\x00', 0, 0xF100)
        try:
            back, _ = assemble('.org $F100\n' + text, 0xF100)
        except AsmError as e:
            bad.append('%02X %s (%s)' % (op, text, e))
            continue
        if back != raw or n != by:
            bad.append('%02X %s -> %s' % (op, text, back.hex()))
    check('all 151 official opcodes: disassemble then assemble gives the same bytes', not bad, '; '.join(bad[:5]))
    # zero-page versus absolute: the written width decides
    a, _ = assemble('LDA $80\nLDA $0080\nSTA $D0,X\nSTA $00D0,X')
    check('a number of two hex digits is zero page, four digits absolute', a == bytes([0xA5, 0x80, 0xAD, 0x80, 0x00, 0x95, 0xD0, 0x9D, 0xD0, 0x00]), a.hex())
    # branches and labels
    a, lab = assemble('.org $F000\nstart: LDX #$05\nloop: DEX\nBNE loop\nJMP start\n', 0xF000)
    check('labels and branches (backward branch -3, JMP absolute)', a == bytes([0xA2, 5, 0xCA, 0xD0, 0xFD, 0x4C, 0x00, 0xF0]), a.hex())
    # refusals
    refused = 0
    for src in ('LDA #$100', 'BNE $F200', 'STX $80,X', 'FOO $10', 'LDA $10,Z'):
        try:
            assemble('.org $F000\n' + src, 0xF000)
        except (AsmError, KeyError):
            refused += 1
    check('five wrong lines are refused (value too big, branch too far, mode that does not exist, unknown mnemonic, bad index)', refused == 5, '%d refused' % refused)
    # the whole ROM
    items = disassemble(rom, 0xF000)
    text = listing(items)
    back, _ = assemble('.org $F000\n' + text, 0xF000)
    check('the whole ROM: disassemble, assemble, byte-identical', back == rom, '%d bytes, %d lines, %d illegal or cut-off bytes written as .byte' % (len(rom), len(items), sum(1 for a, b, t in items if t.startswith('.byte'))))
    # the listing assembled twice and with labels in front gives the same
    lab_src = '\n'.join('L%04X: %s' % (a, t) for a, b, t in items)
    back2, labels = assemble('.org $F000\n' + lab_src, 0xF000)
    check('the same listing with a label on every line assembles to the same bytes', back2 == rom, '%d labels' % len(labels))
    failed = [n for n, ok, d in results if not ok]
    print('%d checks, %d failed' % (len(results), len(failed)))
    return 1 if failed else 0


if __name__ == '__main__':
    if len(sys.argv) >= 3 and sys.argv[1] == 'dis':
        org = int(sys.argv[3], 0) if len(sys.argv) > 3 else 0xF000
        data = pathlib.Path(sys.argv[2]).read_bytes()
        for a, b, t in disassemble(data, org):
            print('%04X  %-9s %s' % (a, b.hex(), t))
    elif len(sys.argv) >= 3 and sys.argv[1] == 'selftest':
        sys.exit(selftest(sys.argv[2]))
    else:
        print(__doc__)
