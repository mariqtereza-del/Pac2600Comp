# Stage F17: game states (2026-10-26)

`python -I tools\f17_states.py rom\pacman.a26` puts every frame of the 70 recorded games (the 2 test games BL-065/066 are skipped) into one of six states from the memory alone, checks that every change from one state to the next has one of the causes listed below, and checks the memory on entry into the main states. This document is written by that tool.

## Result: every change of state explained, every entry as listed

## The states

| State | How the memory says it | What happens in it |
| --- | --- | --- |
| TITLE | `ATTRACT_SHIFT` (`$82`) is not 0 | Power-on, after a game over, and the game-number screen. The bottom logic is skipped (no Pac-Man logic); the ghosts wander (F13) for show; the colours cycle because `ATTRACT_SHIFT` is xor-ed into them and goes up every 256 frames (skipping 0). Fire or Reset starts a game; Select steps the game number. |
| FROZEN | `ATTRACT_SHIFT` = 0, `GAME_FLAGS` bit 7 set, bit 6 clear | The start of a game, of a try or of a map: everything stands still for 60 steps of the event timer (120 frames) at the start places; the bottom logic sets the ghost speed accumulators and timers (F13, F16). |
| PLAY | none of the others | The rules of F10-F16. |
| EAT-PAUSE | `EAT_PAUSE` bit 7 set | 32 frames after a scared ghost was eaten: Pac-Man and the other ghosts stand still, an eaten ghost goes on as eyes, and the event timer stands still (F12, F16). |
| DYING | `GAME_FLAGS` bits 7 and 6 set | After an unscared ghost touched Pac-Man: the event timer starts at `$3F` (126 frames), Pac-Man's dying animation plays; then the next try or game over. |

Where the bits are set: `GAME_FLAGS` (`$E7`) bit 7 = frozen or dying, bit 6 = dying, bit 5 = difficulty switch of the player in turn, bit 4 = game-number screen, bit 3 = a try is running (set at every start of a try), bit 1 = bonus item shown (F15).

## The changes of state (all that occur)

| Times | From | To | Cause |
| --- | --- | --- | --- |
| 37240 | DYING | DYING | stay |
| 241 | DYING | FROZEN | death over, lives left: next try |
| 1 | DYING | TITLE | Select during a game: back to the game-number screen |
| 52 | DYING | TITLE | death over, no lives left: game over |
| 5983 | EAT-PAUSE | EAT-PAUSE | stay |
| 193 | EAT-PAUSE | PLAY | pause over (32 frames) |
| 44562 | FROZEN | FROZEN | stay |
| 675 | FROZEN | FROZEN | stay (Reset pressed: a new game is set up) |
| 367 | FROZEN | PLAY | freeze over (timer ran out) |
| 303 | PLAY | DYING | unscared ghost touched |
| 193 | PLAY | EAT-PAUSE | scared ghost touched |
| 1 | PLAY | FROZEN | Reset: new game |
| 20 | PLAY | FROZEN | last wafer: next map |
| 201735 | PLAY | PLAY | stay |
| 4 | PLAY | TITLE | Select during a game: back to the game-number screen |
| 74 | TITLE | FROZEN | Reset: new game |
| 46 | TITLE | FROZEN | fire on the title screen: new game |
| 146522 | TITLE | TITLE | stay |

## Entry into a state: what the memory holds

| Count | Check |
| --- | --- |
| 143 | Select step sets game number, 1P/2P, flags $10, title timer 1, score line = game number, right |
| 52 | game over sets the title timer $5F and the flags 0, right |
| 121 | game start sets everything as listed, right |
| 20 | next map: frozen $3C, objects at the start places, right |
| 241 | next try after a death sets lives - 1, frozen $3C, Pac-Man and ghosts at the start places, right |

## Game start in detail (`$F890-$F90B`)

Reset (any state) or fire (title screen only) starts a game. The code: random number `$CB` = `$FF`; `CLEAR_SCORES` (both scores and both lives 0); `ATTRACT_SHIFT` = 0; `GAME_FLAGS` = 8; Pac-Man's speed seed (`$09` for game number 2-7, else `$0B`, F10); `NEW_GAME_SETUP` (3 lives, score colours, start level from the game number, F16) which falls into `NEW_MAZE` (the maze filled, pills `$3C`, F11) and `PLACE_OBJECTS` (frozen, timer `$3C`, objects at their start places `x = $4C`, ghosts `y = $1E`, Pac-Man `y = $3C`). With two players (`PLAYER_MODE` bit 6) the player blocks are swapped and the set-up is done a second time for player 2, who gets 4 in the lives byte of the second block, i.e. 3 lives plus the one in play.

## The game-number screen (Select)

Select pressed (`SWCHB` bit 1 = 0): at once and then every 31 frames while held, `GAME_SELECT` (`$84`) goes up by 1 (from `$11` back to 2), `PLAYER_MODE` bit 6 flips (so each step alternates one and two players: the pairs of values 2k and 2k+1 are one game number, eight of them), `GAME_FLAGS` = `$10`, `ATTRACT_SHIFT` = 1 and the frame counter = 1, Pac-Man's `y` = `$60` (out of sight), sound off, and both score bytes show the game number (`((GAME_SELECT / 2) - 2) and 7`, 0 shown as 8). The game number sets the start level (F16) and Pac-Man's speed (F10); two players use the right joystick and right difficulty switch for player 2 (F9).

## The end of a try

When the event timer of a death runs out (`$F51A-$F550`): `GAME_FLAGS` bit 6 off, `PLACE_OBJECTS` (frozen again, `$3C`); in a two-player game, if the other player still has lives, the blocks are swapped and the turn passes (`PLAYER_MODE` bit 7 flips); then if any life is left in either block, the player in turn loses one and `GAME_FLAGS` bit 3 is set (next try); if none is left anywhere: `GAME_FLAGS` = 0, `ATTRACT_SHIFT` = `$5F`, Pac-Man's y = `$5F`: game over, the title screen without a new game number. The code reads the fire button in every frame in which `ATTRACT_SHIFT` is not 0, so it would start a game at once; Stage E6 saw the button work only after about 500 frames after a game over, and F17 did not explain that delay (the recordings show 46 starts by fire, all on the title screen).

## Not claimed

- Two-player games: the turn change (blocks swapped) was recorded 19 times, but player 2 never had a joystick in a recording (F9), so the play of player 2 itself is from the code only. The death animation and sound are F18 and F19.
- The long-idle behaviour of `ATTRACT_SHIFT` after hours of title screen (it counts up and skips 0 for ever).
