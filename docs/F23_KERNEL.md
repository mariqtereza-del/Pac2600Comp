# Stage F23.2: the drawing code as native code (2026-10-26)

`src\game\kernel.hpp/.cpp`: `game::drawPicture` does what the original does between the top logic and the bottom logic of a frame (`$F07F-$F4E6`): the positions of the sprites and of the power-pill missile, the 20-line maze rows (walls and wafers as playfield patterns, the ghost, Pac-Man, the power pills and the bonus item), the bottom wall, the score digits and the life markers. It works on the same 128 bytes, leaves the same scratch bytes in memory, and hands every write to the video chip to a `TimedChip` with the scanline and the colour clock at which the original makes it. The clocks are counted from the processor cycles of the original instructions (3 colour clocks per cycle, one more for a page crossing, a wait for the end of the line at each `WSYNC`); the video-chip model of F23.1 turns the writes into the picture.

## Result: identical

- **8266 sampled frames of all 70 games** (every 53rd frame): given the 128 bytes at `$F07F`, the native drawing code makes **8266 of 8266 lists of writes to the video chip equal to the emulator's, write for write** (scanline, colour clock, address of the writing instruction, register, value; 4528964 writes in all), and leaves **8266 of 8266 memories equal** to the emulator's at `$F4E7` (all bytes except the stack bytes `$F6-$FF`).
- Control: with every colour clock moved by 3 the same test finds 0 of 8266 lists equal (it can tell).
- **Whole native game with the drawing code inside** (`game::Game`, `lockstep_native`): **70 of 70 recorded games, 439680 frames, all 128 bytes identical after every frame with nothing left out except the stack bytes `$F6-$FF`** (F21.27 had to leave out 11 scratch bytes; the drawing code now makes them as well). 8.9 s of work. The collision flags still come from the oracle until F23.4.

## Not claimed

- The picture itself is not compared here: that is F23.3 (these writes through the video-chip model, fingerprint of every frame against the golden traces).
- The start line of the drawing code (the top logic ends on a line that varies by a few lines) is given by the test from the emulator; the game uses line 16. Only the set-up writes before line 34 depend on it, and nothing visible does.
- Only every 53rd frame is compared write by write; F23.3 covers every frame by picture.
