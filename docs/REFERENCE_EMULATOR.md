# Reference emulator (Stage C1, 2026-10-24)

The standalone Stella app is the project's "ruler": everything the port does is compared with what this app shows. It is the same emulator version as the core built into our program (D-002).

## What and where

| Item | Value |
| --- | --- |
| App | Stella 7.0, official Windows build "7.0c" (`Stella.exe` file version 7.0, "The Stella Team") |
| Folder | `F:\Ports\Tools\Stella-7.0c` (you unzipped it there on 2026-10-24; T-C1-1) |
| Start it with | `tools\run_stella.cmd` (adds `-baseinappdir`; extra arguments are passed on) |
| Settings | Inside the app folder: `stella.sqlite3`, plus folders `cfg\`, `nvram\`, `state\`. Nothing in `%APPDATA%` (T-C1-2) |
| Same emulation as | `third_party\stella` (source tag 7.0). 7.0c changed only Windows installer scripts (D-002) |

## Proof it is the official release

- **The official zip, downloaded again with your approval:** `https://github.com/stella-emu/stella/releases/download/7.0/Stella-7.0c-windows.zip`. It is kept locally in `reference\_downloads\`, never in Git.
  - Size: 4,255,411 bytes, the same as listed on the release page.
  - SHA-256: `44102ccf8af74b021cf143db346e561c5d6ed2a2e91d79ee6d3c99d7c42f4653`. Stella publishes no checksum, so this is our record.
- **Compared with your folder:** each file was compared by SHA-256 with the zip's contents (`Stella-7.0c/` prefix removed). Result: **96 identical, 0 different, 0 missing, 0 extra**, before the first start.
- **`Stella.exe`:** 6,692,352 bytes, SHA-256 `79388a8cd5c651f849ad693e069120268ff622e0b09349303935d4081e81e46b`. This build is **not digitally signed** (Windows reports `NotSigned`). That is why the byte-for-byte comparison is the proof.

## First start (with your approval)

- **Start:** `tools\run_stella.cmd` → process `F:\Ports\Tools\Stella-7.0c\Stella.exe`, command line `"…\Stella.exe" -baseinappdir`. The window title was `Stella 7.0`, and it was responding.
- **What you saw:** "The game list / file browser". You closed it with its own close button, and the process ended normally.
- **What Stella wrote, all inside its folder:**
  - `stella.sqlite3`: 57,344 bytes after closing. During the run it had the temporary `-shm`/`-wal` companions, which were merged and removed on exit.
  - Three empty folders: `cfg\`, `nvram\` and `state\`.
- **Not written:** no `%APPDATA%\Stella` and no `%LOCALAPPDATA%\Stella` were created.
- **After closing:** the 96 official files were still identical.
- **No ROM was opened in C1.** The ROM SHA-1s are unchanged.

## Notes for the next stages

- **C2 opens the game.** It should open the working copy `rom\pacman.a26`, or the read-only original. Stella only reads ROM files.
- **Where Stella saves its own files:** snapshots, saved states and properties go to Stella's own folders, with paths set in its settings. C2 checks that nothing is written next to the ROM.
- **Keep the settings at their defaults:** later measurements (C3–C5) record any setting they change, so results can be repeated.
- **If you move or rename the Stella folder,** update `STELLA_DIR` in `tools\run_stella.cmd` and this file.
