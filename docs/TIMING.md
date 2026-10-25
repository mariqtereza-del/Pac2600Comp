# Timing facts (Stage C4, measured 2026-10-24)

All values were **measured** by `tools\stella_probe\timing.cpp` → `stella_timing.exe` (DECISIONS.md T-C4-1), or read from Stella 7.0's source where stated. None is copied from memory.

- **How it measures:** it runs the ROM in the same Stella 7.0 emulator code as the reference app, repeating Stella's own frame steps exactly, and records events after every single CPU instruction.
- **Same scenario as C3:** 24,000 frames; attract mode 0–1799; Game Reset held 1800–1809; then no input until game over.
- **Data:** `reference\c4\` (local only). `frames.csv` holds the per-frame events, `events_1000/2500/6000.csv` the full event lists of three sample frames, and `summary.txt` the summary.
- **Commands:**
  - `tools\build_stella_probe.cmd`
  - `build\stella_probe\stella_timing.exe <rom> reference\c4 24000 1800 1000 2500 6000`
  - `python -I tools\c4_report.py reference\c4 1800 10`
- **Proof the tool doesn't change the emulation:** all 24,000 picture fingerprints and line counts are identical to C3's (`reference\c3\dump1\frames.csv`).

## Speed

| Fact | Value | Source |
| --- | --- | --- |
| CPU cycles per scanline | **76** | Measured: 19,912 cycles ÷ 262 lines. Also Stella's `TIAConstants::H_CYCLES` |
| CPU cycles per frame | **19,912** on all 23,999 full frames (frame 0: 3,293, the partial power-on frame) | Measured: `TIA::cycles()` at each frame end. Stella's own `cyclesPerFrame()` also reports 19,912 |
| Stella's NTSC console clock | **1,193,191.67 Hz** | Stella 7.0 source, `CartARM.cxx` line 59 ("NTSC 6507 clock rate"); same value in `CartCTY.cxx` and `CartDPC.cxx` |
| Frame rate on that clock | **59.923246 Hz** | 1,193,191.67 ÷ 19,912 |
| Rate Stella actually plays at | **1,194,720 cycles/s = 60.000000 Hz** | Measured from `EmulationTiming::cyclesPerSecond()`. Source: `EmulationTiming.cxx` lines 174–188 compute 262 × 76 × 60, via an audio rate of 31,440 Hz × 38 |
| Difference | Stella plays **0.128 % fast**, i.e. one extra frame every **13.0 s** | Computed from the two rows above |

**Meaning for the port:**
- Stella deliberately runs NTSC games at exactly 60 frames a second, slightly faster than the console's own clock as Stella itself states it.
- Our Phase 1 program (built on Stella's core) inherits whatever pacing its host chooses. Phase D must decide whether the port paces at 60.000 Hz, matching the reference app, or at 59.92 Hz, matching the console's clock. Sound pitch and game speed both follow from that choice.
- What happens inside each emulated frame doesn't depend on this, because the emulation is counted in CPU cycles. Only how fast frames are shown and sound is played on the PC changes.

## What happens in each frame

Line numbers are Stella's frame lines 0–261. Line 0 is the line where VSYNC ends.

| Lines | What happens | Evidence (frame 2500 shown) |
| --- | --- | --- |
| 259–261 | **VSYNC**, 3 lines. VBLANK on. At line 261 the game starts the RIOT timer (TIM64T, 64-cycle steps) | `VSYNC on` L259 clock 18; `timer set T64` L261 clock 216; `VSYNC off` L0 |
| 0 – about 20 | **Game logic** (top blank area) | No timer reads until the wait starts |
| about 20 – 33 | **Waiting** for the timer: a tight loop reading INTIM about every 7 cycles | 148 reads, L18–L33 in frame 2500 |
| 34 | **VBLANK off**: drawing starts | `VBLANK off` L34 clock 18 |
| 34–222 | **Drawing.** The maze, sprites and status bar are drawn on these lines (C3: scanlines 34–222) | C3 |
| 195 | While still drawing, the game starts the second timer (TIM64T) | `timer set T64` L195 clock 84 |
| from the end of drawing (L223) to the wait | **Game logic** (bottom area), usually only to L224 in idle play. The screen isn't blanked here, but it is black (C3) | Wait start L224–L234 |
| about 224 – 258 | **Waiting** for the timer | 372 reads, L224–L258 |
| 259 | **VBLANK on and VSYNC on** together, and the next frame begins | `VSYNC on`, `VBLANK on` L259 |

**The pattern never changes.** With the wait start lines hidden, all 23,999 full frames show the identical event sequence, in attract mode, at Game Reset, during the deaths and after game over.

- **Lines 223–258** are not blanked by VBLANK, but nothing is drawn on them, so they show black.
- **Lines 34–258** are 225 unblanked lines in all. Stella's visible window (C3) is lines 23–250.

## Where the game logic runs, and how much time it has

The logic runs in the two places where the CPU is not drawing: the top area after VSYNC and the bottom area after the drawing. The moment the waiting starts shows how much of each area the logic used.

| Area | Wait ends | Wait starts: attract | play / after game over | Least spare time (busiest frame) | Most spare time |
| --- | --- | --- | --- | --- | --- |
| Top (after VSYNC) | L33 | L17–L27, most often L21 | L10–L26, most often L20 | 6 lines = **456 cycles** (wait from L27) | 23 lines (wait from L10) |
| Bottom (after drawing) | L258 | L224 always | L224–L234, most often L224 | 24 lines = **1,824 cycles** (wait from L234) | 34 lines (wait from L224) |

- **No overruns.** Every one of the 23,999 full frames had both waits, so the logic always finished before its timer ran out.
- **The top area is the tight one:** in the busiest frame only 6 of its lines were left over.
- **This is measured in idle play only** (no dots eaten, no power pills, no level change). Busier moments in real play may use more of the spare time. Phase E's recordings of real play should re-check the busiest frame.

## Not yet measured

| Item | Why not | Where |
| --- | --- | --- |
| CPU busy versus waiting inside the drawing lines (WSYNC waits) | Stella only exposes its WSYNC counter in debugger builds; the libretro build has none | Phase F (code reading) |
| The exact values written to the timers | The RIOT's timer state is only readable through debugger-only functions | Phase F (disassembly shows the constants) |
| Timing during level changes, power pills, eating | Idle play never reaches them | Phase E recordings of real play |
| When the joystick is read within the frame | Not needed for C4; belongs to the input facts | C6 / Phase F |

## Notes

- The waits really are idle loops. Reads arrive about every 7 cycles (for example 148 reads over 13 lines = 988 cycles), the cost of a two-instruction "read timer, branch if not done" loop. The instructions themselves aren't identified until Phase F.
- An earlier version of the tool sampled once per scanline. Its numbers were unreliable, because Stella's `updateScanline()` stops at the first instruction boundary after a line ends, a few cycles late. That version was replaced; none of its numbers are used here.

## What the port does (Stage D9)

The PC port runs at **60.000 frames a second**, the rate Stella plays at, steered by the sound card's clock so the music cannot drift against the picture. It is not paced by the monitor. The 59.923 Hz of the console's crystal is not used. Measured over 10 minutes on a 180 Hz screen: 59.9993 frames a second, 93.4 % of game frames shown for exactly 3 refreshes. See `DECISIONS.md` T-D9-1 and T-D9-2.

## Decision (Stage D14, owner's rule D-004)

The port plays at the console's own **59.9227 Hz** with the sound chip's own **31,399.5 Hz** (31,400 Hz to the sound device), not at Stella's 60.000 Hz / 31,440 Hz. See `DECISIONS.md` T-D14-2 (this replaces T-D9-1).
