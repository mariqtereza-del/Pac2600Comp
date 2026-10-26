# Stage G5: leftovers (2026-10-26)

What a clean-up of the moved source found, what was done and what was kept on purpose. Decisions: T-G5-1, T-G5-2.

## 1. Dead and duplicated code

| Check | Result |
| --- | --- |
| Functions in `src\game` with no caller at all | none (two suspects, `renderDrawn` and `endFrame`, are called from inline code in `renderer.hpp`) |
| Duplicate blocks (8 equal lines) in `src\game` and `src\host` | **0** |
| `LITERAL`, `TODO`, `FIXME` markers | **0** (so Gate G's "no literal blocks" holds) |
| Public game functions that only the test tool calls | `game::scoreDigits`: it did the score line's work in F21.21, but the picture code (F23.2, `kernel_score.cpp`) draws the score line with exact timing and never calls it. **Removed** (T-G5-1). `ghostDecide`, `pacmanKeepGoing`, `rngStep` are also called inside their own files, so they stay. `CartridgeData::loadFile` is the loader's file entry used by the tools. |
| The end shared by Pac-Man's walk and the power pill | already one function since G4.4 (`pacmanMove`) |

## 2. What was done

- **`game::scoreDigits` removed** with its test adapter and its registration; the F21 ledger keeps the row with a note (`tools\f21_routines.py`). The proof of the score line is `f23_kernel`.
- **Quirks tagged** with `// ORIGINAL: ... Evidence: <file> (<stage>). Do not fix.`: 8 places (scared ghosts' mirrored x, the unrun wander path, the 4-of-8 ghost touch, no extra life for a score, five score digits, TRY dropping the caller's return, the level changing only the ghosts, one ghost per frame). The checker verifies each evidence path exists. The ninth known quirk (fire works only after about 500 frames after game over, T-E6-1) is **not tagged** because it has not been located in the code; that is listed, not guessed.
- **Long lines:** 12 comment lines wrapped, comments moved above code in 3 places, 2 code lines wrapped in `src\game`. 0 lines over 160 characters in `src\game`.
- **`src\lockstep\natives.cpp` (649 lines) split** into `natives.cpp` (registration, 144), `natives_frame.cpp` (153), `natives_objects.cpp` (337) and `natives_internal.hpp` (84). Text comparison: 609 non-blank lines, 3 differ (default arguments moved to the header).
- **Checker (`tools\g2_conventions_check.py`)** now also finds functions whose `{` is on the next line (the host's style), scans `src\testsupport`, and lists approved exceptions with reasons. `--strict` now says: files over 400 lines 0, functions over 80 lines 0, lines over 160 in `src\game` 0, tags with a missing evidence path 0; exit 0.

## 3. Kept on purpose (approved exceptions, T-G5-2)

| Where | Lines | Why it stays |
| --- | --- | --- |
| `Kernel::mazeRows` (`kernel_maze.cpp`) | 291 | the original's loop over the labels L_F0F4 to L_F360, written as the jumps it has; cutting it at a label changes the jumps |
| `joystickAndMove` (`pacman.cpp`) | 160 | the original's state machine over the labels L_F6DD to L_F781 |
| `Kernel::scoreLine` (`kernel_score.cpp`) | 124 | one straight run of timed chip writes; splitting only moves lines |
| `playWindow`, `padCheck`, `headlessRun`, `main` (host) | 229, 165, 94, 89 | Phase D code moved unchanged: one game loop with its threads; two test scripts; the start-up sequence |
| `main`, `step`, `registerNativeByName` (lockstep tool) | 138, 174, 113, 110 | test code, not shipped |
| 39 code lines over 160 characters in `src\host`, `src\lockstep`, `src\testsupport` | | working Phase D / test code; reformatting has no value |
| `generated_tables.hpp` | 453 | generated |

## 4. Dependencies

- **The shipped program** (`pacman2600.exe`, 305 KB) links SDL3 and our own code; its file contains no "Stella" and no 6502 core. `pacman2600_ref.exe` (3.1 MB, the reference) is the only program with Stella.
- **`src\game` and `src\testsupport` include no outside library at all**; `src\host` includes SDL3 (`audio`, `controls`, `common.hpp`, `main.cpp`) and Stella only in `emulator.cpp`.
- Python tools use the standard library only (`python -I`). Nothing the game no longer needs is left in the build: Stella stays for the reference program and the measuring tools (D-002), SDL3 for the window, sound and pads.
