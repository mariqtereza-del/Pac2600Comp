# Stage F18: the drawing kernel and the flicker schedule (2026-10-26)

`python -I tools\f18_kernel.py rom\pacman.a26` shows what the picture of a frame is made of and proves it against the real emulator. This document is written by that tool.

## Result: every check passed

| Count | Check |
| --- | --- |
| 8240 of 8240 | sampled frames (every 53rd frame of all 70 games): all of the roughly 547 writes per frame to the TIA chip of the picture (scanline, colour clock, instruction address, register, value) equal the emulator's, one by one |
| 101417 | frames in which the ghost flag is read (F12): the ghost drawn as player 0 is exactly number `FRAME_COUNTER & 3` + 1 (right: 101417, wrong: 0) |
| 101417 of 101417 | the ghost flag (set 665 times) is predicted from the two sprites' bitmaps and pixel columns alone (wrong: 0) |
| 141 | rows of sprite pixels in 8 pictures of the emulator that equal the predicted columns exactly (wrong: 0; 3 rows are not decisive because the same colour is also used elsewhere in the row) |

## What this proves

The picture is a pure function of the **128 memory bytes at `$F07F`** (the moment the top logic hands over to the drawing code) and the ROM. `tools\f18_cpu.py` is a small interpreter that runs the original bytes from `$F07F` to `$F4E7` over those 128 bytes; its list of TIA writes is the emulator's, write for write. So nothing else (no timer, no switch, no joystick, no earlier TIA state except the sprite mirror bit, below) influences what is drawn. The native renderer of F23 has to produce the same pixels from the same bytes; this list is its reference.

## The picture, line by line (fixed for every frame)

The code is cycle-counted: 76 processor cycles (228 colour clocks) per scanline, one `STA WSYNC` per line. **The set of registers written on each scanline from 34 to 213 is the same in every one of the 8240 sampled frames** (181 of 190 lines; the others are the status line at the bottom, where the row of life markers is missing when no marker is shown).

| Scanlines | What is drawn |
| --- | --- |
| 0-33 | vertical sync and blank; the top logic (until about line 14), positioning of the sprites (lines 15-19: `RESP`/`HMP`/`HMOVE`), then blank until the timer ends |
| 34 | `KERNEL_START`: video on, colours (background, maze), `CTRLPF` $31 (reflected playfield, ball 8 wide) |
| 35-194 | **8 maze rows of 20 scanlines**: row r starts at line 35 + 20 r (see the row template) |
| 195-198 | bottom wall of the maze (`PF` = $FF $FF $7F), sprites still drawn |
| 199-201 | switch to the score line: sprites repositioned for the score (`RESP0`, `RESP1`, `HMP0`, `HMOVE`), `NUSIZ` = 3 (three copies), vertical delay on both players |
| 202-213 | the **score digits** of the player in turn, 7 lines of 6 digits using the six-picture trick of `SCORE_DIGITS` (players 0 and 1 interleaved, three copies each, with vertical delay) |
| 214-223 | line 214 switches the sprites back to normal; 216 sets the colours of the marker line; 217-222 are the **life markers** (`CTRLPF` $02, `PF0/PF1` patterns, none when `LIVES` is 0); 223 ends the picture (`CTRLPF` 0, `COLUPF` 0) |

### The row template (20 scanlines)

Registers written on each line of a maze row, from the executor (graphics registers `GRP0`/`GRP1` are written on every line, alternately, and are left out of the table):

| Line in the row | Registers written (row 1) |
| --- | --- |
| 0 | PF0, PF1, PF2 |
| 1 | (only the graphics register) |
| 2 | (only the graphics register) |
| 3 | (only the graphics register) |
| 4 | PF0, PF1, PF2 |
| 5 | (only the graphics register) |
| 6 | ENAM0 |
| 7 | (only the graphics register) |
| 8 | (only the graphics register) |
| 9 | (only the graphics register) |
| 10 | (only the graphics register) |
| 11 | PF0, CTRLPF, PF1, PF2, PF0, PF1, PF2 |
| 12 | PF0, PF1, PF2, PF0, PF1, PF2 |
| 13 | PF0, PF1, CTRLPF, PF2 |
| 14 | ENAM1 |
| 15 | (only the graphics register) |
| 16 | (only the graphics register) |
| 17 | ENABL |
| 18 | ENAM0 |
| 19 | (only the graphics register) |

Differences to row 1: row 0: line 6 ENAM0+ENABL, line 9 ENAM1; row 2: line 6 ENAM0+ENABL, line 9 ENAM1; row 4: line 6 ENAM0+ENABL, line 9 ENAM1; row 6: line 6 ENAM0+ENABL, line 9 ENAM1; row 7: line 14 nothing, line 17 nothing, line 18 nothing.

What changes which lines (the executor run twice with one part of the memory changed; this is the evidence for the roles below):

