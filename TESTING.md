# Testing (Stage H1, 2026-10-26)

Every automated test and proof of the project, what it proves, the command, and how long it takes. **Times are measured on the owner's PC in Stage H1** (one run, one test at a time, Release builds); "not timed" means H1 did not run it. Rule 4: a test that is not listed as run in H1 is not claimed to pass by this page.

`python -I tools\h1_testing_check.py` fails if a test program or a tool in `tools\` is missing from this page.

## What "needs the ROM" means

The project's own game needs no ROM to run (D-009). Most proofs compare the game with the **original**, which only exists as the player's ROM, run in the reference program (`pacman2600_ref.exe`, Stella inside). Those tests need the ROM and the local folder `reference\` (traces made from it, never in Git). They run on the owner's PC only. Tests marked **no ROM** run from files in Git and can run on a public build machine (the H4 set).

## The two test commands (Stage H3)

Run from the project folder in PowerShell. Neither builds in the quick case; both print PASS/FAIL per step and end with `result: OK` or `result: FAILED` (exit code 0 / 1; 2 = cannot run because the ROM, `reference\` or Python is missing). Every step keeps its full output in `build\test_runs\<time>-<kind>\logs` (a new folder every run; nothing is deleted).

| Command | Use it | What it runs | Time |
| --- | --- | --- | --- |
| `powershell -NoProfile -ExecutionPolicy Bypass -File tools\test_quick.ps1` (uses `test_common.ps1`) | after every stage | the unit tests of sections 1, the rule checks of section 5, the short proofs (f10, f11, f13 to f17, f18 and f19 with `--cached`, f20 to f24 short ones) and five key game traces (BL-001, BL-035, BL-044, BL-067, BL-070) | 155 s (35 steps; 103 s and 33 steps before H7 added the preservation check and its drill) |
| `powershell -NoProfile -ExecutionPolicy Bypass -File tools\test_full.ps1 [-SkipBuild] [-Slow]` | at gates, before a release | builds both programs, the quick set, then f12, f21_routines, f23_tia, f23_pictures, f23_collisions, f24_sound, f25_games (120 random + 70 recorded), the key replay, all 70 baseline games and the host checks (`tools\host_checks.sh`, needs Git's bash; windows open briefly). `-Slow` adds the full f18 and f19 | 1,704 s = 28 min (48 steps; measured in H7; 1,590 s and 46 steps in H3; all 70 baseline games 403 s) |

The five key traces use five different inputs: BL-046 and BL-049 replay the same input file as BL-035 (the bot route) and give the identical trace, so they add nothing as a second trace.

## The public test set (Stage H4)

`powershell -NoProfile -ExecutionPolicy Bypass -File tools\test_public.ps1 [-Build]` runs only what needs **no ROM and no `reference\` folder**, so a stranger who clones the repository can run it, and the GitHub workflow `.github\workflows\public-tests.yml` runs the same command. It needs Python and the test programs built by `tools\build_lockstep.cmd` (`-Build` does that).

| Step | What it proves without a ROM |
| --- | --- |
| `test_tables` (no argument) | the 34 embedded tables: sizes, order, no overlap, lookup by address, 508 bytes in total (the comparison with the cartridge needs the ROM and is left to the full set) |
| `test_game_modules` | the 83 rule checks of H2 |
| `test_layers` for the same 11 recorded games (Stage I9.1) | the picture in two layers (the game's picture and the same without Pac-Man and the ghosts): the game's own picture and sound stay the recorded ones with the second layer made alongside, the layers differ only in the maze rows (and a few rows past its bottom wall), and the picture without the moving objects stays the same when Pac-Man and the ghosts are moved about in the state while the picture with them changes |
| `test_objects_view` for the same 11 recorded games (Stage I9.2) | the description of the five moving objects (`game::viewObjects`): drawn on the picture without them, the objects of the frame (Pac-Man and the ghost of the frame, power pills above Pac-Man) give the game's own picture pixel for pixel in every frame (40,000 frames); the ghosts that are not in the frame are described as the picture showed them on their last turn (place and rows at least 99%, colour 98%, shape 85%) |
| `test_lookahead` for BL-001, 035, 044 and 070 (Stage I9.2) | the look-ahead (`game::lookAhead`, 4 frames with the controls held): the game's picture and sound are still the recorded ones with a look-ahead made after every frame, and when the recorded controls stay the same the objects of each of the next 4 frames are exactly as predicted (all five objects: place, rows, colour, shape, mirroring); how many predictions the real changes of controls spoiled is counted |
| `test_smooth` for BL-001, 035, 044 and 070 (Stage I9.3) | the smooth motion (`game::Smoother`): with gliding off and only the objects the original draws, its rectangles on the picture without the moving objects are the game's own picture in every frame; with gliding on, no object is ever shown behind the original (along each axis it is between the place the original shows now and the place it shows when the object next changes, and never moves backwards inside a step), an object that changes at the next step is at its new place exactly at the end of this one, and the place at the end of a step is the place the next one starts with (no jumps between steps, except where the game itself jumps) |
| `test_smooth` ... `100000 1000` (Stage I9.6) | the same, with the two extra picture layers switched on at frame 1000 of the game (as the window does when the frame rate is changed in the middle of a game): the layers made from a copy give the game's own picture in every frame from then on |
| host check: the game at frame rates 60, 144 and Unlocked (Stage I9.6) | in `tools\host_checks.sh` (only `pacman2600`): the same recording played in the real window at the three rates gives the same trace, frame for frame: input, switches, picture and sound fingerprints and all 128 bytes of memory |
| `test_no_rom` for 11 recorded games (BL-001, 003, 035, 044, 047, 048, 062, 063, 067, 068, 070; 40,000 frames in all) | the game, in a program with no ROM code in it, replays the tracked input and gives the same picture and sound fingerprint on every frame as the original did: title screen and Select, a whole first level, the power pill and eaten ghosts, the end of the level and the new map, a game to game over, two players, both console switches, random joystick and buttons, ghosts walking home |
| `g2_conventions_check --strict` | file, function and line limits; `ORIGINAL:` tags |
| `g7_gate_check --no-rom` | no ROM or archive in Git; only the start-up check opens a file; Rule 7 text; no markers; architecture complete (checks 2 and 3 of the gate need the ROM and are skipped, and the output says so) |
| `h1_testing_check` | this page lists every test and tool |
| `h7_preservation_check` | the frozen preservation files (`tests\preservation\MANIFEST.txt`) are present and unchanged; the manifest only grew; where Git and the tag `1.0.0-preservation` exist, the frozen files equal the tag's (otherwise those two checks are skipped and the output says so) |

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
| `test_inputlog` | `build\host\test_inputlog.exe <existing scratch folder>` | recording and replay file format: 28 checks (25 without a folder) | **no ROM** | under 1 s |
| `test_trace` | `build\host\test_trace.exe` | the trace line format and fingerprints: 18 checks | **no ROM** | under 1 s |
| `test_windowstate` | `build\host\test_windowstate.exe <scratch folder>` | remembered window position and fullscreen: 29 checks | **no ROM** | under 1 s |
| `test_menu` | `build\host\test_menu.exe` | the Escape menu (Stage I5): the 16 Select steps and game numbers, the presses needed from every state to every game, the table of start levels and Pac-Man speeds, the keys, every explanation fits, the Select and Reset presses of a new game, and the part shared with the game thread (Stage I8: the three pages; Stage I9.5: the frame rate and counter rows; the rates the window's menu goes through with and without smooth motion; Stage I10: the HDR row and the highlights slider): 322 checks | **no ROM** | under 1 s |
| `test_background` | `build\host\test_background.exe` | Stage I7: the seven backgrounds (setting names round trip, wrap-round), the maze colour, the blurred grid, the score and lives read from the game's memory, the mirror tiles, and the Graphics options page of the menu (open, change, hide, back, Escape) | **no ROM** | under 1 s |
| `test_framerate` | `build\host\test_framerate.exe` | Stage I9.4: the six frame rates (30, 60, 75, 120, 144, Unlocked): names round trip, the list goes round, which are smooth, the moments pictures are made (every 1/rate second of the clock, whatever the game step), how far through a game step a picture is, 30 shows every second step: 35 checks | **no ROM** | under 1 s |
| `test_hdr` | `build\host\test_hdr.exe` | Stage I10.1: the HDR decisions and arithmetic (when the HDR pipeline is used, the ceiling of the highlights, the slider, never above the screen's headroom) | **no ROM** | under 1 s |
| `pacman2600 --hdr-report` and `--hdr-check` | see `tools\host_checks.sh` (`--hdr-check` only with `pacman2600`) | Stage I10.1: what the screen says about HDR (Windows HDR on or off, white level, headroom) and a try of a renderer in the HDR colour space; and the game's picture through the standard renderer and the HDR one, pixel by pixel: every pixel in a flat area of the picture is the same (the pixels on an edge between two colours may differ, the HDR renderer blends in linear light); reported as skipped when Windows is not in HDR mode on the computer; and the same recording played in the real window with `--hdr on` and `--hdr off` gives the same trace; `--hdr-highlights <png prefix> <rom> --window WxH --settings <scratch ini>` (Stage I10.3): the same moment of a game drawn with the slider at 0, 50 and 100: against the plain picture only the moving objects differ (6,200 of 1,228,800 window pixels, each lit channel exactly the scale of 2.5 or 4 times), nothing is brighter than the ceiling of 4 times white, the plain picture is never above white; the HDR rows of Game options are worked through the real window by `--push-menu` (`hdr=off`, `hdr_highlights=60` remembered) | needs the ROM | 2 s and 2 x 8 s |
| `pacman2600 --backdrop-check` | `build\host\pacman2600.exe --backdrop-check <png prefix> rom\pacman.a26 --window WxH --settings <scratch ini>` (also with `pacman2600_ref.exe`; run at 1280x1024, 1920x1080 and 1000x1000) | every background drawn in the real window 300 frames into a game: the picture's rectangle holds exactly the pixels Black gives (0 different), the space outside is filled as far as the background is meant to fill it (Black stays black); the Solid colour's pixel is the maze's blue (not the tan of the walls); one PNG per background is saved for looking at | needs the ROM | 3 s each |
| `pacman2600 --menu-check` | `build\host\pacman2600.exe --menu-check <report> rom\pacman.a26 --settings <scratch ini>` (also with `pacman2600_ref.exe`) | the menu and the real game together, no window: three starting states (title screen, game in play, two players chosen with Select) times games 1 to 8, then (Stage I6) all eight games for two players from the title screen, and a two-player game played on one set of controls: player 2 must walk with the same input as player 1 (and not walk when the controls are not shared); the report gives a hash of every picture, which is the same in both programs. The menu is opened and worked with real keys, the game stands still while it is open, and afterwards the game's memory shows that game number, one player, the start level the menu shows, the switches as chosen and Pac-Man's speed as the menu says; the recorded inputs replay to the same picture and memory: 34 scenarios | needs the ROM | 7 s |
| `tools\i6_shared_replay.py` | `python -I tools\i6_shared_replay.py rom\pacman.a26 <scratch folder> [frames] [seed]` | Stage I6: a recorded two-player game with seeded random joystick moves (with and without the recording's `shared_controls=1` line) replayed with no window in `pacman2600.exe` and `pacman2600_ref.exe`: picture and sound of all 6,000 frames equal in both, player 2's turn happened and the game's joystick byte took a direction in it, and without the line the game differs once player 2 plays | needs the ROM | 10 s |
| `pacman2600 --smooth-shot` and `--fps` | see `tools\host_checks.sh` (only `pacman2600`; the reference program has no smooth motion) | Stage I9.4: a game started in the real window with smooth motion, the picture drawn at the start, the middle and the end of three steps (nine PNGs): the moving objects differ between the start and the end of a step and the picture without them does not; and `--timed 6` at frame rates 30, 60, 144 and unlocked: the pictures a second shown are 30, 60, within 85 % of the lesser of 144 and the screen's refreshes, and within 85 % of the refreshes | needs the ROM | 3 s and 4 x 8 s |
| `pacman2600 --menu-shot` and `--push-menu` | see `tools\host_checks.sh` | the menu drawn in the real window (text pixels found), and worked through the window's event queue: opened three times (Stage I9.5: the Game options rows for the frame rate and the counter are worked with the keys and remembered in settings.ini: 75 and on, or 30 and on in the reference program), 0 frames run while open, game 3 with the left difficulty A started from it, closed with Escape, quit from the menu | needs the ROM | 3 s and 8 s |
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
| `tools\d16_gate_check.ps1` | see CLAUDE.md | a release zip unpacked in a new folder passes 33 checks (run in H5 on the zip made in a clean copy: `-Zip <that zip> -Work out\gate-h5`; it also needs the local `recordings\` and `build\stella_probe`, so run it from the real project, not from a clean copy) | needs the ROM (by path) | not timed |

## 5. Repository and rule checks

| Tool | What it proves | ROM | Time |
| --- | --- | --- | --- |
| `g2_conventions_check.py --strict` | file, function and line limits; every `ORIGINAL:` tag links to evidence | no ROM | under 1 s |
| `g7_gate_check.py` | no ROM or cartridge code in Git or the program; tables equal the list; Rule 7 text; architecture complete (`--no-rom`: the six checks that need no ROM) | ROM optional | 2 s |
| `h1_testing_check.py` | this page lists every test | no ROM | under 1 s |
| `h7_preservation_check.py [--add] [--drill <new folder>]` (Stage H7) | the preservation suite is frozen (see `tests\preservation\README.md`): 151 listed files (70 baseline entries, 70 input files, 11 fingerprint files) unchanged, the manifest only grows, the 142 files that exist at the tag `1.0.0-preservation` equal it, new files are reported. `--drill` damages copies in a scratch Git repository six ways (edit, edit with the manifest, delete, shorten the manifest, add an unlisted file, then `--add`) and each must be caught | no ROM | check 5 s; the drill 39 s (measured in H7) |
| `g5a_data_list.py`, `g5b_make_tables.py`, `g5c_make_fingerprints.py` | generators of the table list, `tables.cpp` and the fingerprint files; they rewrite files, so not run as tests | needs the ROM | not timed |
| `f20_gen_tables.py`, `f23_dump.py`, `f24_dump.py`, `f24_cycles.py`, `f26_palette.py` | generators and helpers of the proofs above | needs the ROM | not timed |
| `tools\make_release.ps1`, `tools\make_checkpoint.ps1` | release and backup scripts (the release scan refuses ROM-like files; `make_release.ps1 -TestScan` proves the scan on invented zips, 6 cases) | no ROM | `-TestScan` 2 s; the release in the clean copy about 1 minute |
| `h5_zip_code_scan.py <zip> rom\pacman.a26` | looks inside a release zip and the source zip in it for any of the 3,368 eight-byte runs of cartridge program code (raw and as `0x..` numbers) and for the ROM itself; a zip holding one such run is refused (control) | needs the ROM and `reference\f6` | under 1 minute |
| `tools\h6_performance.ps1 [-Seconds 60]` | speed, sound underruns, frame time, lateness and start-up on the real monitor and five pretend monitors (`--present-hz`); pictures identical across monitor speeds; results in `docs\PERFORMANCE.md` | needs the ROM and a sound device (muted) | about 7 minutes (6 cases of 60 s) |

## The clean-build check (Stage H5)

Done by hand in H5, following `BUILDING.md`: (1) copy the files that would be committed into a new folder `build\clean_build\<time>` (no `rom\`, `reference\`, `build\`, `out\`), run `git init` and `git add -A` there; (2) `tools\build_lockstep.cmd` and `tools\build_host.cmd` there (10 s and 71 s); (3) `tools\test_public.ps1` there (16 of 16); (4) `pacman2600.exe --self-check`; (5) `git commit` there, `tools\make_release.ps1 -TestScan` and `tools\make_release.ps1` (scan of 3,381 files clean); (6) `h5_zip_code_scan.py` on the zip; (7) `d16_gate_check.ps1` from the real project on that zip. What a clean copy cannot run: anything that needs the ROM, `reference\`, `recordings\` or the Stella-based tools in `build\stella_probe` (the lockstep proofs, the gate check's last two steps).

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
- **Performance on other machines** (planned: H6). The **clean build from `BUILDING.md`** was done once in H5 (by hand, see above), not on a second PC, not on a different compiler version, and not on GitHub.
- `tools\test_quick.ps1` and `tools\test_full.ps1` (planned: H3).

## Totals, as measured in H1

The timed ROM-free unit tests take under 5 seconds together. The timed proofs of sections 2 and 3 take about 17 minutes; the host checks 2 minutes. H3 will choose the quick set from this page.
