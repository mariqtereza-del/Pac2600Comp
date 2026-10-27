# Stage F20: the lockstep harness (2026-10-26)

`python -I tools\f20_lockstep.py rom\pacman.a26` runs the harness over all 70 recorded games and tests the harness itself. This document is written by that tool.

## Result: the harness works

| Check | Result |
| --- | --- |
| The original program run by the harness's own 6502 core, compared with the golden memory after every frame | **70 games, 439680 frames, all 128 bytes identical in every frame** (29.9 s of work in all) |
| The same with a natively written `RNG_STEP` in place of the original routine | identical in all 70 games |
| The same with a deliberately broken `RNG_STEP` | stopped at the first difference in 70 games; byte and routine named correctly in 70. Example: `BL-001: first difference: frame 4, byte $CB (RNG_STATE): golden $81, ours $00; last written by RNG_STEP (native, broken on purpose)` |
| The same without the collision-flag oracle (BL-035) | found a difference: `first difference: frame 2288, byte $9A (PILL_FLAGS): golden $ED, ours $EC; last written by MAIN/bottom logic (pc $F5B8)` |

## What the harness is

The goal of Phase F is a game written in our own code that behaves exactly like the original. The harness lets us get there **one routine at a time** and know at once when a rewrite is wrong:

- `src\lockstep\machine.*` is a small 6502 core that runs the original program bytes (read from the player's ROM; never copied into the repository) over the 128 bytes of console memory. It models the **inputs** (joysticks, switches, fire), the **collision flags** and the **frame boundary** (the write that switches the vertical sync off); it does not model time (the timer always reads 0, `WSYNC` does nothing) and does not make the picture or the sound. The picture kernel still runs, because it writes scratch bytes of the memory that are part of the comparison.
- Any routine of the F8 list can be **replaced by a native C++ routine**: `Machine::registerNative(entry, name, function)`. The function reads and writes the memory with `read`/`write` and the registers `a, x, y, p`, and ends with `rts()`. From then on the harness runs the native routine whenever the original program calls that address.
- After every frame all 128 bytes are compared with the golden trace. **On the first different frame it stops and prints the frame, the byte (with its F7 name), both values, and the routine that wrote the byte last** (a routine of the F8 list, a part of the main loop, or the native routine) with the address of the instruction.
- `tools\build_lockstep.cmd` builds `build\lockstep\lockstep.exe` (no Stella and no third-party code). Run it on one game: `lockstep.exe rom\pacman.a26 baseline\BL-035.inputs reference\baseline\BL-035.trace --oracle reference\f20\oracle_BL-035.txt [--native rng] [--poke frame:addr:value] [--all]`.

## Facts the harness needed

- **The frame boundary is the write that switches the vertical sync off (`STX VSYNC` at `$F87C`).** A golden row is the memory at that moment; the inputs of frame f are in place from the start of frame f. With this rule row 0 (from the power-on reset with zeroed memory) and all later rows match.
- **Collision flags.** The game reads them from the chip, which here would need the picture. Until the native renderer exists (F23), the harness takes them from an **oracle**: the values the game read in the emulator (F12 logs, `reference\f20` is made from them by this tool). F18 proved that the flags equal the overlap of the two sprites, so F23 replaces the oracle by a computation.
- **Decimal mode** (the score) is done in the core; the 6502 stack lives in the memory bytes `$F6-$FF` and is compared too.
- The core is only as big as this program needs: no interrupts, no unofficial opcodes (the harness stops with a message if one is met).

## How the next stages use it (F21)

For every routine of the F8 list, smallest first: write the native version, register it under its entry address, run `tools\f20_lockstep.py`-style checks on all 70 games; the routine is accepted when every game stays identical. When a routine is wrong, the first difference points at the byte and, through the last-writer record, at the routine. Routines that drop the caller's return (the TRY_ helpers) must pop the extra address in their native version (`rts()` twice).

## Not claimed

- Picture and sound are not compared here (F23 and F24 have their own comparisons); nor is timing: the cycle-exact reference of the picture code is `tools\f18_cpu.py`.
- Player 2's joystick is not an input of the recordings (F9), so it is not exercised.
