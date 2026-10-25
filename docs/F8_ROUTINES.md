# Stage F8: routines and call map (2026-10-26)

`python -I tools\f8_routines.py rom\pacman.a26` (after f2-f7) finds the routines by following the code from the start-up and from every JSR, names them from reading the code, records who calls whom, and rebuilds the listing with the names (`reference\disasm\pacman_routines.asm`, local only) byte-identical to the ROM. This document is written by the tool.

The main loop (`MAIN`) is the start-up plus the frame loop of F5; it is the only code that is not a subroutine. Every other routine is entered by `JSR`. A **shared tail** is code that two routines reach by a jump; it is listed on its own (`TAIL_Fxxx` unless it has a real name).

## Checks

- PASS: every instruction that ran belongs to the main loop or to exactly one routine (0 do not)
- PASS: no instruction belongs to two routines (0 do)
- PASS: a routine entered only by JSR ran exactly as often as its JSR instructions (0 disagree)
- PASS: no routine calls itself, directly or through others
- PASS: deepest nesting of calls is 10 bytes of stack and the stack area found in F7 is 10 bytes ($F6-$FF)
- PASS: 40 routines in all, 40 with a name from the code, 0 shared tails named by position

Deepest nesting of calls: **10 bytes of stack**.

## Routines

| Entry | Name | Sure? | Bytes | Called from (JSR sites; ran times) | Calls | Drops caller's return | What it does |
| --- | --- | --- | --- | --- | --- | --- | --- |
| $F000 | `MAIN` | sure | 2231 | (start) | ADD_SCORE, ANIMATE_OBJECT, CLEAR_GRAPHICS, CLEAR_SCORES, GHOST_UPDATE, KERNEL_LINE_P0, KERNEL_LINE_P1, KERNEL_LINE_P1_P0, LIVES_MARKERS, LOCATE_OBJECT, MOVE_OBJECT, NEW_GAME_SETUP, NEW_MAZE, PACMAN_KEEP_GOING, PLACE_OBJECTS, SCORE_DIGITS, SET_DIRECTION, SOUND_EFFECTS, SOUND_MUSIC, SOUND_START, SWAP_PLAYERS, WRAP_TO_BOTTOM, WRAP_TO_TOP |  | Start-up (clears memory, first values, positions the objects) and then the endless frame loop of F5: sync, top logic, wait, picture, bottom logic, wait. Never returns. |
| $F053 | `KERNEL_LINE_P1` | sure | 20 | $F067 in KERNEL_LINE_P1_P0 (4396100), $F131 in MAIN (1758440), $F163 in MAIN (1758440), $F19E in MAIN (1758440), $F1D0 in MAIN (1758440), $F2EE in MAIN (3077270), $F2FC in MAIN (3077270), $F313 in MAIN (3516880), $F334 in MAIN (3516880), $F38C in MAIN (439610), $F392 in MAIN (439610) | - |  | One picture line for the second sprite (Pac-Man): loads its next picture byte when this line is inside it, waits for the line start, stores GRP1. |
| $F067 | `KERNEL_LINE_P1_P0` | sure | 3 | $F2CD in MAIN (3077270), $F374 in MAIN (439610), $F377 in MAIN (439610), $F37A in MAIN (439610) | -> KERNEL_LINE_P0, KERNEL_LINE_P1 |  | One line for Pac-Man, then falls into the next routine for the ghost: two lines. |
| $F06A | `KERNEL_LINE_P0` | sure | 21 | $F0E4 in MAIN (439610), $F119 in MAIN (1758440), $F14E in MAIN (1758440), $F183 in MAIN (1758440), $F1BB in MAIN (1758440), $F1F4 in MAIN (3516880), $F2F5 in MAIN (3077270), $F2FF in MAIN (3077270), $F322 in MAIN (3516880), $F38F in MAIN (439610) | - |  | One picture line for the first sprite (the ghost of this frame): the same method, stores GRP0. |
| $F3EE | `SCORE_DIGITS` | likely | 149 | $F3D0 in MAIN (439610), $F3DE in MAIN (20970) | -> CLEAR_GRAPHICS |  | Turns the six decimal digits of one score into the six picture pointers of the score line (blank leading zeros) and draws the line with the six-picture trick. |
| $F483 | `CLEAR_GRAPHICS` | sure | 11 | $F3AB in MAIN (439610) | - |  | Writes 0 to GRP0 and GRP1 (twice each, with the vertical delay on). |
| $F793 | `PACMAN_KEEP_GOING` | ? | 39 | $F78A in MAIN (127587) | -> MAIN, TRY_DOWN, TRY_LEFT, TRY_RIGHT, TRY_UP | yes | Pac-Man with the stick in neutral: if he is exactly on a cell, tries the directions to keep moving; drops its caller's return (PLA PLA) when a direction is taken. |
| $F9AA | `GHOST_UPDATE` | likely | 58 | $F954 in MAIN (352908), $F957 in MAIN (352908), $F95A in MAIN (352908) | -> EXIT_F9E4, -> MOVE_OBJECT, -> SET_DIRECTION, GHOST_DECIDE, LOCATE_OBJECT |  | Everything for one ghost (index in X, 1-4) in one frame: pen exit and return, then the decision routine and the move. |
| $F9E4 | `EXIT_F9E4` | sure | 1 | (jump only) | - |  | A lone RTS: the common exit of the ghost update, move and decision routines. |
| $F9E5 | `SET_DIRECTION` | sure | 9 | $F77B in MAIN (73039) | -> MOVE_OBJECT |  | Stores direction code Y (0 right, 1 left, 2 down, 3 up) into the state bits of object X. |
| $F9EE | `MOVE_OBJECT` | sure | 56 | $F78D in MAIN (64551) | -> EXIT_F9E4 |  | The speed accumulator of object X: when it carries, moves the object one step in its direction (x or y) unless a border stops it. |
| $FA26 | `GHOST_DECIDE` | likely | 214 | $F9DB in GHOST_UPDATE (1036677) | -> EXIT_F9E4, -> TRY_PAIR_1, -> WRAP_TO_BOTTOM, -> WRAP_TO_TOP, RNG_STEP, TRY_DOWN, TRY_LEFT, TRY_PAIR_1, TRY_PAIR_2, TRY_RIGHT, TRY_UP |  | The ghost's decision: where it wants to go (target Pac-Man, or away when scared, or home when eaten), then which allowed direction to take, using the random number. |
| $FAFC | `TRY_PAIR_1` | ? | 9 | $FADA in GHOST_DECIDE (27814), $FAE8 in GHOST_DECIDE (25116) | RNG_STEP, TRY_PAIR_UD | yes | Tries two directions in an order chosen by the random number ($FBE0); returns two levels when one is taken. |
| $FB05 | `TRY_PAIR_UD` | ? | 20 | $FAFF in TRY_PAIR_1 (52930) | TRY_DOWN, TRY_UP | yes | Tries up and down in the order chosen by the caller's flag; returns two levels. |
| $FB19 | `TRY_PAIR_2` | ? | 9 | $FACC in GHOST_DECIDE (6773), $FAF6 in GHOST_DECIDE (6738) | RNG_STEP, TRY_PAIR_LR | yes | As TRY_PAIR_1 for the other pair of directions. |
| $FB22 | `TRY_PAIR_LR` | ? | 20 | $FB1C in TRY_PAIR_2 (13511) | TRY_LEFT, TRY_RIGHT | yes | Tries left and right in the order chosen by the caller's flag; returns two levels. |
| $FB36 | `TRY_UP` | likely | 14 | $F7AF in PACMAN_KEEP_GOING (14565), $FAB0 in GHOST_DECIDE (7633), $FAC9 in GHOST_DECIDE (2293), $FACF in GHOST_DECIDE (0), $FB07 in TRY_PAIR_UD (16156), $FB13 in TRY_PAIR_UD (30804) | -> EXIT_FB5B, -> TRY_COMMIT |  | If the maze allows a step up from this cell, takes direction 3 and returns two levels; otherwise returns. |
| $FB44 | `TRY_COMMIT` | likely | 23 | (jump only) | -> EXIT_FB5B | yes | Shared end of the four TRY routines: unless the ghost would turn back, sets the direction and drops the caller's return. |
| $FB5B | `EXIT_FB5B` | sure | 1 | (jump only) | - |  | A lone RTS: the common exit of the TRY routines. |
| $FB5C | `TRY_DOWN` | likely | 20 | $F7B5 in PACMAN_KEEP_GOING (29560), $FAB6 in GHOST_DECIDE (14158), $FAF3 in GHOST_DECIDE (2269), $FAF9 in GHOST_DECIDE (0), $FB0A in TRY_PAIR_UD (12879), $FB10 in TRY_PAIR_UD (36774) | -> EXIT_FB5B, -> TRY_COMMIT |  | The same for direction 2 (down). |
| $FB70 | `TRY_LEFT` | likely | 23 | $F7A1 in PACMAN_KEEP_GOING (13757), $FAA1 in GHOST_DECIDE (26482), $FAE5 in GHOST_DECIDE (10828), $FAEB in GHOST_DECIDE (17893), $FB24 in TRY_PAIR_LR (4431), $FB30 in TRY_PAIR_LR (3329) | -> EXIT_FB5B, -> TRY_COMMIT |  | The same for direction 1 (left). |
| $FB87 | `TRY_RIGHT` | likely | 24 | $F7A7 in PACMAN_KEEP_GOING (16207), $FAA7 in GHOST_DECIDE (23879), $FAD7 in GHOST_DECIDE (12390), $FADD in GHOST_DECIDE (20105), $FB27 in TRY_PAIR_LR (1132), $FB2D in TRY_PAIR_LR (9080) | -> EXIT_FB5B, -> TRY_COMMIT |  | The same for direction 0 (right). |
| $FB9F | `SWAP_PLAYERS` | sure | 14 | $F52F in MAIN (19), $F8B7 in MAIN (120) | - |  | Exchanges the 20-byte player block $88-$9B with $9C-$AF. |
| $FBAD | `LOCATE_OBJECT` | sure | 51 | $F4EB in MAIN (292605), $F9D8 in GHOST_UPDATE (1036677) | - |  | Works out where object X stands: row, offsets inside the cell and the cell index ($C9, $85, $86, $87, $EE). |
| $FBE0 | `RNG_STEP` | sure | 20 | $FAC4 in GHOST_DECIDE (7433), $FAD2 in GHOST_DECIDE (39123), $FAE0 in GHOST_DECIDE (35083), $FAEE in GHOST_DECIDE (7740), $FAFC in TRY_PAIR_1 (52930), $FB19 in TRY_PAIR_2 (13511) | - |  | Steps the shift-register random number $CB; returns a masked bit pattern (zero or not) in A. |
| $FBF4 | `NEW_GAME_SETUP` | sure | 24 | $F018 in MAIN (70), $F8AB in MAIN (816), $F8BA in MAIN (120) | -> NEW_MAZE |  | Lives to 3, score colours, level step from the game number; falls into NEW_MAZE. |
| $FC0C | `NEW_MAZE` | sure | 21 | $F6C8 in MAIN (20) | -> PLACE_OBJECTS |  | Fills the wafer map with $FF (power pills and wafer holes set), power-pill flags to $3C; falls into PLACE_OBJECTS. |
| $FC21 | `PLACE_OBJECTS` | sure | 36 | $F524 in MAIN (293) | - |  | Freezes the game, sets the pause timer and puts Pac-Man and the four ghosts at their start places. |
| $FC45 | `ADD_SCORE` | sure | 35 | $F57D in MAIN (18), $F5C8 in MAIN (102), $F5F7 in MAIN (193), $F6A2 in MAIN (4294) | - |  | Adds the amount number Y (tables $FF06/$FF0C/$FF0D) to the score of the player whose turn it is, in decimal mode. |
| $FC68 | `LIVES_MARKERS` | likely | 21 | $F497 in MAIN (439610), $F4AA in MAIN (439610) | - |  | Sets the playfield values for the life markers of the status line from the number of lives X. |
| $FC7D | `CLEAR_SCORES` | sure | 14 | $F894 in MAIN (816), $F8D8 in MAIN (143) | - |  | Zeroes both scores and both lives. |
| $FC8B | `ANIMATE_OBJECT` | likely | 155 | $F970 in MAIN (478), $F97D in MAIN (2313) | -> WRAP_TO_BOTTOM |  | The animation of an object whose state bit 5 is set: a ghost coming out of the pen, or Pac-Man dying; counts the steps. |
| $FD26 | `WRAP_TO_BOTTOM` | likely | 12 | $F74C in MAIN (15) | -> WRAP_SET_Y, -> WRAP_TO_TOP |  | Pac-Man or a ghost entering the bottom opening: sets the direction and y = $51. |
| $FD32 | `WRAP_TO_TOP` | likely | 10 | $F756 in MAIN (12) | -> WRAP_SET_Y |  | Entering the top opening: sets the direction and y = $0B. |
| $FD3C | `WRAP_SET_Y` | sure | 3 | (jump only) | - |  | Stores A as the y of object X and returns: the end of both WRAP routines. |
| $FF43 | `SOUND_START` | sure | 18 | $F582 in MAIN (18), $F5CD in MAIN (102), $F602 in MAIN (193), $F6A7 in MAIN (4294) | - |  | Starts sound number Y unless a more important sound is playing. |
| $FF55 | `SOUND_EFFECTS` | likely | 54 | $F7C9 in MAIN (439610) | -> EXIT_FF93, -> SOUND_WRITE |  | Each frame: plays the sound chosen by SOUND_START on channel 0. |
| $FF8B | `SOUND_WRITE` | sure | 8 | (jump only) | -> EXIT_FF93 |  | Writes volume (A), control (D3) and pitch (Y) to the sound channel X. |
| $FF93 | `EXIT_FF93` | sure | 1 | (jump only) | - |  | A lone RTS: the exit of the sound routines. |
| $FF94 | `SOUND_MUSIC` | likely | 85 | $F7CE in MAIN (439610) | -> SOUND_WRITE |  | Each frame: the start tune, the death sound and the scared-ghost sound. |

