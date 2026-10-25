# Stage F5: the frame loop (2026-10-26)

`python -I tools\f5_frame.py rom\pacman.a26` runs `stella_pctrace.exe` on 8 sets of frames and writes `reference\disasm\pacman_frame.asm` (**local only**) and `reference\disasm\frame_stats.txt`. 31 frames were checked; the six landmarks came in the same order in every one (problems: none), and every traced run equals its golden trace.

## The loop

| # | Part | Landmark (address) | Scanlines (measured) |
| --- | --- | --- | --- |
| 0 | Start-up (once) | `RESET` `$F000` | clears `$00-$FF`, first values, positions objects, jumps to `FRAME_START` |
| 1 | Vertical sync and preparation | `FRAME_START` `$F7D6`; the timer for the vertical blank is set at `$F875` | 258-261 (3 lines of sync) |
| 2 | Top logic | `VSYNC_END` `$F87C` | 0 to about 10-22 |
| 3 | Wait for the timer | `VBLANK_WAIT` `$F0C5` | from 10-22 to 33 |
| 4 | The picture ("kernel") | `KERNEL_START` `$F0CA` | 33 to about 198 |
| 5 | Bottom logic | `OVERSCAN_TIMER` `$F389` (the timer is set on line 195) | 195 to 224-240 |
| 6 | Wait for the timer, then back to part 1 | `OVERSCAN_WAIT` `$F7D1` | from 224-240 to 258 |

- The first wait starts on scanline 10 to 22, the second on 224 to 240: the busier the game logic, the shorter the wait (the same ranges as measured from outside in Stage C4).
- The kernel repeats a pattern of 20 scanlines per maze row (calls to the helpers at `$F053`, `$F06A` and `$F067`, and a few jumps), draws the status bar after line 198 and sets the second timer on line 195 while still drawing.
- Frame boundaries: the emulator cuts a frame at scanline 262 = 0, so in a trace the VSYNC end comes at the end of a frame.

## What the listing now shows

Labels `RESET`, `FRAME_START`, `VBLANK_TIMER_SETUP`, `VSYNC_END`, `VBLANK_WAIT`, `KERNEL_START`, `OVERSCAN_TIMER`, `OVERSCAN_WAIT` and a banner before each part, placed where the part begins. The listing is byte-identical to the ROM (SHA-1 `0940fea7f04cdb6d4b90c5ad1a7e344e68f6dbb1`). The routines shared between parts are sorted out in F8, the meaning of what each part computes from F9 on.
