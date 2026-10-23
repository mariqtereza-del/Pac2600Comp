"""Stage A4: quick, read-only structural look at a 4 KB Atari 2600 ROM.

Usage:  python -I tools\\a4_structure.py rom\\pacman.a26

Reads the file only. Prints: size, the three 6502 vectors, bank-switching
signs, and a rough code/data map made by following the program from its
start address (a "recursive descent" trace). The map is an estimate:
jumps through computed addresses cannot be followed, so some real code may
show up as "not reached". Stages F2-F6 produce the exact answer.
"""
import hashlib
import sys

# Official 6502 opcodes: opcode -> (mnemonic, addressing mode).
MODES_LEN = {"imp": 1, "acc": 1, "imm": 2, "zp": 2, "zpx": 2, "zpy": 2,
             "izx": 2, "izy": 2, "rel": 2, "abs": 3, "abx": 3, "aby": 3, "ind": 3}
GROUP8 = ("imm", "zp", "zpx", "abs", "abx", "aby", "izx", "izy")
OPS = {}
def add(mn, pairs):
    for code, mode in pairs:
        OPS[code] = (mn, mode)
for mn, codes in {"ADC": (0x69, 0x65, 0x75, 0x6D, 0x7D, 0x79, 0x61, 0x71),
                  "AND": (0x29, 0x25, 0x35, 0x2D, 0x3D, 0x39, 0x21, 0x31),
                  "CMP": (0xC9, 0xC5, 0xD5, 0xCD, 0xDD, 0xD9, 0xC1, 0xD1),
                  "EOR": (0x49, 0x45, 0x55, 0x4D, 0x5D, 0x59, 0x41, 0x51),
                  "LDA": (0xA9, 0xA5, 0xB5, 0xAD, 0xBD, 0xB9, 0xA1, 0xB1),
                  "ORA": (0x09, 0x05, 0x15, 0x0D, 0x1D, 0x19, 0x01, 0x11),
                  "SBC": (0xE9, 0xE5, 0xF5, 0xED, 0xFD, 0xF9, 0xE1, 0xF1)}.items():
    add(mn, zip(codes, GROUP8))
add("STA", [(0x85, "zp"), (0x95, "zpx"), (0x8D, "abs"), (0x9D, "abx"), (0x99, "aby"), (0x81, "izx"), (0x91, "izy")])
for mn, codes in {"ASL": (0x0A, 0x06, 0x16, 0x0E, 0x1E), "LSR": (0x4A, 0x46, 0x56, 0x4E, 0x5E),
                  "ROL": (0x2A, 0x26, 0x36, 0x2E, 0x3E), "ROR": (0x6A, 0x66, 0x76, 0x6E, 0x7E)}.items():
    add(mn, zip(codes, ("acc", "zp", "zpx", "abs", "abx")))
add("DEC", [(0xC6, "zp"), (0xD6, "zpx"), (0xCE, "abs"), (0xDE, "abx")])
add("INC", [(0xE6, "zp"), (0xF6, "zpx"), (0xEE, "abs"), (0xFE, "abx")])
add("LDX", [(0xA2, "imm"), (0xA6, "zp"), (0xB6, "zpy"), (0xAE, "abs"), (0xBE, "aby")])
add("LDY", [(0xA0, "imm"), (0xA4, "zp"), (0xB4, "zpx"), (0xAC, "abs"), (0xBC, "abx")])
add("STX", [(0x86, "zp"), (0x96, "zpy"), (0x8E, "abs")])
add("STY", [(0x84, "zp"), (0x94, "zpx"), (0x8C, "abs")])
add("CPX", [(0xE0, "imm"), (0xE4, "zp"), (0xEC, "abs")])
add("CPY", [(0xC0, "imm"), (0xC4, "zp"), (0xCC, "abs")])
add("BIT", [(0x24, "zp"), (0x2C, "abs")])
add("JMP", [(0x4C, "abs"), (0x6C, "ind")])
add("JSR", [(0x20, "abs")])
for mn, code in {"BPL": 0x10, "BMI": 0x30, "BVC": 0x50, "BVS": 0x70,
                 "BCC": 0x90, "BCS": 0xB0, "BNE": 0xD0, "BEQ": 0xF0}.items():
    OPS[code] = (mn, "rel")
