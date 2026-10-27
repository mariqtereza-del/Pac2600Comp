# Status report before Phase I (Stage I1, 2026-10-27)

Written for the owner, in plain words. Every number below was taken from a document or command output named next to it. Where something is an estimate, it says so. Nothing here is a decision; Phase I is yours.

## 1. In one page

- **The game is finished as a preservation.** `build\host\pacman2600.exe` plays Atari 2600 Pac-Man with our own game code, written from the reverse-engineered original. It contains no emulator and no cartridge program code, and it reads no game data from the ROM. It still refuses to start without a legal dump of the ROM (size and fingerprint checked), which is the indirect copy-protection you asked for (D-009).
- **It is proved identical to the original, not just "looks right".** After every one of 439,680 frames of 70 recorded games, and 2,400,000 frames of 120 more random-button games, the game's memory, picture and sound are identical to the original's.
- **The behaviour is now frozen.** 151 recordings and expected-result files can no longer be changed without a test failing (Gate H). The Git tag `1.0.0-preservation` marks the baseline.
- **What is not proved:** a comparison with a real console on a real TV (impossible here), play by a person (you chose to avoid that, D-008), other PCs, and GitHub. Section 3.
- **Before any new feature:** the structure allows many additions that never touch the game (pause, screenshots, volume, picture filters, key mapping). Anything that changes how the game plays needs one small, planned opening in the game code. Section 5.
- **Still ahead after Phase I:** Phase J (legal decision, name, public documentation, release). Those are your decisions. Section 6.

## 2. What exists

| Part | Size (Git files, measured today) | What it is |
| --- | --- | --- |
| The game, `src\game` | 51 files, 3,847 lines | The rules of Pac-Man, the picture and the sound, in small modules (`logic`, `picture`, `sound`, `state`, `data`). Rewritten from the original, routine by routine, each checked against the original. |
| The program around it, `src\host` | 42 files, 3,583 lines | Window, scaling, fullscreen, keyboard, PS4 pad, sound output, frame pacing, ROM finding, recordings. |
| Test harness, `src\lockstep`, `src\testsupport` | 14 files, 2,338 lines | Runs the original and our game side by side and compares them. |
| Tests, `tests` | 27 files | Unit tests and the stored expected results. |
| Tools, `tools` | 93 files | The proofs, the test commands, the build scripts. |
| Recorded behaviours of the original, `baseline` | 70 entries (140 files) | "Press these buttons, the original does this." Each has a stored fingerprint of what the original did. |
| Libraries in `third_party` | SDL3 3.4.18, Stella 7.0 | SDL for window and sound. Stella is used only by the **reference** program `pacman2600_ref.exe`, which the tests compare against; it is not in the game you play (a scan finds 0 Stella marker names in `pacman2600.exe`, 300 KB, against 3 MB for the reference). |
| History | 130 commits, tags `v0.1.0`, `0.5.0-native`, `1.0.0-preservation` | Nothing has been pushed anywhere (the repository has no remote at all: `git remote -v` is empty). |

## 3. What is preserved, and how we know