## Data each routine touches

| Name | Reads | Writes | Chip registers |
| --- | --- | --- | --- |
| `MAIN` | ATTRACT_SHIFT, BONUS_TIMER, CELL_INDEX, CELL_ROW, COLOUR_BG, COLOUR_MASK, COLOUR_MAZE, COLOUR_SCORE_P1, COLOUR_SCORE_P2, COLOUR_SPRITE0, COLOUR_SPRITE1, COLOUR_TABLE[], EAT_PAUSE, EVENT_TIMER, FRAME_COUNTER, GAME_FLAGS, GAME_SELECT, GHOST_STATE[], JOY_DIRECTION, LEVEL_SPEED, LIVES, LIVES_P2, MAZE[], MAZE_04[], MAZE_08[], MAZE_12[], OBJ_STATE[], OBJ_X[], OBJ_Y[], PACMAN_STATE, PACMAN_X, PACMAN_Y, PILL_FLAGS, PLAYER_MODE, POS_CODE, ROW_PARITY, SCORE_COLOUR, SCORE_COLOUR_P2, SELECT_REPEAT, SPRITE0_LINES, SPRITE0_Y, SPRITE1_LINES, SPRITE1_PTR_LO[]->, SPRITE1_Y, SPRITE_PTR[]->, SPRITE_X_ARR[], TEMP_D2, TEMP_D2[]->, TEMP_D3, TEMP_D4, TEMP_D5, TEMP_D6, TEMP_D7, TEMP_E1, TEMP_E2, TEMP_E3, TICK256, X_OFFSET, Y_OFFSET | ATTRACT_SHIFT, BONUS_TIMER, CELL_INDEX, CELL_ROW, COLOUR_BG, COLOUR_MASK, COLOUR_MAZE, COLOUR_SCORE_P1, COLOUR_SCORE_P2, COLOUR_SPRITE0, COLOUR_SPRITE1, COLOUR_TABLE[], EAT_PAUSE, EVENT_TIMER, FRAME_COUNTER, GAME_FLAGS, GAME_SELECT, GHOST_STATE[], JOY_DIRECTION, LEVEL_SPEED, LIVES, LIVES_P2, MAZE[], OBJ_TIMER[], PACMAN_STATE, PACMAN_Y, PILL_FLAGS, PLAYER_MODE, RNG_STATE, ROW_PARITY, SCORE_P1_HI, SCORE_P1_LO, SCORE_P1_MID, SCORE_P2_LO, SELECT_REPEAT, SOUND_ACTIVE, SPEED_ACC0, SPEED_ACC[], SPRITE0_LINES, SPRITE0_PTR_HI, SPRITE0_PTR_LO, SPRITE0_Y, SPRITE1_LINES, SPRITE1_PTR_HI, SPRITE1_PTR_LO, SPRITE1_Y, SPRITE_X, TEMP_D2, TEMP_D3, TEMP_D4, TEMP_D5, TEMP_D6, TEMP_D7, TEMP_E1, TEMP_E2, TEMP_E3, TICK256 | AUDV0, AUDV1, COLUBK, COLUP0, COLUP1, COLUPF, CTRLPF, CXCLR, CXM1P, CXP1FB, CXPPMM, ENABL, ENAM0, ENAM1, GRP0, GRP1, HMBL, HMCLR, HMM0, HMM1, HMOVE, HMP0, INPT4, INTIM, NUSIZ0, NUSIZ1, PF0, PF1, PF2, REFP1, RESBL, RESM0, RESM1, RESP0, RESP1, SWCHA, SWCHB, TIM64T, VBLANK, VDELP0, VDELP1, VSYNC, WSYNC |
| `KERNEL_LINE_P1` | SPRITE1_LINES, SPRITE1_PTR_LO[]->, SPRITE1_Y | SPRITE1_LINES | GRP1, WSYNC |
| `KERNEL_LINE_P1_P0` | - | - | - |
| `KERNEL_LINE_P0` | SPRITE0_LINES, SPRITE0_Y, SPRITE_PTR[]-> | SPRITE0_LINES | GRP0, WSYNC |
| `SCORE_DIGITS` | DISPLAY_BUF[]->, SCORE_LO[], SPRITE0_LINES[]->, SPRITE1_Y[]->, SPRITE_PTR[]->, TEMP_D2, TEMP_D7[]->, TEMP_E1, TEMP_E2 | DISPLAY_BUF[], TEMP_D5, TEMP_E1, TEMP_E2 | COLUBK, GRP0, GRP1, WSYNC |
| `CLEAR_GRAPHICS` | - | - | GRP0, GRP1 |
| `PACMAN_KEEP_GOING` | OBJ_STATE[], X_OFFSET, Y_OFFSET | - | - |
| `GHOST_UPDATE` | OBJ_STATE[], OBJ_X[], OBJ_Y[], PILL_FLAGS | OBJ_STATE[] | - |
| `EXIT_F9E4` | - | - | - |
| `SET_DIRECTION` | OBJ_STATE[] | OBJ_STATE[] | - |
| `MOVE_OBJECT` | OBJ_STATE[], OBJ_X[], OBJ_Y[], SPEED_ACC[] | OBJ_STATE[], OBJ_X[], OBJ_Y[], SPEED_ACC[] | - |
| `GHOST_DECIDE` | ATTRACT_SHIFT, LEVEL_SPEED, OBJ_STATE[], OBJ_TIMER[], OBJ_X[], OBJ_Y[], PACMAN_X, PACMAN_Y, PILL_FLAGS, RNG_STATE, TEMP_D2, TEMP_D3, X_OFFSET, Y_OFFSET | OBJ_TIMER[], TEMP_D2, TEMP_D3 | - |
| `TRY_PAIR_1` | - | - | - |
| `TRY_PAIR_UD` | - | - | - |
| `TRY_PAIR_2` | - | - | - |
| `TRY_PAIR_LR` | - | - | - |
| `TRY_UP` | CELL_INDEX, CELL_ROW | - | - |
| `TRY_COMMIT` | OBJ_STATE[] | OBJ_STATE[] | - |
| `EXIT_FB5B` | - | - | - |
| `TRY_DOWN` | CELL_INDEX, CELL_ROW | - | - |
| `TRY_LEFT` | CELL_INDEX, CELL_ROW | - | - |
| `TRY_RIGHT` | CELL_INDEX, CELL_ROW | - | - |
| `SWAP_PLAYERS` | MAZE[], MAZE_P2[] | MAZE[], MAZE_P2[] | - |
| `LOCATE_OBJECT` | CELL_ROW, OBJ_X[], OBJ_Y[] | CELL_INDEX, CELL_ROW, POS_CODE, X_OFFSET, Y_OFFSET | - |
| `RNG_STEP` | RNG_STATE | RNG_STATE | - |
| `NEW_GAME_SETUP` | GAME_SELECT | LEVEL_SPEED, LIVES, SCORE_COLOUR, SCORE_COLOUR_P2 | - |
| `NEW_MAZE` | - | MAZE[], MAZE_02, MAZE_06, PILL_FLAGS | - |
| `PLACE_OBJECTS` | GAME_FLAGS | EVENT_TIMER, GAME_FLAGS, OBJ_STATE[], OBJ_X[], OBJ_Y[], PACMAN_STATE, PACMAN_Y | - |
| `ADD_SCORE` | PLAYER_MODE, SCORE_HI[], SCORE_LO[], SCORE_MID[], TEMP_E1 | SCORE_HI[], SCORE_LO[], SCORE_MID[], TEMP_E1 | - |
| `LIVES_MARKERS` | - | TEMP_D2[], TEMP_D3[], TEMP_D4[] | - |
| `CLEAR_SCORES` | - | LIVES, LIVES_P2, SCORE_LO[] | - |
| `ANIMATE_OBJECT` | OBJ_STATE[], OBJ_Y[], SPRITE_PTR[] | OBJ_STATE[], OBJ_Y[], SPRITE0_LINES[], SPRITE0_Y[], SPRITE_PTR[] | - |
| `WRAP_TO_BOTTOM` | OBJ_STATE[] | OBJ_STATE[] | - |
| `WRAP_TO_TOP` | OBJ_STATE[] | OBJ_STATE[] | - |
| `WRAP_SET_Y` | - | OBJ_Y[] | - |
| `SOUND_START` | SOUND_ACTIVE | SOUND_ACTIVE, SOUND_TIMER | - |
| `SOUND_EFFECTS` | SOUND_ACTIVE, SOUND_TIMER, SOUND_TIMER[] | SOUND_ACTIVE, SOUND_TIMER[], TEMP_D3 | - |
| `SOUND_WRITE` | TEMP_D3 | - | AUDC0, AUDF0, AUDV0 |
| `EXIT_FF93` | - | - | - |
| `SOUND_MUSIC` | EVENT_TIMER, FRAME_COUNTER, GAME_FLAGS, PILL_FLAGS | GAME_FLAGS, TEMP_D3 | AUDC0, AUDC1, AUDF0, AUDF1, AUDV0, AUDV1 |