for mn, code in {"BRK": 0x00, "CLC": 0x18, "CLD": 0xD8, "CLI": 0x58, "CLV": 0xB8, "DEX": 0xCA,
                 "DEY": 0x88, "INX": 0xE8, "INY": 0xC8, "NOP": 0xEA, "PHA": 0x48, "PHP": 0x08,
                 "PLA": 0x68, "PLP": 0x28, "RTI": 0x40, "RTS": 0x60, "SEC": 0x38, "SED": 0xF8,
                 "SEI": 0x78, "TAX": 0xAA, "TAY": 0xA8, "TSX": 0xBA, "TXA": 0x8A, "TXS": 0x9A,
                 "TYA": 0x98}.items():
    OPS[code] = (mn, "imp")
assert len(OPS) == 151, len(OPS)

WRITES = {"STA", "STX", "STY", "INC", "DEC", "ASL", "LSR", "ROL", "ROR"}
BASE = 0xF000  # where a 4 KB cartridge is normally listed (A12 set, mirrored)


def main(path):
    rom = open(path, "rb").read()
    print(f"File: {path}")
    print(f"Size: {len(rom)} bytes   SHA-1: {hashlib.sha1(rom).hexdigest()}")
    if len(rom) != 4096:
        print("STOP: not 4096 bytes; this script only handles plain 4 KB images.")
        return 1

    # 1. Vectors (last 6 bytes, little-endian): NMI, RESET, IRQ/BRK.
    print("\n== 1. Last 6 bytes (vectors) ==")
    print("Raw bytes at $FFFA-$FFFF:", " ".join(f"{b:02X}" for b in rom[-6:]))
    vec = {}
    for name, off in (("NMI", 0xFFA), ("RESET (start)", 0xFFC), ("IRQ/BRK", 0xFFE)):
        v = rom[off] | rom[off + 1] << 8
        vec[name] = v
        inside = "points into the cartridge" if v & 0x1000 else "does NOT point into the cartridge"
        print(f"{name:14} ${v:04X}  (file offset ${v & 0x0FFF:03X}, {inside})")

    def off(addr):
        return addr & 0x0FFF if addr & 0x1000 else None

    # 2. Follow the program from RESET (and IRQ/BRK if it points into ROM).
    code = [False] * 4096
    starts = set()
    reasons_stop = []
    rom_reads, rom_writes, indirect_jumps, unknown_ops = {}, {}, [], []
    zp_writes = {}
    work = [(vec["RESET (start)"], "RESET")]
    if vec["IRQ/BRK"] & 0x1000:
        work.append((vec["IRQ/BRK"], "IRQ/BRK"))
    jsr_targets = set()
    while work:
        addr, why = work.pop()
        o = off(addr)
        if o is None:
            reasons_stop.append(f"${addr:04X} ({why}) is outside the cartridge")
            continue
        while True:
            if code[o]:
                break
            op = rom[o]
            if op not in OPS:
                unknown_ops.append((BASE + o, op))
                break
            mn, mode = OPS[op]
            n = MODES_LEN[mode]
            if o + n > 4096:
                break
            for k in range(n):
                code[o + k] = True
            starts.add(o)
            arg = rom[o + 1] if n >= 2 else None
            if n == 3:
                arg = rom[o + 1] | rom[o + 2] << 8
            here = BASE + o
            if mode in ("abs", "abx", "aby") and mn not in ("JMP", "JSR"):
                if arg & 0x1000:
                    target = rom_writes if mn in WRITES else rom_reads
                    target.setdefault(arg & 0x1FFF, []).append(here)
            if mode in ("zp", "zpx") and mn in WRITES:
                zp_writes[arg] = zp_writes.get(arg, 0) + 1
            if mode == "rel":
                dest = (here + 2 + (arg - 256 if arg > 127 else arg)) & 0xFFFF
                work.append((dest, f"branch from ${here:04X}"))
            elif mn == "JSR":
                jsr_targets.add(arg)
                work.append((arg, f"JSR from ${here:04X}"))
            elif mn == "JMP" and mode == "abs":
                work.append((arg, f"JMP from ${here:04X}"))
                break
            elif mn == "JMP":
                indirect_jumps.append((here, arg))
                break
            elif mn in ("RTS", "RTI", "BRK"):
                break
            o += n
            if o >= 4096:
                break

    print("\n== 2. Bank-switching signs ==")
    print("File size 4096 bytes = 4 KB, the largest size a 2600 can see without bank switching.")
    hot = {a: v for a, v in {**rom_reads, **rom_writes}.items() if 0x1FE0 <= a <= 0x1FFB}
    print("Traced code touching known switching addresses ($1FE0-$1FFB):",
          "none" if not hot else ", ".join(f"${a:04X} from {', '.join(f'${x:04X}' for x in v)}" for a, v in sorted(hot.items())))
    print("Traced code writing anywhere in cartridge space (needed for extra cartridge RAM or switching):",
          "none" if not rom_writes else ", ".join(f"${a:04X}" for a in sorted(rom_writes)))
    sc = rom[0:256]
    print(f"Superchip pattern (first 256 bytes all one value, as in images of cartridges with extra RAM): "
          f"{'YES' if len(set(sc)) == 1 else 'no'} ({len(set(sc))} different values)")
    print(f"Writes to $3F ('3F' switching scheme) in traced code: {zp_writes.get(0x3F, 0)}")

    print("\n== 3. Code versus data (estimate) ==")
    n_code = sum(code)
    print(f"Bytes reached as code: {n_code} of 4096 ({100 * n_code / 4096:.1f}%)")
    print(f"Instructions found: {len(starts)};  subroutines called with JSR: {len(jsr_targets)}")
    print(f"Indirect jumps (cannot be followed): "
          + ("none" if not indirect_jumps else ", ".join(f"${h:04X} JMP (${a:04X})" for h, a in indirect_jumps)))
    print("Stopped at unknown (non-official) opcodes: "
          + ("none" if not unknown_ops else ", ".join(f"${a:04X}=${b:02X}" for a, b in unknown_ops)))
    for r in reasons_stop:
        print("Note:", r)
    rts = sum(1 for o in starts if rom[o] == 0x60)
    print(f"RTS instructions in reached code: {rts}  (many RTS with few JSR can mean jump tables via RTS)")
    brk = sum(1 for o in starts if rom[o] == 0x00)
    print(f"BRK instructions in reached code: {brk}")
    best, val, o = 0, 0, 0
    while o < 4096:
        e = o
        while e < 4096 and rom[e] == rom[o]:
            e += 1
        if e - o > best:
            best, val = e - o, rom[o]
        o = e
    print(f"Longest run of one repeated byte anywhere in the ROM: {best} bytes of ${val:02X}")

    print("\nRegions (address range, length, kind):")
    o = 0
    while o < 4096:
        kind = code[o]
        e = o
        while e < 4096 and code[e] == kind:
            e += 1
        seg = rom[o:e]
        label = "CODE" if kind else "not reached (data or unfollowed code)"
        extra = ""
        if not kind and len(set(seg)) == 1:
            extra = f"  [all bytes ${seg[0]:02X}: likely unused filler]"
        print(f"  ${BASE + o:04X}-${BASE + e - 1:04X}  {e - o:5d}  {label}{extra}")
        o = e

    print("\nData tables read by code (address in cartridge, number of reading instructions):")
    for a in sorted(rom_reads):
        print(f"  ${0xE000 | a:04X}  read by {len(rom_reads[a])}")

    print("\nMost-written hardware/RAM addresses (zero page; $00-$7F = TIA chip, $80-$FF = the 128 bytes of RAM):")
    for a, c in sorted(zp_writes.items(), key=lambda t: -t[1])[:12]:
        print(f"  ${a:02X}: {c} write instructions")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1]))
