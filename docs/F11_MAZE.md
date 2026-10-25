# Stage F11: the maze and the storage of the wafers (2026-10-26)

`python -I tools\f11_maze.py rom\pacman.a26` reads the wall/wafer table of the ROM, works out which memory bit stands for which wafer, and checks a model of the wafer rules against the recorded games. This document is written by that tool.

## Result

- **202502 of 202502 predicted frames exactly right (0 wrong)** over the 70 games: the 16 wafer bytes `$88-$97`, the power-pill bits of `PILL_FLAGS`, the score of the player whose turn it is, and on a cleared map the new wafer bytes, lives and level step.
- Events in the predicted frames: new map 20, none 198257, pill 102, wafer 4123.
- Frames not predicted: Reset pressed during play: 770; Select pressed during play: 5; bonus item appears, goes or is eaten: 86; death: 305; frozen game: 82718; memory edited by the test (BL-065/066): 2; pause after a ghost was eaten: 6176; title screen / not in play: 147046.
- The table gives **128 wafer places, each used by exactly one cell** (checked); the new map starts with **126** of them set (the two places in the middle column, `$8E` bit 7 and `$8A` bit 0, start empty) and four power pills in `PILL_FLAGS`.

## How the maze is stored

- `TBL_FE13` (80 bytes at `$FE13`): one byte per cell; even rows use bytes 0-39, odd rows use bytes 40-79 (the same two row patterns repeat four times: 8 rows of 40 cells). Bit 7 = wall, bit 6 = open upwards, bit 5 = open downwards, **bit 0 = a wafer is kept for this cell, bit 1 and bits 2-4 = which memory bit**.
- A wafer is the memory bit `TBL_FE6A[(t >> 2) & 7]` (`$80, $40, ... $01`) of the maze byte `TBL_FE79[2 * row + ((t >> 1) & 1)]` (offset from `$88`).
- The 16 bytes `MAZE_00..15` at `$88-$97` hold the wafers of the player whose turn it is (a second copy at `$9C-$AB` for the other player, exchanged at the change of turn). The four power pills are bits 2-5 of `PILL_FLAGS` (`$20, $10, $08, $04` for the places (x=4,y=5), (x=4,y=$41), (x=$94,y=5), (x=$94,y=$41)); bits 7-6 of that byte are set when a pill is eaten (scared time).

## When a wafer is eaten (every frame, bottom logic, before the joystick is read)

1. The power pill is tested first: Pac-Man exactly on one of the four places and its bit set: the bit is cleared, `PILL_FLAGS` gets bits 7-6, +5 points, and **the joystick and the step of that frame are skipped**.
2. Otherwise, with Pac-Man exactly on a row (`Y_OFFSET` 0) and not in an opening: the byte of his cell is read; **when he moves vertically and is not in the middle column (x = $4C), the byte of the next cell is OR-ed in** (a wafer between two cells is eaten from either); if bit 0 is set and the memory bit is still set it is cleared, +1 point, sound 0.
3. If all 16 bytes and the pill bits are zero: lives +1 (at most 9), level step +1 (at most $0E), all wafer bytes `$FF` with `$8E = $7F` and `$8A = $FE`, pill bits `$3C`; then the objects are put back and the game freezes.

## The maze (from the table)

Rows 0-7 (y = 0, 10, ... 70), 40 cells of 4 pixels (x = 0 to 156): `#` wall bit, `.` a wafer is kept here, `u` open upwards, `d` open downwards. (The pictures drawn by the kernel are F18.)

