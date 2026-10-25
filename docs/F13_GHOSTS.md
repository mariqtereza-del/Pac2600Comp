# Stage F13: ghost movement and decisions (2026-10-26)

`python -I tools\f13_ghosts.py rom\pacman.a26` holds a model of the ghost code and checks it against the recorded games. From the memory of the frame before, the ROM tables and the pressed buttons it predicts, for the ghost the frame updates, its x, y, state, speed accumulator and timer and the random number `$CB`, and compares them with the recorded memory. This document is written by that tool.

## Result

- **348146 of 348146 predicted frames are exactly right (0 wrong)**, all other ghosts unchanged, in the 70 games (439610 frames).
- Frames not predicted, with the reason: Select or Reset pressed: 2180; change of turn: 19; death or new map starts in the bottom logic (ghosts are put back): 323; difficulty switch on the title screen: 5055; fire on the title screen (a game starts): 46; frozen game (the top logic does not move ghosts; the bottom logic sets their timers): 82443; test-only memory edits (BL-065/066): 1398.

Roads of the code taken in the predicted frames:

| Frames or times | Road |
| --- | --- |
| 14400 | a way is allowed but would turn back: refused |
| 14454 | chase: no direction taken, wander |
| 126538 | decide: a direction was taken |
| 42111 | decide: chase |
| 10 | decide: enters the bottom opening |
| 2 | decide: enters the top opening |
| 3577 | decide: eyes, aim home |
| 812120 | decide: not exactly on a cell |
| 8453 | decide: scared, aim away |
| 74217 | decide: step already due |
| 10854 | decide: timer ran out, reloaded, wander |
| 61543 | decide: title screen, wander |
| 387 | ghost animation step (leaving the pen / opening) |
| 7478 | random chain, direction down |
| 34004 | random chain, direction left |
| 38141 | random chain, direction right |
| 7228 | random chain, direction up |
| 13090 | random pair left/right |
| 51431 | random pair up/down |
| 358 | update: eyes at the pen door x=$58, turn left |
| 192 | update: eyes home, ghost again, go right |
| 20386 | update: eyes wait at the pen while scared |

## The rules (what the code does, in the order it does it)

All of this is in the top logic of a frame, after the joystick, the pill-colour and sprite-pointer lines (`$F92C-$F980`). The ghost handled in a frame is
object `(FRAME_COUNTER & 3) + 1`, where FRAME_COUNTER is the value of the frame before (the counter goes up at the end of a frame, in `FRAME_START`): **each ghost is handled once every 4 frames, but three times in that frame** (`GHOST_UPDATE` is called three times in a row), so its speed accumulator is stepped three times and it can move up to three pixels.

1. *Does it run?* A ghost whose state bit 3 is set (eaten, only eyes) always runs; others run only if `EAT_PAUSE` is not negative (not in the 32-frame pause after a ghost was eaten). Nothing runs while `GAME_FLAGS` bit 7 is set (frozen: death, new life, new map); in that time the bottom logic sets the accumulators and timers of the four ghosts from `LEVEL_SPEED` (objects 4 and 2: accumulator = LEVEL_SPEED; objects 3 and 1: LEVEL_SPEED - 1; all four timers LEVEL_SPEED - 1).
2. *`GHOST_UPDATE`*:
   - eyes at y = $1E: at x = $58 (the pen door) turn left and move; at x = $4C (above the pen) wait there while the ghosts are scared, otherwise become a ghost again (bit 3 cleared), go right and move;
   - a ghost in an opening animation (bit 5) does nothing here;
   - otherwise `LOCATE_OBJECT`, `GHOST_DECIDE`, and then `MOVE_OBJECT` unless the decision put the ghost into an opening (bit 5).
3. *`GHOST_DECIDE`* only decides when the ghost stands exactly on a cell (both offsets 0) and no step is pending (state bit 4 clear). Then:
   - on the title screen (`ATTRACT_SHIFT` not 0): wander (below);
   - eyes: aim at the pen door (x = $58, y = $1F) and chase;
   - at x = $4C, y = 0 or y = $46 and the low four bits of the random number 0 (1 time in 16): enter the top or bottom opening (`WRAP_TO_TOP` / `WRAP_TO_BOTTOM`: state and y set, animation follows);
   - scared (`PILL_FLAGS` bit 7): aim at x = NOT(Pac-Man x + $60) (a mirror image) and **at Pac-Man's y** (the original works out a mirrored y as well but never stores it, so scared ghosts run away horizontally and still go towards Pac-Man's row), and chase;
   - otherwise the ghost's timer goes down by one; while it is not zero: aim at Pac-Man and chase. When it reaches zero the timer is reloaded from the table `$FE6F` indexed by LEVEL_SPEED / 2 (plus 1 for objects 2 and 3) and the ghost wanders for this decision.
4. *Chase*: if the target x is bigger try right, if smaller try left; then if the ghost's y is smaller than the target y try down, otherwise (also equal) try up. The first allowed direction wins and ends the decision (`TRY_COMMIT`). A direction that would turn the ghost straight back is refused, so a ghost never reverses on its own. If nothing was taken, wander.
5. *Wander* (a chain that depends on the direction the ghost is going): the random number is stepped (`RNG_STEP`: a shift register, state `$CB`); if the result is zero try to keep going straight; then step again and try the two sideways directions in an order chosen by the result; then straight again. The chains for up, right, left and down fall through into each other in that order, but a ghost never gets past the first chain because a corridor always offers some direction.
6. *Allowed* (`TRY_*`): up needs bit 6 and down bit 5 of the wall-table byte of the cell (and not row 0 / row 7); left and right need no wall bit in the neighbour cell (cell - 1, cell + 2) and have two exceptions on row 3 (cells $3E and $38).
7. *`MOVE_OBJECT`* as for Pac-Man (F10): multiply the accumulator by 17, a carry is one step. Ghost speeds come from the accumulator start values (level); the other speeds are not claimed beyond what the traces show.
8. After the updates, a ghost in an opening animation (bit 5) is animated by `ANIMATE_OBJECT` (y counts through $51-$5B below the maze or 0-$0B above it); this also runs during the eaten-ghost pause.

## Not claimed

- The random number `$CB` is predicted exactly (it is stepped in 86851 frames of the checked ones) but where it starts is F14 (it is set to $FF when a game starts, `$F890`).
- Frozen frames, the death sequence, the change of turn and new-map frames are skipped here (counted above); F16 and F17 follow them.
- Roads the games never took (a ghost running out of the last wander chain; going up or down out of the pair) are marked "NEVER RUN" in the code listing.
- The ghost speed table and what raises it with the level is F16.

