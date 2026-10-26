# Stage G4: moving code into the modules (running record)

One section per sub-stage. Plan and reasons: `PLAN.md` (G4), T-G4-1. Layout: `docs\G1_MODULE_LAYOUT.md`. Every sub-stage is a pure move: the proofs below must give the same results as before.

## G4.1 — data and state (2026-10-26)

| Before | Now |
| --- | --- |
| `Cartridge` class in `routines.hpp` | `src\game\data\cartridge.hpp` |
| 49 raw table addresses in `cart.at(0xFE13 + ...)` | named in `src\game\data\rom_tables.hpp` (37 tables), used as `cart.at(rom::kMazeWalls + ...)` |
| `src\game\cartridge_data.hpp/.cpp` | `src\game\data\cartridge_data.hpp/.cpp` (`git mv`) |
| `src\game\state.hpp` | `src\game\state\state.hpp` (`git mv`) |
| `gCycles`, `cy()` in `routines.hpp` | `src\game\state\cycles.hpp` |
| `TiaChip`, `SoundChip`, `chip::` in `routines.hpp` | `src\game\chips.hpp` |

Paths updated in: both `CMakeLists.txt`, `native_emulator.cpp`, `native_main.cpp`, `src\testsupport`, `tests\game`, `tools\g3_state_layout.py`, `tools\f21_routines.py`, `tools\f22_loader.py` (texts they print).

Evidence (final build, 2026-10-26): `tools\build_lockstep.cmd` and `tools\build_host.cmd` BUILD OK; `g3_state_layout.py` 128 of 128; `f21_native`, `f21_routines`, `f22_loader`, `f20_lockstep`, `f23_kernel`, `f23_tia`, `f23_pictures`, `f23_collisions`, `f24_audio`, `f24_sound` each `problems: []`, `result: OK`. `f25_games --games 120` and `d10_replay_compare` are run before the last sub-stage and at the end of G4 (nothing in G4.1 touches game logic: names, includes and file places only).

## G4.2 — leaf logic (2026-10-26)

| Module (`src\game\logic\`) | Functions | Lines (hpp/cpp) |
| --- | --- | --- |
| `players` | `clearScores`, `swapPlayers`, `newGameSetup` | 21 / 32 |
| `maze` | `newMaze` | 14 / 18 |
| `objects` | `locateObject`, `moveObject`, `setDirection`, `tryDirection`, `tryPair`, `wrapToBottom`, `wrapToTop` | 74 / 194 |
| `animation` | `animateObject` | 27 / 89 |
| `scoring` | `addScore` (with `bcdAdd`) | 21 / 40 |

`routines.cpp` 1,329 to 1,007 lines, `routines.hpp` 274 to 131. Moved by script (text unchanged, original order), then the build lists in `src\lockstep\CMakeLists.txt` and `src\host\CMakeLists.txt` got the five files. One real change: `swapPlayers` is `std::swap(st.player[0], st.player[1])`; the cycle count line is unchanged. Still in `routines.*`: `rngStep`, `placeObjects` (to `flow`, G4.4), the ghost, sound, status-line, Pac-Man, top, bottom and frame-start code.

## G4.3 — status line and sound logic (2026-10-26)

| File | Functions | Lines (hpp/cpp) |
| --- | --- | --- |
| `picture\status_bar` | `livesMarkers`, `scoreDigits` | 27 / 76 |
| `sound\sound_logic` | `soundStart`, `soundEffects`, `soundMusic` | 30 / 104 |

`routines.cpp` 1,007 to 827 lines, `routines.hpp` 131 to 101. Same method as G4.2 (script, text unchanged). Still in `routines.*`: `rngStep`, `placeObjects`, `ghostDecide`, `ghostUpdate`, `pacmanKeepGoing`, `topLogic`, `bottomLogic`, `frameStart`, `startUp`, `ConsoleInput`, `FrameInput`.

## G4.4 — ghosts, Pac-Man, collisions, flow (2026-10-26)

| File (`src\game\logic\`) | What is in it | Lines (hpp/cpp) |
| --- | --- | --- |
| `ghosts` | `rngStep`, `ghostDecide` (with its `Decider`), `ghostUpdate` | 32 / 125 |
| `flow` | `placeObjects`, `startUp`, `eventTimerStep`, `frozenSpeedSetup` | 28 / 127 |
| `collisions` | `bonusOrPill`, `ghostTouch` | 16 / 120 |
| `pacman` | `pacmanKeepGoing`, `pacmanBottom`, `pacmanMove`, `joystickAndMove` | 24 / 254 |
| `bottom` | `bottomLogic`, `playSounds`, `finishFrame`, `BottomContext` | 32 / 65 |
| `maze` (added) | `eatWafer`, `newMapCheck`, `mazeWall` | 23 / 71 |
| `src\game\inputs.hpp` | `ConsoleInput`, `FrameInput` | 25 |

`routines.cpp` 827 to 161 lines (top logic and frame start remain), `routines.hpp` 101 to 46. Reasons and the one new function: T-G4-2.
Evidence: both builds OK; the ten proofs `problems: []`, `result: OK`; `f25_games --games 120`: 120 new games (2,400,000 frames) and the 70 recorded games identical in memory, picture and sound, `result: OK`; `d10_replay_compare`: 350,460 key frames, 0 differences.

## G4.5 — top logic, frame start, routines.* removed (2026-10-26)

| File (`src\game\logic\`) | What is in it | Lines (hpp/cpp) |
| --- | --- | --- |
| `top` | `topLogic` with its helper functions | 21 / 111 |
| `frame_start` | `frameStart` | 21 / 62 |

`routines.cpp` and `routines.hpp` are deleted (`git rm`, the owner allowed it for G4). `game.hpp`, `kernel.hpp`, `renderer.hpp`, `renderer.cpp`, `kernel.cpp` and the lockstep adapters now include the headers they use. After G4.5 `src\game` has no file over 400 lines except `kernel.cpp` (646, G4.6); `pacman.cpp` is the largest logic file (254).
Evidence: both builds OK; the ten proofs `problems: []`, `result: OK`; 128 of 128 layout checks; `f25_games --games 120` (2,400,000 new frames + 70 recorded games) `result: OK`; `d10_replay_compare` 350,460 key frames, 0 differences.
