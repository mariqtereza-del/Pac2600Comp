# Stage F22, updated in G5c: the ROM check (2026-10-26)

`src\game\data\rom_check.hpp/.cpp`: `game::checkRom` and `checkRomFile` take the player's own ROM and say yes only if it is exactly 4096 bytes **and** its SHA-1 fingerprint is the expected one (`0940fea7f04cdb6d4b90c5ad1a7e344e68f6dbb1`). The check keeps nothing and hands nothing to the game: since G5c (D-009) the game has all its data in `tables.cpp` and reads no ROM. The check is the indirect copy protection of the project: the window program does not start the game without the right file, so nobody without a legal dump can play with it. The check has its own small SHA-1 so the game code does not depend on the window program; the file is only opened for reading.

## Result: OK

- ROM check tests (`tests\game\test_rom_check.cpp`): 13 ok, 0 failed. They check three standard SHA-1 test values, that the right ROM is accepted, that one changed bit is refused (fingerprint), one byte short / long / empty is refused (size), a missing file and a foreign file are refused.
- The native game of F21.27 (memory, picture and sound of every recorded game): native game, scratch and stack bytes left out: 70 of 70 games identical, 439680 frames, 11.3 s of work.
- **The game needs no ROM:** `lockstep_native` given a ROM path that does not exist ends IDENTICAL (True). `test_no_rom`, a program with no ROM code in it, reproduces the recorded pictures and sounds of the fingerprint files in `tests\game\data` (BL-001: result: IDENTICAL (700 frames); BL-035: result: IDENTICAL (3000 frames)).
- **The window program is the gate:** a wrong file is refused (exit 4, no picture): True; the right ROM is accepted (exit 0, picture): True.
- Source scan: 162 files under `src`; runs of 8 consecutive numbers that equal 8 consecutive cartridge bytes (at least 4 different values): **none** outside `src/game/data/tables.cpp`. Since G5b (D-009, Rule 7 as reworded) that one file holds the game's data tables (34 tables, 508 bytes), and every run found in it lies inside a listed table (299 runs, 0 stray). Everywhere else the source holds addresses and logic, not Atari's data. The scan was first shown to work on a control (8 cartridge bytes written as 0x.., decimal and $.. are all found: True).

## Not claimed

- The check is a simple one (size and fingerprint of a file). It keeps honest users honest; it is not strong protection against someone who wants to defeat it, and it was not meant to be (D-009).
- The scan finds copied runs written as numbers; it cannot prove that no data is hidden in some other disguise. The rule of the project (T-F21-1) is that tables are read by address only.