`name[]` = an array reached by an index register; `name->` = a pointer held in that byte and the one after it.

## Call tree

- `MAIN`
  - `KERNEL_LINE_P1`
  - `KERNEL_LINE_P1_P0`
    - `KERNEL_LINE_P0` (continues into)
    - `KERNEL_LINE_P1`
  - `KERNEL_LINE_P0`
  - `SCORE_DIGITS`
    - `CLEAR_GRAPHICS` (continues into)
  - `CLEAR_GRAPHICS`
  - `PACMAN_KEEP_GOING`
    - `MAIN` (continues into)
    - `TRY_UP`
      - `TRY_COMMIT` (continues into)
      - `EXIT_FB5B` (continues into)
    - `TRY_DOWN`
      - `TRY_COMMIT` (continues into)
      - `EXIT_FB5B` (continues into)
    - `TRY_LEFT`
      - `TRY_COMMIT` (continues into)
      - `EXIT_FB5B` (continues into)
    - `TRY_RIGHT`
      - `TRY_COMMIT` (continues into)
      - `EXIT_FB5B` (continues into)
  - `GHOST_UPDATE`
    - `EXIT_F9E4` (continues into)
    - `SET_DIRECTION` (continues into)
    - `MOVE_OBJECT` (continues into)
    - `GHOST_DECIDE`
      - `EXIT_F9E4` (continues into)
      - `TRY_PAIR_1` (continues into)
      - `WRAP_TO_BOTTOM` (continues into)
      - `WRAP_TO_TOP` (continues into)
      - `TRY_PAIR_1`
        - `TRY_PAIR_UD`
          - `TRY_UP`
            - `TRY_COMMIT` (continues into)
            - `EXIT_FB5B` (continues into)
          - `TRY_DOWN`
            - `TRY_COMMIT` (continues into)
            - `EXIT_FB5B` (continues into)
        - `RNG_STEP`
      - `TRY_PAIR_2`
        - `TRY_PAIR_LR`
          - `TRY_LEFT`
            - `TRY_COMMIT` (continues into)
            - `EXIT_FB5B` (continues into)
          - `TRY_RIGHT`
            - `TRY_COMMIT` (continues into)
            - `EXIT_FB5B` (continues into)
        - `RNG_STEP`
      - `TRY_UP`
        - `TRY_COMMIT` (continues into)
        - `EXIT_FB5B` (continues into)
      - `TRY_DOWN`
        - `TRY_COMMIT` (continues into)
        - `EXIT_FB5B` (continues into)
      - `TRY_LEFT`
        - `TRY_COMMIT` (continues into)
        - `EXIT_FB5B` (continues into)
      - `TRY_RIGHT`
        - `TRY_COMMIT` (continues into)
        - `EXIT_FB5B` (continues into)
      - `RNG_STEP`
    - `LOCATE_OBJECT`
  - `SET_DIRECTION`
    - `MOVE_OBJECT` (continues into)
  - `MOVE_OBJECT`
    - `EXIT_F9E4` (continues into)
  - `SWAP_PLAYERS`
  - `LOCATE_OBJECT`
  - `NEW_GAME_SETUP`
    - `NEW_MAZE` (continues into)
  - `NEW_MAZE`
    - `PLACE_OBJECTS` (continues into)
  - `PLACE_OBJECTS`
  - `ADD_SCORE`
  - `LIVES_MARKERS`
  - `CLEAR_SCORES`
  - `ANIMATE_OBJECT`
    - `WRAP_TO_BOTTOM` (continues into)
  - `WRAP_TO_BOTTOM`
    - `WRAP_TO_TOP` (continues into)
    - `WRAP_SET_Y` (continues into)
  - `WRAP_TO_TOP`
    - `WRAP_SET_Y` (continues into)
  - `SOUND_START`
  - `SOUND_EFFECTS`
    - `SOUND_WRITE` (continues into)
    - `EXIT_FF93` (continues into)
  - `SOUND_MUSIC`
    - `SOUND_WRITE` (continues into)

## Notes

- Routines that drop the caller's return (`PLA PLA`) end the whole decision of the caller at once: the C++ version needs an early exit there (F13).
- Two JSR instructions never ran in the 70 games (`$FACF` and `$FAF9`, both in the ghost decision); the routines they call are also called by instructions that did run.
- Not claimed: names marked "?" are a guess from the shape of the code; F10-F13 settle them.
