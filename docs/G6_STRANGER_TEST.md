# Stage G6: the "stranger test" (2026-10-26)

Can a reader who knows nothing of the project learn, from the source alone, how the ghosts choose a direction and how scoring works? Four fresh agents (separate sessions that were told only a question and the folder `src\game`) tried. This page has the method, the answer key (written **before** the first agent ran), the grades, what was fixed and what was not.

## Method

- **Two rounds, two agents each.** Round 1: one agent for "how does a ghost decide its direction?", one for "how does scoring work?". After the fixes, round 2: one agent for "what do the bits of a ghost's state byte and of the pill flags mean, and when is the count of ghosts eaten reset?", one for five questions about the scoring helpers.
- **What each agent was told:** the question and the folder. It could read the `.cpp` and `.hpp` files under `src\game` only (no `docs`, `PLAN.md`, `DECISIONS.md`, `CLAUDE.md`, tools or tests), could not run or change anything, and had to give a plain-words answer, every claim with `file:line`, and a list of what it could not tell or what confused it.
- **Keys:** written first, from the proof documents (`docs\F13_GHOSTS.md`, `docs\F15_SCORING.md`) and the code, kept in the scratch folder of the session; the round-2 key was written after the fixes and before the second pair ran. Grades: right (found, correct, cited), partial (found but vague or incomplete), wrong, missing.
- **A caveat:** the tool may have shown the agents the project notes in `CLAUDE.md` (they were told to ignore them). The grading therefore counts only what the cited lines support. Several answers said "UNSURE" about exactly the points the notes state plainly, which is what an agent working from the code would do. The agents are the same kind of model that wrote the code; a human reader may find other places hard.

## Round 1

**Ghost direction: 9 of 9 right** (`ghosts.cpp`, `objects.cpp`, `top.cpp`; nothing else was read).

| Key item | Grade |
| --- | --- |
| G1 decides only exactly on a cell, no step due | right (`ghosts.cpp:74-75`) |
| G2 eyes aim at the pen door x $58, y $1F | right |
| G3 middle column, one in 16, into the opening | right |
| G4 scared: mirrored x, Pac-Man's own y (original oddity) | right, but the agent could not say what "mirrored" means in game terms (the comment was garbled) |
| G5 chase timer counts down, then reload by level and wander | right |
| G6 aim x first, then y, first allowed turn wins | right |
| G7 never straight back (Pac-Man may) | right |
| G8 the random number picks the order of tries | right |
| G9 the wall table decides what is allowed | right |

**Scoring: 8 of 8 right**, two only partly (`scoring.cpp`, `collisions.cpp`, `maze.cpp`, `kernel_score.cpp`, `flow.cpp`):

| Key item | Grade |
| --- | --- |
| S1 decimal digits, three bytes per player | right |
| S2 amounts 1, 5, 100 and 20/40/80/160 | right |
| S3 ghost count in the pill flags | **partial**: found the count (bits 0-1, up to 3) but not when it is cleared |
| S4 points go to the player in turn | right |
| S5 five digits shown | right |
| S6 no extra life for score; map clear gives one (max 9) | right |
| S7 each event starts a sound | **partial**: two of the four sounds named |
| S8 amounts from tables by amount number | right |

## What the agents said was hard, and what was done

| Found by the agents | Fix |
| --- | --- |
| `kBitMasksAndReloads` and `kScoreAmounts` hide that they hold several tables (offsets 5, 6, 7 as bare numbers) | named offsets in `tables.hpp` (`kChaseTimerReloadAt`, `kPillMaskAt`, `kSoundBitAt`, `kWaferBitMaskAt`, `kScoreAmountLowAt`, `kScoreAmountMidAt`, `kAnimPointerStepAt`, `kColourTvAt`, `kColourBlackWhiteAt`, `kPlayfieldFE0DAt`), written by the generator and used in the code |
| garbled comment on the scared ghost's target ("$ not x + $60 inverted"); a sentence without its subject | rewritten: "the mirror image of Pac-Man's x across the maze (159 - x)", "down if the ghost's y is smaller than the target's" |
| the state byte's bits "never listed in one place"; the pill flags' bits; the title screen reusing the score bytes | one description each in `state.hpp`, and named bits: `kObjMirrored`, `kObjEyes`, `kObjStepDue`, `kObjInOpening`, `kPillGhostsEaten`, `kPillsOnMap`, `kPillScareFirstPart`, `kPillScared` replace the bare masks `0x01 0x08 0x10 0x20 0x03 0x3C 0x40 0x80 0xC0 0x7C` on those bytes |
| 6502 register names as variable names (`x`, `y` for a pill place, a ghost index, an amount number); `onPill` set and never used | `pillPlace`, `ghostIndex`, `ghostAmount`; `onPill` removed |
| the `cy(n)` calls fill the logic and are not explained where they appear | a one-line reading tip at the top of every logic file that has them |
| two-player bit versus player-two's-turn bit not obvious at the place that uses it | comment at the test in `kernel_score.cpp` |
| comment `GHOST_DECIDE ($FA26).` above the helper struct looked like the decision function; `wrapToTop` not found | comments in `ghosts.cpp` |
| nothing said when the ghost count is cleared; which sounds go with which event | comments in `collisions.cpp`, `flow.cpp`, `scoring.hpp` |

## Round 2 (after the first fixes)

**Bit layouts and the ghost count: 5 of 5 right.** The agent produced the full table of the ghost state bits and the pill-flags bits, the resets of the count (scare ends, new map, new game) and a correct point the key had not listed: eating a second pill during a scare does not reset the count.

**Scoring helpers: 5 of 5 right.** `kScoreAmounts` as one joined table with the offsets 0, 6 and 7; `cy(n)` as processor-cycle bookkeeping that can be ignored when reading the rules; the two-player bit (6) versus player two's turn (7); the parameters of `addScore` (`callerX` plays no part in the score); all four sounds.

Left as they were reported (small, not changed): the word "amount" is used for an index and for points (the header now says "amount number ... not a number of points"); the agent could not tell from the score-line code alone how `scoreDigits(4)` and `(5)` map to the score bytes (a comment was added after the round); direction bits of Pac-Man (`0x80`, `0xC0`) are still bare masks because they belong to the direction field, described in `state.hpp`; comments still cite stage documents (`F13`, `docs\F15_SCORING.md`) that these agents were told not to read, so the *documents* were not tested.

## After the edits

The edits are comments, names and constants only. Both builds, `test_tables`, `test_no_rom`, the ten proofs, 120 random games, the key replay and the host checks were run on the final code (see `PROJECT_STATUS.md`).
