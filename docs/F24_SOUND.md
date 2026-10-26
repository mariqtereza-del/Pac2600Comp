# Stage F24.2: the native game's sound, frame by frame (2026-10-26)

The sound registers are written at the end of the bottom logic, whose length varies by several lines; the sound chip makes two samples a line, so the exact moment of those writes decides which samples change. The rewritten bottom logic (`src\game\routines.cpp`, `struct Bottom` and the routines it calls) now counts the processor cycles of the original instructions along the path it takes (a table of the cycles of every instruction of the listing: `python -I tools\f24_cycles.py <from> <to>`), starting from the end of the drawing code (`game::PictureEnd`, the line and colour clock after its last write). `game::Renderer` turns the count into a scanline and colour clock for every write (3 colour clocks a cycle) and feeds them to the video-chip model and the sound-chip model (`game::TiaAudio`). The sound handed over after each frame is compared with the golden trace's.

## Result: sound identical

- **70 of 70 recorded games, 439680 frames: memory (all but the stack bytes), the picture and the sound identical to the emulator's in every frame**, with no emulator, no 6502 and no recorded answers in the program. 239.3 s of work.
- Control: with every write of the bottom logic one processor cycle late, the sound differs in 67 of 70 games (60014 frames in all), so the test can tell and the counted cycles are exact.

## Not claimed

- The Select step of the title screen (it silences the sound in the top logic) is counted from the end of the vertical sync (`TiaChip::writeAtCycle`); only the paths that occur in the recordings are counted (first press and the repeat every 32 frames, with and without the title screen test).
- The other writes of the top logic (the sprite colours) and of the frame start are placed on fixed lines: nothing visible or audible depends on them.
- Only recorded inputs are played; random-input games come in F25.
