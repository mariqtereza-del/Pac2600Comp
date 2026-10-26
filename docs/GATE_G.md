# Gate G: the source is readable and the game stands alone (Stage G7, 2026-10-26)

**Verdict: PASS.** The game in `src\game` is split into small modules, proved identical to the original, reads no data from the ROM, and a stranger could read it. The build at this commit is tagged `1.0.0-preservation`. **That tag is the permanent baseline: modern features may never change what the game does at that tag** (CLAUDE.md, Rule 3).

| # | Gate item | Result | Evidence |
| --- | --- | --- | --- |
| 1 | Lockstep fully green | PASS | On the final code: `f20_lockstep`, `f21_routines`, `f21_native`, `f22_loader`, `f23_kernel`, `f23_tia`, `f23_pictures`, `f23_collisions`, `f24_audio`, `f24_sound` each `problems: []`, `result: OK`; `f25_games --games 120`: 2,400,000 new frames plus the 70 recorded games identical in memory, picture and sound, `result: OK`. |
| 2 | Keys and switches | PASS | `d10_replay_compare.py`: `TOTAL 350460 frames compared, 0 differences`. |
| 3 | The game needs no ROM | PASS | `test_no_rom` (a program with no ROM code in it) reproduces the pictures and sounds of BL-001 (700 frames) and BL-035 (3,000 frames), 0 frames differ. `g7_gate_check` check 5: only `rom_check.cpp` opens a file in `src\game`. |
| 4 | Embedded tables are the listed ones, data only | PASS | `g7_gate_check` check 2: the 34 tables (508 bytes) in `tables.cpp` equal the list in `docs\EMBEDDED_DATA.md`, equal the ROM at those addresses, and no byte is program code in the F6 map. `test_tables`: 1,086 checks, 0 failures. |
| 5 | No cartridge program code anywhere | PASS | `g7_gate_check` check 3: none of 3,368 windows of 8 code bytes is in `pacman2600.exe` or in the 3,650 tracked files; check 1: no ROM, archive or `reference\` file in Git. |
| 6 | The native program holds no emulator | PASS | check 4: none of five Stella marker names in `pacman2600.exe` (the reference program has 3 of 5). |
| 7 | Rule 7 reads as the owner approved it | PASS | check 6: `CLAUDE.md` equals the quoted text in `DECISIONS.md` D-009. |
| 8 | No file over the size limit | PASS | `g2_conventions_check.py --strict`: `result: CLEAN` (12 approved exceptions, each with its reason). |
| 9 | Every `ORIGINAL:` tag links to evidence | PASS | the same checker: 8 tags, all evidence paths exist. No `LITERAL`/`TODO`/`FIXME` marker (check 7). |
| 10 | `ARCHITECTURE.md` shows the modules | PASS | rewritten with a module diagram; check 8: it names every source file of `src\game` and `src\host`. |
| 11 | A stranger can read it | PASS | Stage G6: 4 fresh agents, `docs\G6_STRANGER_TEST.md`. |
| 12 | Unit tests and 128 state-layout checks | PASS | `test_rom_check`, `test_tables` result OK; `g3_state_layout.py` exit 0. |
| 13 | Both programs and the window | PASS | `build\host` checks for `pacman2600` and `pacman2600_ref` (self-check, find-rom, headless, layout, fullscreen, audio, pad, keys, unit tests): `ALL DONE fail=0`. |
| 14 | Builds | PASS | `tools\build_lockstep.cmd` and `tools\build_host.cmd`: BUILD OK. |

## Not done / limits

- No human play test (D-008; offered only as optional).
- The release zip and `tools\d16_gate_check.ps1` were not re-run: no release was asked for (Phase J). `tools\make_release.ps1` still names version 0.1.0.
- `tools\test_quick.ps1` does not exist yet (Phase H).
- The scan in check 3 looks for runs of 8 code bytes written as bytes or as `0x..` numbers; code hidden in another form (compressed, split by other text) would not be found. Nothing suggests it exists, and the program's data comes only from `tables.cpp`.
- Stranger test: agents of the same model family; `docs` were not tested.
- The unit-test programs `test_tia_video`, `test_tia_audio` and `test_kernel` need arguments and are run by the proofs `f23_tia`, `f24_audio` and `f23_kernel`, not on their own.
- The proof tools rewrite the time they took in some proof documents (`docs\F2x_*.md`); those lines changed in this commit.