| Claim | How it is proved | Where |
| --- | --- | --- |
| Our game equals the original, frame by frame | 70 recorded games (439,680 frames) and 120 random games (2,400,000 frames): memory, picture and sound identical after every frame | `docs\GATE_F.md`, `docs\F25_LOCKSTEP.md` |
| Every single routine equals the original's | Each of about 40 routines was rewritten alone and compared call by call (registers, memory, chip writes) with the original code | `docs\F21_ROUTINES.md` |
| The keyboard and switches reach the game the same way | All recorded key input replayed: 350,460 frames, 0 differences | `docs\GATE_F.md` |
| Sound is the original's | Sample for sample equal in 70 games; the program's start tune and death wail were also heard by you in Phase D | `docs\F24_SOUND.md`, D8 |
| The game needs no ROM | A program with no ROM code in it replays 11 recorded games (40,000 frames) from stored fingerprints; the tables the game needs (34 tables, 508 bytes) are part of the program | `docs\GATE_G.md`, `docs\EMBEDDED_DATA.md` |
| The ROM and its code are not in Git or the release | Scans: no ROM file, no run of cartridge code in the program or the tracked files, release zip scanned | `docs\GATE_G.md`, `docs\GATE_H.md` |
| It keeps the console's speed on any monitor | 59.87-59.92 frames a second on your 180 Hz screen and on pretend 30-240 Hz screens, identical pictures, no sound dropouts | `docs\PERFORMANCE.md` |
| A stranger can build it | A clean copy built from `BUILDING.md` alone; public tests 16 of 16 there | Stage H5, `TESTING.md` |
| The proofs cannot be quietly weakened | 151 files frozen with SHA-256, checked by every test run; a tamper drill shows each kind of damage is caught | `docs\GATE_H.md`, `tests\preservation\README.md` |
| The whole test set runs | `test_quick` 35 of 35 steps (155 s), `test_public` 17 of 17 (21 s, no ROM), `test_full` 48 of 48 (1,704 s) | `TESTING.md` |

Test names and times are in `TESTING.md`.

## 4. What is NOT proved (stated plainly)

1. **No comparison with a real Atari 2600.** The reference is Stella's emulation of the chips, and our picture/sound code follows the same model. If Stella is wrong somewhere that this game never shows, we would be wrong the same way. The recorded behaviours of the **game** are independent of that (they come from the ROM running), but the chip behaviour is Stella's.
2. **No human play test as a gate.** You tried versions in Phase D (keys, pad, fullscreen, sound, pacing) and said they feel right. Since Phase E your rule (D-008) has been that machines prove it; nobody has played the final native program through a long session and written it down. That is an offer, not a requirement.
3. **One PC.** Your PC, one monitor (180 Hz), one sound device, one compiler version. A real 60 Hz monitor was not tried (changing it is a Windows-wide setting; the pretend 60 Hz run uses a timer instead).
4. **Rare slow frames.** About 1 frame in 3,600 occasionally takes longer than a whole frame period, probably Windows pausing the program; never heard and never seen in a picture, but the cause is not proved (T-H6-1).
5. **GitHub has not been touched.** The automatic test file `.github\workflows\public-tests.yml` has never run; whether the build finds the compiler there is unknown.
6. **The freeze is a check, not a lock.** Editing a frozen file *and* its list and committing both gets past some of the checks (the tag comparison still catches it for 142 of the 151 files).
7. **One path of the original that no recording runs.** `Game::runFrame` returns "false" if it meets it; no test has ever reached it (`src\game\game.hpp`, F21). It is not known whether any real play can reach it.
8. **Meaning of some memory bytes.** In Stage F7, of the original's 128 memory bytes, 84 were named with certainty, 32 as "likely", 12 marked "?" (shared scratch space or not worked out). This does not affect correctness (the behaviour is proved), but it matters for any feature that must *understand* the game state. The stranger test (G6, four fresh agents reading only `src\game`) scored ghost direction 9 of 9 and scoring 8 of 8 (two partial) in round one; after comment fixes, round two scored 10 of 10 on the state bits (`docs\G6_STRANGER_TEST.md`).

## 5. Differences from a real 1981 console (from `DISCREPANCIES.md`)

| Subject | Status |
| --- | --- |
| Speed 59.9227 Hz, sound 31,400 Hz | Fixed: the port follows the console, not Stella (D-004). |
| Picture shape | Your decision: 12:7 pixels, "Framed" by default (D-005, D-006). `--pixel-aspect stella` gives 19:10. |
| Flicker (one ghost drawn in four frames) | Original behaviour kept; no glow by default, F7 turns the soft glow on (D-007). |
| Power-on memory | Zeros here, random in a real console; no visible effect in 6 random tests (T-D14-3). |
| Colours | Stella's standard palette. No single "authentic" palette exists (TV knobs varied). |

