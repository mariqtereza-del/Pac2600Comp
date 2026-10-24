# Reference captures (Stage C7)

Eight numbered pictures of the original game, kept in `reference\screens\` (local only, like all of `reference\`). Each can be recreated byte-for-byte from the committed script `tools\captures\c7_captures.txt`.

## How they are made

1. `tools\stella_probe\bot.cpp` (`stella_bot.exe`) is a search-based player. It plays ahead with Stella's own save/load and writes its moves as an input script. Its route is `tools\captures\c7_route.txt` (seed 1): it clears the level at frame 7311, then lets the ghosts end the game.
2. Only a **replay from power-on** counts as evidence. `stella_script.exe` replays the route (279 input holds) and saves the pictures: `build\stella_probe\stella_script.exe rom\pacman.a26 <out_folder> 9800 tools\captures\c7_captures.txt`.
3. `tools\c7_captures.py <replay_folder>` finds the moments from RAM and picks the frames; `tools\raw_to_png.py` makes the PNGs (each game pixel drawn 4 wide x 2 high, for viewing only).
4. The replay is deterministic: two replays gave identical `frames.csv` (every frame's fingerprint and RAM) and identical pictures.

Frame numbers count from power-on. The game is reset (Game Reset) at frame 1800, as in C3–C6.

## The pictures

| Picture | Frame | Picture fingerprint (SHA-256, first 16) | What it shows (checked by eye) |
| --- | --- | --- | --- |
| `01_title` | 1500 | `c62f292ca76a88c4` | Attract mode before the game: maze in brown, one ghost, "©1981 ATARI" in the status bar. |
| `02_game_start` | 1898 | `0fd6e0c472714dd5` | Just after the game starts: full maze, 126 wafers, Pac-Man below the pen, a ghost in the pen, 3 life markers, no score yet. |
| `03_power_pill` | 2449 | `4c5031f9fef02366` | Power pill in effect ($F2 bit 1 set): the ghost is the "scared" lilac, score 86, one power pill eaten. |
| `04_ghost_eaten` | 2330 | `b969630b70194722` | Pause right after a ghost was eaten: the ghost's eyes are on Pac-Man, score 82 (it was 21 at frame 2286). |
| `05_death` | 5839 | `ea33a7f2cec89df8` | Pac-Man's death animation (the "V" shape) next to a green ghost, score 474, 3 life markers (the marker goes when the next life starts). |
| `06_bonus_item` | 3455 | `389e903c42c5725e` | The bonus item (yellow) sitting in the box under the ghost pen, score 262. |
| `07_level_end` | 7310 | `c8bb81ac83322990` | One frame before the maze refills: 1 wafer left (RAM), score 505, 2 life markers. |
| `08_game_over` | 9691 | `01b3a011e45c100c` | After the last life: no life markers, lives counter 0, board frozen, score 597. |

## Facts found along the way (working labels until Phase F)

- `$88`–`$97`: wafer map, 126 one-bits at the start of a level; each bit clears when that wafer is eaten, and the map refills when the level ends (frame 7311).
- `$98`: lives counting the one in play (3 at the start, 0 at game over); it goes up by one when a level is cleared.
- `$D2` bit 6: the game is paused (a death for 126 frames, a ghost eaten for 32, and the level end). `$D2` bits 0–3: Pac-Man's direction.
- `$F2` bit 1: a power pill is in effect — during play only; in attract mode it toggles every 512 frames, so it means nothing there.
- Game over: `$98` = 0 and Pac-Man parked at `$B1`/`$B6` = 76/95. Attract mode follows; fire starts a new game (frame 9864 in this route).
- Moments in this route: first movement 1928; power pills 2226–2673, 3124–3985, 6200–6583, 7650–8097; ghosts eaten at 2289, 2323, 3162, 3244, 3419, 3497, 7761, 7820; deaths at 5779, 8352, 8827, 9266 (lives drop 126 frames later); bonus item under the pen 3072–3839 and 8448–9215; game over 9391.
- The bonus item picture is of the item in the box. What the item's shape means (and whether it differs by level) is not determined.

## Not captured

- Two-player play, the other game variations, and black-and-white mode (C6 covers their behaviour in numbers, not pictures).
- Level 2 and beyond (the route ends level 1 and then lets the ghosts win).

## Limits

- The pictures come from the Stella 7.0 core (zeroed RAM at power-on, 60 Hz), not from the Stella app, which starts with random RAM (`docs\DISPLAY.md`). A picture from the app can differ in unrelated things (such as the idle colour).
- Colours are Stella's NTSC palette, not what a particular TV showed.
