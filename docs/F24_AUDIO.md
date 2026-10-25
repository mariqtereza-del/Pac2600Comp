# Stage F24.1: the sound-chip model (2026-10-26)

`src\game\tia_audio.hpp/.cpp`: `game::TiaAudio` is the chip's two sound channels as a small model. It is given the writes to `AUDC0/1`, `AUDF0/1`, `AUDV0/1` with the scanline and colour clock at which they happen and makes the sound samples: each channel has a divider (`AUDF`), a pulse and noise pattern generator (`AUDC`) and a volume (`AUDV`) that is added up on every colour clock; twice a scanline (at colour clocks 37 and 149) the average of the clocks since the last sample is taken for both channels and the two are added through the chip's mixing table (a resistor network: two volumes together are less than twice as loud). That is 524 samples for a frame of 262 lines, 31,440 a second. This game is mono: left and right are the same.

## Result: every frame equal

- **439680 of 439680 frames** of all 70 recorded games: the emulator's own list of writes to the sound registers fed to the model gives sound with the same fingerprint as the golden trace's, frame by frame. 124570 of the frames are not silent (tunes, wafers, power pills, ghosts, death), so the generators are really exercised.
- Controls: moving the effect time of every write by one colour clock (-1 and +1) leaves fewer frames equal, so the exact moment is pinned down: -1: 423132 of 439680; +1: 389836 of 439680.

## Facts found while comparing

- **Packaging:** the emulator hands the sound over in whole fragments of 524 samples. The first frame (power-on to the first vertical sync, 43 lines = 86 samples) therefore gives nothing, and every later frame gives the 524 samples that finished in it: the last 86 of the frame before and the first 438 of its own. 43 lines was found by comparison (41, 42 and 44 to 49 fail).
- **Timing:** the sound counter runs 60 colour clocks behind the clock the picture side reports for a write: a write has its effect 60 clocks later (59 and 61 fail in thousands of frames). Written as `kWriteDelay` in the model.

## Not claimed

- The writes come from the emulator here. Our own game logic writes the sound registers without exact timing (the bottom logic of the original takes a varying number of lines); getting the line of those writes right is the next stage (F24.2).
- Stereo or other cartridges are not modelled (this game is mono).
