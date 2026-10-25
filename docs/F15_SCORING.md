# Stage F15: scoring, the bonus item and lives (2026-10-26)

`python -I tools\f15_scoring.py rom\pacman.a26` states the rules and checks them on every frame of the 70 recorded games (the 2 test games BL-065/066 are skipped). This document is written by that tool.

## Result: all rules hold

| Count | What |
| --- | --- |
| 20 | A:   frames with a new map |
| 4585 | A:   frames with points |
| 18 | A:   total bonus items |
| 193 | A:   total ghosts |
| 102 | A:   total power pills |
| 4276 | A:   total wafers |
| 290818 | A: frames checked |
| 290818 | A: score change right |
| 1637 | C: frames with the 256-frame tick |
| 434279 | C: other frames: timer and item flag unchanged, right |
| 1637 | C: tick frames right |
| 222 | D: death with lives left: one life less, new try, right |
| 52 | D: death with no lives left: game over (attract timer $5F, flags 0), right |
| 438212 | frames |
| 147375 | skipped (A,B): Select/Reset or title screen |
| 19 | skipped (A,B): change of turn |

Changes of the lives byte seen: in turn -1: death resolved (222); in turn +1: new map (20). Highest score seen: 165431.

## Score

- Each player has a 6-digit decimal score in three bytes (low, middle, high; `$CC/$CE/$D0` for player 1, `$CD/$CF/$D1` for player 2), added in decimal mode by `ADD_SCORE` (`$FC45`) to the player whose turn it is (`PLAYER_MODE` bit 7). Only four places call it, and they are all in the bottom logic:

| Amount number Y | Points | Called from | Event |
| --- | --- | --- | --- |
| 4 | +1 | `$F6A2` | a wafer eaten (F11) |
| 5 | +5 | `$F5C8` | a power pill eaten (F11) |
| 0-3 | +20, +40, +80, +160 | `$F5F7` | a scared ghost eaten; Y is the pill counter (F12) |
| 6 | +100 | `$F57D` | the bonus item eaten (F12) |

- The change of the score of the player in turn equals the sum of those amounts in every frame (the last wafer of a map counts +1 in the frame that refills the map); the other player score never changes. The amounts are in the tables `$FF06` (tens and ones) and `$FF0C` (hundreds).
- There are only six digits: a carry out of the top byte is lost, so the score would wrap from 999,999 to 0 (analysis of the code; not seen, the highest score in the recordings is 165431).
- **There is no extra life for reaching a score**: nothing in `ADD_SCORE` or its callers looks at the score, and the lives byte never changes except as listed below (checked on all frames).

## Lives

- A game starts with 3 (`NEW_GAME_SETUP` `$FBF4`); the markers on the status line show the lives (`LIVES_MARKERS`). Counting the one in play, that is four tries.
- +1 when a map is cleared (up to 9, F11); nothing else adds lives.
- At the end of a death (when the event timer, which started at `$3F`, runs out; F17 follows the sequence): if lives are left, one less and the next try starts frozen; if none are left, game over: `GAME_FLAGS` = 0 and `ATTRACT_SHIFT` = `$5F` (the title screen follows; plus one if the frame counter wrapped in the same frame).
- Two-player games swap the 20-byte player block (maze, lives, level, pills, colour) at the change of turn (F7); the recordings never played player 2 (F9), so only the code, not a recording, shows that part.

## The bonus item

- The item (drawn with missile 1 and the ball, F12) is shown while `GAME_FLAGS` bit 1 is set. `BONUS_TIMER` (`$EC`) starts at `$0C` at power-on and counts down by one each time the frame counter wraps (every 256 frames), except in the tick where the idle counter `TICK256` also wraps (then the attract timer goes up instead).
- When the timer reaches 0 the item flag flips and the timer is reloaded: **item turns on -> 3 ticks (768 frames), or 2 ticks when `GAME_FLAGS` bit 5 is set; item turns off -> `$12` = 18 ticks (4,608 frames)**. Eating it (+100) turns it off at once and sets the timer to `$12`.
- The frames of the 256-frame tick were predicted exactly (timer, item flag, attract timer): 1637 of 1637; the item turned on 86 times and off by the timer 49 times in the recordings; in every other frame the timer and the flag do not change.
- The value is always +100, whatever the level. Bit 5 of `GAME_FLAGS` is set from the difficulty switch of the player in turn (`$F90D-$F926`); the shorter showing for it comes from the code only: no recording uses that switch position while the item is on the way.

## Not claimed

- Two-player scoring and lives (no recording), the 999,999 wrap (not reached), the difficulty-switch effect on the bonus (code only). The death sequence timing is F17; level changes are F16.
