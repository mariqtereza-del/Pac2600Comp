# Gate H: the test suite is complete and the preservation suite is frozen (Stage H7, 2026-10-27)

**Verdict: PASS.** Every test is listed and timed, the quick, full and ROM-free commands run end to end, a clean copy builds from `BUILDING.md`, the game keeps the console's speed at every monitor speed tried, and the files that define the original's behaviour are frozen (`tests\preservation\`). The permanent baseline is still the Git tag `1.0.0-preservation`.

| # | Gate item | Result | Evidence |
| --- | --- | --- | --- |
| 1 | `TESTING.md` lists every test and tool | PASS | `h1_testing_check.py`: 85 names checked, 0 missing (H1). |
| 2 | Unit tests per module | PASS | `test_game_modules` 83 checks, no ROM (H2); full run of this stage: PASS. |
| 3 | Two test commands | PASS | `tools\test_quick.ps1`: 35 of 35 steps in 155 s, `result: OK`. `tools\test_full.ps1`: **48 of 48 steps in 1,704 s (28 min), `result: OK`** (all 70 baseline games equal their stored SHA-256, 120 random + 70 recorded games identical, 350,460 key frames, 0 differences, host checks `fail=0`). |
| 4 | ROM-free test set | PASS | `tools\test_public.ps1`: 17 of 17 steps in 21 s, `result: OK` (11 recorded games without a ROM). |
| 5 | Clean-build and release checks | PASS | Stage H5: clean copy built from `BUILDING.md`, public tests 16 of 16 there, release zip scan clean, `d16_gate_check` 33 of 33. Not re-run in H7 (nothing in the build changed except the new test files). |
| 6 | Performance and pacing | PASS | Stage H6: 59.87-59.92 frames a second on the real 180 Hz monitor and pretend 30-240 Hz, 0 sound underruns, pictures identical; `docs\PERFORMANCE.md`. |
| 7 | The preservation suite is frozen | PASS | `tests\preservation\MANIFEST.txt` lists 151 files (70 entries, 70 inputs, 11 fingerprint files). `h7_preservation_check.py`: all present and unchanged; the 142 that exist at the tag are identical to it (`git diff 1.0.0-preservation -- baseline` is empty); no unlisted files. |
| 8 | The freeze catches tampering | PASS | `h7_preservation_check.py --drill`: untouched copy passes; an edited file, a file edited together with the manifest, a deleted file, a shortened manifest and a new unlisted file are each caught; after `--add` the new file is accepted (7 of 7). Part of `test_quick`. |
| 9 | Without Git the check says what it skipped | PASS | On a copy with no Git above it: checks 2 and 3 print SKIPPED, 1 and 4 pass, `result: OK`. |
| 10 | Rules intact | PASS | `g7_gate_check` 9 of 9 and `g2_conventions_check --strict` CLEAN (inside the full run); no ROM staged (checked before the commit). |

## Not done / limits

- **The freeze is a check, not a lock** (`tests\preservation\README.md`): editing a file and the manifest together and committing both passes checks 1 and 2; check 3 catches it for the 142 files the tag has, and the other 9 (fingerprint files added after the tag) rest on Git history.
- No real 60 Hz monitor was tried in H6 (global Windows setting); the pretend 60 Hz run uses a timer.
- The GitHub workflow has still never run on GitHub (nothing is pushed).
- One PC, one sound device, one compiler version.
- The frozen traces themselves (`reference\baseline\*`, each 200 KB to several MB) stay local and are not in Git; the entries hold their SHA-256, and `baseline.py check` compares fresh traces with them.
- `docs\F2x_*.md` proof documents had their running-time lines rewritten by the proof tools, as always.
