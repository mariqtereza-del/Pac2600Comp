# Stage F12: collisions (2026-10-26)

`python -I tools\f12_collisions.py rom\pacman.a26` finds what the game uses to detect that two things touch, replays all 70 recorded games and checks the game's reaction to the collision flags on every frame. This document is written by that tool.

## What the game uses: the chip's own collision flags

The TIA chip notes by itself, while it draws the picture, which of its objects (two sprites P0 and P1, two missiles M0 and M1, the ball BL, the playfield PF) overlapped; the program only reads the flags and clears them once per frame. **The game has no checks of its own that compare positions**: all touching is decided by the picture the kernel draws.

Instructions in the ROM that use a collision register (all are in the main loop):

| Address | Instruction | Register | Meaning |
| --- | --- | --- | --- |
| $F573 | `LDA` | `CXM1P` | bit 6 = missile 1 touched player 1 (Pac-Man) |
| $F575 | `AND` | `CXP1FB` | bit 6 = player 1 (Pac-Man) touched the ball |
| $F5D9 | `LDA` | `CXPPMM` | bit 7 = player 0 touched player 1 (a ghost touched Pac-Man); bit 6 (the two missiles) is not used |
| $F7DE | `STA` | `CXCLR` | clears all flags (written once per frame, at the start of the frame) |

Everything else the chip can report (the missile 0 and playfield pairs, the ball and playfield pairs, and so on) is never read. The wafers are drawn with missile 0 and the playfield, but the game never asks whether Pac-Man touched them: eating a wafer is decided by Pac-Man's cell (F11).

## What the three reads decide

All of this is in the bottom logic, once per frame, after the picture is drawn and before the joystick is read:

1. **Bonus item** (`$F573-$F58F`): the flag byte is the AND of CXM1P and CXP1FB; its bit 6 means that missile 1 *and* the ball both overlapped Pac-Man (the item is drawn with these two). Then: +100 points (`ADD_SCORE` 6), sound 1, the item's on-flag (`GAME_FLAGS` bit 1) toggled off, `BONUS_TIMER` = $12.
2. **Power pills** are not tested with the flags: Pac-Man standing exactly on one of the four places (F11).
3. **A ghost** (`$F5D3-$F61B`): only when `FRAME_COUNTER` bit 2 is clear, so on 4 frames of every 8; the ghost tested is number `FRAME_COUNTER & 3`, which is exactly the one the kernel draws as player 0 in that frame (one ghost is drawn per frame, flicker). If CXPPMM bit 7 is set:
   - ghost scared (`PILL_FLAGS` bit 7) and not yet eaten: set bit 3 of its state ("eaten, eyes only"); `ADD_SCORE` with Y = `PILL_FLAGS & 3` (+20, +40, +80, +160 for the 1st to 4th ghost of one pill); count +1 (stays at 3); sound 3; `EAT_PAUSE` = $9F (the game freezes for 32 frames: EAT_PAUSE counts from $9F down to $7F);
   - ghost scared but already eaten: nothing (the eyes are harmless);
   - ghost not scared and not eaten: Pac-Man dies: `GAME_FLAGS` |= $C0, `EVENT_TIMER` = $3F, and the rest of the frame is skipped (the death sequence is F17);
   - ghost not scared but eaten (eyes still travelling home after the scare ended): nothing.
4. The flags are cleared by `STA CXCLR` at the start of the next frame (`FRAME_START`, $F7DE), so each picture starts with clean flags.

Because only one ghost is tested per frame and only on 4 frames of 8, each ghost is tested once every 8 frames: a ghost that touches Pac-Man for fewer frames can be missed; this is part of the original's behaviour and the rebuilt game must keep it.

## Result of the check

- Replays: 70 runs, all equal to their golden traces (picture and memory).
- **no mismatch**: 0 problems in 439610 frames (the 2 test-only runs BL-065/066 are skipped).

| Count | What |
| --- | --- |
| 303 |     death starts right (flags $C0, event timer $3F) |
| 193 |     eaten reactions right (flag 8 of ghost, pause $9F, count, +20/40/80/160) |
| 18 |   bonus reactions right |
| 169 |   ghost already eaten (eyes): nothing happens |
| 193 |   scared ghost: eaten |
| 303 |   unscared ghost: Pac-Man dies |
| 18 | bonus item eaten |
| 439610 | frames |
| 202618 | frames in which the bonus flag is read |
| 101417 | frames in which the ghost flag is read |
| 100752 | ghost flag clear, no reaction (right) |
| 665 | ghost flag set |
| 1398 | skipped: test-only memory edits (BL-065/066) |

