# How Pac2600Comp works

This page explains, without needing to know how to program, what is inside the program and why you can trust that it plays the original game. For the exact module list see [ARCHITECTURE.md](../ARCHITECTURE.md); for how the game was preserved, [PRESERVATION.md](PRESERVATION.md).

## The idea in one paragraph

The Atari 2600 had no operating system and no graphics card. The game ran on a small processor and wrote to two simple chips, one for the picture and one for the sound, at exact moments while the television beam swept down the screen. A common way to play such a game today is an **emulator**: a program that pretends to be the whole console and runs the original cartridge's code. Pac2600Comp does something different. The game's rules, the way it draws its picture and the way it makes its sound were **written again as ordinary C++**, and each part was proved to behave exactly like the original. The result needs no emulator and no cartridge code to run, only a few tables of numbers that the game reads (the maze, the shapes, the sound steps; 512 bytes in all, built into the program).

## Why there is still a game file

The program asks for your own copy of the cartridge file and checks its size and SHA-1 before it starts. It does not read the game from it. The check exists so that nobody without a legal copy of the original game can play with this program. The file is never copied, sent anywhere or kept.

## One frame

A television picture on the console is drawn 59.92 times a second. For every one of those frames the program does what the console did:

1. **Read the controls** (joystick, fire button, the console's switches) and move Pac-Man and the four ghosts.
2. **Draw the picture.** The original produces the picture line by line by writing to the video chip at exact moments. The program makes the same writes from the game's state.
3. **Run two chip models.** A model of the video chip turns those writes into the 160-pixel-wide picture and the collision flags the game depends on. A model of the sound chip turns the sound writes into samples.
4. **Check what happened** (a wafer eaten, a ghost caught), add the score, start sounds, run the timers.
5. **Prepare the next frame.**

The window program around it shows the picture, plays the sound, reads the keyboard and pads, and keeps the pace of the console (59.9227 frames a second) by following the sound card's clock, so the sound does not crackle.

## Two programs

| Program | Contains | Used for |
|---|---|---|
| `Pac2600Comp.exe` (built as `pacman2600.exe`) | window, sound, controls, the rebuilt game | what you run |
| `pacman2600_ref.exe` | the same window around the **Stella** emulator running the original game | the **reference**: what every proof compares against; never part of a release |

## How it was proved

Every rewrite was checked against the reference, not against anyone's memory of the game:

- The reference program played **70 recorded games** and **120 games with random input**, writing down, after every single frame, the game's whole memory, a fingerprint of the picture and a fingerprint of the sound. The rebuilt game played the same inputs and its record was compared. All of it is identical: 439,680 frames of recorded games and 2,400,000 frames of random games, picture, sound and memory.
- A separate run of 9,800 frames, played by a program that searches for a good route, was compared with Stella in its own program, with 10 whole pictures equal.
- Saved pictures of four recorded games, with and without the optional colour glow, were compared with the reference's and are pixel for pixel the same.

The tests are listed in [TESTING.md](../TESTING.md). Most need no game file; the ones that compare with the original do.

## The extras are kept apart

Smoother motion, HDR highlights, CRT filters, backgrounds, pad lights and rumble, achievements and statistics are all **separate code that only reads** what the game does (or draws around the picture). They are off by default, and each has its own test showing that the game is the same with it switched on or off. For example, the game is replayed at 60, 144 and unlocked frame rates and its memory, picture and sound traces are compared, frame for frame. The default start is the original look.

## What it does not do

- It does not contain the cartridge's program code, and it cannot run any other game.
- It does not reproduce every possible hardware accident of a real console: the picture and sound models cover what this game does, and they were proved only for that.
- It has been proved identical for the **NTSC** original at its own speed. Other regions and speeds are not covered.

## Where the tables come from

The 34 tables (512 bytes) were found by running the original game over many recorded and random games and noting every table byte the game ever read; the program then carries exactly those. A read outside a table stops the program instead of guessing.
