# Stage F22: the data loader (2026-10-26)

`src\game\data\cartridge_data.hpp/.cpp`: `game::CartridgeData` takes the player's own ROM file (`loadFile`) or a buffer (`load`), refuses it unless it is exactly 4096 bytes **and** its SHA-1 fingerprint is the expected one (`0940fea7f04cdb6d4b90c5ad1a7e344e68f6dbb1`), and keeps a private copy. `view()` gives the `game::Cartridge` that the game reads by address. After a refusal the object holds nothing (all zero), so a wrong file can never be played by accident. The loader has its own small SHA-1 so the game code does not depend on the window program; the file is only opened for reading.

## Result: OK

- Loader tests (`tests\game\test_cartridge_data.cpp`): 16 ok, 0 failed. They check three standard SHA-1 test values, that the right ROM loads and every byte $F000-$FFFF comes back as in the file, that one changed bit is refused (fingerprint), one byte short / long / empty is refused (size), a missing file and a foreign file are refused.
- The native game of F21.27 now takes **all** its cartridge data from the loader (`lockstep_native` no longer reads the file itself): native game, scratch and stack bytes left out: 70 of 70 games identical, 439680 frames, 9.4 s of work.
- `lockstep_native` given a file that is not the cartridge stops with exit code 2 and the message "The ROM file is not 4096 bytes long, so it is not this cartridge.".
- Source scan: 81 files under `src`; runs of 8 consecutive numbers that equal 8 consecutive cartridge bytes (at least 4 different values): **none**. So the source holds addresses and logic, not Atari's data. The scan was first shown to work on a control (8 cartridge bytes written as 0x.., decimal and $.. are all found: True).

## Not claimed

- The window program (`pacman2600`) is not changed: it still starts the emulator from the ROM file with its own ROM finder (D3). The native game will use this loader when the renderer and sound (F23, F24) exist and the emulator is taken out (F26).
- The scan finds copied runs written as numbers; it cannot prove that no data is hidden in some other disguise. The rule of the project (T-F21-1) is that tables are read by address only.
