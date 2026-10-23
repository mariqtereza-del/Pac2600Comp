# Display facts (Stage C3, measured 2026-10-24)

All values below were **measured** by `tools\stella_probe` (DECISIONS.md T-C3-1), which runs the ROM in the same Stella 7.0 emulator code as the reference app and records every frame's TIA output. None is copied from memory.

- **Data:** `reference\c3\dump1\frames.csv` (local only). Summary in `reference\c3\dump1\summary.txt`, pictures in `reference\c3\dump1\png\`.
- **Run:** working copy `rom\pacman.a26`, 24,000 frames (≈ 6 min 40 s at 60 Hz):
  - frames 0–1799: attract mode, no input;
  - frames 1800–1809: Game Reset held;
  - then no input at all, so the ghosts catch Pac-Man until game over.
- **Settings:** Stella defaults (format AUTO, palette "standard", phosphor "byrom").
- **Commands:**
  - `tools\build_stella_probe.cmd`
  - `build\stella_probe\stella_probe.exe <rom> reference\c3\dump1 24000 1800 300`
  - `python -I tools\c3_report.py reference\c3\dump1 third_party\stella\src 1800 10`

## The facts

| Fact | Value | How measured |
| --- | --- | --- |
| TV format | **NTSC** (Stella: `NTSC*` = auto-detected) | `Console::about().DisplayFormat`; same as C2 |
| Cartridge | **4K** (Stella: `4K* (4K)`) | `Console::about().BankSwitch`; same as C2 |
| Scanlines per frame | **262**, on 23,999 of 24,000 frames | `TIA::scanlinesLastFrame()` after every frame |
| Exception | Frame 0 only: **45** lines | The partial first frame right after power-on, before the game's first vertical sync |
| Did the line count ever change? | **No**, in attract mode, at Game Reset, during the deaths and after game over. All frames from 1 to 23,999 are 262 lines | The same per-frame record, one unbroken run of 262 |
| Stella's visible window | Starts at scanline **23**, **228** lines tall (scanlines 23–250), identical on every frame | `TIA::startLine()`, `TIA::height()` |
| Width | **160** pixels; the game draws across all of them (columns 0–159) | Raw TIA framebuffer, 160 pixels per line; leftmost/rightmost non-black pixel |
| Lines the game draws on | Rows **11–192** of the window in attract mode, **11–199** in play, i.e. scanlines **34–222**. Above and below is black | First/last row with any non-black pixel |
| Pictures produced | One per frame, with no skipped frames | `getVideoReady()` true on all 24,000 frames |

## Colours

The TIA stores a colour number per pixel; Stella turns it into RGB with its "standard" NTSC palette. The RGB values below are read directly from Stella 7.0's palette table (`src/common/PaletteHandler.cxx`, `ourNTSCPalette`).

**Active play** (frames 1810–3000, 1,191 frames): only 9 colours.

| TIA value | RGB (Stella NTSC) | Appears in | Seen as (from the pictures) |
| --- | --- | --- | --- |
| $00 | #000000 | every frame | black border above/below |
| $84 | #2d32b8 | every frame | maze background (blue) |
| $C4 | #328432 | every frame | green bar under the maze |
| $F6 | #a28638 | every frame | maze walls |
| $FA | #d2b656 | every frame | yellow (Pac-Man / bright items) |
| $CC | #80e880 | 298 frames (¼) | one of four ghost colours |
| $DC | #b4e775 | 298 frames (¼) | ghost colour |
| $EC | #ccd86e | 297 frames (¼) | ghost colour |
| $FC | #e8cc63 | 298 frames (¼) | ghost colour |

- **Ghost flicker, in numbers:** each of the four ghost colours is present in about one frame in four. The four ghosts take turns, one per frame.
- **Labels:** the "Seen as" column comes from looking at the pictures. Which ghost has which colour is not yet established (Phase F).

**Attract mode and after game over:** the game slowly **shifts all its colours** while nobody plays. The pictures show the maze in brown, green, grey and magenta schemes. Over the whole run, **87 different TIA colours** appear (full list with RGB and frame counts in `summary.txt`). So the port needs the whole NTSC palette, not just the 9 play colours.

## What the pictures show

Pictures saved every 300 frames (`png\frame_NNNNNN_160x228.png`, drawn 4× wide and 2× tall for viewing only):

| Frame | What it shows |
| --- | --- |
| 1500 | Attract mode: maze in dim colours, one ghost, "©1981 ATARI" on the green bar |
| 2100 | Game running: Pac-Man, a ghost, score "1", three life markers bottom left |
| 3000 | Still playing, one life marker left (two lives lost) |
| 4200 | Pac-Man gone, colours shifted: game over |
| 6000, 15000, 23700 | Game over, colours still shifting (green/blue, grey/purple, brown/magenta) |

## Not yet measured

| Item | Why not | Where it will be measured |
| --- | --- | --- |
| Line count during a **level change** | With no input, Pac-Man eats no dots, so no level is ever cleared | E-phase recordings of real play (E3/E7), or a C7 capture with scripted input |
| The exact frame of each death and of game over | Pictures were saved every 300 frames, which is enough to prove deaths happened but not their exact frames | C7 / Phase E (with game RAM known) |
| Stella's **phosphor** blending of the shown picture | The probe reads the raw TIA picture, before Stella's display effects | D6 decides how the port presents flicker |

## Things learned about the emulator (for Phases D and E)

1. **Runs repeat exactly.** All 13 completed 24,000-frame runs (`run2` from the first build, `try1`–`try6` and `dump1`–`dump6` from the final one) gave byte-identical per-frame data, and the 80 saved pictures of `run2` and `dump1` match.
2. **Why they repeat (an important difference):** Stella's libretro wrapper, `StellaLIBRETRO::runFrame()`, writes its own copy of the console's 128 bytes of RAM back into the emulator before every frame. Before the first frame that copy is all zeros. So the libretro core starts with **zeroed RAM**, while the Stella **app** starts with **random** RAM (`plr.ramrandom` = true, like a real console).
   - Display facts aren't affected.
   - But the Phase 1 program (built on the libretro core) and the reference app may differ in their first frames, or in anything the game derives from uninitialised RAM. Phase D/E must check this rather than assume identical behaviour.
3. **"File open/read error" at start-up is harmless.** Stella's libretro file layer (`FSNodeLIBRETRO`) reports every file as existing. When Stella tries an optional settings file that isn't there, the read fails and Stella catches and prints the error. The ROM itself is read through another path, and its name and MD5 were reported correctly. The probe wrote nothing into the project folder (checked).
4. **One unexplained crash.** The very first probe run (first build) stopped with an access violation (`0xC0000005`) at frame ≈ 23,973.
   - The 23,972 frames it had recorded are identical to the good runs.
   - All 13 later full runs (with and without picture saving) completed, plus one 5-frame logging run.
   - The probe now prints the location of any crash (offset into `stella_probe.exe`; see the linker map `build\stella_probe\stella_probe.map`).
   - Cause unknown; listed as an open problem.
