"""Stage F3: the standard names of the video chip (TIA) and the timer/input chip (RIOT) in the listing.

  python -I tools\\f3_names.py <rom>

Takes reference\\disasm\\pacman_raw.asm (Stage F2), replaces every operand that is a TIA or RIOT address by its standard name
(WSYNC, GRP0, INTIM ...), writes reference\\disasm\\pacman_named.asm (LOCAL ONLY) and requires the assembled result to be
byte-identical to the ROM. The names are those of the chips' data sheets as used by the community header `vcs.h`; they are facts
about the hardware, checked against the tables of the vendored Stella debugger source on every run.

Rules (docs\\F3_HARDWARE_NAMES.md):
- A store (STA STX STY) writes, every other instruction reads. The same address has different names for a write and a read
  (address $08 is COLUPF when written and INPT0 when read).
- TIA writes: the chip looks at the low 6 bits of the address ($00-$2C used), so $40-$6C are copies; reads: the low 4 bits
  ($00-$0D), so $10-$1D, $30-$3D ... are copies. A copy is written NAME_AT<hex address>, an equate with the exact address.
- RIOT: $0280-$0297 (SWCHA ... T1024T); copies are written the same way.
- An indexed access with base $00 (`STA $00,X`) is not a register but a sweep over the whole page and stays a number.
- An absolute-mode instruction with an address below $100 stays a number (the written width is part of the bytes).
- Instructions that read-modify-write a chip register would be reported (none expected).
"""
import hashlib
import pathlib
import re
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import f1_6502 as asm  # noqa: E402

ROOT = pathlib.Path(__file__).resolve().parent.parent
OUT = ROOT / 'reference' / 'disasm'
ORG = 0xF000

TIA_WRITE = ['VSYNC', 'VBLANK', 'WSYNC', 'RSYNC', 'NUSIZ0', 'NUSIZ1', 'COLUP0', 'COLUP1', 'COLUPF', 'COLUBK', 'CTRLPF', 'REFP0', 'REFP1',
             'PF0', 'PF1', 'PF2', 'RESP0', 'RESP1', 'RESM0', 'RESM1', 'RESBL', 'AUDC0', 'AUDC1', 'AUDF0', 'AUDF1', 'AUDV0', 'AUDV1',
             'GRP0', 'GRP1', 'ENAM0', 'ENAM1', 'ENABL', 'HMP0', 'HMP1', 'HMM0', 'HMM1', 'HMBL', 'VDELP0', 'VDELP1', 'VDELBL',
             'RESMP0', 'RESMP1', 'HMOVE', 'HMCLR', 'CXCLR']  # $00-$2C
TIA_READ = ['CXM0P', 'CXM1P', 'CXP0FB', 'CXP1FB', 'CXM0FB', 'CXM1FB', 'CXBLPF', 'CXPPMM', 'INPT0', 'INPT1', 'INPT2', 'INPT3', 'INPT4', 'INPT5']  # $00-$0D
RIOT = {0x280: 'SWCHA', 0x281: 'SWACNT', 0x282: 'SWCHB', 0x283: 'SWBCNT', 0x284: 'INTIM', 0x285: 'TIMINT',
        0x294: 'TIM1T', 0x295: 'TIM8T', 0x296: 'TIM64T', 0x297: 'T1024T'}
WRITERS = {'STA', 'STX', 'STY'}
RMW = {'INC', 'DEC', 'ASL', 'LSR', 'ROL', 'ROR'}
LINE = re.compile(r'^(\s+)([A-Z]{3})\s+(\(?)\$([0-9A-F]{2}|[0-9A-F]{4})((?:,[XY])?\)?(?:,Y)?)(\s*;.*)?$')


def name_for(addr, mnemonic):
    """(name, canonical address) of a chip register as seen by this instruction, or None."""
    write = mnemonic in WRITERS
    if 0x280 <= addr <= 0x29F:  # RIOT (A4 and A2 choose between ports, timer read and timer write)
        if (addr & 0x14) == 0x00:
            base = 0x280 | (addr & 3)  # SWCHA, SWACNT, SWCHB, SWBCNT
        elif write and (addr & 0x14) == 0x14:
            base = 0x294 | (addr & 3)  # TIM1T, TIM8T, TIM64T, T1024T
        elif not write and (addr & 0x14) == 0x04:
            base = 0x284 | (addr & 1)  # INTIM, TIMINT
        else:
            return None
        n = RIOT.get(base)
        return (n, base) if n else None
    if addr < 0x80:  # TIA
        if write:
            reg = addr & 0x3F
            return (TIA_WRITE[reg], reg) if reg < len(TIA_WRITE) else None
        reg = addr & 0x0F
        return (TIA_READ[reg], reg) if reg < len(TIA_READ) else None
    return None


