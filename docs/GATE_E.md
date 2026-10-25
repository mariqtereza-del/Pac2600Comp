# Gate E: the baseline is complete (Stage E8, 2026-10-26)

**Verdict: PASS, with two named items moved to Phase F (T-E8-2).** 70 baseline entries exist (BL-001 to BL-070); every one has a plain-words text, an input file (both in Git) and a golden trace (local) that repeats byte for byte. The gate rests on machine checks, not on anyone playing (D-008).

## The checklist

| # | Gate item | Result | Evidence |
| --- | --- | --- | --- |
| 1 | At least 30 entries | PASS | 70 (`docs\BASELINE.md` lists them). |
| 2 | Each has a golden trace that repeats byte for byte | PASS | `python -I tools\baseline.py check rom\pacman.a26 BL-001 ... BL-070`: fresh trace equals the stored SHA-256 for 70 of 70 (run again for this gate, after the host change of this stage). Every entry was also made twice at creation and required identical. |
| 3 | Each has a plain-words text that could be checked against the real game | PASS | Every entry has `observations:` computed from its own trace plus a "how to check it" line. The two ARTIFICIAL entries (BL-065, BL-066) say so, and that they need a cheat device on a real console. |
| 4 | The format is lockstep-ready | PASS | `docs\BASELINE.md`: one line per frame with the input, switches, picture and sound fingerprints and all 128 memory bytes; the only extra is the `pokes:` line of two entries (T-E8-1). |
| 5 | The traces agree with an independent reference | PASS | Whole-route runs of Stage E1/D14; Stage E2 against `reference\c6`; E3 to E8 (6, 5, 1, 1, 5 and 7 inputs respectively) run through Stella's own script runner by `tools\d14_side_by_side.py`: 0 fingerprint differences, 0 different pictures (counts in each stage's note). |
| 6 | Start-up and the 16 selections (brief: start-up) | PASS | BL-002 to BL-025. |
| 7 | Pac-Man's movement (speed, walls, turns, reversing, diagonals, the openings) | PASS | BL-026 to BL-037, BL-050 to BL-056, BL-068/069 (rules held over more than 270,000 frames). The brief's "side tunnel" is in fact the opening at the top and bottom (T-E3-1). |
| 8 | Ghosts: leaving the box, chasing, speed, eaten ghosts returning, randomness | PASS | BL-038 to BL-044, BL-070; speed per level in BL-054 to BL-056; no randomness found. |
| 9 | Ghosts: flicker order, the scared state, turn choice | MOVED TO PHASE F | T-E8-2. Measured so far: the 4-frame rotation, the route independence for 126 frames, the power-pill window in byte `$F2`; not isolated: the byte of the scared state, the drawing order, the rule of turn choice. |
| 10 | Eating and scoring: wafers, pills, ghost chain, bonus item, extra lives | PASS | BL-045 to BL-047, BL-054 to BL-057, BL-065/066. Status bar equals memory on 155 pictures (BL-046). Extra life: one at every level clear; none from the score (artificial test). |
| 11 | Dying and game over | PASS | BL-048, BL-049. |
| 12 | Long play: later levels | PASS | BL-054 to BL-056 (five levels each, the bot); the ghosts speed up level by level (T-E7-1). |
| 13 | Edge cases: Reset, Select, switches, long idle, score limit, two players | PASS | BL-058 to BL-067 (BL-064: 40,000 frames idle). |
| 14 | A net for the unforeseen | PASS | BL-050 to BL-053 and BL-068/069: seeded random joystick, console buttons and switches over 30,000 and 24,000 frames; no rule broken. |

## Other checks, run again for this gate (after the host change)

- Unit tests: trace 18, controls 119, layout 231, input recordings 25, window state 29, ROM finder: all pass.
- Key replay of every recorded Phase C input: 350,460 frames, 0 differences.
- No `.a26`, `.bin`, `.zip`, `rom\` or `reference\` file is tracked by Git (0 found); the ROM was only read.
- `tools\test_quick.ps1` does not exist yet (Phase H).

## What the baseline does not contain

The bonus item was eaten 15 times in the bot games, always for 100 points; the score cannot reach 10,000 by play within these runs; a game over was not reached in the long bot games (E6 covers it by idle play). Hidden behaviours that need a particular level beyond the sixth are untested. Phase F's lockstep test is the net for all of that.
