# Testing (Stage H1, 2026-10-26)

Every automated test and proof of the project, what it proves, the command, and how long it takes. **Times are measured on the owner's PC in Stage H1** (one run, one test at a time, Release builds); "not timed" means H1 did not run it. Rule 4: a test that is not listed as run in H1 is not claimed to pass by this page.

`python -I tools\h1_testing_check.py` fails if a test program or a tool in `tools\` is missing from this page.

## What "needs the ROM" means

The project's own game needs no ROM to run (D-009). Most proofs compare the game with the **original**, which only exists as the player's ROM, run in the reference program (`pacman2600_ref.exe`, Stella inside). Those tests need the ROM and the local folder `reference\` (traces made from it, never in Git). They run on the owner's PC only. Tests marked **no ROM** run from files in Git and can run on a public build machine (the H4 set).

## 1. Unit tests (small, made-up or fixed data)

| Test | Command | What it proves | ROM | Time |
| --- | --- | --- | --- | --- |
| `test_rom_check` | `build\lockstep\test_rom_check.exe rom\pacman.a26` | the start-up check accepts only 4,096 bytes with the right SHA-1 and gives a plain message for every wrong case | needs the ROM as the good example | under 1 s |
| `test_tables` | `build\lockstep\test_tables.exe rom\pacman.a26` | the 34 embedded tables (508 bytes) equal the ROM; 1,086 lookups | needs the ROM | under 1 s |
| `test_no_rom` | `build\lockstep\test_no_rom.exe baseline\BL-001.inputs tests\game\data\BL-001.fingerprints.txt` (also BL-035) | a program with no ROM code reproduces the pictures and sounds of 700 and 3,000 frames from stored fingerprints | **no ROM** | 0 s and 1 s |
| `test_tia_video` | run by `f23_tia.py` | the video-chip model against stored emulator pictures | needs `reference\f23` | inside f23_tia |
| `test_tia_audio` | run by `f24_audio.py` | the sound-chip model against stored sound | needs `reference\f24` | inside f24_audio |
| `test_kernel` | run by `f23_kernel.py` | the drawing code gives the emulator's chip writes | needs the ROM and `reference\f23` | inside f23_kernel |
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
| `baseline.py check` | `python -I tools\baseline.py check rom\pacman.a26 BL-nnn` | a fresh trace of one of the 70 recorded games equals the stored SHA-256 (run with the reference program only) | BL-001 under 1 s, BL-049 8 s; all 70 not timed |
| `e2_entries`, `e3_entries`, `e4_entries`, `e5_entries`, `e6_entries`, `e7_entries`, `e8_entries` | `python -I tools\eN_entries.py rom\pacman.a26` | remake the baseline entries BL-002 to BL-070 from the traces (they overwrite the entry texts: run only on purpose) | not timed |
| `d14_side_by_side` | see CLAUDE.md | the same input in Stella's own script runner and in our program gives the same fingerprints and pictures | not timed |
| `c3_report`, `c4_report`, `c5_report`, `c6_experiments`, `c7_captures`, `raw_to_png`, `a4_structure` | see `docs\` | Phase A-C measuring and reporting tools; read the reference emulator's output | not timed |
| host checks (`--self-check`, `--find-rom`, `--headless`, `--layout-check`, `--fullscreen-check`, `--audio-check`, `--pad-check`, `--keys-replay`, `--help-check`, unit tests) for both programs | the script of Stage G4.7 | window, sound device, pad and key layers work in both programs | 118 s, `fail=0` |
| `tools\d16_gate_check.ps1` | see CLAUDE.md | a release zip unpacked in a new folder passes 31 checks | not run in H1 (no release) |

## 5. Repository and rule checks

| Tool | What it proves | ROM | Time |
| --- | --- | --- | --- |
| `g2_conventions_check.py --strict` | file, function and line limits; every `ORIGINAL:` tag links to evidence | no ROM | under 1 s |
| `g7_gate_check.py` | no ROM or cartridge code in Git or the program; tables equal the list; Rule 7 text; architecture complete | needs the ROM | 2 s |
| `h1_testing_check.py` | this page lists every test | no ROM | under 1 s |
| `g5a_data_list.py`, `g5b_make_tables.py`, `g5c_make_fingerprints.py` | generators of the table list, `tables.cpp` and the fingerprint files; they rewrite files, so not run as tests | needs the ROM | not timed |
| `f20_gen_tables.py`, `f23_dump.py`, `f24_dump.py`, `f24_cycles.py`, `f26_palette.py` | generators and helpers of the proofs above | needs the ROM | not timed |
| `tools\make_release.ps1`, `tools\make_checkpoint.ps1` | release and backup scripts (the release scan refuses ROM-like files) | no ROM | not run in H1 |

## What is not tested

- **Real hardware.** There is no real console or capture card. The assumption, written down on purpose: **Stella 7.0 is accurate for this game.** The original behaviour is whatever Stella's core does with the ROM; where it is known to differ from a 1981 NTSC console (speed, sound rate, power-on memory) the project follows the console (D-004, `DISCREPANCIES.md`).
- **A person playing.** Never required (D-008).
- **Performance on other machines**, and a **clean build from the README steps** (planned: H5, H6).
- `tools\test_quick.ps1` and `tools\test_full.ps1` (planned: H3).

## Totals, as measured in H1

The timed ROM-free unit tests take under 5 seconds together. The timed proofs of sections 2 and 3 take about 17 minutes; the host checks 2 minutes. H3 will choose the quick set from this page.
