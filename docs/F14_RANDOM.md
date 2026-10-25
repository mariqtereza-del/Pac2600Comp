# Stage F14: randomness (2026-10-26)

`python -I tools\f14_random.py rom\pacman.a26` finds every source of random numbers in the game and checks the finding against the 70 recorded games. This document is written by that tool.

## Finding: one random byte, no outside source

The game has **one** random byte, `RNG_STATE` (`$CB`). It is written in exactly three places and read in two:

| Address | Instruction | What |
| --- | --- | --- |
| $F016 | `STX RNG_STATE` | write |
| $F892 | `STA RNG_STATE` | write |
| $FBE7 | `ROL RNG_STATE` | write |
| $FA4F | `LDA RNG_STATE` | read |
| $FBE0 | `LDA RNG_STATE` | read |
| $FBE3 | `EOR RNG_STATE` | read |
| $FBE9 | `LDA RNG_STATE` | read |
| $FBF1 | `AND RNG_STATE` | read |

- `$F016` (start-up) and `$F892` (every time a game starts) set it to **$FF**. So every game starts from the same number, whatever happened before, and the game is a pure function of the buttons pressed: the same recording always plays the same game (the 70 replays, all equal to their golden traces, twice, show it).
- `RNG_STEP` (`$FBE0`) is the only other writer: a shift register. With the state `r`: `a = ((r << 1) ^ r) << 1` (8 bits); the carry out of the last shift is rotated into the low end of `r`: `r = (r << 1 | carry) & $FF`. Then the routine returns `TBL_FE6A[r & 7] & r` (one bit of the new state, picked by its low three bits); the callers only ask whether that is zero or not.
- The other reader, `$FA4F`, looks at the low four bits of `$CB` without stepping it (a ghost on the middle column takes an opening when they are 0).

**No clock, timer or input feeds it.** The game reads the chips at only these places: `F0C5 LDA INTIM`, `F61E LDA SWCHA`, `F7D1 LDA INTIM`, `F857 AND SWCHB`, `F886 LDA INPT4_AT3C`, `F88A LDA SWCHB`, `F91C AND SWCHB`. The timer `INTIM` is read only in the two wait loops (`$F0C5`, `$F7D1`) which test it for zero and keep nothing; the joystick and console switches go to the game rules (F9), not to `$CB`. The frame counter and the idle counter are not mixed in either. Randomness therefore comes **only from the number of times `RNG_STEP` has been called since the game started**, and that number depends on where the ghosts are and what they decide (F13).

## The generator

Starting from $FF the sequence visits 128 different states; it is periodic with a period of **127 steps** after 1 lead-in steps. The state 0 would stay 0 for ever (yes), but $FF never leads to it. First 12 steps from $FF (state, value returned): $FE/$02, $FC/$08, $F8/$80, $F0/$80, $E0/$80, $C0/$80, $81/$00, $02/$00, $04/$00, $08/$00, $10/$00, $20/$00.

## Check against the recorded games

In all 70 games (the 2 test games BL-065/066 are skipped) every change of `$CB` between two frames is either the start value $FF or a number of steps of the generator from the value before; nothing else ever writes it. (F13 showed more: the exact value in every frame where a ghost decides, with the step count the model predicts.)

| Count | What |
| --- | --- |
| 89039 | changed by 1-3 steps |
| 70 | first frame value $FF |
| 438212 | frames |
| 121 | set to the start value $FF (a game starts) |
| 349052 | unchanged |

Steps in one frame (0 means unchanged): 1: 22830, 2: 66209.

## Where the numbers are used

- Ghost wandering: sideways choices and whether to keep straight (`GHOST_DECIDE`, `TRY_PAIR_1`, `TRY_PAIR_2`), when the ghost timer reaches zero, on the title screen, or when a chase finds no way (F13).
- The ghost opening (low four bits are 0).
- Nowhere else: not the bonus item, not the scoring, not the sound.

## For the rebuilt game

The rebuilt game needs no real random source and no seed choice: it starts `$CB` at $FF with every game and calls the same step in the same order. Because the original is deterministic, **the lockstep test (F20) can feed both versions the same buttons and compare everything**; no recording of random numbers is needed. A modern "random start" would be a modernisation and would have to be a separate, optional feature (Rule 3).

## Not claimed

- The moments at which each ghost decision happens are F13; the long-run statistics of the numbers are not studied (they are not needed to reproduce the game).