Ghosts tested per index (0-3): 0: 25406, 1: 25423, 2: 25300, 3: 25288.

## Where the flag is set (ghost against Pac-Man)

The flag is true pixel overlap of the two pictures, so it depends on both sprite shapes and on the exact pixel column each sprite is placed at (rounded by the position routine at `$F09C`). That belongs to the drawing kernel (F18) and the renderer (F23), which must give the same flags.
What the logged flags show about position (dx = ghost x - Pac-Man x, dy = ghost y - Pac-Man y, both as stored in memory):

- 13110 different (dx, dy) pairs were tested; the flag was set in 128 of them; **the flag was set only with |dx| <= 7 and |dy| <= 7** (hits outside: 0 of 99213 frames).
- Inside that square the flag was set in 664 and clear in 211 tested frames. In 106 of the 128 (dx, dy) pairs with a hit the flag was set every time; in 22 the same pair gave both answers (the picture shapes and the animation phase decide there).

Picture of the square (`#` always set, `+` set sometimes, `.` never set, blank = never tested), dy down, dx across:

```
      dx: -9 -8 -7 -6 -5 -4 -3 -2 -1  0  1  2  3  4  5  6  7  8  9
dy  -9:   .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .
dy  -8:   .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .
dy  -7:   .  .  .  .  .  +  #  #  #  #  #  +  #  #  +  .  .  .  .
dy  -6:   .  .  .  .  +  #  #     .  +  #     #  #  +  +  .  .  .
dy  -5:   .  .  +  +  +  #  #  #  #  +  #  #     #  #  +  +  .  .
dy  -4:   .  .  #     #  +           +  #  #           #  +  .  .
dy  -3:   .  .  +     #     #  #  #  #  #  #     #  #  #  #  .  .
dy  -2:   .  .  +  #  #  #           #     #     #  #  #  .  .  .
dy  -1:      .  #  #           #     #  #        #  #     #  .  .
dy   0:   .  .  +  +  #  #  #  #  #  #  #  #  #  #  #  +  +  .  .
dy   1:      .  #  #  #  #                    #     #     #  .  .
dy   2:   .  .  #  #     #     #  #  #        #        #  .  .  .
dy   3:      .  .  #  #  #  #  #     #     #                 .  .
dy   4:   .  .  .        #  #  #     #     #     #        .  .  .
dy   5:   .  .  .  .     #     #     #  #  #  #     #  .  .  .  .
dy   6:   .  .  .  .  .  #  #  #     #     #     #  .  .  .  .  .
dy   7:   .  .  .  .  .  .  .  .  #  #        .  .  .     .  .  .
dy   8:   .  .  .     .  .     .  .  .     .  .  .  .  .  .  .  .
dy   9:   .  .  .  .  .  .  .  .  .  .     .  .  .  .  .  .  .  .
```

## Where the bonus flag is set

The bonus item is drawn only in maze row 3 of the kernel (the row of the ghost pen; Pac-Man stands at y = 40 when he touches it), with missile 1 and the ball: there the kernel loads ENAM1 and ENABL from `GAME_FLAGS` (bit 1 = item shown) and in every other row it clears them (`$F18C`, `$F1D5`, `$F2BD`, `$F2F3`, `$F316-$F320`). While it was shown (`GAME_FLAGS` bit 1) and the flag was read, Pac-Man stood on 4 different (x, y) places when the flag was set (18 times in all); the same place also gave "not set" in 4 cases (the flag needs both objects to overlap, which depends on the exact pixel).
Places with a hit: (72,40), (73,40), (78,40), (80,40).

## Not claimed

- The pixel-exact overlap is not predicted here: the flags used above are the ones the emulator produced. A native game needs its own renderer to produce them (F23); the logs (`reference\f12`, local) are the test data for that.
- The death sequence, the pen and the ghost-eyes journey home, and the scared timers are F13, F16 and F17.
- Sound numbers are named but their effect on the sound chip is F19.
