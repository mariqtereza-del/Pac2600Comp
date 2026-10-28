# Stage F23.1: the video-chip model (2026-10-26)

`src\game\tia_video.hpp/.cpp`: `game::TiaVideo` is the video chip as a small model. It is given the writes the program makes to the chip, each with the scanline and colour clock at which it happened, and makes the 160 x 228 picture of the television window: playfield (left half and its repeat or mirror), two players with their copies and mirror, delayed graphics registers, two missiles, the ball, horizontal motion (`HMOVE` and the blank it makes at the left edge of a line), the priority and score-mode rules of `CTRLPF`, `VBLANK`, and the positions set by the `RESxx` strobes. It knows nothing about Pac-Man; what the game writes comes later (F23.3).

## Result: every picture equal

- **8266 of 8266 sampled frames** (every 53rd frame of all 70 recorded games, with the frame before each, so that settings left by the previous frame are right): the emulator's own list of chip writes fed to the model gives a picture whose fingerprint equals the golden trace's (the emulator's picture of 228 x 160 colour values). The only thing added to the writes is the power-on frame of the original (the places of missile 1 and the ball: 6 writes).
- Controls (the test can tell): player position - 1: 0 of 8266 equal; player position + 1: 0 of 8266 equal; missile position - 1: 958 of 8266 equal; missile position + 1: 958 of 8266 equal; ball position - 1: 7299 of 8266 equal; ball position + 1: 7299 of 8266 equal. So the position constants (first pixel column of a player = clock at the end of the strobe - 68 + 5; missile and ball + 4) are pinned exactly by the pictures.
- Timing windows: how far each write's effect time (in colour clocks after the end of its instruction) can move before any of the 8266 pictures changes: playfield (PF0-PF2): -12 to 6; player graphics (GRP0, GRP1): 0 to 4; missile and ball enables: -14 to 28; colours, CTRLPF, NUSIZ, REFP, VDELP: -8 to 32. The game writes these registers outside the places where the exact moment matters, so the pictures cannot pin them more closely; the model uses 0 for the playfield, 2 for the player graphics, 0 for the others (the middle of the windows).
- Values the game writes to the registers whose many modes the model supports: NUSIZ0: $00, $03, $20; NUSIZ1: $00, $03, $20; CTRLPF: $00, $02, $30, $31; REFP1: $00, $08; VDELP0: $00, $03; VDELP1: $00, $03.

## Not claimed

- The model code for other values (other `NUSIZ` copy patterns, a mirrored player with the other sizes, `RESMP`, mid-line `HMOVE`, the exact timing windows above) is written from the chip's documented behaviour but is not exercised by any recorded game, so it is not proved.
- The collision flags are not made here (F23.4). The writes come from the emulator, not yet from our own drawing code (F23.3).
