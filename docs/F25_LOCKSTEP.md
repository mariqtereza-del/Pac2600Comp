# Stage F25: whole-game lockstep (2026-10-26)

The finished native game (`game::Game` with the drawing code, the video-chip model, the collision flags from the picture and the sound-chip model: no 6502, no emulator, no original code, no recorded answers) plays the same inputs as the emulator and is compared after **every frame** in three ways: all 128 memory bytes (but the stack bytes `$F6-$FF`), the fingerprint of the 228 x 160 picture and the fingerprint of the sound handed over. `python -I tools\f25_games.py rom\pacman.a26` makes 120 new games with seeded random input (the golden traces are made by the program with the emulator inside, `pacman2600 --headless --replay --trace`) and runs them together with the 70 recorded games.

## Result: identical

- **120 new games, 2400000 frames, and the 70 recorded games, 439680 frames: memory, picture and sound identical in every frame.**

| Kind of game | Games | Frames |
| --- | --- | --- |
| play | 24 | 480000 |
| two | 24 | 480000 |
| switches | 24 | 480000 |
| title | 24 | 480000 |
| chaotic | 24 | 480000 |

Kinds: `play` (Game Reset at frame 240, then random holds of the joystick, now and then fire, sometimes a new game later), `two` (Select before the start: two players, the second player's turns with a neutral right joystick), `switches` (difficulty and colour switches flipped at random times), `title` (Select pressed and held for different lengths so that the game number steps, fire and Reset at random times), `chaotic` (every few frames a random combination of all inputs including opposite directions together). The same seeds always give the same games.

What the new games reached (counted in the golden traces): 1245463 frames of play (not on the title screen), 423179 of them with two players, 183187 with the bonus item showing, 13400 frames with the ghosts scared, 1543 deaths, highest level number 10.

## Not claimed

- The right joystick and the right fire button are not in the recordings of this project, so the second player of a two-player game never moves.
- Random play seldom reaches the later levels or the bonus item; those are covered by the long recorded games (the bot of E7).
- Time: 1505.3 s of work for all runs.
