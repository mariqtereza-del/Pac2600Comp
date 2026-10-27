# Stage F21.27: the whole game as native code (2026-10-26)

`python -I tools\f21_native.py rom\pacman.a26` runs the game of `src\game` with **no 6502 core and no original program code**: `game::Game::runFrame` (the start-up and the frame start on the first frame; later the top logic, the bottom logic and the frame start; the drawing is the renderer's job, F23). The ROM is read only to give the game its cartridge data by address. The inputs come from the recordings and the two collision flags from the oracle of F20 (until the renderer computes them).

## Result: identical

- **70 of 70 recorded games, 439680 frames: all 128 memory bytes identical after every frame**, except the bytes that are not game state: the stack bytes `$F6-$FF` (the native game has no stack) and the 11 scratch bytes of the drawing code (F21.26): `$87 CELL_INDEX`, `$C9 CELL_ROW`, `$D2 TEMP_D2`, `$D3 TEMP_D3`, `$D4 TEMP_D4`, `$D5 TEMP_D5`, `$D6 TEMP_D6`, `$D8 SPRITE0_Y`, `$DA TEMP_DA`, `$E1 TEMP_E1`, `$E2 TEMP_E2`.
- With nothing left out but the stack bytes, the bytes that differ in some frame are exactly those scratch bytes:  (outside the list: none).
- Running time: 9.0 seconds of work for the 439680 frames (48795 frames a second per core), so the game logic is far faster than needed.

## What this ends

F21 (rewrite one routine per session) is complete: every routine and every part of the main loop of the original (F8, F5) is now written in C++ in `src\game` and proved against the original with the lockstep tool; the two together run a whole game without the original program. What the tests still lean on from outside the game code: the **oracle** for the collision flags (to be replaced by the renderer of F23, which computes them from the sprites: F18), the **cartridge data view** (to be filled by the loader of F22 from the player's ROM), and the **chip** interface (the renderer of F23 and the sound generator of F24 stand behind it).

## Not claimed

- The picture and the sound are not made here; nothing is drawn or heard (F23, F24).
- The collision flags are still given; the 11 scratch bytes are left out of the comparison on the evidence of the recordings (F21.26).
