# Stage F2: the raw disassembly (2026-10-26)

`python -I tools\f2_disasm.py rom\pacman.a26` makes the listing `reference\disasm\pacman_raw.asm` (**local only**: it is the original program in readable form and is never put in the public repository, Rule 7), assembles it with our own assembler (`tools\f1_6502.py`) and requires the rebuilt bytes to equal the ROM. Statistics are in `reference\disasm\stats.txt`.

| Item | Result |
| --- | --- |
| Listing lines | 2,184 |
| Instructions | 1,815 (8 of them never run in the 29 recorded games) |
| Data bytes | 544 |
| Labels | 279 (242 for code: `L_Fxxx`; 37 for data: `D_Fxxx`) |
| References through the mirror $1000-$1FFF | 0 |
| Rebuilt ROM | 4,096 bytes, **byte-identical** (SHA-1 `0940fea7f04cdb6d4b90c5ad1a7e344e68f6dbb1`, SHA-256 equal too) |
| The rebuilt ROM against the 70 golden traces | 70 of 70 equal (one single unexplained mismatch in a first run, not reproduced in 72 repeats: `DECISIONS.md` T-F2-2) |

The listing has no names and no comments yet: addresses of the chips (F3), routines (F4 onward) and meanings come in later stages, each of which must keep the listing rebuilding. Every code/data decision comes from the map of F1; the eight never-run instructions were recognised because they decode exactly into official instructions between executed code.
