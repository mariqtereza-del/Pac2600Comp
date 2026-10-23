Current stage: C2 — First run of the original (Phase C)

Last updated: 2026-10-24, end of Stage C1. Phase A complete (Gate A met). Phase B complete (Gate B met, `docs\GATE_B.md`).

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
- Git exists since B5. Every stage now ends with one commit. `rom\` and `reference\` stay local only: `rom\EXPECTED_SHA1.txt` is therefore not in Git, and its value is also recorded in `docs\ROM_IDENTITY.md`.

## Pending decisions

- None right now. D-002 was decided in B7 (Stella 7.0).

## Next stage

C2 — First run of the original. The first time the game itself runs.

**This needs you:**
- Claude Code starts Stella with the working copy `rom\pacman.a26`, with your OK.
- You play for about one minute with the keyboard, and with your PS4 controller if it's connected.
- Then you tell Claude Code in a sentence whether it looked and sounded right.

Claude Code also reads and records Stella's game-properties entry for this ROM (cartridge type, TV format, display settings), and checks that nothing was written next to the ROM.

## Handoff from the last session

Stage C1 done: the reference emulator is ready.
- **It's genuine:** the Stella folder you unzipped (`F:\Ports\Tools\Stella-7.0c`) is identical, file for file, to Stella's official 7.0c download. I downloaded that zip again with your OK and compared all 96 files.
- **Launcher:** the new script `tools\run_stella.cmd` starts Stella so that it keeps all its settings in its own folder, not in your Windows profile. This was proved on the first start.
- **First start:** you saw the game list and closed it normally. No game was opened yet.

New: `docs\REFERENCE_EMULATOR.md`, `tools\run_stella.cmd`. Updated: `CLAUDE.md`, `DECISIONS.md` (T-C1-1, T-C1-2), `docs\GLOSSARY.md`, `PLAN.md` (C1 entry, Done), `PROJECT_STATUS.md`. Local only: `reference\_downloads\Stella-7.0c-windows.zip`. Outside the project, created by Stella itself: its settings in `F:\Ports\Tools\Stella-7.0c`. Nothing installed into Windows, nothing deleted, nothing pushed, ROMs untouched.