The nine "open items" listed in Stage C8 (`docs\REFERENCE.md`) were all taken up by later phases: the line count during a level change (never measured as a separate fact; but the recorded games include cleared levels, the bot clears five, and the picture is identical in every frame of them, so a different line count would have shown), the flicker presentation (D6/D-007), pacing (D9/D14), timing constants and joystick reading (F5, F9), the sounds of power pill, eaten ghost and level clear (F19, proved write by write rather than recorded by ear), what the eight game variations do (F16: only the starting level, four values), the PS4 pad (D11), the meaning of the memory bytes (F7, with the gaps in section 4), and two-player, black-and-white and later-level pictures (Phase E entries and the whole-game proofs). "Settled" here means covered by the proofs above, not measured on a real console. The reference document itself still lists them as open; it is a record of Gate C and was left as written.

## 6. The structure, and what it means for additions

**Two layers, and a clear line between them.** The game (`src\game`) is one object, `Game`, that runs one frame at a time: it is given the buttons and switches of that frame and a chip to write to. The program around it (`src\host`) runs frames at the right speed, shows the picture and plays the sound. The game does not know about windows, files, keys or time.

**Additions that need no change to the game (touch `src\host` only).** These cannot alter what the game does, so the frozen suite stays green automatically:
- pausing, speed control for the player's convenience, mute and volume, screenshots, video or audio capture;
- picture treatment (scanline or glow filters, different scaling, borders, a bigger or smaller window);
- key and pad mapping, extra shortcuts, an on-screen help or statistics display;
- more settings remembered in `settings.ini`.

Some of these may add a library or more code; each would be explained when proposed (Rule 5).

**Additions that need the game's state to be saved or read (host side, but need care).** Save and load a game position, rewind, or an on-screen display of score and lives. The game keeps its main state in one 128-byte block (`GameState`, in the original's memory order) plus a few small extra values (`started_`, `shift_`, `lastLines_`). Whether those are *all* the state that must be saved (the picture and sound models may hold some too) has not been checked and is a first task if such a feature is chosen. This is an estimate, not a test.

**Additions that change how the game plays** (a different rule, extra lives, other speeds, new levels or items, removing the flicker by changing the draw order, a new mode). These need a small, planned opening in `src\game`: an "options" value handed to `Game`, whose default means "exactly the original". The rules in the project already require this (Rule 3; Phase I: the Original mode is byte-for-byte the 1.0.0 baseline, and a new option is tested to change nothing when it is off). Because the logic is split into small modules (ghosts, Pac-Man, maze, scoring, flow, picture, sound) an option can usually be added in one module. The modules that understand their own bytes best (scoring, ghosts, Pac-Man movement) are the easiest; the picture and the exact timing of chip writes are the hardest, because they are reproduced cycle for cycle.

**Structural changes that would make additions easier** (recommendations for I2; none is started):
1. A game-options value, defaulting to "original", passed to `Game` (small; it changes no behaviour while unused, and the existing proofs would show that).
2. A short list, kept in the repository, of the game state that a save/load needs (a check, then a test that saving and loading in the middle of a recorded game gives identical results).
3. A second family of tests for modes that change play, kept apart from the frozen preservation suite: "off equals original" plus the mode's own expectations.
4. Finishing the meaning of the 12 "?" and 32 "likely" bytes only if a chosen feature needs them (not before).

## 7. What the owner may want to know about Phase J (not started)

- **J1 legal decision is yours** and needs real legal advice (not given here): publishing source built from studying Atari's code; the "Pac-Man" name is a Bandai Namco trademark (so the project needs another name, J2); Stella's code is GPL (only the reference program contains it).
- The release script still says version 0.1.0 and its text is the 0.1.0 text (T-H5-1).
- Nothing is pushed; every `git push` and every GitHub action will wait for your approval.

## 8. What I suggest for I2 (a suggestion only)

Present the candidate features one at a time, grouped by the three kinds in section 6, each with the eight answers the plan requires, starting with those that need no change to the game. Nothing is proposed as decided. You can also decide that no feature is wanted: the project is complete as a preservation, and Phase J can follow directly.