| Change in memory | Scanlines that change | Registers |
| --- | --- | --- |
| wafer bytes `$88-$97` all 0 | 46-47, 66-67, 86-87, 106-107, 126-127, 146-147, 166-167, 186-187 | PF0, PF1, PF2 |
| `PILL_FLAGS` = 0 (no power pill) | 53, 173 | ENAM0 |
| `LIVES` = 5 | 217-222 | PF0, PF1 |
| score 123456 (player 1) | 207-213 | GRP0, GRP1 |
| bonus item on (`GAME_FLAGS` bit 1) | 121, 124 | ENABL, ENAM1 |

So:

- **The wafers are playfield dots**: only lines 11 and 12 of each maze row (46-47, 66-67, ... 186-187) depend on the wafer bytes, and only through `PF0/PF1/PF2` (written twice per line, left half and right half, with `CTRLPF` $30 so the right half is not a mirror image; line 13 restores $31). The walls are playfield too (lines 0 and 4 of a row and the sides), chosen from the tables in the ROM; none of the five tests above changed them.
- **Player 0 is the ghost of this frame**, **player 1 is Pac-Man**; every picture row of a sprite is two scanlines high (player 0 changes on odd lines, player 1 on even lines); graphics come from the ROM through the pointers in `$DD-$E0`.
- **Missile 0 draws the power pills**: one line (18 of a row) in the rows that start at lines 35 and 155 (rows 0 and 6), 2 = on.
- **Missile 1 and the ball together draw the bonus item**: lines 6 and 9 of the row that starts at line 115 (row 4), only while `GAME_FLAGS` bit 1 is set (kernel row counter 3, `$F316-$F320`).
- **The score** is lines 207-213 (7 lines of `GRP0/GRP1`); **the life markers** are lines 217-222 (`PF0/PF1`), one marker per life, none when `LIVES` is 0.

## The flicker schedule

- **Ghosts:** only **one** ghost is drawn per frame, as player 0: ghost number `(FRAME_COUNTER & 3) + 1` (checked in 101417 frames). Each ghost is therefore on the screen 1 frame in 4 (and its position is updated in that same frame, F13). The picture shows four ghosts only through the persistence of the television; the screenshots of a single frame show one.
- **Pac-Man** is drawn in every frame, as player 1. Its mouth direction uses the sprite mirror (`REFP1`, written in the bottom logic from `PACMAN_STATE` bit 0 and cleared by the score-line set-up): it is mirrored exactly when bit 0 of the state was set at the end of the previous frame (needed for 100% of the collision predictions).
- **Power pills** blink: missile 0 sits at one of two places depending on `FRAME_COUNTER` bit 3 (position code at `$F088-$F094`: its `RESM0` strobe falls at colour clock 216 for bit 3 = 0 and at 72 for bit 3 = 1), and the pill row/side chosen by the code at `$F2D2-$F2E5` follows the same bit, so each existing pill is visible for 8 frames and hidden for 8.
- **Bonus item:** missile 1 and the ball are positioned once at power-on (`RESBL`/`RESM1` at `$F040/$F042`, strobe clocks 30 and 3, fine move `HMBL` $80 and `HMM1` $F0) and enabled only in the row that starts at line 115 while `GAME_FLAGS` bit 1 is set.

## Where a sprite is on the screen

Horizontal: the position routine (`$F09C-$F0BF`) cuts the x byte into a coarse part (a delay loop of `3 + (x + 1) / 15` passes) and a fine part (the `HMP` nibble); `RESPn` is strobed at colour clock `c` (the clock at the end of the `STA RESPn,X` instruction) and `HMOVE` then shifts the sprite left by the signed `HMP` nibble. **The first graphics bit appears at pixel column `c - 68 + 5 - HMP` (mod 160)**; player 0 and player 1 use the same offset (checked by the collision flags: any other relative shift gives wrong flags).
Vertical: with `SPRITE0_Y`/`SPRITE1_Y` = object y + 11 (the bytes `$D8/$D9`), the first scanline with graphics is **2 x `SPRITE0_Y` + 17 for the ghost (player 0) and 2 x `SPRITE1_Y` + 18 for Pac-Man (player 1)**, i.e. 2 y + 39 and 2 y + 40 from the game y; this holds in 1065 of 1154 play frames of three games for player 0 and 1096 of 1154 for player 1 (the rest start 2 or more scanlines later: the first graphics byte is empty, or the sprite is in an opening animation). Each graphics byte takes 2 scanlines; `SPRITE0_LINES`/`SPRITE1_LINES` start at 7 and 6 and count down to -1, so a ghost has 8 bytes and Pac-Man 7.

## Collisions follow from the picture

With the two sprites' bitmaps and columns as above, the chip's flag "player 0 touched player 1" (the ghost flag of F12) is predicted for every frame in which the game reads it: 101417 of 101417 (665 set). The native renderer of F23 therefore does not need a separate collision routine: it only has to report overlapping pixels of the two sprites in the maze lines 34-198 (the score digits never overlap, as the flags show).

## Not claimed

- Colours and the full pixel-for-pixel picture (playfield bit patterns, wafers, walls, score digits, markers) are not rebuilt here; the executor gives their TIA writes exactly and F23 turns them into pixels and compares with the emulator picture by picture.
- The pixel columns of missile 0, missile 1 and the ball are given as strobe clocks, not as columns.
- The table of which tile/wall patterns each maze byte selects is read from the code and the executor, not listed in full here.
