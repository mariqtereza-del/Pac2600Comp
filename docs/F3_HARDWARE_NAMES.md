# Stage F3: hardware names (2026-10-26)

`python -I tools\f3_names.py rom\pacman.a26` turns the raw listing of F2 (`reference\disasm\pacman_raw.asm`) into `reference\disasm\pacman_named.asm` (**local only**) by naming the video chip (TIA) and timer/input chip (RIOT) operands, assembles it with `tools\f1_6502.py` and requires the result to equal the ROM.

| Item | Result |
| --- | --- |
| Operands named | 178, in 47 different registers, with 51 equates (a name plus its address) |
| Most used | WSYNC 28, PF1 15, PF2 15, GRP0 13, PF0 13, GRP1 9, AUDV0 5, CTRLPF 5 |
| Chip copies (`NAME_AT<address>`) | the collision/input read registers at `$31 $33 $37 $3C` |
| Left as numbers | `STA $00,X` (a sweep over the whole page, not a register) |
| Chip addresses with no standard name | none |
| Name tables | equal to Stella's own debugger tables (45 write names, 14 read names, RIOT ports and timers) |
| Rebuilt ROM | byte-identical (SHA-1 `0940fea7f04cdb6d4b90c5ad1a7e344e68f6dbb1`) |

Rules: a store (`STA STX STY`) writes, any other instruction reads, and a register has one name for writing and another for reading (address `$08` is COLUPF written and INPT0 read). The assembler got equates (`NAME = value`) for this; 9 self-tests pass.

The names tell what the chip does, not what the game does: that comes with the routines (F5 onward).
