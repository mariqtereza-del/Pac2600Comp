# Gate D: Phase 1 done (Stage D16, 2026-10-26)

**State: every check I can make myself PASSES, on the release zip's own program. Waiting for the owner's full play-through, then the tag `v0.1.0`.**

What was checked: `out\PacMan2600-0.1.0-windows.zip` (made from commit `72d8ca7`), unzipped into a fresh folder by `tools\d16_gate_check.ps1`, which then ran **that folder's** `pacman2600.exe` (33 checks, all PASS, report in `out\gate-d3\report.txt`, local only). The ROM was given to the program by path only and never copied.

| # | Gate item | Result | Evidence |
| --- | --- | --- | --- |
| 1 | Correct shape | PASS | Default mode Framed: pixel shape 12:7 (1.714) within 0.5 % at 2560x1440, 1920x1080, 1366x768 and 1280x1024; a black border on every side (8 game pixels, D-006); nothing outside the picture is lit. Layout tests 231 checks. |
| 2 | Fullscreen and windowed | PASS | Alt+Enter into borderless fullscreen and back in the real window: the picture checked in fullscreen, the odd window size and place came back, the state saved. |
| 3 | Sound | PASS (by measurement); **your ears** | 20 s on the real sound device: 0 underruns, source 31,400 Hz (the console's sound rate), 59.90 frames a second (settling; 9 minutes gave 59.9235 Hz, T-D14-2). Identical to the Phase C recording bit for bit (D8). How it sounds is yours to judge. |
| 4 | Keyboard | PASS | Injected key presses in the real window reached the game: joystick byte 1, 2, 4, 8; Reset (switch register 3e); left and right difficulty and B&W (7f, ff, f7); remembered in `settings.ini`. 117 control tests; 350,460 recorded frames replayed through the key code with 0 differences. |
| 5 | Pad | PASS (virtual); **your pad** | 31 checks with a virtual gamepad: every button, the stick, the dead zone, unplug and replug. The real PS4 pad worked in D11 (owner). |
| 6 | ROM picker | PASS (search); **the dialog is yours** | Nothing found: exit 5; a wrong 4096-byte file: refused, exit 4; the right ROM given by path: exit 0; a normal start remembers the path. The Windows file dialog itself cannot be clicked by me. |
| 7 | Replays identical | PASS | A recording made in the window replays to the same fingerprint on every frame, with no window (719 frames) and in the window (480 frames): 0 differences. The double-click launchers `Record a game.cmd` and `Replay a game.cmd` (added at this gate, T-D15-1) were run from the unzipped release: a recording was written to `recordings\`, the title bars said RECORDING and REPLAY, and both the newest recording and a dropped file with a space in its path replayed. |
| 8 | Discrepancies list reviewed | PASS by me; **yours to read** | `DISCREPANCIES.md`: no differences from Stella's emulation core (your recording 1,860 frames, a test recording, the 9,800-frame bot route: 0 differences, 30 of 30 pictures identical, by the release program too); the known and chosen differences from the Stella app and the 1981 console are listed there. |
| 9 | Version 0.1.0 tagged in Git | **after your OK** | `git tag v0.1.0` on the Gate D commit (local; nothing is pushed). |
| 10 | The release contains no ROM | PASS | 13 files, none ending `.a26 .bin .rom`, none with the ROM's SHA-1; the build's scan (3,281 files including the source zip inside) found nothing; the exe's SHA-256 equals `BUILD-INFO.txt`. |

## Found by the owner during the gate

- No `--record` without a command line: double-click launchers `Record a game.cmd` / `Replay a game.cmd` added to the release (T-D15-1).
- A faint trail behind moving objects, most visible in B&W: Stella's phosphor glow, now off by default with F7 to turn it on (D-007).

## Other checks, run again for this gate

- Unit tests: controls 117, layout 231, window state 29, input recordings 25, ROM finder all pass.
- Key replay of every recorded Phase C input: 350,460 frames, 0 differences.
- ROM SHA-1 `0940fea7...dbb1` is the same for `rom\pacman.a26`, the original in the project folder and the spare in `F:\Ports\ROM-originals\`; no `.a26` or `.bin` is tracked by Git.
- `tools\test_quick.ps1` does not exist yet (Phase H).

## What only you can judge

1. Play a full game (all lives) from the release, unzipped somewhere new: sound, keys, the PS4 pad, fullscreen, the ROM picker dialog (first start, no ROM beside it).
2. Make a recording: double-click `Record a game.cmd`, play, close the game with Esc; the file appears in the `recordings` folder next to the program. Then double-click `Replay a game.cmd` to watch it again.
3. Read `DISCREPANCIES.md`.

## Not covered (stated plainly)

- A PC that never had Visual Studio: the DLL list says everything needed is in the zip or in Windows, but this PC has Visual Studio.
- What the game variations and difficulty switches do: not yet determined (Phase E and F).
- The default scaling mode was chosen by you in D-005 and D-006 (Framed).
