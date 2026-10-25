# Stage F21: the routine ledger (2026-10-26)

`python -I tools\f21_routines.py rom\pacman.a26` checks every rewritten routine and writes this ledger. **F21 is not one session: it is one routine per session** (F21.1, F21.2, ...). This document is the list, kept up to date by each session.

## How a routine is rewritten (the protocol)

1. The routine is written as a C++ function in `src\game\routines.cpp`, working on the 128 memory bytes with the names of F7 (`src\game\ram_map.hpp`, generated). No ROM data and no emulator code is allowed there; things the original reads from the cartridge come later through the data loader (F22).
2. An adapter in `src\lockstep\natives.cpp` registers it under the entry address of the original and reproduces what the original leaves in the registers and flags (the callers may depend on them).
3. `lockstep.exe ... --native <name> --verify` runs all recorded games with the native routine in place of the original. It checks (a) all 128 bytes after every frame and (b) at every call, the native results against the original code run on a copy (memory, `A`, `X`, `Y`, stack pointer, return address, flags). Both must be clean in all 70 games.
4. Only then is the routine marked done here and in `PLAN.md`, and the stage is committed. Fixtures that are wrong on purpose prove the checks bite.

## Result of this run: all checks clean

- **CLEAR_SCORES**: identical in 70 of 70 games; 959 calls compared with the original code, 0 different.
- **NEW_MAZE**: identical in 70 of 70 games; 1026 calls compared with the original code, 0 different.
- **PLACE_OBJECTS**: identical in 70 of 70 games; 1319 calls compared with the original code, 0 different.
- **WRAP_TO_BOTTOM**: identical in 70 of 70 games; 40 calls compared with the original code, 0 different.
- **WRAP_TO_TOP**: identical in 70 of 70 games; 40 calls compared with the original code, 0 different.
- **WRAP_SET_Y**: identical in 70 of 70 games; 40 calls compared with the original code, 0 different.
- **RNG_STEP**: identical in 70 of 70 games; 155820 calls compared with the original code, 0 different.
- **SWAP_PLAYERS**: identical in 70 of 70 games; 139 calls compared with the original code, 0 different.
- The fixture "wrong on purpose" (forgets `LIVES_P2`) was found by the memory comparison in 68 of the 68 games that call the routine; the fixture with a wrong `X` register leaves every frame identical but was found by the routine check in 68 games: this is why the routine check exists.

## The ledger

