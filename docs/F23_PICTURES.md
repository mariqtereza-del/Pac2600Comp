# Stage F23.3: whole games, every picture (2026-10-26)

`lockstep_native --pictures` plays the recorded inputs of each game through `game::Game` with the drawing code (`src\game\kernel.*`) and the video-chip model (`tia_video.*`, joined by `renderer.*`). There is **no 6502, no emulator and no original program code** in it. After every frame it compares the 128 memory bytes (all but the stack bytes) and the **fingerprint of the whole 228 x 160 picture** with the golden trace, which holds the emulator's picture of every frame. The logic parts (top logic, bottom logic, frame start) have no exact timing; their few writes to the video chip (colours, `REFP1`, `VBLANK`) are placed on fixed scanlines (5, 240, 259: the drawing of a two-player picture runs to about line 236, so the bottom logic must come after it), which is early or late enough that nothing visible depends on it.

## Result: every picture identical

- **70 of 70 recorded games, 439680 frames: every picture and all memory identical to the emulator's**, the flicker of the ghosts included (231.7 s of work for all of it).
- Control: poking five life markers into one frame of one game makes the picture difference appear at exactly that frame: True.

## Not claimed

- Frame 0 (the partial first picture after power-on, 45 lines) is not compared.
- The two collision flags still come from the recorded answers (the oracle of F20); F23.4 makes them from the model.
- Fingerprints prove equality of the whole picture (a 64-bit hash of all 36,480 colour values), not a pixel list; the earlier F23.1 test showed single-pixel differences are caught.
