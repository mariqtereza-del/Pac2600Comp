# How the game was preserved

This is the story and the findings of the project in plain words. It describes how the game behaves and how that was proved. It does not contain the cartridge's code or data; the cartridge belongs to its rights holders (see the notice in [README.md](../README.md)).

## The goal

Keep the 1981 Atari 2600 *Pac-Man* playable and understood **exactly as it was**, on a modern PC, without needing an emulator to run it, and without publishing the game itself. "Exactly" is the rule of the project: nothing that changes what the game does is ever added to the default way of playing, and every optional extra must be proved neutral.

## The method

1. **Identify the file.** The game is a 4,096-byte cartridge with no bank switching. Its size and SHA-1 fingerprint are written down, and the program accepts only that file.
2. **Choose a reference.** The Stella emulator (version 7.0) runs the original cartridge. Every claim is compared with it.
3. **Measure the console around the game.** The picture, the timing of the frame, the sound and the controls were recorded in detail: how many lines, when each chip is written, what each switch does.
4. **Record the game.** 70 short games were played and recorded on purpose, each to show one thing: starting up, moving, the ghosts, scoring, dying, long play, two players, odd situations such as pressing Game Reset in the middle of a game. After every frame the reference's memory, picture and sound were written down. These are the *baseline*.
5. **Work out how the game works.** The original's parts were listed and understood one at a time (about forty routines), each described by what it does to the game's memory.
6. **Rewrite each part, and prove it.** A routine was written again in C++ and swapped into a machine that otherwise ran the original. The whole frame was compared with the original after each swap; only an exact match was kept.
7. **Remove the last of the original.** When every part was rewritten, the game ran with no cartridge code at all. The same proofs were run again on the whole game.
8. **Keep the proofs.** The 70 recorded games, with their inputs, stay in the repository, and a *preservation suite* fixes them with fingerprints, so that no later change can quietly alter the game.

## The result

- 70 recorded games, 439,680 frames, and 120 games played with random input, 2,400,000 frames: the rebuilt game's memory, picture and sound are **identical to the original, after every frame**.
- The picture and sound are made by our own models of the video and sound chips. Saved pictures of recorded games match the reference pixel for pixel.
- Where the reference emulator differs from a real 1981 NTSC console, the program follows the console: the speed is 59.9227 frames a second (not Stella's round 60), the sound pitch is the console's, and the console's memory starts at zero.

## What was found about the game

Rules and behaviour, as the original does them:

- **The game is a function of the buttons.** It uses one random number, set to the same value at the start of every game and advanced by a shift register that repeats after 127 steps. No clock, timer or button feeds it. Press the same buttons at the same frames and the same game happens, every time. That is why a recording replays perfectly.
- **Movement.** Pac-Man and the ghosts move on a grid of 4-pixel cells and 10-pixel rows, one pixel per step. Pac-Man takes about 1.8 frames per step in the slower games and about 1.45 in the faster ones; each ghost moves on its own turn, one frame in four. The maze has no side tunnel; the openings are in the top and bottom walls.
- **The eight games.** The console's Game Select offers eight games, for one or two players. They differ in the level the game starts at and in whether Pac-Man is slower or faster.
- **Levels.** The level changes only the ghosts' speed and how long they chase; Pac-Man does not speed up. Each cleared maze adds a level, up to 14.
- **The ghosts.** A ghost decides where to turn only when it stands exactly on a cell. When chasing, it tries to close the horizontal gap first, then the vertical one, takes the first way that is open, and never turns straight back. When scared, it aims at the mirror image of Pac-Man's horizontal position but at Pac-Man's own row: a quirk of the original that this program keeps. When its chase timer is out it wanders at random.
- **Scoring.** A wafer is 1 point, a power pill 5, ghosts 20, 40, 80 and 160 in a row, the bonus item 100. Clearing a maze refills it and adds a life (up to 9; the original supports 8 and 9 lives and draws them). The score is six digits. There is no extra life for reaching a score.
- **Scared time.** After a power pill the ghosts are scared for 320 frames (160 with the difficulty switch on A) and then flash for 64 frames. This does not change with the level.
- **The flicker.** The video chip can only draw so many sprites on a line, so the game draws **one ghost per frame**, taking turns. The ghosts flicker; that is the original, and the program keeps it by default.
- **After a death** the game pauses, the life is taken about two seconds later, and Pac-Man and the ghosts go back to their start. After game over the fire button works only after about eight seconds.

## What is not covered

- The reference is an emulator, not a real console. The comparison tells that this program equals Stella running the original, and Stella is well tested; it was not compared with a capture from real hardware.
- Only this cartridge, the NTSC version, is covered. PAL speed and other cartridges are out of scope.
- Hardware accidents the game never triggers (for instance, behaviours of the chips that no recorded or random game reached) were not modelled and are not claimed.

## Reproducing the proofs

The tests are listed in [TESTING.md](../TESTING.md) and how to build and run them in [BUILDING.md](../BUILDING.md). The proofs against the original need your own copy of the game file; the other tests do not.
