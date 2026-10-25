# Stage F10: Pac-Man's movement and turning (2026-10-26)

`python -I tools\f10_pacman.py rom\pacman.a26` holds a model of Pac-Man's rules written from the code and checks it against the recorded games: from the memory of the frame before, the joystick of the frame and the wall table of the ROM it predicts Pac-Man's x, y, state bits, speed accumulator, JOY_DIRECTION and idle counter **for every frame in which his code runs in the normal way, and compares them with the recorded memory**. This document is written by that tool.

## Result

- **202473 of 202473 predicted frames are exactly right (0 wrong)**, in all 70 games (439,680 frames).
- Frames not predicted, with the reason: Select pressed during play (the game number steps and Pac-Man goes to y=$60): 5; frozen game (start of level, new map): 83737; pause after a ghost was eaten: 6176; power pill eaten: 97; title screen / not in play: 147122.
- In the 6176 frames of the pause after a ghost was eaten, Pac-Man's bytes did not change in 6176 (the pause freezes him).

Roads of the code taken in these frames (every road of the rules below was taken at least once, except the x border):

| Frames | Road |
| --- | --- |
| 77661 | move: the speed accumulator carried, one step |
| 6284 | move: y stopped at the border (below 0 or $47) |
| 2313 | opening: animation step |
| 6 | opening: the stick turns him round |
| 15 | stick down at y=$46 in the middle column: into the bottom opening |
| 6161 | stick down on a row: turn down |
| 16821 | stick left/right: turn left |
| 17904 | stick left/right: turn right |
| 127273 | stick neutral |
| 53450 | stick neutral between cells: keeps moving |
| 62770 | stick neutral on a cell: blocked, stands still |
| 11053 | stick neutral on a cell: keeps going |
| 12 | stick up at y=0 in the middle column: into the top opening |
| 9300 | stick up on a row: turn up |
| 22701 | stick up/down between two rows: reverses vertically at once |

## The rules (what the code does, in the order it does it)

Pac-Man is object 0 of the five moving objects; his bytes are `PACMAN_X` ($B1), `PACMAN_Y` ($B6), `PACMAN_STATE` ($BB) and the speed accumulator `SPEED_ACC0` ($C0). The maze is a grid of **cells 4 pixels wide and rows 10 pixels high** (8 rows, y = 0 to $47). `LOCATE_OBJECT` turns x and y into: the row (y / 10), `Y_OFFSET` (0 when y is exactly on a row, else negative), `X_OFFSET` (x mod 4, 0 = exactly on a cell column) and the cell index (x / 4, plus 40 on odd rows: 80 cells). The wall table `TBL_FE13` has one byte per cell: **bit 7 = a wall blocks entering this cell from the side, bit 6 = he may go up from this cell, bit 5 = he may go down**; bits 0-4 describe the wafer (F11).

**State bits:** bits 7-6 = direction (0 right, 1 left, 2 down, 3 up); bit 4 = "a step is due" (set when a direction is set, cleared by the step); bit 0 = faces left (mirrors the picture; set and cleared only by left and right turns); bit 5 = he is in one of the two openings of the maze (top or bottom wall, middle column) and is being animated.

**Each frame, in this order:**

1. *Top logic:* if bit 5 is set, `ANIMATE_OBJECT` moves him one step along the opening path (y counts through $51-$5B below the maze or $00-$0B above it) and, when the path ends, puts him back in the maze going down (from the bottom opening he comes out at the top) or up.
2. *Bottom logic* (the game is running: not the title screen, not frozen, no ghost-eaten pause, no death): `LOCATE_OBJECT`, wafer eating, the check for a cleared map, then the joystick:
   - **In an opening (bit 5):** the stick can only turn him round (up while his direction is horizontal-coded, or down while vertical-coded); nothing else.
   - **Stick neutral:** if he is exactly on a cell (both offsets 0) he tries to go on in his direction; if the wall table forbids, he **stands still** (no step); otherwise, or when he is between cells, he steps.
   - **Stick pressed:** the code tests the axis perpendicular to his travel first, then the other: (a) *moving vertically:* left or right is taken only if he is exactly on a row (`Y_OFFSET` 0) and the neighbour cell (cell - 1 for left, cell + 2 for right, with two exceptions on row 3) has no wall bit and, for left, the position code is not $F9; if he is between two cell columns the horizontal turn is always allowed. (b) *vertical:* only with `X_OFFSET` 0; in the middle column (x = $4C) at y = 0 with up, or y = $46 with down, he enters the opening (state $F0 and y = $0B, or state $30 and y = $51); between two rows a vertical reversal is always allowed; on a row up needs bit 6 and down needs bit 5 of the cell. If the wanted direction is not possible the code falls back to the other axis, and in the end to the neutral road.
   - Setting a direction writes the two direction bits and bit 4, then takes the step at once.
3. *The step* (`MOVE_OBJECT`): the accumulator is multiplied by 17 (mod 256); **a carry out means one step**. x changes by one (not onto 2 or $96); y changes by one (not below 0 and not onto $47). So speed is set only by the starting accumulator, and it never changes during a game: Pac-Man does not speed up.

## Speed

Start accumulator `$09` when `GAME_SELECT` ($84) is 2 to 7 (games 7, 8 and 1, either number of players) or `$0B` when it is 8 to $11 (games 2 to 6) ($F89F-$F8A9; in the 70 games the accumulator of Pac-Man shows the seed $09 only for $84 below 8 and $0B only for $84 from 8 up). So **Pac-Man is slower in games 1, 7 and 8**. The accumulator repeats after 16 frames for `$09` (144 steps in 256 frames = 0.562 px per frame) and after 16 frames for `$0B` (176 steps in 256 frames = 0.688 px per frame).

## Not claimed

- Wafer eating (the `TBL_FE13` bits 0-4, `TBL_FE79`, the maze bytes) and scoring are F11/F15; ghost collisions, the power pill and the bonus item are F12; the model was not asked to predict them and the frames where the power pill is eaten are skipped.
- The x border road (x onto 2 or $96) was never taken: Pac-Man cannot reach it in this maze, so its meaning is not tested.
- Player 2 never had a joystick in the recordings (F9): in his turn the model was given a neutral stick, which is what the recordings did.
