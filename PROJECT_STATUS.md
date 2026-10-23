Current stage: C1 — Install the reference emulator (Phase C)

Last updated: 2026-10-24, end of Stage B9. Phase A complete (Gate A met). Phase B complete (Gate B met, `docs\GATE_B.md`).

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
- What is still assumed, and where it gets checked: `docs\DISCOVERY_REPORT.md`.

## Open problems

- None blocking.
- Git exists since B5. Every stage now ends with one commit. `rom\` and `reference\` stay local only: `rom\EXPECTED_SHA1.txt` is therefore not in Git, and its value is also recorded in `docs\ROM_IDENTITY.md`.

## Pending decisions

- None right now. D-002 was decided in B7 (Stella 7.0).

## Next stage

C1 — Install the reference emulator (first stage of Phase C).

**This needs you:**
- Download the official Stella Windows zip, `Stella-7.0c-windows.zip` (4,255,411 bytes), from `https://github.com/stella-emu/stella/releases/tag/7.0`, the version you chose in D-002.
- Unzip it to `F:\Ports\Tools\Stella`. That is outside the project folder, and it is a "portable" copy, so nothing is installed into Windows.

Claude Code gives every click, records the zip's fingerprint, and checks that Stella opens to its game list.

## Handoff from the last session

Stage B9 done: **Gate B is met, and Phase B (development environment) is complete.**
- All six checklist items passed, each re-checked from scratch with evidence (`docs\GATE_B.md`):
  - the tools work, shown by a fresh full build of both libraries;
  - your ROM is unchanged and locked;
  - the ROM has never entered Git, by name or by content;
  - the memory files and rules are intact;
  - the Git history is healthy;
  - the first backup is valid.
- With your OK, an end-of-phase backup was made: `pacman2600-2026-10-24-Gate-B.zip`.

New: `docs\GATE_B.md`. Updated: `PLAN.md` (B9 entry, Done), `PROJECT_STATUS.md`. Outside the project: the new Gate-B zip and its `.sha256`. Local only: `build\gate_b\` (the fresh test build, ignored by Git). Nothing installed, nothing deleted, nothing pushed, ROMs untouched.
