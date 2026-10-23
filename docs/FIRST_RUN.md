# First run of the original (Stage C2, 2026-10-24)

The original Atari 2600 Pac-Man ran for the first time in the reference app (Stella 7.0c, `docs\REFERENCE_EMULATOR.md`).

## How it was run

- **ROM:** the working copy `rom\pacman.a26` (SHA-1 `0940fea7…dbb1`, MD5 `6e372f076fb9586aff416144f5cfe1cb`), never the read-only original (T-C2-1).
- **Start:** `tools\run_stella.cmd "F:\Ports\Pac-Man - Atari 2600\rom\pacman.a26"`.
  - Process command line: `"F:\Ports\Tools\Stella-7.0c\Stella.exe" -baseinappdir "F:\Ports\Pac-Man - Atari 2600\rom\pacman.a26"`.
  - Window title: `Stella 7.0: "Pac-Man (1982) (Atari)"`, responding.
- **Input:** keyboard only, your choice. The PS4 controller was not connected, so it is left for C6 (controls).
- **Starts:** Stella was started twice for play, the second time at your request after you had closed it. It also ran once in `-rominfo` mode (see below).

## Your verdict

> "Looks, sounds and plays right"

(The answer you chose after playing with the keyboard. The suggested keys were arrow keys, Space, and F2 = Game Reset; how long you played wasn't measured.)

## What Stella knows and detects about this ROM

**Stella 7.0's built-in database** (`third_party\stella\src\emucore\DefProps.hxx`, line 1625): the same entry A3 read from Stella's development version.

| Field | Value |
| --- | --- |
| Cart_Name | Pac-Man (1982) (Atari) |
| Cart_Manufacturer | Atari, Tod Frye - Sears |
| Cart_ModelNo | CX2646 - 49-75185 |
| Cart_Type, Display_Format, all controller and console fields | empty, so Stella auto-detects |
| Display_Phosphor | YES (Stella's "afterglow" blending is on for this game) |
| Bezel_Name | Pac-Man (Atari) (USA) |

**Detected while the game ran**, read by you in Stella's "Game Properties" → "Emulation" tab:

| Field | Shown | Meaning |
| --- | --- | --- |
| Type (*) | **4K detected** | Plain 4 KB cartridge, no bank switching; agrees with A4's own analysis |
| TV format | **NTSC detected** | The US TV standard (60 frames per second); C3 measures it directly |

- **Why it was read by eye:** Stella's `-rominfo` option, which should print these details, produced no output on Windows. Both output streams were empty, because Stella is a windowed program. So the values were read from the in-app dialog instead.
- **Where the two fields are:** both are on the **Emulation** tab, not "Cartridge" (Stella 7.0 source, `src/gui/GameInfoDialog.cxx`). I first sent you to the wrong tab.

## What changed on disk

- **`rom\`:** unchanged. A fingerprint snapshot of every file before C2 equals the one after, and nothing was written next to the ROM.
- **ROMs:** all three copies (original, working copy, spare) still have SHA-1 `0940FEA7…DBB1`; the original and spare are still read-only.
- **Stella's own folder:**
  - Only `stella.sqlite3` (its settings) was updated, same size, during the `-rominfo` run.
  - `cfg\`, `nvram\` and `state\` are still empty, and its 96 official files are still identical to the release.
- **Your Windows profile:** no `%APPDATA%\Stella`.

## Notes for later stages

- **C3 (display):** measure, don't trust the "NTSC detected" label. Also note that the phosphor effect is on by default for this game and affects how frames look.
- **C6 (controls):** test the PS4 controller. With DS4Windows running it may appear as an Xbox controller. Also cover F1 (Select) and the difficulty switches.
