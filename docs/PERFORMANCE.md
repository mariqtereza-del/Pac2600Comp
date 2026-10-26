# Performance and pacing (Stage H6, 2026-10-27)

Measured with `powershell -NoProfile -ExecutionPolicy Bypass -File tools\h6_performance.ps1` (60 seconds per case, muted, the real window, the owner's PC with a 180 Hz monitor on a 2560x1440 screen). The program is `build\host\pacman2600.exe` (our own game code, no Stella).

## What was measured

Six cases: the real monitor (180 Hz, vsync) and five **pretend monitors** (`--present-hz N`: vsync is off and a timer plays the monitor, 30, 60, 75, 144, 240 Hz). The game thread is independent of the window thread in all of them. Each case is the same game (Game Reset at frame 120, then the title/attract run), and its picture fingerprints are compared with the real-monitor run.

## Result of the final run (`build\h6\run4`, local)

| Case | Game frames/s | Sound underruns | Compute per frame: mean / 99 % / longest | Start late (99 %) | Frames over 16.7 ms |
| --- | --- | --- | --- | --- | --- |
| real 180 Hz | 59.878 | 0 | 0.46 / 0.96 / 2.6 ms | 0.87 ms | 0 |
| pretend 30 Hz | 59.916 | 0 | 0.48 / 0.84 / 5.9 ms | 1.18 ms | 0 |
| pretend 60 Hz | 59.916 | 0 | 0.47 / 1.11 / 6.7 ms | 1.78 ms | 0 |
| pretend 75 Hz | 59.898 | 0 | 0.46 / 1.04 / **18.1 ms** | 1.63 ms | **1** |
| pretend 144 Hz | 59.883 | 0 | 0.50 / 1.24 / 5.6 ms | 1.21 ms | 0 |
| pretend 240 Hz | 59.865 | 0 | 0.47 / 1.14 / 9.2 ms | 1.44 ms | 0 |

- **No frame-rate dependence:** the console's own rate is 59.9227; every case is within 0.1 % of it, and the picture of every frame (about 3,593 compared per case) is identical to the real-monitor run in all five comparisons (0 differences). How many monitor refreshes a game frame is on screen changes with the monitor (3 at 180 Hz for 93.6 % of frames; 1 at 60 Hz for 98 %), the game does not.
- **Speed margin:** with no window the game runs 1,994 frames a second, 33 times real time (6,000 frames in 3.0 s). A normal frame costs 0.46 ms of the 16.7 ms available.
- **Start-up:** the first picture is handed to the window about 1 ms after the game thread starts; the whole program, from launch to exit for a one-frame headless run, takes 1.0 s (most of it loading and checking the ROM and starting SDL).
- **Sound:** 0 underruns in every run.

## Spikes (honest note)

Now and then one frame takes much longer than the average: runs before the final one had single frames of 9-18 ms (about 1 frame in 3,600), in a different case each time. The count of frames over one frame period is reported in every run; the check allows at most 5 per run (owner's decision, T-H6-1). I believe these are Windows pausing the program for a moment (the same code runs the 1,994 frames-a-second flat-out test without such delays), but I have not proved the cause. They never reached the sound (the 4-frame cushion absorbs them) and never changed a picture. Frame starts up to 22 ms late were also seen once or twice a run, for the same reason.

## What was NOT done

- **A real 60 Hz monitor.** Switching the monitor is a global Windows setting, so it needs the owner's OK and was not done. The pretend 60 Hz case runs the same game code and the same pacing, but not the same present path (no vsync wait). To check a real 60 Hz screen, set the monitor to 60 Hz and run the script again; the first line of its output shows the refresh rate Windows reports.
- **Other PCs and other sound cards.** One PC, one sound device (44.1 kHz).
- **Long runs.** 60 s per case (the 10-minute run of Stage D9 is still the long-run record: 59.9993 frames a second at that time, with the old 60 Hz pace).
