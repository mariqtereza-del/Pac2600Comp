# Changelog

All notable changes to Pac2600Comp, newest first. The version number in the file `VERSION` is the one of the newest entry.

## How the numbers work

A version is three numbers, **big.feature.fix** (for example 1.2.3):

- **big** goes up only if what the game does by default changes (this is not intended: the default is the original game), or if files that people keep (`settings.ini`, recordings) would stop working.
- **feature** goes up when something new is added: an option, a screen, a tool. New things are off by default and never change the game.
- **fix** goes up for a repair, a better document, a test or a build change that adds nothing new.

The Git tag of a published release is `vX.Y.Z` (for example `v1.0.0`). Three older tags mark milestones of the work and are not releases: `v0.1.0`, `0.5.0-native` and `1.0.0-preservation` (the permanent baseline of the game: no later change may alter what the game does at that tag).

## [1.0.0] - unreleased

The first public version. The date is the day it is published.

### The game

- Plays the Atari 2600 game *Pac-Man* (1981) with its own rebuilt game code, picture and sound, with no emulator inside. Its memory, picture and sound are identical to the original's after every frame in 70 recorded games (439,680 frames) and 120 games with random input (2,400,000 frames).
- Runs at the console's own speed (59.9227 pictures a second), shows the picture in its true 12:7 pixel shape, and keeps the ghost flicker, as the original does.
- The game itself is not included: the program asks for the player's own copy of the game file and checks its size and SHA-1 before it starts.

### Added (optional, and off by default unless the original look needs it)

- A menu (Esc) that pauses the game and explains each row: **Game options** (game 1 to 8, one or two players taking turns, the two difficulty switches, frame rate, frame counter), **Graphics options**, **Sound options**, **Control options**, **Achievements** and **Statistics**.
- Smoother motion at 30, 75, 120 and 144 pictures a second, or unlocked (60, the original look, is the default).
- Looks: backgrounds around the picture (black, ambient glow, solid colour, gradient, arcade panel, TV bezel), three CRT filters (CRT-Lottes, CRT-Geom, GDV NTSC 2600), HDR highlights on an HDR screen, TV type (colour or black and white), soft glow, five picture modes, fullscreen.
- Sound: volume and mute.
- Controls: arrow keys, W A S D or both; PlayStation (DualShock 4, DualSense) and Xbox game pads, with the menu, the difficulty switches and the TV type on the pad's buttons; a light bar that follows the game and optional rumble on a DualShock 4.
- The 14 achievements of the RetroAchievements set, worked out offline, and statistics of your own play (games, time, high score, wafers, power pills, ghosts, times caught, most extra lives, longest game), kept between runs.
- Recording a game and playing it back exactly.

### Fixed

- The original's 8 and 9 extra lives are now drawn and counted as the original does.
- The program no longer crashes when Windows takes the graphics device away.
- A mistyped command-line option is no longer taken for a game file.
- The shader files in a fresh Windows clone are no longer changed by line-ending conversion.

### The project

- Licensed under the GNU General Public License, version 2 or later; notices and credits for everything from others are in `THIRD_PARTY_NOTICES.md`.
- A README with screenshots, a build guide, and write-ups of how it works and how the game was preserved.
- Tests that need no game file run on GitHub on every change; a release zip is built by a script and scanned so that it can hold no game file.

## [0.5.0] - 2026-10-26

A milestone (tag `0.5.0-native`), not published. The program that is played now runs on the rebuilt game code; the Stella emulator stays only in a reference program used by the tests.

## [0.1.0] - 2026-10-25

A milestone (tag `v0.1.0`), not published. The first working PC program: a window around the Stella emulator running the original game, with sound, keyboard and pad, a remembered window, and recordings.
