# Stage F23.4: the collision flags from the video-chip model (2026-10-26)

The game decides whether a ghost caught Pac-Man and whether he took the bonus item from two flags of the video chip (F12). The video-chip model (`game::TiaVideo`) now keeps the same latches while it draws: player 0 touched player 1 (the ghost), missile 1 touched player 1 and the ball touched player 1 (the bonus item), cleared by `CXCLR`. `game::Game` asks the picture for them after the drawing code and before the bottom logic (`TimedChip::collisions`). The recorded answers of F20 (the oracle) are no longer used.

## Result: identical

- **70 of 70 recorded games, 439680 frames, with no oracle at all: all 128 memory bytes (but the stack) and the picture identical in every frame.** 227.6 s of work.
- Control: without the flags (and without pictures) 52 of 70 games fail, so the flags matter and the test can tell.
- For scale, the oracle files list 203264 frames; the bonus flag is set in 18 and the ghost flag in 667 of them.

## What this ends

The native game now needs nothing from the original or from recordings except the player's inputs and the cartridge data: its logic, its drawing code, its chip model and its collision flags are all our own code. What is still missing from a finished game: the sound generator (F24), the whole-game checks including random-input games (F25) and taking Stella out of the program (F26).

## Not claimed

- Sound is not made (F24).
- Only recorded inputs are played; random-input games come in F25.
