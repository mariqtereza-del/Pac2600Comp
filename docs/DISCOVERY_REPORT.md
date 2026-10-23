# Phase A discovery report

Written 2026-10-23, Stage A6. This is the one-page summary of Phase A. The details and evidence are in the files named in each line.

## What the ROM is

Your file is **Atari 2600 Pac-Man, the original 1982 Atari release for the USA** (model CX2646, programmed by Tod Frye). It is a plain **4 KB** cartridge, 4096 bytes. Two independent game databases, Stella's and No-Intro's, match its fingerprint exactly. The program starts at its very first byte ($F000) and uses no bank switching. About 87% of it looks like program code and about 13% like data (an estimate). Source: `docs\ROM_IDENTITY.md`.

## Confirmed (with proof)

| Fact | Proved by |
| --- | --- |
| PC: Windows 11 Pro 25H2, RTX 5070, 2560×1440 at 180 Hz, 86.7 GB free on F: | `docs\ENVIRONMENT.md` (A2) |
| Python 3.14.0 installed; no Git for Windows, Microsoft compiler or Stella yet | `docs\ENVIRONMENT.md` (A2) |
| ROM size 4096 bytes; MD5 `6e372f076fb9586aff416144f5cfe1cb`; SHA-1 `0940fea7f04cdb6d4b90c5ad1a7e344e68f6dbb1` | `docs\ROM_IDENTITY.md` (A3) |
| Identity: Pac-Man (USA), 1982, CX2646 | Stella and No-Intro database entries quoted in `docs\ROM_IDENTITY.md` (A3) |
| Original never changed; working copy `rom\pacman.a26` identical | SHA-1 rechecked at the start and end of every stage A3–A6 |
| Start address $F000; no bank switching | `tools\a4_structure.py` output in `docs\ROM_IDENTITY.md` (A4) |
| Stella turns on its "phosphor" (afterglow) display effect for this game by default | Stella database entry, field 28 (A3) |
| Tool list approved | `DECISIONS.md` D-001 (A5) |

## Still assumed (and the stage that will check it)

| Assumption | Checked in |
| --- | --- |
| The code/data split is only an estimate (data is probably a bit more than 13%) | F2–F6 |
| The console has 128 bytes of RAM and 160-pixel-wide lines (standard 2600 facts, not yet measured on this game) | C3, F7 |
| How many picture lines the game draws, and whether that changes | C3 |
| Frame rate just under 60 per second (NTSC) | C4 |
| 2600 pixels are about 12:7 as wide as tall | D6 |
| Stella 7.0 has a portable Windows download (its file list didn't load on 2026-10-23) | C1 |
| The Microsoft compiler builds Stella's current core (C++23) and SDL3 cleanly | B3, B7 |
| Where the ghosts' randomness comes from | F14 |
| Stella is accurate enough to stand in for a real console (a permanent, stated assumption) | written in `TESTING.md` (H1) |
| Whether the reconstructed source may be published | Your decision, J1 |

## Changes to the plan so far

1. **Emulator core:** Stella's own current libretro port, at the same version as the reference Stella app, instead of the 2014 copy the roadmap named (D-001). Our program and our "ruler" will run the same emulator.
2. **Tools:** Microsoft's compiler (VS 2026 Build Tools) and Git for Windows are confirmed as needed, so B1 and B2 both go ahead (D-001). MSYS2, already on the PC, is not used.
3. **`DECISIONS.md` has two lists:** numbered decisions you approve (D-…), and small technical choices Claude Code makes (T-…), so the plan's D-001/D-002 numbering stays as written.
4. **`tools\` folder created early** for the read-only ROM analysis script (T-A4-1); B5 would have created it anyway.
5. **Noted, no change:** a second file named `Pac-Man (1981) (Atari).a26` exists in `F:\Ports`. The project only ever uses the copy inside the project folder (T-A3-1).

None of these changes the plan's goals or the order of the stages.

## Gate A

| Gate item | Status | Evidence |
| --- | --- | --- |
| ROM identified | **Met** | `docs\ROM_IDENTITY.md` "Verdict"; SHA-1 matches A3 |
| Structure known | **Met** | `docs\ROM_IDENTITY.md` "Structure": start $F000, no bank switching |
| Toolchain approved | **Met** | `DECISIONS.md` D-001, your answers recorded |
| `PLAN.md` updated | **Met** | A1–A5 marked Done, A6 marked Done with this report, 102 stage markers present |

**Phase A is complete.** Next: Phase B, starting with B1 (you install Git for Windows, with click-by-click guidance).
