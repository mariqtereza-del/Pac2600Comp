# Gate F: the emulator is out (Stage F26, 2026-10-26)

**Verdict: PASS.** The program you play, `build\host\pacman2600.exe`, runs on our own game code (`src\game`) and contains no Stella. `build\host\pacman2600_ref.exe` is the same program with Stella inside; it is the reference the tests compare with and is not part of a release (T-F26-1).

| # | Gate item | Result | Evidence |
| --- | --- | --- | --- |
| 1 | The game program holds no emulator | PASS | A byte scan of `pacman2600.exe` finds 0 hits for `Stella`, `libretro`, `M6502`, `TIA`, `Cartridge4K` (the reference program: 7, 1, 3, 30, 3). Size 300 KB against 3 MB. |
| 2 | The 70 recorded games, replayed in the native program | PASS | Field by field against the golden traces: 439,680 frames; picture fingerprint, sound fingerprint, input and switches identical; all memory identical except the stack `$F6-$FF` and the 10 scratch bytes the native game does not keep (T-F26-1). 0 games with a difference. |
| 3 | All recorded key input incl. console switches | PASS | `python -I tools\d10_replay_compare.py rom\pacman.a26`: `TOTAL 350460 frames compared, 0 differences`. |
| 4 | Whole-game lockstep, random input | PASS | `python -I tools\f25_games.py rom\pacman.a26 --games 120`: 120 of 120 new games (2,400,000 frames) and 70 of 70 recorded games identical in memory, picture and sound; `result: OK`. |
| 5 | Independent reference | PASS | `d14_side_by_side.py` on the bot route: 9,800 frames, 0 fingerprint differences, 10 whole pictures identical. |
| 6 | Colours | PASS | Saved pictures of BL-001, BL-046, BL-057, BL-070, with and without soft glow, pixel-identical to the reference's (first build was not: 12 colours differed until Stella's gamma was added, T-F26-2). |
| 7 | Window, fullscreen, sound, pad | PASS | `--self-check`, `--layout-check`, `--fullscreen-check` result=OK; `--audio-check 10 ... --mute --reset-at 120` underruns=0 result=OK; `--pad-check` result=OK; unit tests test_layout (231), test_controls (119), test_windowstate (29), test_inputlog (25), test_trace (18), test_rom_finder: 0 failures. All with scratch `--settings` files. |
| 8 | Reference still reproduces the stored traces | PASS | `baseline.py check` on all 70 entries with the reference program: equals the stored SHA-256 for 70 of 70. |

## Not done / limits

- No human play test (D-008; offered only as optional).
- The release zip and `tools\d16_gate_check.ps1` were not re-run: no release was asked for. `tools\make_release.ps1` text now says the program has no emulator but uses Stella's colour table (GPL kept).
- `tools\test_quick.ps1` does not exist yet (Phase H).
- One slip, repaired: running `baseline.py check` with the native program overwrote the local golden traces; they were regenerated with the reference and match the stored hashes (item 8). CLAUDE.md now warns about it.
