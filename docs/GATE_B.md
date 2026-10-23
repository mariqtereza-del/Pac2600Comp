# Gate B — Development environment (Stage B9, 2026-10-24)

**Verdict: all six items PASS. Phase B is complete.** Every item was checked again in this session, from the files and tools themselves rather than from earlier notes. All checks only read, apart from one fresh build into a new folder `build\gate_b` (ignored by Git).

| # | Gate item | Result | Evidence (this session) |
| --- | --- | --- | --- |
| 1 | All tools verified | PASS | See section 1 |
| 2 | ROM locked | PASS | See section 2 |
| 3 | Git ignoring ROMs (proved) | PASS | See section 3 |
| 4 | Memory files present | PASS | See section 4 |
| 5 | First commit made | PASS | See section 5 |
| 6 | First checkpoint zip exists | PASS | See section 6 |

## 1. All tools verified

- **Versions:**
  - Git `2.56.0.windows.3` at `C:\Program Files\Git\cmd\git.exe`.
  - Python `3.14.0`.
  - `vswhere`: Visual Studio Build Tools 2026, `18.11.1`, `isComplete: 1`.
  - In the x64 Developer environment: `cl` version `19.51.36260 for x64`, CMake `4.3.1-msvc1`, Ninja `1.13.2`.
- **Whole chain:** a **fresh** build of both libraries into the new folder `build\gate_b`:
  - 570 steps in 66 s, 0 errors, 0 compiler warnings;
  - Stella core from 148 source files;
  - `stella_libretro.dll` 2,971,136 bytes and `SDL3.dll` 2,747,392 bytes, the same sizes as in B7 and in the B8 restore test.

## 2. ROM locked

- **Expected value:** `rom\EXPECTED_SHA1.txt` = `0940fea7f04cdb6d4b90c5ad1a7e344e68f6dbb1`, equal to the A3 value.
- **Original** (`Pac-Man (1981) (Atari).a26`, project folder):
  - SHA-1 matches, 4096 bytes, last written 1997-08-26 03:17:44;
  - read-only;
  - opening it for writing is refused (`UnauthorizedAccessException`).
- **Spare** (`F:\Ports\ROM-originals\`): the same result on every point.

## 3. Git ignoring ROMs (proved)

- **Ignore rules:** `git check-ignore -v` matched `Pac-Man (1981) (Atari).a26` (`*.a26`), `rom/pacman.a26` (`/rom/`), `reference/x.a26`, `docs/Copy.A26` (upper case), `third_party/x.bin` (`*.bin`) and the download archives (`/third_party/_downloads/`).
- **History:** 0 file names ending `.a26`/`.bin`/`.rom` in any commit, and 0 among the 3,258 files tracked now.
- **By content:** the ROM's Git fingerprint (`3a7ed122075803e474f8c596e1023ec9abff2361`) is not among the repository's 3,829 objects, and `git cat-file -e` confirms it isn't stored at all. So the ROM's bytes have never entered Git under any name.

## 4. Memory files present

- **Present and tracked in Git:** `CLAUDE.md`, `PROJECT_STATUS.md`, `PLAN.md`, `DECISIONS.md` and `docs\GLOSSARY.md`.
- **Rules:** the 12 rules in `CLAUDE.md` are identical to the text you pasted in B6 (scripted comparison, 12 rule headings).
- **Plan:**
  - `PLAN.md` has its 102 stage markers.
  - A1–A6 and B1–B8 are Done, and B9 was In progress during the check.
- **Status line:** line 1 of `PROJECT_STATUS.md` names the current stage.
- **Decisions:** 22 decisions are recorded. Every decision cited in the memory files exists.

## 5. First commit made

- **History:** `ffef5a0` Stage B5 (2026-10-23), `d17cef6` B6, `1435e80` B7, `1bb3e97` B8.
- **Health:** `git fsck --full` reports no errors.

## 6. First checkpoint zip exists

- **The zip:** `F:\Ports\Pac-Man - Atari 2600 checkpoints\pacman2600-2026-10-24-B8.zip`.
- **Fingerprint:** its SHA-256 equals the value in its `.sha256` file.
- **Contents:** 7,105 entries including `.git/HEAD`, with 0 ROM-like names and 0 entries with the ROM's content (every 4096-byte entry was hashed).
- **Restore:** the restore test in B8 proved it unpacks, matches the project and builds.

## End-of-phase checkpoint (with your approval)

- **The zip:** `pacman2600-2026-10-24-Gate-B.zip` in the checkpoints folder: 83,537,815 bytes, 7,116 entries, no ROM inside (checked by name and content).
- **Fingerprint:** SHA-256 `da29d7bd24e22aadfaa4bfda86982d90af984f77289059d4a10cf6768451fe47`, matching its `.sha256` file.
- **What it holds:** taken just before the B9 commit, so it contains B9's documents as uncommitted changes (T-B8-2).

## Notes

- **Leftover build folder:** `build\gate_b` (about 89 MB) is left in place. It sits in the ignored `build\` area and is re-created by any build. It can be removed later with your OK.
- **What Phase B does not prove:** the emulator runs nothing yet. Running Stella and the ROM starts in Phase C (C1, C2).
