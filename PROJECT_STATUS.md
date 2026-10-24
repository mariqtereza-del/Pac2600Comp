Current stage: D3 — Finding your ROM (Phase D)

Last updated: 2026-10-24, end of Stage D2. Phase C complete (Gate C met, `docs\GATE_C.md`). Phase A complete (Gate A met). Phase B complete (Gate B met, `docs\GATE_B.md`).

## Stages done

| Stage | Evidence |
| --- | --- |
| A1 — Install Claude Code and make the project folder | Claude Code runs inside `F:\Ports\Pac-Man - Atari 2600`. |
| A2 — Confirm the PC | `docs\ENVIRONMENT.md`, every finding with its command. `PLAN.md` created with 102 stage markers. |
| A3 — Identify the ROM | `docs\ROM_IDENTITY.md`: Pac-Man (USA), 1982, CX2646, 4096 bytes, SHA-1 `0940fea7f04cdb6d4b90c5ad1a7e344e68f6dbb1`, matched in Stella's and No-Intro's databases. `rom\pacman.a26` identical. |
| A4 — Quick structural look | `docs\ROM_IDENTITY.md` "Structure": start $F000, no bank switching, ~87% code / ~13% data (estimate). |
| A5 — Confirm tools and language | `DECISIONS.md` D-001: Microsoft C++ (VS 2026 Build Tools), Git for Windows, SDL3, Stella's own libretro core, Python 3.14.0. |
| A6 — Discovery report | `docs\DISCOVERY_REPORT.md`. Gate A checked item by item against the files (ROM_IDENTITY verdict and structure lines, D-001 answers, PLAN markers A1–A5 Done, 102 markers); SHA-1 of the original unchanged. |
| B1 — Git for Windows | `git version 2.56.0.windows.3` at `C:\Program Files\Git\cmd\git.exe` in a fresh session; commit name/email `mariqtereza-del` / `mariqtereza@gmail.com` (kept as they were, your choice). `docs\ENVIRONMENT.md`, D-001 note in `DECISIONS.md`. |
| B2 — Microsoft C++ compiler and CMake | `vswhere`: Visual Studio Build Tools 2026 18.11.1, complete, with the C++ workload and CMake component. In the Developer environment: `cl` 19.51.36260 for x64 (MSVC 14.51.36231), CMake 4.3.1-msvc1, Ninja 1.13.2. `docs\ENVIRONMENT.md`, D-001 note and T-B2-2 in `DECISIONS.md`. |
| B3 — Prove the compiler works | CMake + Ninja + MSVC built and ran a C++23 `hello.exe`: printed `hello`, compiler 195136260, C++ level 202302, 64-bit, exit code 0. C++23 switch pinned to `/std:c++23preview` (T-B3-2). Scratch folder deleted with your OK. `docs\ENVIRONMENT.md`. |
| B4 — Lock the original ROM | Spare copy at `F:\Ports\ROM-originals\Pac-Man (1981) (Atari).a26` (SHA-1 match). Read-only flag on the original and the spare. Open-for-writing refused by Windows on both. Original unchanged (SHA-1, size, date). `rom\EXPECTED_SHA1.txt` written. `docs\ROM_IDENTITY.md` "Protection", T-B4-1/T-B4-2. |
| B5 — Project skeleton and Git | Folders `src\host`, `src\game`, `third_party`, `tests`, `reference` (local only). `.gitignore` / `.gitattributes` (T-B5-1, T-B5-2). Repository on branch `main` (T-B5-3). `git check-ignore` and `git status --ignored` prove every `.a26` and `rom\` is ignored. First commit `Stage B5: Project skeleton and Git` with no ROM in it (checked by script). |
| B6 — The memory files | `CLAUDE.md`: your 12 rules, identical to your pasted text (scripted comparison, 42 lines), plus working preferences and technical facts. `docs\GLOSSARY.md`. Memory files cross-checked (102 markers, A1–B5 consistent, all cited decisions exist). T-B6-1. |
| B7 — Fetch the libraries | D-002 = Stella 7.0. SDL3 3.4.18 (SHA-256 = GitHub's) and Stella 7.0 (all 1057 files identical to tag 7.0) in `third_party\`. Stella's 459 test ROM files and SDL's AI-instruction files left out (0 ROM-like files). Both build with our toolchain: `SDL3.dll` 3.4.18, `stella_libretro.dll` (25 `retro_*` exports), 0 errors, 0 warnings. `docs\DEPENDENCIES.md`, T-B7-1…4. |
| B8 — Checkpoint script and a restore test | `tools\make_checkpoint.ps1` (refusals tested). Checkpoint `pacman2600-2026-10-24-B8.zip` (83.5 MB, 7105 entries, no ROM, SHA-256 file) in `F:\Ports\Pac-Man - Atari 2600 checkpoints\`. Restore test: all files identical, same `git status`, `git fsck` clean, libraries rebuilt with 0 errors. Test folder deleted with your OK. T-B8-1/T-B8-2. |
| B9 — Gate B | All 6 items PASS, re-checked this session (`docs\GATE_B.md`): tools plus a fresh library build (0 errors), ROM locked (SHA-1, read-only, write refused), no ROM anywhere in Git (by name and by content), memory files and rules intact, history healthy (`git fsck`), B8 checkpoint verified. End-of-phase checkpoint `pacman2600-2026-10-24-Gate-B.zip` made with your OK. |
| C1 — Install the reference emulator | Your folder `F:\Ports\Tools\Stella-7.0c`: all 96 files identical to the official `Stella-7.0c-windows.zip` (re-downloaded with your OK, SHA-256 `44102ccf…4653`). First start through `tools\run_stella.cmd` (`-baseinappdir`): window "Stella 7.0", you saw the game list. Settings stayed in the app folder, none in `%APPDATA%`. `docs\REFERENCE_EMULATOR.md`, T-C1-1/T-C1-2. |
| C2 — First run of the original | Game ran in Stella from `rom\pacman.a26` (window `Stella 7.0: "Pac-Man (1982) (Atari)"`). Your verdict: "Looks, sounds and plays right" (keyboard). Game Properties: "4K detected", "NTSC detected". Database entry recorded. `rom\` unchanged, ROM SHA-1s unchanged. `docs\FIRST_RUN.md`, T-C2-1. |
| C3 — Display facts, measured | `tools\stella_probe` (Stella 7.0 core, T-C3-1), 24,000 frames: 262 lines per frame throughout (frame 0: 45), visible window line 23 + 228 lines, drawn scanlines 34–222 over 160 columns, 9 play colours (ghost flicker ¼ each), 87 overall (idle colour shifting). 13 identical runs. Level change not reached. `docs\DISPLAY.md`. |
| C4 — Timing facts | `stella_timing.exe` (T-C4-1), fingerprints identical to C3. 19,912 cycles/frame (76/line). Stella plays at 60.000 Hz; its NTSC console clock gives 59.923 Hz (+0.128 %). Every frame: VSYNC L259-261, logic, wait to L33, drawing from L34, timer at L195, logic, wait to L258. Spare time at least 6 lines (top), 24 (bottom), no overruns. `docs\TIMING.md`. |
| C5 — Sound facts | `stella_sound.exe` (T-C5-1), idle run identical to C3; 8 runs incl. scripted joystick play. Only 3 sounds: start tune (2 channels, 63 frames), scoring blip (C9 F9, 8 frames, 376/376 with a score change), death sound (C4, AUDF 31->0 rising, about 2.1 s). Attract silent, no background sound. Power pill, ghost eaten, level clear not captured. `docs\SOUND.md`. |
| C6 — Controls and game variations | `stella_script.exe` (T-C6-1), idle script identical to C3. Defaults B/B/Colour. Select: games 1-8 x 1/2 players (16), power-on 1(1P). Reset and fire start a game (fire: from attract/game over only). Directions read into $ED. B&W: game uses greys. Difficulty switches change when the bonus item appears (left P1, right P2). Variations: idle catch timings repeat every 4 games; Pac-Man speed differs. PS4 pad in Stella: stick and a button work. `docs\CONTROLS.md`. |
| C7 — Reference captures | Eight numbered pictures in `reference\screens\` (local only): title, game start, power pill, ghost eaten, death, bonus item, level end, game over, each checked by eye. Made by a search-based bot (`stella_bot.exe`) whose route script `tools\captures\c7_route.txt` is replayed from power-on by `stella_script.exe` (`tools\captures\c7_captures.txt`); two replays byte-identical (frames.csv and all 8 pictures). Level 1 cleared (1 wafer left at frame 7310, map refilled 7311), 4 deaths, game over frame 9391. `docs\CAPTURES.md`, T-C7-1. |
| C8 — Gate C | All 6 items PASS, re-checked this session (`docs\GATE_C.md`): fresh C7 replay reproduced all 8 picture fingerprints and the C7 per-frame log; fresh 6,000-frame idle run has 0 fingerprint differences from C3; ROM SHA-1s unchanged; no ROM or `reference\` file in Git; `git fsck` clean. `docs\REFERENCE.md` (how to recreate the reference, 9 open items). The Stella app folder was not re-read (outside the project). |
| D1 — Architecture note | `ARCHITECTURE.md`: the three parts (host, Stella core, ROM), data flow, planned file table for D2-D13, who decides what (with `docs\` evidence), what Phase 2 replaces, licences, open choices. T-D1-1: static link of `stella_core`, calling `StellaLIBRETRO`. Checked by script: every named path exists or is marked planned, every `docs\` file exists, D2-D13 exist in `PLAN.md`, the five wrapper calls are used by the probes. No code written. |
| D2 — An empty window | First host code: `src\host\main.cpp`, `CMakeLists.txt`, `tools\build_host.cmd` -> `build\host\pacman2600.exe` + `SDL3.dll`. Build 0 errors, 0 warnings. `--self-check`: title "pacman2600 (working name)", 960x720 pixels, centre pixel black, exit 0. Normal start: responding window, close request -> exit 0, no leftover process. Your double-click: black window, closed, no errors. T-D2-1. |

## Key facts so far

- PC: Windows 11 Pro 25H2, RTX 5070, 2560×1440 at 180 Hz, 86.7 GB free on F:. Python 3.14.0 present.
- ROM: original 1982 USA Pac-Man, plain 4 KB, no bank switching, starts at $F000.
- ROM protection (B4):
  - The original is read-only.
  - A read-only spare is in `F:\Ports\ROM-originals\`.
  - The expected SHA-1 is in `rom\EXPECTED_SHA1.txt`.
  - If a tool ever says "access denied" on the original, that is the protection working. Never remove the flag to "fix" it.
- Git for Windows 2.56.0.windows.3 (B1). VS Build Tools 2026 18.11.1 at `C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools` (B2). The compiler, CMake and Ninja only work inside the Developer environment (`VsDevCmd.bat -arch=x64 -host_arch=x64`), never on the normal PATH. That is intended.
- C++23 on this compiler means `/std:c++23preview` (T-B3-2). `/std:c++23` is silently ignored, and the code is then built as C++14. Watch for this when building outside code.
- Libraries (B7):
  - SDL3 3.4.18 and Stella 7.0 are vendored, byte-identical, in `third_party\` (never edit them). Stella 7.0 itself is C++20.
  - One build-time fix is applied to a copy of Stella's `libretro.cxx` (T-B7-4).
  - Rebuild check: `tools\build_third_party.cmd`.
- Checkpoints (B8):
  - `tools\make_checkpoint.ps1 -Stage <ID>` writes zips to `F:\Ports\Pac-Man - Atari 2600 checkpoints\`. This is outside the project, so it needs your OK each time.
  - Zips hold the full project and Git history, but no ROM and no `build\`.
  - To restore, the ROM comes back from `F:\Ports\ROM-originals\`.
- Reference emulator (C1):
  - Stella 7.0c in `F:\Ports\Tools\Stella-7.0c`, proved genuine.
  - Always start it with `tools\run_stella.cmd`, never by double-clicking `Stella.exe`. Settings stay in its own folder.
- What is still assumed, and where it gets checked: `docs\DISCOVERY_REPORT.md`.

## Open problems

- None blocking.
- **Zeroed vs random RAM (C3), for Phase D/E:** Stella's libretro wrapper (used by the probe and by the planned Phase 1 program) starts the game with **zeroed** console RAM. The Stella app starts with **random** RAM. The first frames, or anything that depends on uninitialised RAM, may differ between our program and the "ruler". This must be checked, not assumed, before lockstep comparisons (`docs\DISPLAY.md`).
- **One unexplained crash of `stella_probe` (C3):** an access violation at frame ≈ 23,973 in the very first run. It was not reproduced in 13 further full runs, and the data recorded before it was correct. The probe now reports any crash's location. Watch for it in later stages.
- Git exists since B5. Every stage now ends with one commit. `rom\` and `reference\` stay local only: `rom\EXPECTED_SHA1.txt` is therefore not in Git, and its value is also recorded in `docs\ROM_IDENTITY.md`.

## Pending decisions

- None right now. D-002 was decided in B7 (Stella 7.0).

## Next stage

D3 — Finding your ROM: on first start with no ROM known, a Windows "Open file" dialog asks for the `.a26`; the path is remembered in `settings.ini` next to the `.exe`. Also accepted: a `roms` folder next to the `.exe`, or a path on the command line. Size and fingerprint are checked, with clear messages for the four failure cases. You will be asked to pick your ROM file in the dialog.

## Handoff from the last session

Stage D2 done: **`pacman2600.exe` opens a black 960x720 window and closes cleanly.** You double-clicked it: black, closed, no errors. Window title is a working name (the real name is chosen in J2; "Pac-Man" is a trademark).

New: `src\host\main.cpp`, `src\host\CMakeLists.txt`, `tools\build_host.cmd`. Updated: `DECISIONS.md` (T-D2-1), `ARCHITECTURE.md`, `CLAUDE.md`, `docs\GLOSSARY.md`, `PLAN.md`, `PROJECT_STATUS.md`. Local only: `build\host\`, `reference\d2\`. Nothing installed, deleted or pushed; ROMs untouched.
