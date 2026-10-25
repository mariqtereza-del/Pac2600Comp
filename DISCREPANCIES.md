# Discrepancies: our program against Stella

Written in Stage D14 (2026-10-25). Method and reasons: `DECISIONS.md` T-D14-1. Tool: `tools\d14_side_by_side.py`.

## Result

**No differences found.** The same input was run in Stella 7.0's emulation core (the Phase C reference tool) and in our program, headless and in the real window:

| Input | Frames | Fingerprint differences (headless) | Fingerprint differences (window) | Whole pictures compared |
| --- | --- | --- | --- | --- |
| Your own recording (human play, B&W and the left difficulty switch changed) | 1,860 | 0 | 0 | 10 of 10 identical |
| Test recording (Reset, directions, switches) | 219 | 0 | not run | 10 of 10 identical |
| The C7 bot route (ghost eaten, power pills, death, bonus item, level end, game over) | 9,800 | 0 | 0 | 10 of 10 identical |

A deliberate change (one key press removed from your recording) was seen by the tool: 963 differing frames, the first at frame 897. So "0" means "the same", not "could not tell".

## Authenticity: our program against a real NTSC Atari 2600 of 1981 (owner's rule D-004)

| What | Stella | A real console of 1981 | Our program now | Status |
| --- | --- | --- | --- | --- |
| Frame rate | 60.000 Hz | **59.9227 Hz** (3.579545 MHz / (228 x 262)) | **59.92 Hz** (measured 59.9235 over 9 minutes) | **Fixed** (T-D14-2) |
| Sound pitch / sample rate | 31,440 Hz | **31,399.5 Hz** | 31,400 Hz | **Fixed** (T-D14-2) |
| Memory at power-on | zeros in Stella's core (random in the Stella app) | random | zeros | **Examined, no visible effect** on this game (6 random seeds, 3,000 frames, 0 differences); kept (T-D14-3) |
| Picture shape (a game pixel's width : height) | 19:10, from stretching all 228 picture rows to a 4:3 box | depends on how many lines the TV showed; the standard NTSC arithmetic gives about 12:7 (1.71) | **12:7**, Framed (key 5): game area with an 8-game-pixel border on every side, fitted and centred (default; `--pixel-aspect stella` = 19:10) | **Fixed** by your decisions D-005 and D-006 |
| Colours | Stella's "standard" NTSC palette | varied from TV to TV (hue and brightness knobs) | Stella's standard palette | Not a fault: there is no single authentic palette |
| Flicker | Stella's game list turns on a phosphor glow (60 % of the previous picture) for this ROM | the ghosts flickered (one in four frames); the picture tube's glow did not smooth them much | no glow by default; **F7** turns Stella's glow on | **Fixed** by your decision D-007 |
| Console switches | physical | physical, left where the owner put them | same, remembered | Authentic |
| CPU, TIA, RIOT behaviour | Stella core | the chips | the same Stella core (0 differences against the reference, above) | Same emulation; the core's accuracy itself is Stella's |

**Picture shape:** decided by the owner on 2026-10-25 (D-005): 12:7, fitted to the display and centred.

## Known differences from the Stella app (by design)

| What | Stella app | Our program | Where it is recorded |
| --- | --- | --- | --- |
| Memory at power-on | random | zeros (so a recording always replays the same) | `docs\DISPLAY.md`, T-D13-1 |
| Game speed | exactly 60.000 Hz | 59.92 Hz, the console's own (changed in D14) | `docs\TIMING.md`, T-D14-2 |
| Picture shape and size | its own scaling options | Sharp / Fill / TV frame / Game area (keys 1-4), pixel shape 12:7 by default (D-005) | T-D6-1 |
| Controls | its own key map | `docs\CONTROLS.md` | T-D10-1, T-D11-1 |

## Not compared

- Sound (proved identical to the Phase C recording in D8, bit for bit).
- The Stella app itself: it cannot play a recording, so a frame-by-frame comparison is impossible. An eye comparison is possible whenever you want one.
