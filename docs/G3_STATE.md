# Stage G3: the game state has named fields (2026-10-26)

Before: the native game worked on `std::array<uint8_t,128>` and named bytes by address, `at(ram, ram::GHOST_X1)`, in 357 places. Now it works on `game::GameState` (`src\game\state.hpp`), a plain struct whose fields follow the original's memory order and add up to exactly 128 bytes: `st.objX[2]`, `st.player[0].lives`, `st.score[4]`, `st.colour[kColourMaze]`. T-G3-1 has the reasons.

## What lives where

| Piece | Place |
| --- | --- |
| The state, with its fields | `src\game\state.hpp` (`GameState`, `PlayerBlock`, `Scratch`, `Colour`) |
| The F7 address names (`ram::GHOST_X1 = 0xB3`) | `src\testsupport\ram_map.hpp` (made by `tools\f20_gen_tables.py`) |
| Proof that every field is where the F7 map says | `src\testsupport\state_layout_checks.hpp`: 128 `static_assert`s, made by `python -I tools\g3_state_layout.py` (must say "128 of 128") |
| State to and from the original's 128 bytes | `src\testsupport\state_bytes.hpp` (`toBytes`, `fromBytes`, `byteAt`: one copy) |
| Who includes the test-support folder | the lockstep tool, the host's trace and `--poke` option (`native_emulator.cpp`), `tests\game\test_kernel.cpp`. Not `src\game`. |

## Evidence (all run on the final build, 2026-10-26)

- Both builds (`tools\build_lockstep.cmd`, `tools\build_host.cmd`): 0 errors; the 128 layout checks compile.
- `f21_native`: 70 of 70 games, 439,680 frames identical in all 128 bytes (stack aside). `f21_routines`: every rewritten routine equal to the original call by call, result OK. `f20_lockstep`, `f22_loader`, `f23_kernel`, `f23_tia`, `f23_pictures`, `f23_collisions`, `f24_audio`, `f24_sound`: result OK (the two controls inside them still find their deliberate faults).
- `f25_games --games 120`: 120 new random games (2,400,000 frames) and the 70 recorded ones identical in memory, picture and sound. `d10_replay_compare`: 350,460 key frames, 0 differences.
- Before/after direct comparison: the committed version (built in a scratch checkout) and the new build both give IDENTICAL, nothing left out, on BL-001 (700 frames), BL-035 (9,800) and BL-050 (30,000).
- Search: `src\game` holds no `ram::`, no `Ram`, no `ram_map` and no include of `testsupport`; the remaining `0x....` numbers there are cartridge-table addresses.
- The old `src\game\ram_map.hpp` was deleted with the owner's OK (it stays in Git history).

## What is not finished (on purpose)

- The picture code keeps its scratch bytes `$D2-$E3` as one array `scratch[]` with named places, because the original gives the same bytes several jobs (digit pointers over the sprite set-up bytes). G5 may look again.
- The kernel is still a register-by-register copy of the 6502 code (`A`, `X`, `Y`); that is G4/G5 work, not state work.
- The old doc lines in `docs\F21_NATIVE.md` that listed 11 differing scratch bytes were stale since F23.2; the tools rewrote them truthfully (none differ). Five other proof documents changed only in timings and file counts.
