# How to recreate the reference (Stage C8)

The "reference" is the original Atari 2600 Pac-Man running in Stella 7.0, plus the measurements and pictures taken from it. Everything the port does is compared with it. This page says exactly what is needed to get the same numbers and pictures again.

## What the reference is made of

| Part | Exact identity | Where it is described |
| --- | --- | --- |
| The game | `Pac-Man (1981) (Atari).a26`, 4096 bytes. SHA-1 `0940fea7f04cdb6d4b90c5ad1a7e344e68f6dbb1`, MD5 `6e372f076fb9586aff416144f5cfe1cb`. Stella names it "Pac-Man (1982) (Atari)" (CX2646). Plain 4 KB, no bank switching, NTSC, starts at $F000. The ROM is **never** in Git (Rule 7); you provide it. | `docs\ROM_IDENTITY.md`, `docs\FIRST_RUN.md` |
| Reference app | Stella 7.0, official Windows build 7.0c, in `F:\Ports\Tools\Stella-7.0c`. Official zip `Stella-7.0c-windows.zip`, 4,255,411 bytes, SHA-256 `44102ccf8af74b021cf143db346e561c5d6ed2a2e91d79ee6d3c99d7c42f4653`. Started only with `tools\run_stella.cmd` (`-baseinappdir`). Settings at their defaults. | `docs\REFERENCE_EMULATOR.md` |
| Measuring tools | Built from the same Stella 7.0 source in `third_party\stella` (tag 7.0, byte-identical), with MSVC and CMake. | `docs\DEPENDENCIES.md`, `tools\stella_probe\` |
| Starting condition of the tools | Zeroed RAM at power-on, Stella defaults (format auto = NTSC, palette "standard"). The Stella **app** starts with random RAM, so its first moments can differ. | `docs\DISPLAY.md` |
| Timeline | Frame numbers count from power-on. Attract mode needs no input. **Game Reset** is pressed at frame 1800 (held 10 frames) in every measurement. | `docs\DISPLAY.md` |

## Recreating it, step by step

1. Put the original ROM at `rom\pacman.a26` and check its SHA-1 equals the value above (`rom\EXPECTED_SHA1.txt` is the local record).
2. Build the measuring tools: `tools\build_stella_probe.cmd`. It builds `stella_probe`, `stella_timing`, `stella_sound`, `stella_script` and `stella_bot` into `build\stella_probe\`.
3. Each fact document gives its own commands. They run the program, write into a folder under `reference\` (local only) and print a summary:

| Facts | Document | Tool and report |
| --- | --- | --- |
| Display | `docs\DISPLAY.md` | `stella_probe.exe`, `tools\c3_report.py` |
| Timing | `docs\TIMING.md` | `stella_timing.exe`, `tools\c4_report.py` |
| Sound | `docs\SOUND.md` | `stella_sound.exe`, `tools\c5_report.py` |
| Controls, variations | `docs\CONTROLS.md` | `stella_script.exe`, `tools\c6_experiments.py` |
| Eight reference pictures | `docs\CAPTURES.md` | `build\stella_probe\stella_script.exe rom\pacman.a26 <out_folder> 9800 tools\captures\c7_captures.txt`, then `python -I tools\raw_to_png.py <out_folder> third_party\stella\src` |

4. **How to know it is the same:** the picture fingerprints of an idle run (Game Reset at 1800) equal C3's (checked in C4, C5, C6 and again in C8); the eight capture pictures equal the SHA-256 prefixes in `docs\CAPTURES.md`, and replays are byte-identical.

## What the reference is not

- It is Stella's emulation (the same code the port will contain), not a real console on a real TV. Colours are Stella's NTSC palette.
- Anything the port copies from Stella's core is not independently verified by this reference; what is independent is the **ROM's behaviour** (the game itself), measured through it.

## Open items (not yet measured or determined)

Collected from the fact documents; each has the stage that is expected to settle it.

| Open item | Where it comes from | Expected stage |
| --- | --- | --- |
| Line count during a level change; exact frame of each death | `docs\DISPLAY.md` (C7's route now contains a level change; its line count was not re-measured) | Phase E |
| Stella's phosphor blending of the shown picture, flicker presentation | `docs\DISPLAY.md` | D6 |
| Pacing at 60.000 Hz (Stella) versus 59.923 Hz (console clock) | `docs\TIMING.md` | Phase D (D9) |
| Timer constants, WSYNC waits, when the joystick is read in a frame | `docs\TIMING.md` | Phase F |
| Sounds for power pill, ghost eaten, level clear (route now reaches them; sound not recorded) | `docs\SOUND.md` | Phase E |
| What variations 1-8 change; the difficulty switches' exact bonus-item rule | `docs\CONTROLS.md` | Phases E and F |
| PS4 pad d-pad; fire button's other uses | `docs\CONTROLS.md` | Phase D (controls), E |
| Meaning of the RAM addresses (labels are working labels) | `docs\CAPTURES.md` | Phase F |
| Two-player, black-and-white and level-2 pictures | `docs\CAPTURES.md` | Phase E |
