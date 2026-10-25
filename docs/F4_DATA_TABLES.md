# Stage F4: code versus data (2026-10-26)

> **Correction in Stage F9 (2026-10-26):** `$F5C2-$F5C3`, `$F832` and `$F907-$F908` are code, not data. The tables are now **539 data bytes in 61 segments** (TBL 263 / 34, UNREAD 24 / 11); see `docs/F9_INPUT.md`. The text below is the original F4 result.


`python -I tools\f4_tables.py rom\pacman.a26` (run after `tools\f2_disasm.py` and `tools\f3_names.py`) makes `reference\disasm\pacman_tables.asm` (**local only**) and `reference\disasm\tables_stats.txt` (the full segment table), assembles the listing and requires it to equal the ROM.

| Kind | Bytes | Segments | How it was decided |
| --- | --- | --- | --- |
| GFX | 208 | 5 | read by an instruction whose value ends in `GRP0` or `GRP1` (sprite shapes) |
| COLOR | 20 | 2 | ends in a colour register |
| PF | 10 | 6 | ends in `PF0`, `PF1` or `PF2` |
| SND | 8 | 2 | ends in a sound register (`AUDF0`, `AUDF1`) |
| TBL | 266 | 37 | read by the program but not stored straight into a chip register; meaning for later stages |
| VECTORS | 6 | 1 | `$FFFA-$FFFF`: NMI `$0000`, RESET `$F000`, IRQ/BRK `$0000` |
| UNREAD | 26 | 13 | never read in the 29 recorded games |
| Total | 544 | 66 | |

The big blocks: GFX `$FD8B-$FDF9` (shapes read through the pointers kept in RAM), `$FE89-$FEF1` (read through five pointers), TBL `$FE13-$FE62` (80 bytes read by nine instructions), COLOR `$FF2B-$FF40`, SND `$FFF1-$FFF8`.

The listing is byte-identical to the ROM (SHA-1 `0940fea7f04cdb6d4b90c5ad1a7e344e68f6dbb1`). The kind says how the bytes are used, not what they mean in the game; the cut between shapes inside a GFX block and the meaning of the TBL segments come with the routines.
