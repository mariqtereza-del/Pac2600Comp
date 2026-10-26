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
