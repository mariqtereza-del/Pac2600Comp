r"""Stage F24.2 helper: prints a stretch of the routine listing with the processor cycles of every instruction.

  python -I tools\f24_cycles.py <first hex address> <last hex address>

Reads reference\disasm\pacman_routines.asm (local) and the opcode table of tools\f1_6502.py. For a branch it shows the cycles when not taken, the extra
cycle when taken and the extra cycle when the taken branch crosses a page; for an indexed read (abs,X abs,Y (zp),Y) it marks the extra cycle when the
index crosses a page. Used to count the cycles of the original code by hand (the native game has to know the exact time of its sound writes).
"""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / 'tools'))
import f1_6502 as asm  # noqa: E402

READS = {'lda', 'ldx', 'ldy', 'adc', 'sbc', 'and', 'ora', 'eor', 'cmp', 'cpx', 'cpy', 'bit'}


def main():
    lo, hi = int(sys.argv[1], 16), int(sys.argv[2], 16)
    rx = re.compile(r'^(\s+)(.*?)\s*; \$([0-9A-F]{4})\s+((?:[0-9A-F]{2} ?)+)')
    pending = []
    for line in (ROOT / 'reference' / 'disasm' / 'pacman_routines.asm').read_text().splitlines():
        if re.match(r'^[A-Za-z_][A-Za-z_0-9]*:', line):
            pending.append(line)
            continue
        if line.startswith(';'):
            continue
        m = rx.match(line)
        if not m:
            continue
        addr = int(m.group(3), 16)
        if addr < lo or addr > hi:
            pending = []
            continue
        for p in pending:
            print(p)
        pending = []
        b = bytes(int(x, 16) for x in m.group(4).split())
        mn, mode, cyc, ln, _ = asm.TABLE[b[0]]
        note = ''
        if mode == 'RELATIVE':
            off = b[1] - 256 if b[1] >= 128 else b[1]
            tgt = addr + 2 + off
            note = 'not taken %d, taken %d%s' % (cyc, cyc + 1, ' +1 page' if (tgt >> 8) != ((addr + 2) >> 8) else '')
        elif mn in READS and mode in ('ABSOLUTE_X', 'ABSOLUTE_Y', 'INDIRECT_Y'):
            note = '%d (+1 if page crossed)' % cyc
        else:
            note = str(cyc)
        print('%04X  %-34s %s' % (addr, m.group(2).strip()[:34], note))


if __name__ == '__main__':
    main()
