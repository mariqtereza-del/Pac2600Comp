# Stage F16: levels, speed-ups and timers (2026-10-26)

`python -I tools\f16_levels.py rom\pacman.a26` lists everything in the code that depends on the level, works out the speeds and timers from the ROM and checks the rules on every frame of the 70 recorded games (the 2 test games BL-065/066 are skipped). This document is written by that tool.

## What the level changes (complete list)

The level is the byte `LEVEL_SPEED` (`$99`). The code that reads or writes it, and the game number `GAME_SELECT` (`$84`), is all there is:

| Address | Instruction | What |
| --- | --- | --- |
| $F55F | `LDA LEVEL_SPEED` | while the game is frozen: ghost speed accumulators and timers are set from the level |
| $F6C0 | `LDA LEVEL_SPEED` | new map: level + 1 ... |
| $F6C6 | `INC LEVEL_SPEED` | ... up to $0E |
| $FA86 | `LDA LEVEL_SPEED` | the ghost timer is reloaded from the table by level / 2 |
| $FC0A | `STA LEVEL_SPEED` | game start: level stored |
| $F00E | `STA GAME_SELECT` | power-on: game number 6 |
| $F8A1 | `LDX GAME_SELECT` | game start: Pac-Man speed seed from the game number |
| $F8C8 | `INC GAME_SELECT` | game number: Select steps it, show it on the score line |
| $F8E7 | `LDA GAME_SELECT` | game number: Select steps it, show it on the score line |
| $F8EF | `STA GAME_SELECT` | game number: Select steps it, show it on the score line |
| $FC00 | `LDA GAME_SELECT` | game number -> start level |

**So the level changes only two things: how fast the ghosts are and how long their chase timer lasts.** It does not change the scared time, Pac-Man's speed (that comes from the game number only, F10), the bonus item, the points or the number of lives. Nothing is looked up in a table of "levels" with fruit or other pictures: the maze and everything shown are the same on every level.

## Start level and level steps

- A game starts at level `((game number - 2) and 6) + 4`: 4, 6, 8 or 10 (game numbers 2-3 -> 4, 4-5 -> 6, 6-7 -> 8, 8-9 -> 10, then it repeats, so the eight variations use four starting levels). Each cleared map adds 1, up to `$0E` (14).
- Checked: game starts: level = ((game number - 2) and 6) + 4, right: 46; level change = +1 at a new map (max $0E), right: 20.
- Game starts seen (game number, level): ($06, 8) x44, ($08, 10) x2.

## Ghost speed

Every ghost has a speed accumulator that is multiplied by 17 (mod 256) each time the ghost is updated; a carry out is one step (F10). The start value is set while the game is frozen (new life, new map, new game): **objects 2 and 4 get the level L, objects 1 and 3 get L - 1**. The accumulator then cycles, so the speed is fixed by the start value:

| Level L | Start L: steps per cycle | Start L-1: steps per cycle | Chase timer reload (table `$FE6F`, +1 for objects 2 and 3) |
| --- | --- | --- | --- |
| 4 | 1 of 4 updates = 0.250 | 3 of 16 = 0.188 | 1 |
| 5 | 5 of 16 updates = 0.312 | 1 of 4 = 0.250 | 1 |
| 6 | 3 of 8 updates = 0.375 | 5 of 16 = 0.312 | 2 |
| 7 | 7 of 16 updates = 0.438 | 3 of 8 = 0.375 | 2 |
| 8 | 1 of 2 updates = 0.500 | 7 of 16 = 0.438 | 3 |
| 9 | 9 of 16 updates = 0.562 | 1 of 2 = 0.500 | 3 |
| 10 | 5 of 8 updates = 0.625 | 9 of 16 = 0.562 | 5 |
| 11 | 11 of 16 updates = 0.688 | 5 of 8 = 0.625 | 5 |
| 12 | 3 of 4 updates = 0.750 | 11 of 16 = 0.688 | 7 |
| 13 | 13 of 16 updates = 0.812 | 3 of 4 = 0.750 | 7 |
| 14 | 7 of 8 updates = 0.875 | 13 of 16 = 0.812 | 8 |

A ghost is updated 3 times in the frame that handles it, once every 4 frames, so the speed in pixels per frame is 0.75 x the step share. For comparison Pac-Man (seed $09 or $0B) moves 0.562 or 0.688 pixels a frame. The measured average steps per handled frame (normal play, ghost not eyes and not in an opening) by level, next to 3 x the theoretical share of the two start values:

