# Controls and game variations (Stage C6, measured 2026-10-24)

The facts below were **measured** with `tools\stella_probe\script.cpp` → `stella_script.exe` (DECISIONS.md T-C6-1), unless marked otherwise.

- **How it measures:** it runs the ROM in the same Stella 7.0 emulator code as the reference app, with inputs from a script file, and records per frame the picture and status-bar fingerprints, the colours, the console switch positions and the 128 bytes of RAM.
- **The experiments:** generated and compared by `tools\c6_experiments.py`. Results and pictures are in `reference\c6\` (local only).
- **Proof the tool doesn't change the emulation:** its idle script (Game Reset at 1800) reproduced all 24,000 of C3's picture fingerprints.
- **The one exception, the PS4 test:** done by you in the reference Stella app.

Notes on the evidence:
- "No effect found" always means **in the tested scenario only**.
- RAM addresses are given as supporting evidence. What they mean is Phase F's job.

## Console switches: defaults and behaviour

| Item | Measured | Evidence |
| --- | --- | --- |
| Stella's default positions for this game | Left difficulty **B**, right difficulty **B**, TV type **Colour** | `Switches` state at frame 0 (`reference\c6\idle`) |
| Game Reset | **Starts a new game**: score 0, three life markers, start positions. Works from attract mode and **during play** (the game in progress is abandoned) | `midgame\reset`, picture at 2550 |
| Game Select (from attract/power-on) | Steps through **16 selections**: game **1–8**, each for **1 or 2 players**, in the order 1(1P) → 1(2P) → 2(1P) → 2(2P) → … → 8(2P) → back to 1(1P). Power-on state is game 1, one player. The first press swaps "©1981 ATARI" for the game number on the bar; two bars (green and red) mean two players | `select`: 16 presses, pictures after presses 1–3, 15 and 16; RAM $CC = game number, $E6 = $40 for two players |
| Game Select during play | **Ends the game** and moves to the **next selection** (game 1 one player → game 1 two players) | `midgame\select`, picture at 2550 |
| Colour / B&W | With B&W the **game itself switches to grey colours**: during play TIA $00, $06, $0A, $0E instead of $84/$C4/$F6/$FA. Takes effect at once (frame 101, switch moved at 100) | `switches\bw` colours per frame |
| Left difficulty (A vs B) | Read by the game from the moment it's set (RAM differs from frame 100). **Effect found: the bonus item.** The square that appears under the ghost pen (picture rows 98–108, columns 76–83) is shown at different moments. Two-player game, frame 3627: shown with B, not with A. One-player game after game over, frame 3584: shown with A, not with B. Everything else in the picture was identical | `switches\left_a`, `more\two_players_left_a`, `diffs\` |
| Right difficulty (A vs B) | One-player game: **no effect found** (picture and RAM identical for 6,000 frames). Two-player game: RAM first differs at frame 2337, about when player 2's first turn begins. The first picture difference (frame 9003) is in the same bonus-item square. So it is evidently **player 2's** switch | `switches\right_a`, `more\two_players_right_a` |

**The difficulty switches' exact rule is not determined:** when the bonus item appears, and for how long, with A versus B. It needs longer, targeted play (Phase E) or the code (Phase F).

## Joystick and fire button

| Item | Measured | Evidence |
| --- | --- | --- |
| Directions | All four are read by the game into RAM byte $ED: **up = 1, down = 2, left = 4, right = 8**, none = 0 | `directions`, `turns` |
| Left / right | Move Pac-Man horizontally. $B1 (likely Pac-Man's x) goes 76 → 64 holding left and 76 → 88 holding right, stopping at walls | `directions` |
| Up / down | Move Pac-Man vertically **where the maze allows**. At the start position up/down do nothing (that run is identical to no input), because there are walls above and below. After moving left: up takes $B6 (likely Pac-Man's y) 60 → 42, down 60 → 70 | `directions`, `turns` |
| Fire button, during play | **No effect found**: picture and RAM identical to no fire for the whole game | `switches\fire` (identical until frame 3584) |
| Fire button, in attract mode or after game over | **Starts a new game**, like Game Reset. Pressed at frame 1000 in attract mode, the picture changes at once to the game-start screen. Held through a game, a new game starts at the moment the game ends (frame 3584) | `more\fire_in_attract` picture at 1050; `switches\fire` picture at 4200 |
| Movement starts | Pac-Man first moves at frame 1928–1929, i.e. at the end of the start tune (C5: tune from 1865, 63 frames) | `directions`, `variations2` |

## Game variations 1–8

**At game start:** games 1–8 (one player) show an identical status bar (score 0, three life markers). **The game number isn't shown during play.** The selection sets different values in RAM at game start, for example:

| Game | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| $81 | 07 | 06 | 06 | 05 | 05 | 04 | 04 | 03 |
| $EC | 05 | 06 | 06 | 07 | 07 | 08 | 08 | 09 |
| $99 / $C2 | 08 | 0a | 04 | 06 | 08 | 0a | 04 | 06 |

**Measured differences:**

| Game | Pac-Man: 12 px to the left wall at the start (same input) | No input: status-bar changes (scoring, lost lives) at frames |
| --- | --- | --- |
| 1 | 20 frames | 2350, 2850, 3352, 3856 |
| 2 | 16 frames | 2263, 2719, 3175, 3631 |
| 3 | 16 frames | 2439, 3063, 3815, 4679 |
| 4 | 16 frames | 2313, 2903, 3409, 3999 |
| 5 | 16 frames | 2349, 2849, 3351, 3855 |
| 6 | 16 frames | 2263, 2719, 3175, 3631 |
| 7 | 20 frames | 2439, 3063, 3815, 4679 |
| 8 | 20 frames | 2313, 2903, 3409, 3999 |

- **Repeat every 4 games:** the moments an idle Pac-Man is caught are identical for games 2/6, 3/7 and 4/8, and for 1/5 they are within 1 frame. RAM bytes $99 and $C1–$C8 follow the same 4-game cycle, so they are probably the ghost-related setting.
- **Pac-Man's speed** differs (20 versus 16 frames for the same 12 pixels), but not in a simple pattern (games 1, 7 and 8 are slower than 2–6). The cause is **not determined**. It may also depend on the frame Pac-Man starts moving on (1928 versus 1929).
- **Not determined:** what each variation is meant to change. That needs the code (Phase F), or longer comparisons of real play (Phase E).

**Two players** (selections 1(2P) … 8(2P)):
- A second, **red** bar shows player 2's score and life markers. At the start player 2 shows four markers, player 1 three.
- The players take turns. While player 2 is playing, the red bar is shown on top.
- Evidence: `variations2\game1_2p_idle` (picture at 1860), `diffs\p2_left` (picture at 3627).

## PS4 controller in the Stella app (your test, 2026-10-24)

- **Set-up:** the pad was connected as "Wireless Controller" (DualShock 4), with DS4Windows **not** running, and started through `tools\run_stella.cmd`.
- **Your answers:**
  - **"Left stick moves Pac-Man"**
  - **"A pad button starts the game"** (which button wasn't named)
- **D-pad:** not ticked, so **not confirmed** to work with Stella's default settings.
- **Settings:** Stella saved its settings in its own folder (`stella.sqlite3`); nothing was written to `%APPDATA%`.
- **For the port:** this shows what Stella does. The port's own controller handling (stick and d-pad, which button does Reset/Select) is designed in Phase D.

## Not yet determined

| Item | Where |
| --- | --- |
| What variations 1–8 are meant to change, and why Pac-Man's speed differs as it does | Phase F (code), Phase E (real play) |
| The difficulty switches' exact rule for the bonus item | Phase E / F |
| Whether the fire button does anything else (for example with a power pill active) | Phase E |
| D-pad behaviour in Stella | Phase D (port controls) |

## The port's keys (Stage D10)

Arrows or W A S D = joystick; Space or Z = fire; F2 = Game Select (held); F3 = Game Reset (held); F5 / F6 = left / right difficulty A or B; F4 = Colour / B&W; 1-4 = picture mode; Alt+Enter = fullscreen; F1 = list of keys on the screen; Esc = quit. Chosen in `DECISIONS.md` T-D10-1. Proof: replaying the recorded inputs of this stage's experiments through the keys gives the same picture in all 344,460 frames compared (`tools\d10_replay_compare.py`). The PS4 controller is D11; Colour/B&W is D12.

## The PS4 pad in the port (Stage D11)

D-pad or left stick = joystick (50 % push to start, 30 % to let go, one direction at a time); Cross or Square = fire; **Options = Game Reset; Share = Game Select**; the difficulty switches stay on F5 and F6. Unplugging mid-game releases everything; plugging in again works. Chosen in `DECISIONS.md` T-D11-1; proof with a virtual gamepad (`--pad-check`, 31 checks) and by the owner's own pad (see `PROJECT_STATUS.md`).

## The console switches in the port (Stage D12)

F4 = Colour / B&W, F5 = left difficulty, F6 = right difficulty; each is a real switch (it stays where you put it) and F1 shows where it is. They are remembered in `settings.ini` and restored at the next start. Chosen in `DECISIONS.md` T-D12-1.
