# Provenance of every file in Git (Stage J3)

Made by `python -I tools\j3_provenance.py rom\pacman.a26`; do not edit by hand. It lists what Git holds (3809 files), not what is only on this PC (section 5).
The categories follow the plan: our own code, ROM-derived material, emulator code, third-party libraries, documentation, build tools and generated files, plus test data, measuring tools and settings.
This is a factual list, not legal advice (see `docs\LEGAL_BRIEFING.md`; the owner's position is D-028).

## 1. Summary

| Category | Files | Bytes | Licence or origin | What it is |
| --- | --- | --- | --- | --- |
| own-code | 192 | 963,856 | ours; GPL v2 or later if published (D-027) | the program: the rewritten game, the window program and the tests of both |
| rom-derived-data | 1 | 8,382 | ours in form, Atari's in content (D-009) | the 512 bytes of game tables the program needs; the only ROM-derived material in Git |
| generated | 18 | 1,917,423 | made by a tool of ours from our own inputs or from third-party sources | files written by a script; never edited by hand |
| emulator | 1057 | 25,881,425 | Stella 7.0, GPL v2 or any later version, unchanged (D-002) | the reference emulator, used by the test tools and the reference program only |
| emulator-tool | 8 | 67,833 | ours, links Stella (GPL) | the measuring tools that run the original game inside Stella |
| third-party | 2208 | 49,938,758 | SDL3 (zlib) and the libretro shader sources (per file: GPL v2 or later, public domain) | libraries and shader sources, unchanged |
| build-tool | 89 | 800,361 | ours | scripts that build, test, release and check the project |
| test-data | 147 | 311,928 | ours | recorded joystick inputs, entries and frozen files of the preservation suite (no ROM bytes) |
| documentation | 85 | 2,739,082 | ours | the notes and documents of the project |
| config | 4 | 3,702 | ours | Git and GitHub settings |
| **total** | 3809 | 82,632,750 |  |  |

Every tracked file is in exactly one category, and no file fits no rule (the tool stops otherwise).

## 2. The only ROM-derived material in Git

`src/game/data/tables.cpp` holds the **512 bytes in 34 tables** that the program needs and that the cartridge holds (`docs\EMBEDDED_DATA.md`, D-009), written as source text. It is data only; Gate G proves the tables equal the ROM's and that no run of cartridge program code is anywhere in the tracked files or in `pacman2600.exe`.
The cartridge file itself and its program code are not in Git, not in the release and not in any test file (Rule 7).

## 3. Notes and code that name ROM addresses

A mention is an address between `$F000` and `$FFFF` (or `0xF...`, or a label `L_F...`): the places in the cartridge's program that our notes and comments refer to. A hex run is a line with nine or more hex bytes in a row. Neither contains the cartridge's code as bytes; they are evidence that these files were written by studying it.

### Documentation: 34 files, 925 address mentions, 1 hex-run lines

| File | Address mentions | Hex-run lines |
| --- | --- | --- |
| `docs/EMBEDDED_DATA.md` | 172 | 0 |
| `docs/F8_ROUTINES.md` | 144 | 0 |
| `DECISIONS.md` | 108 | 0 |
| `PLAN.md` | 94 | 0 |
| `docs/F21_ROUTINES.md` | 56 | 0 |
| `docs/F7_RAM_MAP.md` | 50 | 0 |
| `docs/F6_COVERAGE.md` | 44 | 0 |
| `docs/ROM_IDENTITY.md` | 35 | 1 |
| `docs/F9_INPUT.md` | 33 | 0 |
| `CLAUDE.md` | 24 | 0 |
| `docs/F4_DATA_TABLES.md` | 18 | 0 |
| `docs/F12_COLLISIONS.md` | 16 | 0 |
| `docs/F14_RANDOM.md` | 14 | 0 |
| `docs/F18_KERNEL.md` | 13 | 0 |
| `docs/G1_MODULE_LAYOUT.md` | 13 | 0 |
| `docs/F16_LEVELS.md` | 12 | 0 |
| `PROJECT_STATUS.md` | 11 | 0 |
| `docs/F19_SOUND.md` | 11 | 0 |
| `docs/F5_FRAME_LOOP.md` | 11 | 0 |
| `docs/F15_SCORING.md` | 10 | 0 |
| `docs/F13_GHOSTS.md` | 4 | 0 |
| `docs/F17_STATES.md` | 4 | 0 |
| `docs/F23_KERNEL.md` | 4 | 0 |
| `docs/G5_LEFTOVERS.md` | 4 | 0 |
| `docs/CONVENTIONS.md` | 3 | 0 |

(9 more files with fewer mentions are not listed.)

### Our own code (comments such as `ORIGINAL: $Fxxx`): 43 files, 463 address mentions, 0 hex-run lines

| File | Address mentions | Hex-run lines |
| --- | --- | --- |
| `src/game/logic/pacman.cpp` | 74 | 0 |
| `src/game/picture/kernel_maze.cpp` | 70 | 0 |
| `src/game/picture/kernel_score.cpp` | 61 | 0 |
| `src/lockstep/natives.cpp` | 42 | 0 |
| `src/lockstep/natives_frame.cpp` | 30 | 0 |
| `src/lockstep/natives_objects.cpp` | 24 | 0 |
| `src/game/logic/objects.hpp` | 16 | 0 |
| `src/game/picture/kernel_internal.hpp` | 13 | 0 |
| `src/game/sound/sound_logic.hpp` | 9 | 0 |
| `src/game/picture/kernel_positions.cpp` | 8 | 0 |
| `tests/game/test_tables.cpp` | 8 | 0 |
| `src/game/logic/flow.cpp` | 7 | 0 |
| `src/game/logic/top.cpp` | 7 | 0 |
| `src/lockstep/machine.cpp` | 7 | 0 |
| `src/game/logic/frame_start.hpp` | 6 | 0 |
| `src/game/logic/animation.hpp` | 5 | 0 |
| `src/game/logic/bottom.cpp` | 5 | 0 |
| `src/game/logic/collisions.cpp` | 5 | 0 |
| `src/game/logic/top.hpp` | 5 | 0 |
| `src/game/logic/bottom.hpp` | 4 | 0 |
| `src/game/logic/flow.hpp` | 4 | 0 |
| `src/game/logic/maze.cpp` | 4 | 0 |
| `src/game/logic/objects.cpp` | 4 | 0 |
| `src/game/sound/sound_logic.cpp` | 4 | 0 |
| `tests/game/test_kernel.cpp` | 4 | 0 |

(18 more files with fewer mentions are not listed.)

### Build tools: 33 files, 522 address mentions, 0 hex-run lines

| File | Address mentions | Hex-run lines |
| --- | --- | --- |
| `tools/g5b_make_tables.py` | 61 | 0 |
| `tools/f21_routines.py` | 54 | 0 |
| `tools/f7_ram.py` | 54 | 0 |
| `tools/f8_routines.py` | 51 | 0 |
| `tools/f10_pacman.py` | 29 | 0 |
| `tools/f13_ghosts.py` | 24 | 0 |
| `tools/g5a_data_list.py` | 24 | 0 |
| `tools/f9_input.py` | 23 | 0 |
| `tools/f18_kernel.py` | 20 | 0 |
| `tools/f1_6502.py` | 20 | 0 |
| `tools/f18_cpu.py` | 18 | 0 |
| `tools/f19_sound.py` | 18 | 0 |
| `tools/f20_gen_tables.py` | 18 | 0 |
| `tools/f12_collisions.py` | 14 | 0 |
| `tools/f15_scoring.py` | 14 | 0 |
| `tools/f5_frame.py` | 14 | 0 |
| `tools/f2_disasm.py` | 10 | 0 |
| `tools/f16_levels.py` | 9 | 0 |
| `tools/f14_random.py` | 7 | 0 |
| `tools/f4_tables.py` | 5 | 0 |
| `tools/g7_gate_check.py` | 5 | 0 |
| `tools/a4_structure.py` | 4 | 0 |
| `tools/f17_states.py` | 4 | 0 |
| `tools/f23_kernel.py` | 4 | 0 |
| `tools/g2_conventions_check.py` | 4 | 0 |

(8 more files with fewer mentions are not listed.)

### Measuring tools: 1 files, 1 address mentions, 0 hex-run lines

| File | Address mentions | Hex-run lines |
| --- | --- | --- |
| `tools/stella_probe/coverage.cpp` | 1 | 0 |

### Generated files: 2 files, 119 address mentions, 0 hex-run lines

| File | Address mentions | Hex-run lines |
| --- | --- | --- |
| `src/game/data/tables.hpp` | 72 | 0 |
| `src/lockstep/generated_tables.hpp` | 47 | 0 |

### Every line that holds bytes as text (1 lines in 1 files)

These lines quote bytes of the cartridge or of the game's memory as text. None is cartridge program code (Gate G finds no run of it), but any that quote cartridge bytes are ROM-derived material in a note; the owner decides about them at J11.

| File | Line | Starts with |
| --- | --- | --- |
| `docs/ROM_IDENTITY.md` | 153 | `Bytes $FFE9-$FFFF: <bytes>` |

## 4. Binary files that Git holds

Outside `third_party/` (5 files): `docs/images/crt-lottes.png`, `docs/images/crt-off.png`, `docs/images/gameplay.png`, `docs/images/menu-game-options.png`, `docs/images/menu-graphics-options.png`.
Inside `third_party/` (unchanged files of the libraries: pictures, fonts, archives of their own): third_party/SDL3: 121, third_party/shaders: 4, third_party/stella: 98.
No tracked file ends in `.a26 .bin .rom .rar .7z .zip .tar` or `.gz`, except Stella's own `template.dmg.gz` (part of its unchanged source; pinned by SHA-256 in `tools\make_release.ps1`).

## 5. What is only on this PC (not in Git)

`.gitignore` keeps these out: `rom/` (the working copy of the cartridge file), `reference/` (traces, listings and the raw disassembly), `build/`, `out/`, `recordings/`, archives, and any `*.a26` or `*.bin`. The original cartridge file in the top folder and the spare in `F:\Ports\ROM-originals\` are read-only and never tracked.

## 6. Checks

- Gate G (`tools\g7_gate_check.py`): result: OK (9 of 9 checks)
- Unclassified tracked files: 0. ROM-like tracked files: 0.
- ROM-derived data files: `src/game/data/tables.cpp`.

## 7. What this means for a public repository (to be decided in J11)

- **Can go with the code, under the licences above:** `own-code`, `generated`, `build-tool`, `config`, `third-party`, `emulator` (only if the Stella-based tools are part of what is published; the program itself has no emulator in it).
- **ROM-derived data:** `tables.cpp` is needed for the program to run; the owner's position on it is D-028 and the lawyers' advice the owner reports.
- **For the owner to decide at J11:** the documentation that names ROM addresses (section 3). The release zip leaves `docs\` out entirely (T-D15-1). A private repository holds everything above; a public one could hold the same or leave the notes out.
- **Never:** the cartridge file, its code, the raw disassembly and the traces (they are not in Git).
