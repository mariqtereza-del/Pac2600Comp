# The cartridge data the game really uses (Stage G5a, D-009)

Written by `python -I tools\g5a_data_list.py rom\pacman.a26`. It contains names, sizes, address ranges and counts only; **no byte value of the cartridge**. The byte-level list is `reference\g5a\embedded_tables.txt` (local only).

## How it was measured

The native game was run with a counter on every read of the cartridge by address (`lockstep_native_reads`, a test build; the game's own builds have no counter) over the 70 recorded games and the 120 seeded random games of F25 (all play states: title, play, death, game over, two players, bonus item, power pill, ghost eaten, level changes). Every game ended identical to its golden trace, so the counts belong to the real game. The table map is the one of F4 (`docs\F4_DATA_TABLES.md`).

## Result

- **503 different cartridge addresses were read** (of 4,096).
- **503 of them lie in data tables of the F4 map, 0 lie outside every data table** (that is, in the cartridge's program code).
- **48 tables are read at least once, together 504 bytes; of these 1 bytes are never read** (the rest of a table that is only partly used). The tables that are never read: TBL_FE63 (4), TBL_FE67 (3).
- **Data the program would carry: 504 bytes in 48 tables** (or 503 bytes if only the bytes that are read were kept). The cartridge has 4,096 bytes; its machine code is not part of this.
- Reads in the program code: **none**.
- Check against F4: 504 bytes carried + 7 bytes in tables the native code never reads + 28 bytes in segments never read = **539** = the 539 data bytes of the F4 map.

## The tables

| Table | Kind | Address range | Size | Bytes read | Reads (all games) | Used for |
| --- | --- | --- | --- | --- | --- | --- |
| `TBL_FD3F` | TBL | $FD3F-$FD46 | 8 | 8 | 11357960 | reached by index or pointer (docs\F18_KERNEL.md, F19_SOUND.md) |
| `TBL_FD47` | TBL | $FD47-$FD56 | 16 | 16 | 22715920 | reached by index or pointer (docs\F18_KERNEL.md, F19_SOUND.md) |
| `TBL_FD57` | TBL | $FD57-$FD5A | 4 | 4 | 11357960 | reached by index or pointer (docs\F18_KERNEL.md, F19_SOUND.md) |
| `TBL_FD5B` | TBL | $FD5B-$FD5F | 5 | 5 | 1497568 | reached by index or pointer (docs\F18_KERNEL.md, F19_SOUND.md) |
| `TBL_FD61` | TBL | $FD61-$FD62 | 2 | 2 | 9860392 | reached by index or pointer (docs\F18_KERNEL.md, F19_SOUND.md) |
| `TBL_FD63` | TBL | $FD63-$FD6A | 8 | 8 | 22715920 | reached by index or pointer (docs\F18_KERNEL.md, F19_SOUND.md) |
| `TBL_FD6B` | TBL | $FD6B-$FD6F | 5 | 5 | 495703 | reached by index or pointer (docs\F18_KERNEL.md, F19_SOUND.md) |
| `TBL_FD71` | TBL | $FD71-$FD77 | 7 | 7 | 84164 | reached by index or pointer (docs\F18_KERNEL.md, F19_SOUND.md) |
| `TBL_FD79` | TBL | $FD79-$FD7A | 2 | 2 | 10778093 | reached by index or pointer (docs\F18_KERNEL.md, F19_SOUND.md) |
| `TBL_FD7B` | TBL | $FD7B-$FD82 | 8 | 8 | 11357960 | reached by index or pointer (docs\F18_KERNEL.md, F19_SOUND.md) |
| `TBL_FD83` | TBL | $FD83-$FD84 | 2 | 2 | 342302 | reached by index or pointer (docs\F18_KERNEL.md, F19_SOUND.md) |
| `TBL_FD86` | TBL | $FD86-$FD8A | 5 | 5 | 11015658 | reached by index or pointer (docs\F18_KERNEL.md, F19_SOUND.md) |
| `GFX_FD8B` | GFX | $FD8B-$FDAA | 32 | 32 | 22541978 | reached by index or pointer (docs\F18_KERNEL.md, F19_SOUND.md) |
| `TBL_FDAB` | TBL | $FDAB-$FDAE | 4 | 4 | 2839680 | ghost pictures (kGhostPictures) |
| `GFX_FDAF` | GFX | $FDAF-$FDC1 | 19 | 19 | 8730792 | reached by index or pointer (docs\F18_KERNEL.md, F19_SOUND.md) |
| `TBL_FDC2` | TBL | $FDC2-$FDC5 | 4 | 4 | 2865379 | Pac-Man pictures (kPacmanPictures) |
| `GFX_FDC6` | GFX | $FDC6-$FDF9 | 52 | 52 | 2081765 | reached by index or pointer (docs\F18_KERNEL.md, F19_SOUND.md) |
| `TBL_FDFA` | TBL | $FDFA-$FE09 | 16 | 16 | 338115 | dying Pac-Man pictures (kPacmanDyingPictures) |
| `PF_FE0A` | PF | $FE0A-$FE0B | 2 | 2 | 45431840 | reached by index or pointer (docs\F18_KERNEL.md, F19_SOUND.md) |
| `PF_FE0C` | PF | $FE0C-$FE0C | 1 | 1 | 22715920 | reached by index or pointer (docs\F18_KERNEL.md, F19_SOUND.md) |
| `PF_FE0D` | PF | $FE0D-$FE0D | 1 | 1 | 34073880 | reached by index or pointer (docs\F18_KERNEL.md, F19_SOUND.md) |
| `TBL_FE0E` | TBL | $FE0E-$FE0E | 1 | 1 | 11357960 | reached by index or pointer (docs\F18_KERNEL.md, F19_SOUND.md) |
| `PF_FE0F` | PF | $FE0F-$FE10 | 2 | 2 | 19876430 | reached by index or pointer (docs\F18_KERNEL.md, F19_SOUND.md) |
| `PF_FE11` | PF | $FE11-$FE12 | 2 | 2 | 19876430 | reached by index or pointer (docs\F18_KERNEL.md, F19_SOUND.md) |
| `TBL_FE13` | TBL | $FE13-$FE62 | 80 | 80 | 2555444 | maze wall table (kMazeWalls) |
| `TBL_FE63` | TBL | $FE63-$FE66 | 4 | 0 | 0 | never read |
| `TBL_FE67` | TBL | $FE67-$FE69 | 3 | 0 | 0 | never read |
| `TBL_FE6A` | TBL | $FE6A-$FE6B | 2 | 2 | 95019 | wafer bit masks (kWaferBitMask) |
| `TBL_FE6C` | TBL | $FE6C-$FE6D | 2 | 2 | 2885956 | power-pill masks (kPillMask) |
| `TBL_FE6E` | TBL | $FE6E-$FE6E | 1 | 1 | 1467254 | sound bits (kSoundBit) |
| `TBL_FE6F` | TBL | $FE6F-$FE75 | 7 | 7 | 1771942 | ghost chase timer reload (kChaseTimerReload) |
| `TBL_FE79` | TBL | $FE79-$FE88 | 16 | 16 | 478150 | wafer byte index (kWaferByteIndex) |
| `GFX_FE89` | GFX | $FE89-$FEEB | 99 | 99 | 54136679 | reached by index or pointer (docs\F18_KERNEL.md, F19_SOUND.md) |
| `GFX_FEEC` | GFX | $FEEC-$FEF1 | 6 | 6 | 77539723 | life-marker playfield, second (kLivesMarkerPf2) |
| `TBL_FEF2` | TBL | $FEF2-$FEF3 | 2 | 2 | 13323 | reached by index or pointer (docs\F18_KERNEL.md, F19_SOUND.md) |
| `TBL_FEF6` | TBL | $FEF6-$FF05 | 16 | 16 | 18001535 | score digit pointers (kDigitPointerLow) |
| `TBL_FF06` | TBL | $FF06-$FF0B | 6 | 6 | 15085 | score amounts, low byte (kScoreAmountLow) |
| `TBL_FF0C` | TBL | $FF0C-$FF0C | 1 | 1 | 127 | score amounts, middle byte (kScoreAmountMid) |
| `TBL_FF0D` | TBL | $FF0D-$FF12 | 6 | 6 | 16251 | animation pointer steps (kAnimPointerStep) |
| `TBL_FF19` | TBL | $FF19-$FF1A | 2 | 2 | 1239 | animation pointer choice (kAnimWhichPointer) |
| `TBL_FF1B` | TBL | $FF1B-$FF22 | 8 | 8 | 5678980 | life-marker playfield, first (kLivesMarkerPf1) |
| `TBL_FF25` | TBL | $FF25-$FF26 | 2 | 2 | 280 | animation next y (kAnimNextY) |
| `COLOR_FF2B` | COLOR | $FF2B-$FF39 | 15 | 15 | 16137181 | colours, colour switch (kColourTableTv) |
| `UNREAD_FF3A` | UNREAD | $FF3A-$FF3B | 2 | 1 | 9 | reached by index or pointer (docs\F18_KERNEL.md, F19_SOUND.md) |
| `COLOR_FF3C` | COLOR | $FF3C-$FF40 | 5 | 5 | 899750 | reached by index or pointer (docs\F18_KERNEL.md, F19_SOUND.md) |
| `PF_FF41` | PF | $FF41-$FF42 | 2 | 2 | 19876430 | reached by index or pointer (docs\F18_KERNEL.md, F19_SOUND.md) |
| `TBL_FFE9` | TBL | $FFE9-$FFEC | 4 | 4 | 15112 | sound blockers (kSoundBlockers) |
| `TBL_FFED` | TBL | $FFED-$FFF0 | 4 | 4 | 14875 | sound lengths (kSoundLength) |
| `SND_FFF1` | SND | $FFF1-$FFF4 | 4 | 4 | 187476 | start tune notes, first (kStartTuneA) |
| `SND_FFF5` | SND | $FFF5-$FFF8 | 4 | 4 | 187476 | start tune notes, second (kStartTuneB) |

11 further small segments (28 bytes, kinds UNREAD and VECTORS in the F4 map) are never read by the game and would not be carried. (F4 found one UNREAD segment unread in its 29 recordings; the larger set of games here reads one byte of it, so it is listed in the table above.)

## The ROM check afterwards (D-009)

- The program still asks for the ROM at start-up and checks its size (4,096 bytes) and SHA-1 (`0940fea7...6dbb1`) with the existing finder (D3) and loader checks (F22). It then starts the game **without handing it any byte**.
- The lockstep suite, the measuring tools and `pacman2600_ref` keep using the ROM as the reference; they are test tools and are not part of a release.
- F22's source scan ("the source holds no cartridge bytes") changes in G5b into a check that the only cartridge bytes in the source are the tables listed above.

## Rule 7 as approved by the owner (2026-10-26, D-009)

Before: *The original ROM must never be included in the public repository or executable release.*

Now (in `CLAUDE.md`): *The original ROM file, and the cartridge's program code, must never be included in the public repository or executable release. The game data it needs (the tables listed in `docs\EMBEDDED_DATA.md`) is part of the program and is included with the release: the game reads no game data from the ROM. The ROM exists only as an indirect form of DRM. The program requires the player to have a legal dump of the original game and checks it (size and SHA-1) before it starts, so that nobody without a legal dump can play it with this project.*

The owner's words: the ROM is an indirect DRM, preventing anyone without a legal dump from playing with this project; no game data is read from the ROM; it is all included with the release.