```
row 0 y=00  # . . . . . .  # . .. . # .  . . . . . #
         up       u    u      u     u      u    u     
         down  d    d  d   d  d     d  d   d  d    d  
row 1 y=10  # . . . # . . . . .  . . . . . # . . . #
         up    u    u  u   u  u     u  u   u  u    u  
         down     d    d      d     d      d    d     
row 2 y=20  # . . . . . .  # . .. . # .  . . . . . #
         up       u    u      u     u      u    u     
         down  d    d  d   d  d     d  d   d  d    d  
row 3 y=30  # . . . # . . . . .  . . . . . # . . . #
         up    u    u  u   u  u     u  u   u  u    u  
         down     d    d      d     d      d    d     
row 4 y=40  # . . . . . .  # . .. . # .  . . . . . #
         up       u    u      u     u      u    u     
         down  d    d  d   d  d     d  d   d  d    d  
row 5 y=50  # . . . # . . . . .  . . . . . # . . . #
         up    u    u  u   u  u     u  u   u  u    u  
         down     d    d      d     d      d    d     
row 6 y=60  # . . . . . .  # . .. . # .  . . . . . #
         up       u    u      u     u      u    u     
         down  d    d  d   d  d     d  d   d  d    d  
row 7 y=70  # . . . # . . . . .  . . . . . # . . . #
         up    u    u  u   u  u     u  u   u  u    u  
         down     d    d      d     d      d    d     
```

## Which bit is which wafer (maze byte, bit -> row, cell)

| Byte | bit 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| $88 | r7 c2 | r7 c4 | r7 c6 | r7 c10 | r7 c12 | r7 c14 | r7 c16 | r7 c18 |
| $89 | r5 c2 | r5 c4 | r5 c6 | r5 c10 | r5 c12 | r5 c14 | r5 c16 | r5 c18 |
| $8A | r3 c2 | r3 c4 | r3 c6 | r3 c10 | r3 c12 | r3 c14 | r3 c16 | r3 c18 |
| $8B | r1 c2 | r1 c4 | r1 c6 | r1 c10 | r1 c12 | r1 c14 | r1 c16 | r1 c18 |
| $8C | r7 c21 | r7 c23 | r7 c25 | r7 c27 | r7 c29 | r7 c33 | r7 c35 | r7 c37 |
| $8D | r5 c21 | r5 c23 | r5 c25 | r5 c27 | r5 c29 | r5 c33 | r5 c35 | r5 c37 |
| $8E | r3 c21 | r3 c23 | r3 c25 | r3 c27 | r3 c29 | r3 c33 | r3 c35 | r3 c37 |
| $8F | r1 c21 | r1 c23 | r1 c25 | r1 c27 | r1 c29 | r1 c33 | r1 c35 | r1 c37 |
| $90 | r0 c2 | r0 c4 | r0 c6 | r0 c8 | r0 c10 | r0 c12 | r0 c17 | r0 c19 |
| $91 | r6 c2 | r6 c4 | r6 c6 | r6 c8 | r6 c10 | r6 c12 | r6 c17 | r6 c19 |
| $92 | r4 c2 | r4 c4 | r4 c6 | r4 c8 | r4 c10 | r4 c12 | r4 c17 | r4 c19 |
| $93 | r2 c2 | r2 c4 | r2 c6 | r2 c8 | r2 c10 | r2 c12 | r2 c17 | r2 c19 |
| $94 | r0 c20 | r0 c22 | r0 c26 | r0 c29 | r0 c31 | r0 c33 | r0 c35 | r0 c37 |
| $95 | r6 c20 | r6 c22 | r6 c26 | r6 c29 | r6 c31 | r6 c33 | r6 c35 | r6 c37 |
| $96 | r4 c20 | r4 c22 | r4 c26 | r4 c29 | r4 c31 | r4 c33 | r4 c35 | r4 c37 |
| $97 | r2 c20 | r2 c22 | r2 c26 | r2 c29 | r2 c31 | r2 c33 | r2 c35 | r2 c37 |

(Column "bit 7" is index 0 of the table, `$80`; column "0" is index 7, `$01`.)

## Not claimed

- How the kernel turns these bytes into dots on the screen (F18); the exact scoring of ghosts, the bonus item and extra lives (F15); the ghosts' use of the same wall table (F13); what happens to the objects when a new map starts (F12/F16).
- Frames with the bonus item appearing or going, a change of turn, a death, a ghost-eaten pause and Select during play are not predicted (counts above); the score is not compared in the frames where a ghost was eaten.