def check_against_stella():
    """The three name tables above must equal Stella's own (vendored source, read only)."""
    text = (ROOT / 'third_party' / 'stella' / 'src' / 'debugger' / 'CartDebug.cxx').read_text(encoding='utf-8')

    def table(name):
        body = text[text.index(name):]
        body = body[:body.index('};')]
        return re.findall(r'"([^"]+)"', body)
    w, r, io = table('CartDebug::ourTIAMnemonicW'), table('CartDebug::ourTIAMnemonicR'), table('CartDebug::ourIOMnemonic')
    ok = (w[:len(TIA_WRITE)] == TIA_WRITE and r[:len(TIA_READ)] == TIA_READ and io[0:6] == ['SWCHA', 'SWACNT', 'SWCHB', 'SWBCNT', 'INTIM', 'TIMINT']
          and io[20:24] == ['TIM1T', 'TIM8T', 'TIM64T', 'T1024T'])
    return ok, len(TIA_WRITE), len(TIA_READ)


def main():
    same_names, nw, nr = check_against_stella()
    assert same_names, 'the name tables differ from the ones in Stella source'
    rom = pathlib.Path(sys.argv[1]).read_bytes()
    src = (OUT / 'pacman_raw.asm').read_text(encoding='utf-8').split('\n')
    out, equates = [], {}
    counts, unnamed, rmw_hits, kept_abs, sweeps = {}, {}, [], 0, 0
    for line in src:
        m = LINE.match(line)
        if not m:
            out.append(line)
            continue
        ind, mn, par, num, rest, cmt = m.groups()
        cmt = cmt or ''
        addr = int(num, 16)
        is_abs_form = len(num) == 4
        chip = addr < 0x80 or 0x280 <= addr <= 0x29F
        if not chip or mn in ('JMP', 'JSR'):
            out.append(line)
            continue
        if mn in RMW:
            rmw_hits.append((mn, addr))
            out.append(line)
            continue
        if is_abs_form and addr < 0x100:
            kept_abs += 1
            out.append(line)
            continue
        if addr == 0 and rest.startswith(',X') and not par:
            sweeps += 1
            out.append(line)
            continue
        if par:  # (zp),Y or (zp,X): pointers in the TIA range make no sense; leave
            out.append(line)
            continue
        got = name_for(addr, mn)
        if got is None:
            unnamed[(mn, addr)] = unnamed.get((mn, addr), 0) + 1
            out.append(line)
            continue
        name, canon = got
        if canon == addr:
            use = name
            equates[use] = addr
        else:
            use = '%s_AT%X' % (name, addr)
            equates[use] = addr
            equates.setdefault(name, canon)
        counts[name] = counts.get(name, 0) + 1
        # keep the comment columns: the text of the operand becomes the name
        text = '%s %s%s' % (mn, use, rest)
        out.append('%s%-18s%s' % (ind, text, cmt if cmt else ''))
    head = ['; Hardware names of the video chip (TIA) and the timer/input chip (RIOT): the standard names, one equate each.',
            '; A name with _AT<address> is a copy of the register at that address (the chips ignore some address bits).']
    for name, addr in sorted(equates.items(), key=lambda kv: (kv[1] < 0x100, kv[1], kv[0])):
        head.append('%-14s = $%04X' % (name, addr) if addr >= 0x100 else '%-14s = $%02X' % (name, addr))
    # the equates go before `.org`; write all after the two comment lines of the raw listing
    idx = next(i for i, l in enumerate(out) if l.strip().startswith('.org'))
    text = '\n'.join(out[:idx] + head + out[idx:]) + '\n'
    (OUT / 'pacman_named.asm').write_text(text, encoding='utf-8', newline='')
    back, labels = asm.assemble(text, ORG)
    same = back == rom
    (OUT / 'pacman_named_rebuilt.bin').write_bytes(back)
    stats = ['name tables (%d TIA write names, %d TIA read names, RIOT ports and timers) equal the tables of the Stella debugger source: yes' % (nw, nr),
             'operands given a hardware name: %d (%d different registers, %d equates)' % (sum(counts.values()), len(counts), len(equates)),
             'by name: ' + ', '.join('%s %d' % kv for kv in sorted(counts.items())),
             'left as numbers on purpose: %d sweeps with base $00 (STA $00,X), %d absolute-form addresses below $100' % (sweeps, kept_abs),
             'chip addresses without a standard name for that access (left as numbers): %s' % (', '.join('%s $%X x%d' % (a[0], a[1], n) for a, n in sorted(unnamed.items())) or 'none'),
             'read-modify-write instructions on chip addresses (left as numbers): %s' % (rmw_hits or 'none'),
             'ROM SHA-1:     %s' % hashlib.sha1(rom).hexdigest(), 'rebuilt SHA-1: %s' % hashlib.sha1(back).hexdigest(),
             'result: %s' % ('BYTE-IDENTICAL' if same else 'DIFFERENT')]
    (OUT / 'names_stats.txt').write_text('\n'.join(stats) + '\n', encoding='utf-8')
    print('\n'.join(stats))
    return 0 if same else 1


if __name__ == '__main__':
    sys.exit(main())