| Level | Object | Measured steps per handled frame | Handled frames | 3 x theory |
| --- | --- | --- | --- | --- |
| 4 | 1 | 0.555 | 400 | 0.562 |
| 4 | 2 | 0.741 | 394 | 0.750 |
| 4 | 3 | 0.555 | 398 | 0.562 |
| 4 | 4 | 0.744 | 398 | 0.750 |
| 6 | 1 | 0.932 | 278 | 0.938 |
| 6 | 2 | 1.113 | 274 | 1.125 |
| 6 | 3 | 0.932 | 280 | 0.938 |
| 6 | 4 | 1.115 | 278 | 1.125 |
| 8 | 1 | 1.307 | 24257 | 1.312 |
| 8 | 2 | 1.498 | 25026 | 1.500 |
| 8 | 3 | 1.308 | 24392 | 1.312 |
| 8 | 4 | 1.499 | 24921 | 1.500 |
| 9 | 1 | 1.499 | 5861 | 1.500 |
| 9 | 2 | 1.687 | 6013 | 1.688 |
| 9 | 3 | 1.500 | 5930 | 1.500 |
| 9 | 4 | 1.686 | 5883 | 1.688 |
| 10 | 1 | 1.684 | 5149 | 1.688 |
| 10 | 2 | 1.871 | 5187 | 1.875 |
| 10 | 3 | 1.684 | 4902 | 1.688 |
| 10 | 4 | 1.871 | 5087 | 1.875 |
| 11 | 1 | 1.874 | 3036 | 1.875 |
| 11 | 2 | 2.062 | 3344 | 2.062 |
| 11 | 3 | 1.874 | 3072 | 1.875 |
| 11 | 4 | 2.061 | 3053 | 2.062 |
| 12 | 1 | 2.062 | 3950 | 2.062 |
| 12 | 2 | 2.249 | 4003 | 2.250 |
| 12 | 3 | 2.062 | 4119 | 2.062 |
| 12 | 4 | 2.250 | 3742 | 2.250 |
| 13 | 1 | 2.249 | 3111 | 2.250 |
| 13 | 2 | 2.436 | 2923 | 2.438 |
| 13 | 3 | 2.249 | 2990 | 2.250 |
| 13 | 4 | 2.435 | 2859 | 2.438 |

The measured values scatter around the theory because the accumulator restarts from its start value at every freeze, and a ghost that turns round or stops at a wall does not always gain the step. The exact step-by-step behaviour was already proved in F13 (348,146 frames).

## The event timer `$E4` (scared time, freeze, death delay)

One byte times three things. It goes down by 1 only in frames where the frame counter seen by the logic is odd (every second frame) and not during the eaten-ghost pause; when it reaches 0:

- **Scared, first part** (bits 7 and 6 of `PILL_FLAGS` set): bit 6 is cleared and the timer set to `$20`: the ghosts flash (the picture changes colour) for 32 steps = 64 frames.
- **Scared, second part** (bit 7 only): the scare ends (`PILL_FLAGS` AND `$7C`: the ghost counter goes back to 0).
- **Frozen** (`GAME_FLAGS` bit 7): the freeze ends (the bit is cleared). A freeze starts with the timer at `$3C` (60 steps = 120 frames) after a new map or a new try.
- **A death** (`GAME_FLAGS` bit 6): the timer started at `$3F`; at 0 the next try is set up or the game ends (F15).

A power pill sets bits 7 and 6 and the timer to **`$A0` (160 steps = 320 frames), or `$50` (160 frames) when `GAME_FLAGS` bit 5 is set** (the difficulty switch of the player in turn), then the flashing part of 64 frames. The eaten-ghost pause is 32 frames (`EAT_PAUSE` counts from `$9F` down to `$7F`) and the timer stands still in it, so every ghost eaten lengthens the scare by 32 frames. **The scared time does not depend on the level.** Eating a second pill during a scare starts the time again.

Results of the frame-by-frame check:

| Count | What |
| --- | --- |
| 293 | 4:   death delay over: new try ($3C freeze) or game over, right |
| 102 | 4:   pill eaten: timer set to $A0 ($50 with the difficulty bit), right |
| 6176 | 4:   timer unchanged in the pause, right |
| 6176 | 4: eaten-ghost pause: timer frozen |
| 303 | 4: skipped: the frame also sets the timer (death) |
| 20 | 4: skipped: the frame also sets the timer (map) |
| 102 | 4: skipped: the frame also sets the timer (pill) |
| 147323 | 4: skipped: title screen or Select/Reset |
| 293 | 4: timer ran out in a death (F15/F17) |
| 283995 | 4: timer, scare bits and freeze right |

Measured: frames from the last power pill to the end of the scare: 448 frames x27, 416 frames x11, 383 frames x10, 384 frames x10, 511 frames x9, 415 frames x7, 479 frames x7, 480 frames x5 (the spread of 1 frame is the odd/even frame counter; shorter values follow a pill eaten while ghosts were already flashing).

## Not claimed

- Levels above 14 (the level cannot go further); the difficulty-A scared time ($50) is from the code and was seen only if the table above shows a 160-step pill; two-player level handling (no recording).
- The picture colours of the scared and flashing ghosts are the kernel (F18).
