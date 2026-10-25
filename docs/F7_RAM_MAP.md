# Stage F7: the 128 bytes of RAM (2026-10-26)

`python -I tools\f7_ram.py rom\pacman.a26` (after `f2`..`f6`) names every byte of the console's memory, `$80-$FF`, replaces the zero-page operands of the listing by these names (`reference\disasm\pacman_ram.asm`, local only) and requires the rebuilt ROM to be byte-identical. This document is written by that tool.

**How the names were found:** each byte was followed through every instruction that touches it (the code map of F6; the frame loop of F5) and watched over all 70 baseline traces (439680 frames). A name is **sure** when the code and the traces agree and the use is unambiguous, **likely** when the code shows the use but a detail is open, **?** when the byte is shared scratch space or its use was not worked out. The meaning of a name is what the byte *holds*, not yet how the game uses it (F9 onward).

Counts: 84 sure, 32 likely, 12 uncertain.

## Checks that test the names

- PASS: FRAME_COUNTER goes up by 1 every frame, except 143 times where Select sets it to 1 and the frame adds 1 (so it reads 2)
- PASS: block $88-$9B and $9C-$AF exchange when the turn changes (19 turn changes, 0 not an exchange; the incoming lives are also lowered by 1)
- PASS: score bytes hold decimal digits in every frame of play (292553 frames, 0 wrong; P1 low byte excluded: it holds the game number during Select)
- PASS: Pac-Man and ghost x always between $04 and $94
- PASS: ghost y never above $5B
- PASS: no instruction names a stack byte $F6-$FF directly (0 do)
- PASS: UNUSED_EA/EB: no instruction names them and they are 0 in every frame
- PASS: LIVES never above 7

## The map

