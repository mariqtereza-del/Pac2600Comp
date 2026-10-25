"""A small 6502 interpreter for the drawing kernel (Stage F18).

It runs the original program bytes of the ROM (read only) over a copy of the 128 memory bytes and records every write to the TIA with the
scanline and colour clock at which the instruction ended, exactly as `stella_coverage ... tiaw=` logs them from the real emulator.
It knows the official instructions the kernel uses (decimal mode and interrupts are not needed) and counts the cycles of the table of
tools\\f1_6502.py plus one for a page crossing of an indexed read and one or two for a taken branch.
The scanline clock: 76 cycles of 3 colour clocks per line; a write to WSYNC holds the processor until the start of the next line.
"""
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import f1_6502 as asm  # noqa: E402

READS_INDEXED = {'lda', 'ldx', 'ldy', 'adc', 'sbc', 'and', 'ora', 'eor', 'cmp'}


class Machine:
    def __init__(self, rom, ram):
        self.rom = rom
        self.ram = bytearray(ram)
        self.a = self.x = self.y = 0
        self.sp = 0xFF
        self.n = self.v = self.z = self.c = 0
        self.pc = 0xF000
        self.line = 0
        self.cyc = 0            # cycles used in the current line
        self.writes = []        # (line, clock, pc, register, value)
        self.hooks = {}

    # ---- memory ----
    def rd(self, addr):
        addr &= 0xFFFF
        if addr & 0x1000:
            return self.rom[addr & 0x0FFF]
        if addr & 0x80:
            return self.ram[addr & 0x7F]
        return 0                # TIA / timer reads: not used by the kernel

    def wr(self, addr, val, pc):
        addr &= 0xFFFF
        val &= 0xFF
        if addr & 0x1000:
            return
        if addr & 0x80:
            self.ram[addr & 0x7F] = val
            return
        reg = addr & 0x3F
        if reg == 0x02:
            self._wsync = True
            return
        self.writes.append((self.line, None, pc, reg, val))   # clock filled in after the instruction
        self._pending = len(self.writes) - 1

    def flags(self, v):
        v &= 0xFF
        self.z = int(v == 0)
        self.n = v >> 7
        return v

    def tick(self, cycles):
        self.cyc += cycles
        while self.cyc >= 76:
            self.cyc -= 76
            self.line += 1

    # ---- one instruction ----
    def step(self):
        pc = self.pc
        op = self.rd(pc)
        ent = asm.TABLE[op]
        if not ent or not ent[4]:
            raise RuntimeError('unknown opcode %02X at %04X' % (op, pc))
        mn, mode, cycles, length, _ = ent
        b1 = self.rd(pc + 1)
        b2 = self.rd(pc + 2)
        self._wsync = False
        self._pending = None
        extra = 0
        ea = None
        if mode == 'ZERO_PAGE':
            ea = b1
        elif mode == 'ZERO_PAGE_X':
            ea = (b1 + self.x) & 0xFF
        elif mode == 'ZERO_PAGE_Y':
            ea = (b1 + self.y) & 0xFF
        elif mode == 'ABSOLUTE':
            ea = b1 | (b2 << 8)
        elif mode == 'ABSOLUTE_X':
            base = b1 | (b2 << 8)
            ea = (base + self.x) & 0xFFFF
            if mn in READS_INDEXED and (base & 0xFF00) != (ea & 0xFF00):
                extra = 1
        elif mode == 'ABSOLUTE_Y':
            base = b1 | (b2 << 8)
            ea = (base + self.y) & 0xFFFF
            if mn in READS_INDEXED and (base & 0xFF00) != (ea & 0xFF00):
                extra = 1
        elif mode == 'INDIRECT_Y':
            base = self.rd(b1) | (self.rd((b1 + 1) & 0xFF) << 8)
            ea = (base + self.y) & 0xFFFF
            if mn in READS_INDEXED and (base & 0xFF00) != (ea & 0xFF00):
                extra = 1
        elif mode == 'INDIRECT_X':
            p = (b1 + self.x) & 0xFF
            ea = self.rd(p) | (self.rd((p + 1) & 0xFF) << 8)
        npc = pc + length
        if mode == 'IMMEDIATE':
            val = b1
        elif mode in ('ACCUMULATOR', 'IMPLIED', 'RELATIVE'):
            val = None
        else:
            val = None            # read lazily
        def m():
            return b1 if mode == 'IMMEDIATE' else self.rd(ea)
        if mn == 'lda':
            self.a = self.flags(m())
        elif mn == 'ldx':
            self.x = self.flags(m())
        elif mn == 'ldy':
            self.y = self.flags(m())
        elif mn == 'sta':
            self.wr(ea, self.a, pc)
        elif mn == 'stx':
            self.wr(ea, self.x, pc)
        elif mn == 'sty':
            self.wr(ea, self.y, pc)
        elif mn in ('tax', 'tay', 'txa', 'tya', 'tsx', 'txs'):
            if mn == 'tax':
                self.x = self.flags(self.a)
            elif mn == 'tay':
                self.y = self.flags(self.a)
            elif mn == 'txa':
                self.a = self.flags(self.x)
            elif mn == 'tya':
                self.a = self.flags(self.y)
            elif mn == 'tsx':
                self.x = self.flags(self.sp)
            else:
                self.sp = self.x
        elif mn == 'inx':
            self.x = self.flags(self.x + 1)
        elif mn == 'iny':
            self.y = self.flags(self.y + 1)
        elif mn == 'dex':
            self.x = self.flags(self.x - 1)
        elif mn == 'dey':
            self.y = self.flags(self.y - 1)
        elif mn in ('inc', 'dec'):
            v = self.flags(self.rd(ea) + (1 if mn == 'inc' else -1))
            self.wr(ea, v, pc)
        elif mn == 'and':
            self.a = self.flags(self.a & m())
        elif mn == 'ora':
            self.a = self.flags(self.a | m())
        elif mn == 'eor':
            self.a = self.flags(self.a ^ m())
        elif mn == 'bit':
            v = m()
            self.z = int((self.a & v) == 0)
            self.n = v >> 7
            self.v = (v >> 6) & 1
        elif mn in ('cmp', 'cpx', 'cpy'):
            r = {'cmp': self.a, 'cpx': self.x, 'cpy': self.y}[mn]
            v = m()
            t = r - v
            self.c = int(r >= v)
            self.flags(t)
        elif mn == 'adc':
            v = m()
            t = self.a + v + self.c
            self.v = int(((self.a ^ t) & (v ^ t) & 0x80) != 0)
            self.c = int(t > 0xFF)
            self.a = self.flags(t)
        elif mn == 'sbc':
            v = m() ^ 0xFF
            t = self.a + v + self.c
            self.v = int(((self.a ^ t) & (v ^ t) & 0x80) != 0)
            self.c = int(t > 0xFF)
            self.a = self.flags(t)
        elif mn in ('asl', 'lsr', 'rol', 'ror'):
            v = self.a if mode == 'ACCUMULATOR' else self.rd(ea)
            if mn == 'asl':
                self.c, r = v >> 7, (v << 1) & 0xFF
            elif mn == 'lsr':
                self.c, r = v & 1, v >> 1
            elif mn == 'rol':
                self.c, r = v >> 7, ((v << 1) | self.c) & 0xFF
            else:
                self.c, r = v & 1, (v >> 1) | (self.c << 7)
            r = self.flags(r)
            if mode == 'ACCUMULATOR':
                self.a = r
            else:
                self.wr(ea, r, pc)
        elif mn == 'clc':
            self.c = 0
        elif mn == 'sec':
            self.c = 1
        elif mn in ('nop', 'cld', 'sei', 'cli', 'clv'):
            pass
        elif mn == 'php':
            self.wr(0x100 + self.sp, self.n << 7 | self.v << 6 | 0x30 | self.z << 1 | self.c, pc)
            self.sp = (self.sp - 1) & 0xFF
        elif mn == 'plp':
            self.sp = (self.sp + 1) & 0xFF
            p = self.rd(0x100 + self.sp)
            self.n, self.v, self.z, self.c = p >> 7, (p >> 6) & 1, (p >> 1) & 1, p & 1
        elif mn == 'pha':
            self.wr(0x100 + self.sp, self.a, pc)
            self.sp = (self.sp - 1) & 0xFF
        elif mn == 'pla':
            self.sp = (self.sp + 1) & 0xFF
            self.a = self.flags(self.rd(0x100 + self.sp))
        elif mn == 'jmp':
            npc = (b1 | (b2 << 8)) if mode == 'ABSOLUTE' else self.rd(b1 | (b2 << 8)) | (self.rd((b1 | (b2 << 8)) + 1) << 8)
        elif mn == 'jsr':
            ret = pc + 2
            self.wr(0x100 + self.sp, ret >> 8, pc)
            self.sp = (self.sp - 1) & 0xFF
            self.wr(0x100 + self.sp, ret & 0xFF, pc)
            self.sp = (self.sp - 1) & 0xFF
            npc = b1 | (b2 << 8)
        elif mn == 'rts':
            self.sp = (self.sp + 1) & 0xFF
            lo = self.rd(0x100 + self.sp)
            self.sp = (self.sp + 1) & 0xFF
            hi = self.rd(0x100 + self.sp)
            npc = ((hi << 8) | lo) + 1
        elif mode == 'RELATIVE':
            cond = {'bne': not self.z, 'beq': self.z, 'bpl': not self.n, 'bmi': self.n, 'bcc': not self.c, 'bcs': self.c,
                    'bvc': not self.v, 'bvs': self.v}[mn]
            if cond:
                off = b1 - 256 if b1 >= 128 else b1
                tgt = (pc + 2 + off) & 0xFFFF
                extra = 1 + (1 if (tgt & 0xFF00) != ((pc + 2) & 0xFF00) else 0)
                npc = tgt
        else:
            raise RuntimeError('instruction %s not implemented at %04X' % (mn, pc))
        self.tick(cycles + extra)
        if self._pending is not None:
            i = self._pending
            ln, _, p, reg, val = self.writes[i]
            self.writes[i] = (self.line, 3 * self.cyc, p, reg, val)
        if self._wsync:
            if self.cyc != 0:
                self.line += 1
                self.cyc = 0
        self.pc = npc & 0xFFFF
        return pc

    def run(self, start, stop, line, max_steps=200000):
        """Run from `start` (line number `line` before the first instruction) until pc == stop."""
        self.pc, self.line, self.cyc = start, line, 0
        for _ in range(max_steps):
            if self.pc == stop:
                return
            if self.pc == 0xF0C5:            # VBLANK_WAIT: the timer wait ends at line 33 (checked against the real console)
                self.pc, self.line, self.cyc = 0xF0CA, 33, 0
            self.step()
        raise RuntimeError('did not stop')
