# Gate C — Reproducible reference (Stage C8, 2026-10-24)

**Verdict: all six items PASS. Phase C is complete.** Checked in this session from files and fresh runs, not from earlier notes. Only files inside the project were read or written (new local output in `reference\c8\`).

| # | Gate item | Result | Evidence |
| --- | --- | --- | --- |
| 1 | Display documented | PASS | `docs\DISPLAY.md`: each value has its measuring method; unmeasured items listed. Fresh check: an idle run of 6,000 frames has **0** picture fingerprints different from C3's. |
| 2 | Timing documented | PASS | `docs\TIMING.md`: cycles per line and frame measured, Stella's 60.000 Hz versus the console clock's 59.923 Hz stated with sources; the pacing choice is left to Phase D on purpose. |
| 3 | Sound documented | PASS | `docs\SOUND.md`: three sounds measured at register level; sounds not captured are listed. |
| 4 | Controls and variations documented | PASS | `docs\CONTROLS.md`: 16 selections, switches, joystick, with "no effect found" limited to the scenario tested and the undetermined rules listed. |
| 5 | Snapshots present and repeatable | PASS | 8 pictures in `reference\screens\`. A fresh replay from power-on gave all 8 equal to the fingerprints in `docs\CAPTURES.md` and a `frames.csv` identical to C7's. |
| 6 | Reference recreatable, nothing "believed" | PASS | `docs\REFERENCE.md` lists exact versions, fingerprints, commands and 9 open items with the stage that settles them. A scan of the fact documents for "assume/believe/probably/likely/guess" found only labelled working labels and one warning to check, no unmeasured claim stated as fact. |

## Other checks

- ROM: `rom\pacman.a26`, the original and the spare in `F:\Ports\ROM-originals\` all have SHA-1 `0940fea7f04cdb6d4b90c5ad1a7e344e68f6dbb1` (same as `rom\EXPECTED_SHA1.txt`).
- Git: no `.a26`, `.bin`, archive, `rom\` or `reference\` file is tracked; `git fsck` clean; tree clean at the start.
- `tools\test_quick.ps1` does not exist yet (Phase H), so it was not run.

## Not re-checked in this session

- The Stella app folder `F:\Ports\Tools\Stella-7.0c` is outside the project, so it was not read again. C1's proof (96 of 96 files identical to the official zip) stands. Its settings file has since changed legitimately (C2 and C6 runs), as `docs\FIRST_RUN.md` and `docs\CONTROLS.md` note.
- The sound and timing runs (24,000 frames each) were not repeated; their tools reproduced C3's fingerprints when they were built.
