Current stage: B7 — Fetch the libraries (Phase B)

Last updated: 2026-10-23, end of Stage B6. Phase A complete (Gate A met).

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

## Key facts so far

- PC: Windows 11 Pro 25H2, RTX 5070, 2560×1440 at 180 Hz, 86.7 GB free on F:. Python 3.14.0 present.
- ROM: original 1982 USA Pac-Man, plain 4 KB, no bank switching, starts at $F000.
- ROM protection (B4):
  - The original is read-only.
  - A read-only spare is in `F:\Ports\ROM-originals\`.
  - The expected SHA-1 is in `rom\EXPECTED_SHA1.txt`.
  - If a tool ever says "access denied" on the original, that is the protection working. Never remove the flag to "fix" it.
- Git for Windows 2.56.0.windows.3 (B1). VS Build Tools 2026 18.11.1 at `C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools` (B2). The compiler, CMake and Ninja only work inside the Developer environment (`VsDevCmd.bat -arch=x64 -host_arch=x64`), never on the normal PATH. That is intended.
- C++23 on this compiler means `/std:c++23preview` (T-B3-2). `/std:c++23` is silently ignored, and the code is then built as C++14. Watch for this when building Stella's core (B7).
- What is still assumed, and where it gets checked: `docs\DISCOVERY_REPORT.md`.

## Open problems

- None blocking.
- Git exists since B5. Every stage now ends with one commit. `rom\` and `reference\` stay local only: `rom\EXPECTED_SHA1.txt` is therefore not in Git, and its value is also recorded in `docs\ROM_IDENTITY.md`.

## Pending decisions

- **Stella version (D-002), needed by B7.** The plan reserves D-002 for C1, but B7 already downloads Stella's source "at the same release tag as the C1 reference app". So B7 will ask you to approve the Stella version first. The latest release noted in D-001 is 7.0, and B7 re-checks it.

## Next stage

B7 — Fetch the libraries. Claude Code downloads pinned versions of SDL3 and the Stella source into `third_party\`, each with its licence, and records them in `docs\DEPENDENCIES.md`.

**This needs your approvals:**
- the Stella version (D-002, brought forward from C1);
- the downloads themselves, after Claude Code shows you their names, official sources and sizes.

Nothing is installed: they are source files kept inside the project.

## Handoff from the last session

Stage B6 done:
- `CLAUDE.md` now holds your 12 engineering rules. A script compared them with the text you pasted and found them identical.
- It also holds the working preferences from the resume prompt and the technical facts learned so far.
- `docs\GLOSSARY.md` explains about 70 project terms in plain English.
- `PLAN.md`, `PROJECT_STATUS.md` and `DECISIONS.md` were cross-checked and agree.
- Line 1 of this file now reads "Current stage: B7".

New: `CLAUDE.md`, `docs\GLOSSARY.md`. Updated: `DECISIONS.md` (T-B6-1), `PLAN.md` (B6 entry, Done), `PROJECT_STATUS.md`. Nothing installed, nothing deleted, nothing pushed, ROMs untouched.
