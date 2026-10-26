# Testing (Stage H1, 2026-10-26)

Every automated test and proof of the project, what it proves, the command, and how long it takes. **Times are measured on the owner's PC in Stage H1** (one run, one test at a time, Release builds); "not timed" means H1 did not run it. Rule 4: a test that is not listed as run in H1 is not claimed to pass by this page.

`python -I tools\h1_testing_check.py` fails if a test program or a tool in `tools\` is missing from this page.

## What "needs the ROM" means

The project's own game needs no ROM to run (D-009). Most proofs compare the game with the **original**, which only exists as the player's ROM, run in the reference program (`pacman2600_ref.exe`, Stella inside). Those tests need the ROM and the local folder `reference\` (traces made from it, never in Git). They run on the owner's PC only. Tests marked **no ROM** run from files in Git and can run on a public build machine (the H4 set).

## The two test commands (Stage H3)

Run from the project folder in PowerShell. Neither builds in the quick case; both print PASS/FAIL per step and end with `result: OK` or `result: FAILED` (exit code 0 / 1; 2 = cannot run because the ROM, `reference\` or Python is missing). Every step keeps its full output in `build\test_runs\<time>-<kind>\logs` (a new folder every run; nothing is deleted).

| Command | Use it | What it runs | Time |
| --- | --- | --- | --- |
| `powershell -NoProfile -ExecutionPolicy Bypass -File tools\test_quick.ps1` (uses `test_common.ps1`) | after every stage | the unit tests of sections 1, the rule checks of section 5, the short proofs (f10, f11, f13 to f17, f18 and f19 with `--cached`, f20 to f24 short ones) and five key game traces (BL-001, BL-035, BL-044, BL-067, BL-070) | 103 s (33 steps) |
| `powershell -NoProfile -ExecutionPolicy Bypass -File tools\test_full.ps1 [-SkipBuild] [-Slow]` | at gates, before a release | builds both programs, the quick set, then f12, f21_routines, f23_tia, f23_pictures, f23_collisions, f24_sound, f25_games (120 random + 70 recorded), the key replay, all 70 baseline games and the host checks (`tools\host_checks.sh`, needs Git's bash; windows open briefly). `-Slow` adds the full f18 and f19 | 1,590 s = 26.5 min (46 steps; measured in H3; all 70 baseline games 370 s) |

The five key traces use five different inputs: BL-046 and BL-049 replay the same input file as BL-035 (the bot route) and give the identical trace, so they add nothing as a second trace.

## The public test set (Stage H4)

`powershell -NoProfile -ExecutionPolicy Bypass -File tools\test_public.ps1 [-Build]` runs only what needs **no ROM and no `reference\` folder**, so a stranger who clones the repository can run it, and the GitHub workflow `.github\workflows\public-tests.yml` runs the same command. It needs Python and the test programs built by `tools\build_lockstep.cmd` (`-Build` does that).

| Step | What it proves without a ROM |
| --- | --- |
| `test_tables` (no argument) | the 34 embedded tables: sizes, order, no overlap, lookup by address, 508 bytes in total (the comparison with the cartridge needs the ROM and is left to the full set) |
| `test_game_modules` | the 83 rule checks of H2 |
| `test_no_rom` for 11 recorded games (BL-001, 003, 035, 044, 047, 048, 062, 063, 067, 068, 070; 40,000 frames in all) | the game, in a program with no ROM code in it, replays the tracked input and gives the same picture and sound fingerprint on every frame as the original did: title screen and Select, a whole first level, the power pill and eaten ghosts, the end of the level and the new map, a game to game over, two players, both console switches, random joystick and buttons, ghosts walking home |
| `g2_conventions_check --strict` | file, function and line limits; `ORIGINAL:` tags |
| `g7_gate_check --no-rom` | no ROM or archive in Git; only the start-up check opens a file; Rule 7 text; no markers; architecture complete (checks 2 and 3 of the gate need the ROM and are skipped, and the output says so) |
| `h1_testing_check` | this page lists every test and tool |

Not in the public set: the lockstep proofs, `test_rom_check`, the comparison of the tables with the cartridge, the scan for cartridge code (all need the ROM), and the host program tests (`test_controls`, `test_layout` and the others need SDL built; planned for H5). The fingerprint files (`tests\game\data\BL-nnn.fingerprints.txt`, made by `tools\g5c_make_fingerprints.py` from the reference traces) hold hashes of the output only. The workflow file has never run on GitHub (nothing is pushed); whether the build script finds the compiler there is untested.

## 1. Unit tests (small, made-up or fixed data)

| Test | Command | What it proves | ROM | Time |
| --- | --- | --- | --- | --- |
| `test_rom_check` | `build\lockstep\test_rom_check.exe rom\pacman.a26` | the start-up check accepts only 4,096 bytes with the right SHA-1 and gives a plain message for every wrong case | needs the ROM as the good example | under 1 s |
| `test_tables` | `build\lockstep\test_tables.exe [rom\pacman.a26]` | the 34 embedded tables (508 bytes) equal the ROM; 1,086 lookups (without a ROM argument: structure only) | ROM optional | under 1 s |
| `test_no_rom` | `build\lockstep\test_no_rom.exe baseline\BL-001.inputs tests\game\data\BL-001.fingerprints.txt` (11 games, see the public set) | a program with no ROM code reproduces the pictures and sounds of recorded games from stored fingerprints | **no ROM** | 0 to 3 s each |
| `test_tia_video` | run by `f23_tia.py` | the video-chip model against stored emulator pictures | needs `reference\f23` | inside f23_tia |
| `test_tia_audio` | run by `f24_audio.py` | the sound-chip model against stored sound | needs `reference\f24` | inside f24_audio |
| `test_kernel` | run by `f23_kernel.py` | the drawing code gives the emulator's chip writes | needs the ROM and `reference\f23` | inside f23_kernel |
| `test_game_modules` | `build\lockstep\test_game_modules.exe` | 83 checks of the rules, with made-up states and values worked out from the written rules (not copied from the code): decimal score and carry, ghost and wafer points, player in turn; object location in the grid; movement, its borders and speed seeds; directions and the openings; turns; new maze (126 wafers, 4 pills, start places); new map (one life, next level, caps); players, start level, score colours; the random number (lead-in, period 127, never 0); the start-up | **no ROM** | under 1 s |
| `test_controls` | `build\host\test_controls.exe` | keyboard and pad rules: 119 checks | **no ROM** | under 1 s |
| `test_layout` | `build\host\test_layout.exe` | where the picture goes in every window mode: 231 checks | **no ROM** | under 1 s |
| `test_inputlog` | `build\host\test_inputlog.exe <existing scratch folder>` | recording and replay file format: 25 checks (22 without a folder) | **no ROM** | under 1 s |
| `test_trace` | `build\host\test_trace.exe` | the trace line format and fingerprints: 18 checks | **no ROM** | under 1 s |
| `test_windowstate` | `build\host\test_windowstate.exe <scratch folder>` | remembered window position and fullscreen: 29 checks | **no ROM** | under 1 s |
| `test_rom_finder` | `build\host\test_rom_finder.exe <scratch folder> rom\pacman.a26` | the ROM search and its refusal of wrong files | needs the ROM | under 1 s |
| `g3_state_layout.py` | `python -I tools\g3_state_layout.py rom\pacman.a26` | the 128 layout checks of the game state | needs the ROM to regenerate | under 1 s |

The scratch folder must exist before `test_inputlog` runs: a missing folder gives two false failures (seen again in H1).

## 2. Proofs against the original (lockstep)

Run on the owner's PC. Each says `result: OK`. Together they are the proof that the native game equals the original.

| Tool (`python -I tools\NAME.py rom\pacman.a26`) | What it proves | Time |
| --- | --- | --- |
| `f20_lockstep` | the harness runs the original 6502 code equal to the golden memory in 70 games (439,680 frames) | 9 s |
| `f21_routines` | each rewritten routine equals the original call by call | 128 s |
| `f21_native` | the whole native game, memory identical in 70 games | 2 s |
| `f22_loader` | the ROM check, and no cartridge bytes outside the tables | 5 s |
| `f23_tia`, `f23_kernel`, `f23_pictures`, `f23_collisions` | video chip model; drawing code; every picture of 70 games; collision flags | 268 s, 11 s, 34 s, 34 s |
| `f24_audio`, `f24_sound` | sound chip model; sound of every frame of 70 games | 26 s, 74 s |
| `f25_games --games 120` | 120 seeded random-input games (2.4 million frames) plus the 70 recorded: memory, picture and sound identical | 218 s |
| `d10_replay_compare` | all recorded key input through the real key layer: 350,460 frames, 0 differences | 134 s |

## 3. Findings checks (the behaviour rules, predicted from the traces)

Each re-derives a rule of the game from the golden traces and checks it on every frame: `f10_pacman` (1 s), `f11_maze` (2 s), `f12_collisions` (102 s), `f13_ghosts` (3 s), `f14_random` (under 1 s), `f15_scoring` (3 s), `f16_levels` (2 s), `f17_states` (1 s), `f18_kernel` (`--cached` 5 s; about 15 minutes without), `f19_sound` (`--cached` 4 s), `f9_input`, `f8_routines`, `f7_ram`, `f5_frame`, `f6_coverage` (replays all 70 games with an instruction log), `f4_tables`, `f3_names`, `f2_disasm` (rebuilds the ROM byte-identical from the listing), `f1_coverage`, `f1_6502` (`selftest`), `f18_cpu` (helper of f18). Not timed in H1: `f9_input`, `f8_routines`, `f7_ram`, `f5_frame`, `f6_coverage`, `f4_tables`, `f3_names`, `f2_disasm`, `f1_coverage`, `f1_6502`, `f18_kernel` without `--cached`, `f19_sound` without `--cached`. All need the ROM and `reference\`.

## 4. Baseline games, side by side and window checks

| Test | Command | What it proves | Time |
| --- | --- | --- | --- |
| `baseline.py check` | `python -I tools\baseline.py check rom\pacman.a26 BL-nnn` | a fresh trace of one of the 70 recorded games equals the stored SHA-256 (run with the reference program only) | BL-001 under 1 s, BL-049 8 s; all 70 games 370 s (in the full command) |
| `e2_entries`, `e3_entries`, `e4_entries`, `e5_entries`, `e6_entries`, `e7_entries`, `e8_entries` | `python -I tools\eN_entries.py rom\pacman.a26` | remake the baseline entries BL-002 to BL-070 from the traces (they overwrite the entry texts: run only on purpose) | not timed |
| `d14_side_by_side` | see CLAUDE.md | the same input in Stella's own script runner and in our program gives the same fingerprints and pictures | not timed |
| `c3_report`, `c4_report`, `c5_report`, `c6_experiments`, `c7_captures`, `raw_to_png`, `a4_structure` | see `docs\` | Phase A-C measuring and reporting tools; read the reference emulator's output | not timed |
| host checks (`--self-check`, `--find-rom`, `--headless`, `--layout-check`, `--fullscreen-check`, `--audio-check`, `--pad-check`, `--keys-replay`, `--help-check`, unit tests) for both programs | the script of Stage G4.7 | window, sound device, pad and key layers work in both programs | 118 s, `fail=0` |
| `tools\d16_gate_check.ps1` | see CLAUDE.md | a release zip unpacked in a new folder passes 31 checks | not run in H1 (no release) |

## 5. Repository and rule checks

| Tool | What it proves | ROM | Time |
| --- | --- | --- | --- |
| `g2_conventions_check.py --strict` | file, function and line limits; every `ORIGINAL:` tag links to evidence | no ROM | under 1 s |
| `g7_gate_check.py` | no ROM or cartridge code in Git or the program; tables equal the list; Rule 7 text; architecture complete (`--no-rom`: the six checks that need no ROM) | ROM optional | 2 s |
| `h1_testing_check.py` | this page lists every test | no ROM | under 1 s |
| `g5a_data_list.py`, `g5b_make_tables.py`, `g5c_make_fingerprints.py` | generators of the table list, `tables.cpp` and the fingerprint files; they rewrite files, so not run as tests | needs the ROM | not timed |
| `f20_gen_tables.py`, `f23_dump.py`, `f24_dump.py`, `f24_cycles.py`, `f26_palette.py` | generators and helpers of the proofs above | needs the ROM | not timed |
| `tools\make_release.ps1`, `tools\make_checkpoint.ps1` | release and backup scripts (the release scan refuses ROM-like files) | no ROM | not run in H1 |

## Which test covers which module (Stage H2)

| Module or subject | Covered by |
| --- | --- |
| maze lookups: wall table, new maze, wafers, new map | `test_game_modules`, `test_tables` (the table bytes) |
| score table and decimal scoring | `test_game_modules` |
| speed tables and seeds, movement | `test_game_modules` |
| turns, directions, openings | `test_game_modules` |
| random number | `test_game_modules` |
| players, start level, start-up | `test_game_modules` |
| ROM loader and check | `test_rom_check`, `test_rom_finder` |
| scaling maths and picture layout | `test_layout` |
| video chip, sound chip, drawing code | `test_tia_video`, `test_tia_audio`, `test_kernel` (through the proofs) |
| ghost decisions, collisions, sounds, the bottom and top logic | the proofs of section 2 and the findings checks of section 3 (they need the ROM); no stand-alone unit test yet |

## What is not tested

- **Real hardware.** There is no real console or capture card. The assumption, written down on purpose: **Stella 7.0 is accurate for this game.** The original behaviour is whatever Stella's core does with the ROM; where it is known to differ from a 1981 NTSC console (speed, sound rate, power-on memory) the project follows the console (D-004, `DISCREPANCIES.md`).
- **A person playing.** Never required (D-008).
- **Performance on other machines**, and a **clean build from the README steps** (planned: H5, H6).
- `tools\test_quick.ps1` and `tools\test_full.ps1` (planned: H3).

## Totals, as measured in H1

The timed ROM-free unit tests take under 5 seconds together. The timed proofs of sections 2 and 3 take about 17 minutes; the host checks 2 minutes. H3 will choose the quick set from this page.