| Stage | Routine | Entry | Bytes | Status | Note |
| --- | --- | --- | --- | --- | --- |
| F21.4 | `WRAP_SET_Y` | $FD3C | 3 | **done** | the shared end of both (written inside wrapToTop/wrapToBottom; never entered on its own) |
| F21.4 | `WRAP_TO_TOP` | $FD32 | 10 | **done** | game::wrapToTop: direction bits $F0 and y = $0B (the opening in the top wall) |
| F21.4 | `WRAP_TO_BOTTOM` | $FD26 | 12 | **done** | game::wrapToBottom: direction bits $30 and y = $51 (the opening in the bottom wall); registered together with WRAP_TO_TOP and the shared end WRAP_SET_Y |
| F21.2 | `SWAP_PLAYERS` | $FB9F | 14 | **done** | game::swapPlayers: the 20-byte player blocks $88-$9B and $9C-$AF exchanged |
| F21.1 | `CLEAR_SCORES` | $FC7D | 14 | **done** | game::clearScores in src/game/routines.cpp: both scores and both lives to 0 |
| F21.3 | `RNG_STEP` | $FBE0 | 20 | **done** | game::rngStep: the shift-register random number; its bit-mask table is 0x80 >> n (checked equal to the cartridge bytes) |
| F21.5 | `NEW_MAZE` | $FC0C | 21 | **done** | game::newMaze: the wafer map filled (two places empty), pill flags $3C, then PLACE_OBJECTS |
| F21.5 | `PLACE_OBJECTS` | $FC21 | 36 | **done** | game::placeObjects: freeze on, timer $3C, Pac-Man and the ghosts at their start places |
| - | `EXIT_F9E4` | $F9E4 | 1 | to do | a lone RTS: nothing to write |
| - | `EXIT_FB5B` | $FB5B | 1 | to do | a lone RTS: nothing to write |
| - | `EXIT_FF93` | $FF93 | 1 | to do | a lone RTS: nothing to write |
| - | `KERNEL_LINE_P1_P0` | $F067 | 3 | to do | picture only (F23) |
| - | `SOUND_WRITE` | $FF8B | 8 | to do | sound chip only (F24) |
| - | `SET_DIRECTION` | $F9E5 | 9 | to do | table TBL_FE63 (F22); falls into MOVE_OBJECT |
| - | `TRY_PAIR_1` | $FAFC | 9 | to do | drops the caller's return (rts twice) |
| - | `TRY_PAIR_2` | $FB19 | 9 | to do | drops the caller's return |
| - | `CLEAR_GRAPHICS` | $F483 | 11 | to do | picture only (F23) |
| - | `TRY_UP` | $FB36 | 14 | to do |  |
| - | `SOUND_START` | $FF43 | 18 | to do | needs ROM tables (F22) |
| - | `KERNEL_LINE_P1` | $F053 | 20 | to do | writes only chip registers (picture): proved with the renderer (F23) |
| - | `TRY_PAIR_UD` | $FB05 | 20 | to do | drops the caller's return |
| - | `TRY_PAIR_LR` | $FB22 | 20 | to do | drops the caller's return |
| - | `TRY_DOWN` | $FB5C | 20 | to do |  |
| - | `KERNEL_LINE_P0` | $F06A | 21 | to do | picture only (F23) |
| - | `LIVES_MARKERS` | $FC68 | 21 | to do | needs ROM tables (F22) |
| - | `TRY_COMMIT` | $FB44 | 23 | to do | drops the caller's return |
| - | `TRY_LEFT` | $FB70 | 23 | to do |  |
| - | `TRY_RIGHT` | $FB87 | 24 | to do |  |
| - | `NEW_GAME_SETUP` | $FBF4 | 24 | to do |  |
| - | `ADD_SCORE` | $FC45 | 35 | to do | needs the amount tables of the cartridge (F22) |
| - | `PACMAN_KEEP_GOING` | $F793 | 39 | to do | drops the caller's return |
| - | `LOCATE_OBJECT` | $FBAD | 51 | to do |  |
| - | `SOUND_EFFECTS` | $FF55 | 54 | to do | needs ROM tables (F22); chip writes (F24) |
| - | `MOVE_OBJECT` | $F9EE | 56 | to do | ends in the border tests; leaf |
| - | `GHOST_UPDATE` | $F9AA | 58 | to do | after GHOST_DECIDE |
| - | `SOUND_MUSIC` | $FF94 | 85 | to do | needs ROM tables (F22); chip writes (F24) |
| - | `SCORE_DIGITS` | $F3EE | 149 | to do | needs ROM tables (F22) and the picture |
| - | `ANIMATE_OBJECT` | $FC8B | 155 | to do |  |
| - | `GHOST_DECIDE` | $FA26 | 214 | to do | large; after the TRY_ routines |

Order for the next sessions: smallest first among the routines whose result shows in the memory (the picture-only and sound-chip-only routines wait for F23/F24; those that need cartridge tables wait for F22 or use a table checked equal to the ROM): SWAP_PLAYERS, RNG_STEP, the WRAP routines, NEW_MAZE and PLACE_OBJECTS, LOCATE_OBJECT, MOVE_OBJECT, the TRY routines, ANIMATE_OBJECT, then the ghost routines. The main loop (`MAIN`: start-up, top and bottom logic, kernel) is cut into routines in the last sessions of F21.