| Address | Name | Sure? | Seen values (distinct / changes) | Code (read / write / change / indexed) | Meaning |
| --- | --- | --- | --- | --- | --- |
| $80 | `FRAME_COUNTER` | sure | 256 / 439610 | change 1, read 8, write 1 | Counts frames, +1 every frame ($F812). Bits 0-1 choose which ghost is handled in this frame ($F92C); bit 2 is tested before the ghost-collision check ($F5D3); bit 3 alternates the missile/ball positions in the kernel ($F088, $F2D2) and, with bit 4, picks the Pac-Man mouth picture; bits 4-5 pick the ghost picture ($F7E0-$F7ED). Select sets it to 1. |
| $81 | `TICK256` | likely | 157 / 2645 | change 1, write 1 | Counts 256-frame ticks (+1 whenever FRAME_COUNTER wraps, $F816); cleared when a joystick direction is pressed ($F633). The wrap of this byte steps ATTRACT_SHIFT. |
| $82 | `ATTRACT_SHIFT` | sure | 158 / 686 | change 1, read 7, write 4 | 0 while a game is being played. Not 0 in the title/attract state: it is then XORed into every colour ($F996) so the picture changes colour; stepped (never to 0) when TICK256 wraps. Power-on value $FF; Select sets 1; fire or Reset sets 0. |
| $83 | `SELECT_REPEAT` | likely | 11 / 1573 | change 1, read 2, write 1 | Counts frames while the Select switch is held ($F8FF); a new game number is taken when it is 0, and again every 32 frames ($F903). |
| $84 | `GAME_SELECT` | sure | 16 / 143 | change 1, read 3, write 2 | Game choice: +1 per Select press, 2 to $11 (16 choices: 8 games x 1 or 2 players), $12 wraps to 2 ($F8E7). Sets LEVEL_SPEED at the start of a game. |
| $85 | `Y_OFFSET` | sure | 47 / 108408 | read 5, write 1 | Where the object being looked at stands in its maze row, vertically: 0 = exactly on a row, otherwise negative ($F6-$FD) = (y mod 10) - 10. Made by the position routine $FBAD. |
| $86 | `X_OFFSET` | sure | 4 / 135187 | read 4, write 1 | The same horizontally: x AND 3 (0 = exactly on a cell column). Made by $FBAD. |
| $87 | `CELL_INDEX` | sure | 67 / 14930 | read 15, write 3 | The maze cell the object being looked at is in: x/4, +40 on odd rows (0-79), index into the maze wall table TBL_FE13. The kernel reuses this byte as the row pointer of the picture. |
| $88 | `MAZE_00` | sure | 38 / 438 | read(indexed) 7, write(indexed) 3 | Wafer map of the current player: 16 bytes, one bit per wafer or power-pill place; all $FF at the start of a level ($FC0C), $8E=$7F and $8A=$FE (the bits that are not wafers). A bit is cleared when the wafer is eaten ($F69D). The kernel draws the dots from these bytes. |
| $89 | `MAZE_01` | sure | 61 / 307 | no direct use | Wafer map, byte 1 of 16 (see MAZE_00). |
| $8A | `MAZE_02` | sure | 40 / 239 | write 1 | Wafer map, byte 2 of 16 (see MAZE_00). |
| $8B | `MAZE_03` | sure | 60 / 284 | no direct use | Wafer map, byte 3 of 16 (see MAZE_00). |
| $8C | `MAZE_04` | sure | 25 / 428 | read(indexed) 2 | Wafer map, byte 4 of 16 (see MAZE_00). |
| $8D | `MAZE_05` | sure | 55 / 326 | no direct use | Wafer map, byte 5 of 16 (see MAZE_00). |
| $8E | `MAZE_06` | sure | 43 / 220 | write 1 | Wafer map, byte 6 of 16 (see MAZE_00). |
| $8F | `MAZE_07` | sure | 36 / 248 | no direct use | Wafer map, byte 7 of 16 (see MAZE_00). |
| $90 | `MAZE_08` | sure | 43 / 272 | read(indexed) 3 | Wafer map, byte 8 of 16 (see MAZE_00). |
| $91 | `MAZE_09` | sure | 27 / 519 | no direct use | Wafer map, byte 9 of 16 (see MAZE_00). |
| $92 | `MAZE_10` | sure | 46 / 271 | no direct use | Wafer map, byte 10 of 16 (see MAZE_00). |
| $93 | `MAZE_11` | sure | 55 / 287 | no direct use | Wafer map, byte 11 of 16 (see MAZE_00). |
| $94 | `MAZE_12` | sure | 36 / 230 | read(indexed) 2 | Wafer map, byte 12 of 16 (see MAZE_00). |
| $95 | `MAZE_13` | sure | 33 / 419 | no direct use | Wafer map, byte 13 of 16 (see MAZE_00). |
| $96 | `MAZE_14` | sure | 45 / 268 | no direct use | Wafer map, byte 14 of 16 (see MAZE_00). |
| $97 | `MAZE_15` | sure | 43 / 250 | no direct use | Wafer map, byte 15 of 16 (see MAZE_00). |
| $98 | `LIVES` | sure | 8 / 376 | change 2, read 3, write 3 | Lives left of the current player (3 at the start; shown as life markers). A copy for the other player is LIVES_P2. |
| $99 | `LEVEL_SPEED` | likely | 8 / 34 | change 1, read 3, write 1 | Level/difficulty step of the current player: 4, 6, 8 or $0A at the start by game number, +1 on every cleared map up to $0E ($F6C2). Chooses the ghost move timers (TBL_FE6F) and the speed-ups. |
| $9A | `PILL_FLAGS` | sure | 99 / 493 | change 1, read 12, write 4 | Bit 7 = power-pill time (ghosts scared); bit 6 = scare running out (blink); bits 2-5 = which of the four power pills are still there ($3C at the start); bits 0-1 = ghosts eaten in this power-pill time (0-3). |
| $9B | `SCORE_COLOUR` | likely | 2 / 19 | read 1, write 1 | Colour number of the current player's score ($0A for player 1); copied to COLOUR_SCORE_P1 every frame ($F87E). |
| $9C | `MAZE_P2_00` | sure | 2 / 12 | read(indexed) 1, write(indexed) 1 | The other player's wafer map (16 bytes). The whole block $88-$9B is exchanged with $9C-$AF when the turn passes to the other player ($FB9F). Zero while only one player plays. |
| $9D | `MAZE_P2_01` | sure | 3 / 14 | no direct use | The other player's wafer map, byte 1 of 16. |
| $9E | `MAZE_P2_02` | sure | 2 / 12 | no direct use | The other player's wafer map, byte 2 of 16. |
| $9F | `MAZE_P2_03` | sure | 2 / 12 | no direct use | The other player's wafer map, byte 3 of 16. |
| $A0 | `MAZE_P2_04` | sure | 2 / 12 | no direct use | The other player's wafer map, byte 4 of 16. |
| $A1 | `MAZE_P2_05` | sure | 2 / 12 | no direct use | The other player's wafer map, byte 5 of 16. |
| $A2 | `MAZE_P2_06` | sure | 2 / 12 | no direct use | The other player's wafer map, byte 6 of 16. |
| $A3 | `MAZE_P2_07` | sure | 2 / 12 | no direct use | The other player's wafer map, byte 7 of 16. |
| $A4 | `MAZE_P2_08` | sure | 2 / 12 | no direct use | The other player's wafer map, byte 8 of 16. |
| $A5 | `MAZE_P2_09` | sure | 4 / 28 | no direct use | The other player's wafer map, byte 9 of 16. |
| $A6 | `MAZE_P2_10` | sure | 2 / 12 | no direct use | The other player's wafer map, byte 10 of 16. |
| $A7 | `MAZE_P2_11` | sure | 2 / 12 | no direct use | The other player's wafer map, byte 11 of 16. |
| $A8 | `MAZE_P2_12` | sure | 2 / 12 | no direct use | The other player's wafer map, byte 12 of 16. |
| $A9 | `MAZE_P2_13` | sure | 4 / 16 | no direct use | The other player's wafer map, byte 13 of 16. |
| $AA | `MAZE_P2_14` | sure | 2 / 12 | no direct use | The other player's wafer map, byte 14 of 16. |
| $AB | `MAZE_P2_15` | sure | 2 / 12 | no direct use | The other player's wafer map, byte 15 of 16. |
| $AC | `LIVES_P2` | sure | 5 / 26 | change 1, read 3, write 2 | The other player's lives (the block $88-$9B swapped with $9C-$AF at a change of turn). |
| $AD | `LEVEL_SPEED_P2` | likely | 5 / 12 | no direct use | The other player's LEVEL_SPEED. |
| $AE | `PILL_FLAGS_P2` | sure | 2 / 12 | no direct use | The other player's PILL_FLAGS. |
| $AF | `SCORE_COLOUR_P2` | likely | 2 / 19 | read 1, write 1 | The other player's score colour number ($09 at the start); copied to COLOUR_SCORE_P2. |
| $B0 | `SPRITE_X` | sure | 146 / 349927 | read(indexed) 1, write 1 | Horizontal place of the figure on the first sprite in this frame: set to the x of the ghost being drawn ($F95F); the byte after it ($B1) is Pac-Man's, so the two are positioned by one loop ($F09F). |
| $B1 | `PACMAN_X` | sure | 145 / 47233 | read 5, read(indexed) 7, write(indexed) 2 | Pac-Man's x (4 to $94). Also the start of the x array for objects: Pac-Man, then the four ghosts. |
| $B2 | `GHOST_X0` | sure | 145 / 59435 | no direct use | Ghost 0 x. |
| $B3 | `GHOST_X1` | sure | 145 / 58974 | no direct use | Ghost 1 x. |
| $B4 | `GHOST_X2` | sure | 145 / 58869 | no direct use | Ghost 2 x. |
| $B5 | `GHOST_X3` | sure | 145 / 58148 | no direct use | Ghost 3 x. |
| $B6 | `PACMAN_Y` | sure | 121 / 27190 | change(indexed) 5, read 6, read(indexed) 10, write 5, write(indexed) 7 | Pac-Man's y (0 to about $5F). Start of the y array for objects. |
| $B7 | `GHOST_Y0` | sure | 80 / 33242 | no direct use | Ghost 0 y. |
| $B8 | `GHOST_Y1` | sure | 80 / 33611 | no direct use | Ghost 1 y. |
| $B9 | `GHOST_Y2` | sure | 81 / 33491 | no direct use | Ghost 2 y. |
| $BA | `GHOST_Y3` | sure | 81 / 33944 | no direct use | Ghost 3 y. |
| $BB | `PACMAN_STATE` | likely | 24 / 75667 | read 10, read(indexed) 24, write 4, write(indexed) 12 | Pac-Man's state bits: bit 0 = facing left (picture mirrored, $F7BA); bits 7-6 = direction code and bit 4 = a step is waiting ($F9E5/$FB54 use the codes $10, $50, $90, $D0); bit 5 = dying animation ($F96A); bit 3 never set for Pac-Man; the exact meaning of the direction code and bit 7 when set at the start is not yet certain. |
| $BC | `GHOST_STATE0` | likely | 19 / 19628 | read(indexed) 3, write(indexed) 1 | Ghost 0 state bits (same layout as PACMAN_STATE): bit 3 = eaten and going home ($08), bit 5 = in the pen/leaving it (animation), bit 4 = step waiting, bits 7-6 = direction code. |
| $BD | `GHOST_STATE1` | likely | 19 / 21160 | no direct use | Ghost 1 state bits. |
| $BE | `GHOST_STATE2` | likely | 22 / 19788 | no direct use | Ghost 2 state bits. |
| $BF | `GHOST_STATE3` | likely | 22 / 20922 | no direct use | Ghost 3 state bits. |
| $C0 | `SPEED_ACC0` | likely | 33 / 137707 | read(indexed) 2, write 1, write(indexed) 4 | Pac-Man's speed accumulator: the byte times 17 is added to itself every frame and a carry means "move one step" ($F9EE). Reset at a new level. |
| $C1 | `SPEED_ACC1` | likely | 110 / 87184 | no direct use | Ghost 0 speed accumulator (same method; start value $0F). |
| $C2 | `SPEED_ACC2` | likely | 90 / 86779 | no direct use | Ghost 1 speed accumulator. |
| $C3 | `SPEED_ACC3` | likely | 110 / 86978 | no direct use | Ghost 2 speed accumulator. |
| $C4 | `SPEED_ACC4` | likely | 90 / 86252 | change(indexed) 2, write(indexed) 3 | Ghost 3 speed accumulator. |
| $C5 | `GHOST_TIMER0` | likely | 13 / 12933 | no direct use | Ghost 0 countdown to the next change of target ($FA82); reloaded from TBL_FE6F by LEVEL_SPEED. |
| $C6 | `GHOST_TIMER1` | likely | 13 / 14495 | no direct use | Ghost 1 countdown. |
| $C7 | `GHOST_TIMER2` | likely | 13 / 12890 | no direct use | Ghost 2 countdown. |
| $C8 | `GHOST_TIMER3` | likely | 13 / 14437 | no direct use | Ghost 3 countdown. |
| $C9 | `CELL_ROW` | likely | 10 / 3077 | change 2, read 14, write 3 | Maze row of the object being looked at (y / 10) with flag bits mixed in by $FBD7-$FBDB; the kernel counts maze rows in the same byte while drawing. |
| $CA | `ROW_PARITY` | ? | 1 / 0 | read 5, write 2 | Used by the kernel as the even/odd marker of the maze row being drawn (1 or 0); written only there ($F0D4, $F2C6). Not used by the logic. |
| $CB | `RNG_STATE` | sure | 125 / 89502 | change 1, read 5, write 2 | The game's pseudo-random number: a shift register stepped by $FBE0 (shift, exclusive-or of two bits, rotate in); set to $FF at power-on and at the start of every game ($F892). Used when a ghost chooses a direction. Stage F14 follows it. |
| $CC | `SCORE_P1_LO` | sure | 101 / 4789 | read(indexed) 3, write 2, write(indexed) 2 | Score of player 1: the two lowest decimal digits (tens, ones; each nibble a digit, decimal mode). Shows $CB (blank pattern) before the first game; holds the game number while Select is pressed. |
| $CD | `SCORE_P2_LO` | sure | 9 / 114 | write 1 | Score of player 2: the two lowest digits. |
| $CE | `SCORE_P1_MID` | sure | 45 / 203 | read(indexed) 1, write 1, write(indexed) 1 | Player 1: hundreds and thousands (value $ED = blank pattern before the first game). |
| $CF | `SCORE_P2_MID` | sure | 1 / 0 | no direct use | Player 2: hundreds and thousands. |
| $D0 | `SCORE_P1_HI` | sure | 3 / 69 | read(indexed) 1, write 1, write(indexed) 1 | Player 1: ten-thousands and hundred-thousands (value $0F = blank pattern). |
| $D1 | `SCORE_P2_HI` | sure | 1 / 0 | no direct use | Player 2: ten-thousands and hundred-thousands. |
| $D2 | `TEMP_D2` | ? | 9 / 10439 | pointer 1, read 11, write 11, write(indexed) 1 | Scratch. Holds the first playfield value of a maze row in the kernel; the target y when a ghost chooses a direction; Pac-Man's wanted direction in the move code; the score-line pointers. |
| $D3 | `TEMP_D3` | ? | 8 / 6440 | read 5, write 10, write(indexed) 1 | Scratch (second playfield value in the kernel; target x of a ghost). |
| $D4 | `TEMP_D4` | ? | 3 / 6 | read 4, write 3, write(indexed) 1 | Scratch (kernel playfield values). |
| $D5 | `TEMP_D5` | ? | 2 / 15 | pointer 1, read 6, write 6, write(indexed) 3 | Scratch (kernel playfield values; score digit pointers). |
| $D6 | `TEMP_D6` | ? | 4 / 25 | read 5, write 2 | Scratch (kernel playfield values). |
| $D7 | `TEMP_D7` | ? | 1 / 0 | pointer 1, read 5, write 2 | Scratch (kernel playfield values; score digit pointers). |
| $D8 | `SPRITE0_Y` | likely | 2 / 70 | read 5, write 1, write(indexed) 2 | First line of the first sprite (ghost) in the picture: the ghost's y + 11 ($F966). In the score line it holds a pointer byte. |
| $D9 | `SPRITE1_Y` | sure | 121 / 27190 | pointer 1, read 4, write 1 | First line of Pac-Man in the picture: PACMAN_Y + 11 ($F84A). |
| $DA | `TEMP_DA` | ? | 2 / 70 | no direct use | Only used as a pointer byte of the score line ($FE after it). |
| $DB | `SPRITE0_LINES` | likely | 1 / 0 | pointer 1, read 5, write 6, write(indexed) 1 | Line counter of the ghost picture: 7 down to -1 as its lines are drawn. |
| $DC | `SPRITE1_LINES` | likely | 1 / 0 | read 4, write 5 | Line counter of the Pac-Man picture: 6 down to -1. |
| $DD | `SPRITE0_PTR_LO` | sure | 4 / 27476 | pointer 6, read(indexed) 1, write 2, write(indexed) 1 | Low byte of the pointer to the ghost picture (one of the four pictures of TBL_FDAB; high byte = SPRITE0_PTR_HI). |
| $DE | `SPRITE0_PTR_HI` | sure | 1 / 0 | write 1 | High byte of that pointer: always $FD ($F841). |
| $DF | `SPRITE1_PTR_LO` | sure | 10 / 47618 | pointer 4, write 1 | Low byte of the pointer to the Pac-Man picture (the mouth phase from TBL_FDC2, or the dying pictures from TBL_FDFA). |
| $E0 | `SPRITE1_PTR_HI` | sure | 1 / 0 | write 1 | High byte of that pointer: always $FD. |
| $E1 | `TEMP_E1` | ? | 11 / 9222 | change 3, read 7, write 4 | Scratch (kernel: the missile/ball enable value; score line: digit counter). |
| $E2 | `TEMP_E2` | ? | 7 / 235 | read 4, write 5 | Scratch (kernel; score line). |
| $E3 | `TEMP_E3` | ? | 1 / 0 | read 2, write 1 | Scratch (kernel: the third playfield value). |
| $E4 | `EVENT_TIMER` | likely | 161 / 61701 | change 1, read 4, write 4 | Countdown of the current pause/effect: the power-pill time, the pause after a death ($3F), the start-of-level pause ($3C). Counts down every second frame ($F500); its value also shapes the death sound. |
| $E5 | `EAT_PAUSE` | likely | 34 / 6369 | change 1, read 2, write 1 | Pause after eating a ghost: set to $9F, counts down while negative ($F4F2); while bit 7 is set the ghosts do not move ($F94C). |
| $E6 | `PLAYER_MODE` | sure | 3 / 162 | read 10, write 3 | Bit 6 = two-player game; bit 7 = it is player two's turn. |
| $E7 | `GAME_FLAGS` | likely | 17 / 1314 | read 19, write 14 | Bit 7 = the game is frozen (start of a level, ready to go: ghosts at home); bit 6 = death sequence; bit 5 = the difficulty switch of the current player is set to A; bit 4 = set by Select, tested at game start (meaning not certain); bit 3 = start tune playing; bit 1 = bonus item showing. |
| $E8 | `SOUND_ACTIVE` | likely | 5 / 8136 | read 2, write 3 | The sound now playing, as a bit (8, 4, 2, 1 from TBL_FE6E/FE6F); 0 = none. A new sound starts only if its mask in TBL_FFE9 allows it ($FF43). |
| $E9 | `SOUND_TIMER` | likely | 48 / 46284 | change(indexed) 1, read 2, write 1 | Frames left of that sound; loaded from TBL_FFED. |
| $EA | `UNUSED_EA` | likely | 1 / 0 | no direct use | No instruction reads or writes it; always 0. |
| $EB | `UNUSED_EB` | likely | 1 / 0 | no direct use | No instruction reads or writes it; always 0. |
| $EC | `BONUS_TIMER` | likely | 18 / 1661 | change 1, write 3 | Counts 256-frame ticks to the next appearance/disappearance of the bonus item ($F81A): when it reaches 0, GAME_FLAGS bit 1 is flipped and the timer is reloaded with $12 (item gone) or 3 (item showing). Power-on value $0C; the attract screen shows the same timing. |
| $ED | `JOY_DIRECTION` | sure | 8 / 8944 | read 2, write 1 | The joystick of the current player: bit 0 up, 1 down, 2 left, 3 right (0 = none) - from SWCHA, upper or lower half by player ($F61E-$F62D). |
| $EE | `POS_CODE` | ? | 219 / 156272 | read 1, write 1 | Cell index x 4 plus row flags from the position routine ($FBDD); read once ($F6F4, compared with $F9). Meaning not certain. |
| $EF | `COLOUR_MASK` | sure | 3 / 184 | read 2, write 1 | $FF for colour, $0F for black-and-white (from the TV TYPE switch, $F85E); bit 3 flipped in attract mode. |
| $F0 | `COLOUR_BG` | sure | 1 / 0 | read 1, read(indexed) 1, write 1, write(indexed) 1 | Background colour: starts every frame as colour number 7 ($F869) and is turned into a real colour (table lookup, XOR ATTRACT_SHIFT, AND COLOUR_MASK) in the top logic ($F992). |
| $F1 | `COLOUR_MAZE` | sure | 1 / 0 | read 1, write 1 | Maze colour: colour number 8, converted the same way. |
| $F2 | `COLOUR_SPRITE0` | sure | 109 / 394074 | read 1, write 1 | Colour of the first sprite (the ghost drawn in this frame): the ghost number 0-3, or 4/5 when scared; converted to a colour. |
| $F3 | `COLOUR_SPRITE1` | sure | 85 / 887 | read 1, write 1 | Colour of Pac-Man: number 6, converted. |
| $F4 | `COLOUR_SCORE_P2` | sure | 2 / 19 | read 2, write 1 | Colour of player 2's score line (from SCORE_COLOUR_P2). |
| $F5 | `COLOUR_SCORE_P1` | sure | 83 / 778 | read 2, write 1 | Colour of player 1's score line (from SCORE_COLOUR). |
| $F6 | `STACK_F6` | sure | 9 / 47379 | no direct use | The processor's stack (return addresses of subroutine calls), $FF downwards; the deepest chain of calls needs 10 bytes ($F6-$FF). |
| $F7 | `STACK_F7` | sure | 2 / 70 | no direct use | Stack. |
| $F8 | `STACK_F8` | sure | 3 / 21048 | no direct use | Stack. |
| $F9 | `STACK_F9` | sure | 2 / 70 | no direct use | Stack. |
| $FA | `STACK_FA` | sure | 15 / 88955 | no direct use | Stack. |
| $FB | `STACK_FB` | sure | 7 / 204988 | no direct use | Stack. |
| $FC | `STACK_FC` | sure | 6 / 15448 | no direct use | Stack. |
| $FD | `STACK_FD` | sure | 7 / 18136 | no direct use | Stack. |
| $FE | `STACK_FE` | sure | 2 / 70 | no direct use | Stack. |
| $FF | `STACK_FF` | sure | 2 / 70 | no direct use | Stack. |

## Dual use (important for the rebuild)

- `$87`, `$C9`, `$CA`, `$D2-$D7`, `$E1-$E3` are used by the kernel (picture) and by the logic for different things; they never hold both at once because the logic runs outside the picture. The rebuild must keep them as separate variables or save them.
- `$D5-$DE` also hold pointers to the score digits while the score line is drawn.
- `$F0-$F5` hold colour *numbers* at the start of a frame and real colours after `$F992`.

## Not claimed

Bits marked "likely" or "?" are listed with what is certain. The ghost direction code and states, the speed accumulators and the flags in `GAME_FLAGS` are settled in F10-F17. The random number (`RNG_STATE`) is a real shift register, which corrects the Stage E4 note "no randomness found": it is seeded the same way every game, so replays stay identical, but it is the game's own random source (F14).
