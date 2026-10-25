# Phase F tools (Stage F1, 2026-10-26)

Three tools and one map. None needs anything downloaded. Decisions: `DECISIONS.md` T-F1-1, T-F1-2.

## 1. `tools\f1_6502.py` — disassembler and assembler

```
python -I tools\f1_6502.py dis rom\pacman.a26 [org]     linear listing of the whole file to the screen (default start $F000)
python -I tools\f1_6502.py selftest rom\pacman.a26      the 8 tests
```

As a library (`import f1_6502`): `disassemble(bytes, org)` gives `(address, bytes, text)` per instruction; `assemble(text, org)` returns `(bytes, labels)`. Syntax: `LDA #$10`, `LDA $80` (zero page), `LDA $0080` (four hex digits: absolute even below $100), `LDA ($80),Y`, `JMP ($1234)`, `ASL A`, `BNE label`, `label:`, `.org`, `.byte`, `.word`. The opcode table comes from Stella's own source (never edited); illegal opcodes and cut-off instructions are written `.byte`. A linear listing treats data as code, so it contains wrong-looking instructions in the data area; it still assembles back to the same bytes, which is the point (F2 separates the two using the map below).

Tests (all pass): table of 256 with 151 official; eleven opcodes known from the data sheet; all 151 opcodes survive disassemble-then-assemble; zero-page versus absolute by written width; labels and branches; five wrong lines refused; **the whole ROM disassembled and assembled is byte-identical** (2,190 lines, 127 bytes as `.byte`); the same with a label on every line.

## 2. `build\stella_probe\stella_coverage.exe` — what the CPU runs

```
build\stella_probe\stella_coverage.exe <rom> <inputs file> <out csv> [<trace file>]
```

Runs a recording one instruction at a time on the Stella core and records every ROM address executed and every ROM address read as data. With the trace file it checks every frame (picture and the 128 memory bytes) against the golden trace; any difference makes the exit code 5. Built by `tools\build_stella_probe.cmd` (it is now one of its targets).

## 3. `tools\f1_coverage.py` — the code/data map

```
python -I tools\f1_coverage.py rom\pacman.a26
```

Runs the coverage tool on 29 baseline recordings (start-up, selections, movement, ghosts, scoring, dying, long games, edge cases, two players, random play), 365,780 frames, every run equal to its golden trace, and writes `reference\f1\coverage_union.txt` (local): per ROM offset `C` (first byte of an executed instruction), `c` (its other bytes), `D` (read as data only), `.` (neither).

| | Bytes |
| --- | --- |
| First bytes of executed instructions (C) | 1,807 |
| Other bytes of executed instructions (c) | 1,733 |
| Read as data, never executed (D) | 512 |
| Neither (.) | 44 |

Checks: every executed first byte is an official opcode; no executed instruction starts inside another; none was executed outside the ROM: **no problems**.

## What the map does not prove

Code that runs only in a situation that no recording reaches would show as D or `.`; the 44 untouched bytes (7 at the very end, four 3-byte pieces in the code area, 2 single bytes, 23 in the data table) are listed in the map. F2 uses the map as the starting point, not as the last word.
