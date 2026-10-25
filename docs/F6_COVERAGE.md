# Stage F6: what actually runs (2026-10-26)

`python -I tools\f6_coverage.py rom\pacman.a26` replays **all 70** baseline entries (439,680 frames; BL-065/066 with their test-only memory edits) one instruction at a time. Every run equals its golden trace on every frame (picture and all 128 memory bytes): 70 lines with 0 differences. The map is `reference\f6\coverage_all.txt` and the run counts `reference\f6\run_counts.csv` (both local only).

| Class | Meaning | Bytes |
| --- | --- | --- |
| C | first byte of an instruction that ran | 1,807 |
| c | its other bytes | 1,733 |
| D | read as data (a real fetch by a load/compare/etc.), never executed | 509 |
| n | touched only by the processor's look-ahead at the byte after an instruction | 3 |
| . | never executed and never read | 44 |
| | total | 4,096 |

Checks: every executed first byte is an official opcode; no instruction starts inside another; nothing ran outside the ROM; no executed byte is also fetched as data (**problems: none**).

## What changed against F1 (29 recordings)

Nothing new ran: the 41 other recordings add no instruction. Only three bytes changed class, `$F5C2`, `$F832` and `$F907`, from D to n. The processor looks at the byte after every "implied" instruction (and a branch not taken) and the measuring core records that look; F1 counted it as a data read. F6 counts a read as data only when the instruction really fetches data (modes other than implied, accumulator, relative, immediate). The 509 D bytes are the table area `$FD3F-$FF42` and `$FFE9-$FFF8`.

## The 44 bytes nothing touched

- **Code never run (4 pieces, 12 bytes):** `$FACF-$FAD1` (`20 36 FB`, a JSR), `$FAF9-$FAFB` (`20 5C FB`, a JSR), `$FB2A-$FB2C` and `$FB33-$FB35` (`68 68 60`, PLA PLA RTS). They sit among code that runs, so they are instructions that none of the 70 recordings reaches. F8 (routine list) says whether they are reachable at all.
- **Single bytes next to code:** `$F5C3` (`50`) and `$F908` (`BF`), right after the look-ahead bytes `$F5C2` and `$F907`.
- **In the data area (21 bytes):** `$FD60`, `$FD70`, `$FD78`, `$FD85`, `$FE76-$FE78`, `$FEF4-$FEF5`, `$FF13-$FF18` (`06 05 04 03 02 01`), `$FF23-$FF24`, `$FF27-$FF2A`, `$FF3A-$FF3B`: holes inside tables whose neighbours are read.
- **The end of the ROM:** `$FFF9-$FFFF` (7 bytes: `00 00 00 00 F0 00 00`, the vectors' tail; the processor's reset read of the vectors is not logged as an instruction read).

## Rarely run instructions

Over all entries only ten instructions ran 5 times or fewer: `$F659`, `$F65B` and `$FA59` (twice each), `$F648-$F650` (five instructions, four times each) and `$F8ED`, `$F8F9` (five times). These are the best places to read first for unusual game situations.

## Not claimed

"Never run" means never run by these 70 recordings (play, random input, long bot games, edge cases). It does not prove the code cannot run in some other situation. What the code means is the job of F7 onward.
