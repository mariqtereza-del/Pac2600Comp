# Stage F9: input reading (2026-10-26)

`python -I tools\f9_input.py rom\pacman.a26` (after `f1`..`f8`) finds every place where the program reads the console's inputs, checks what it does with them against the 70 baseline games, runs two extra experiments, and measures the scanline of each read. This document is written by that tool.

## Checks

- PASS: exactly five instructions read inputs: $F61E LDA SWCHA, $F857 AND SWCHB, $F886 LDA INPT4, $F88A LDA SWCHB, $F91C AND SWCHB
- PASS: JOY_DIRECTION: in 8944 changes during player 1 turns it equals the left joystick bits (8943 times), in player 2 turns it is 0 although the left stick is pressed (1 times), 0 other
- PASS: game starts: 46 by fire, 76 by Reset, 0 unexplained
- PASS: the game number steps 143 times, 0 of them without Select pressed
- PASS: GAME_FLAGS bit 5 equals the left difficulty switch (A = 1) in 288391 frames of play of player 1, differs in 0
- PASS: COLOUR_MASK is $FF (colour) or $0F (black and white) in 292553 frames of play and with bit 3 flipped in 147057 frames of the title screen; 0 differ
- PASS: player 2 turn (frames 1000-1199): JOY_DIRECTION follows the RIGHT joystick (left 40, right 40, down 40 frames) and ignores the LEFT joystick pushed up (up bits seen: 0); GAME_FLAGS bit 5 set while the RIGHT difficulty is A in all 190 frames: True
- PASS: Select held from frame 100 to 259: the game number steps at frames [100, 131, 162, 193, 224, 255] (first on the press, then every 31 frames)
- PASS: all five read points were seen in the traced frames: $F61E at scanline 226/238, $F857 at scanline 261, $F886 at scanline 0, $F88A at scanline 0, $F91C at scanline 0/2/16
- PASS: three instructions that no baseline game runs were run by the extra games: BEQ $F8C8 at $F907 (the game number steps again while Select is held) in X-select-hold, first at frame 131; DEY at $F832 (difficulty A, bonus item showing) in X-left-A-idle, first at frame 3071; LDY #$50 at $F5C2 (difficulty A, power pill eaten) in X-power-A, first at frame 2225

## What the program reads (all of it)

| Where | Reads | What it is | When | Used for |
| --- | --- | --- | --- | --- |
| `$F61E` | `SWCHA` | the two joysticks: upper half = left joystick, lower half = right joystick; 0 = pressed | bottom logic of every frame in which the game is running (not on the title screen, not frozen, and not in the frames where a ghost kills Pac-Man) | `JOY_DIRECTION` (`$ED`): inverted, **left joystick in player 1's turn, right joystick in player 2's turn** ($E6 bit 7); bit 0 up, 1 down, 2 left, 3 right. Any direction also zeroes `TICK256`. |
| `$F857` | `SWCHB` bit 3 | TV type: 1 = colour, 0 = black and white | start of every frame | `COLOUR_MASK`: `$FF` or `$0F`, bit 3 flipped on the title screen |
| `$F886` | `INPT4` (via `$3C`) | fire button of the **left** joystick; bit 7 = 0 when pressed | top logic, only when `ATTRACT_SHIFT` is not 0 (title screen / after game over) | starts a game |
| `$F88A` | `SWCHB` bits 0, 1 | Reset (bit 0) and Select (bit 1); 0 = pressed | top logic, every frame | Reset starts a game (also during play); Select steps the game number: at once on the press and again every 31 frames while it is held, giving 16 choices ($84, 2 to $11; the choice also toggles the two-player bit of `PLAYER_MODE`) |
| `$F91C` | `SWCHB` bit 6 / bit 7 | difficulty: **left** switch (bit 6) in player 1's turn, **right** switch (bit 7) in player 2's turn; 1 = A | top logic, every frame | `GAME_FLAGS` bit 5 (the difficulty of the player whose turn it is): A shortens the bonus item cycle and the scared time of the ghosts |

Nothing else is read: the second fire button, both paddles and `SWACNT`/`SWBCNT` are never touched. In play the fire button has no effect (C6). In a two-player game the right joystick's fire button and the left joystick are never used for player 2.

## For the rebuild

The input of one frame is: joystick bits (left and right, 4 bits each), left fire, Select, Reset, left difficulty, right difficulty, TV type. Every read can use the state at the start of the frame (the host sets the inputs once per frame); the reads fall on scanline 261 (TV type), scanlines 0-16 (fire, Reset, Select, difficulty) and 226-238 (joysticks), as measured, so no input needs to change inside a frame. The recorded games (`.inputs` files) contain only the left joystick: **in player 2's turn they therefore never move Pac-Man**. A faithful port must read the right joystick there; the host program (D10) maps the keyboard and pad to the left joystick only and needs a second set of keys for player 2 (a decision for the owner at Phase H, noted in `DECISIONS.md` T-F9-1).

## A correction to F2-F6 (code that was listed as data)

Three places of the listing were data in F2 to F8 and are code: `$F5C2-$F5C3` (`LDY #$50`), `$F832` (`DEY`) and `$F907-$F908` (`BEQ $F8C8`). Each is the not-taken side of a branch that no baseline game takes: the difficulty switch at A with a power pill or with the bonus item showing, and the Select switch held for 31 frames. The reading of the input led to them; three extra games (`tools\f9_inputs\X-*.inputs`, no golden trace, only their instructions count) now run them, and `f1_coverage.py` and `f6_coverage.py` include them. The map is now 1,810 instruction starts, 1,735 operand bytes, 509 data bytes and 42 bytes never touched; the listing rebuilds byte-identical at every stage.

The lesson, kept as a rule for Phase F: **a byte that no recording runs is not proof that it is data.** The 42 untouched bytes (12 bytes of never-run code at `$FACF-$FAD1`, `$FAF9-$FAFB`, `$FB2A-$FB2C`, `$FB33-$FB35`, and 30 bytes in table holes and the end of the ROM) stay as listed in `docs/F6_COVERAGE.md`; later stages must read the code around any table hole before trusting it.
