# ROM identity (Stage A3)

Checked on 2026-10-23 by Claude Code. The original file was only read, never changed.

## Verdict

**Identified: Atari 2600 Pac-Man, original 1982 Atari release (USA), model CX2646, programmed by Tod Frye.** 4096 bytes (4 KB), as the plan expected. Two independent databases match the fingerprint exactly. The plan continues without changes.

## The file

| Item | Value |
| --- | --- |
| Original (never modified) | `Pac-Man (1981) (Atari).a26`, top level of the project folder |
| Working copy (made in A3) | `rom\pacman.a26` |
| Size | **4096 bytes** |
| MD5 | `6e372f076fb9586aff416144f5cfe1cb` |
| SHA-1 | `0940fea7f04cdb6d4b90c5ad1a7e344e68f6dbb1` |
| SHA-256 | `58e781b472e0583b851b7e56cbfe87bd8294a291e6d39d49d7c999107ed5b918` |
| CRC32 | `DDC9A881` |
| File date | 1997-08-26 03:17:44 (unchanged by the copy) |

The "1981" in the file name is only the file's name; both databases list the release as 1982.

## Evidence: fingerprints before and after the copy

1. **Original, before copying** — command:

    ```powershell
    $f = 'Pac-Man (1981) (Atari).a26'; (Get-Item -LiteralPath $f).Length; foreach ($a in 'MD5','SHA1','SHA256') { (Get-FileHash -LiteralPath $f -Algorithm $a).Hash.ToLower() }
    ```

    ```text
    Size bytes: 4096
    MD5     6e372f076fb9586aff416144f5cfe1cb
    SHA1    0940fea7f04cdb6d4b90c5ad1a7e344e68f6dbb1
    SHA256  58e781b472e0583b851b7e56cbfe87bd8294a291e6d39d49d7c999107ed5b918
    ```

2. **The copy** — command:

    ```powershell
    New-Item -ItemType Directory -Path rom; Copy-Item -LiteralPath 'Pac-Man (1981) (Atari).a26' -Destination 'rom\pacman.a26'
    ```

    Then the same fingerprint command on both files gave identical MD5, SHA-1 and SHA-256 for `Pac-Man (1981) (Atari).a26` and `rom\pacman.a26`, both 4096 bytes, both dated 1997-08-26 03:17:44.

3. **Original, after all A3 work** — MD5 and SHA-1 recomputed: unchanged (same values as above). CRC32 computed with Python's `zlib.crc32` (`python -I -c ...` on both files):

    ```text
    Pac-Man (1981) (Atari).a26 DDC9A881
    rom\pacman.a26 DDC9A881
    ```

## Evidence: database lookups

### 1. Stella's built-in game database

Stella isn't installed yet (C1), so its database was read from its source code on GitHub:
<https://raw.githubusercontent.com/stella-emu/stella/master/src/emucore/DefProps.hxx> (master branch, read 2026-10-23; the file holds 3661 entries of 30 fields each).

The row for MD5 `6e372f076fb9586aff416144f5cfe1cb`, with field names taken from the property list in <https://raw.githubusercontent.com/stella-emu/stella/master/src/emucore/Props.hxx>:

| # | Field | Value |
| --- | --- | --- |
| 1 | Cart_MD5 | `6e372f076fb9586aff416144f5cfe1cb` |
| 2 | Cart_Manufacturer | Atari, Tod Frye - Sears |
| 3 | Cart_ModelNo | CX2646 - 49-75185 |
| 4 | Cart_Name | Pac-Man (1982) (Atari) |
| 5–27 | (all others, including Cart_Type and Display_Format) | empty, so Stella uses its automatic detection |
| 28 | Display_Phosphor | **YES** |
| 29 | Display_PPBlend | empty |
| 30 | Bezel_Name | Pac-Man (Atari) (USA) |

What this means:
- **Cart_Type empty:** Stella has no special bank-switching note for this game; it detects the type itself. Stage A4 checks the structure directly.
- **Display_Phosphor = YES:** Stella switches on its "phosphor" effect for this game by default. It blends each frame with the previous one, like an old TV's afterglow, which softens the ghost flicker. This is a display aid from the emulator, not part of the ROM. It matters for C2/C3 (what "looks right" in Stella) and Phase I (a possible anti-flicker option).
- "Sears" and 49-75185 refer to the version sold by Sears; it is listed under the same fingerprint.

### 2. No-Intro catalogue (via the libretro database)

<https://raw.githubusercontent.com/libretro/libretro-database/master/metadat/no-intro/Atari%20-%202600.dat> (master branch, read 2026-10-23):

```text
Pac-Man (USA): size 4096, crc DDC9A881, md5 6E372F076FB9586AFF416144F5CFE1CB, sha1 0940FEA7F04CDB6D4B90C5AD1A7E344E68F6DBB1
```

All four values match the file exactly. For context, the same catalogue lists a different `Pac-Man (Europe)` (md5 `FC2233FC116FAEF0D3C31541717CA2DB`), so this project targets the USA (NTSC) version.

### 3. Plain web search

A web search for the bare MD5 found no page containing it. That isn't unusual: search engines rarely index the contents of large database files. It doesn't weaken the two matches above.

## Not checked

| Item | Reason |
| --- | --- |
| The second `Pac-Man (1981) (Atari).a26` in `F:\Ports` | Outside the project folder; not needed. Not opened. |

## Structure (Stage A4, 2026-10-23)

All results come from `tools\a4_structure.py`, a read-only script, run on the working copy:

```powershell
python -I tools\a4_structure.py rom\pacman.a26
```

SHA-1 of `rom\pacman.a26` before and after the run, and of the original after: all `0940fea7f04cdb6d4b90c5ad1a7e344e68f6dbb1`.

### Verdict

| Question | Answer |
| --- | --- |
| Start address | **$F000**, the first byte of the ROM |
| Bank switching | **None.** A plain 4 KB cartridge (Stella's type "4K"). |
| Code vs data (estimate) | About **3557 bytes (87%) code**, about **539 bytes (13%) data**, mostly in one block at $FD3F–$FF42 |

The plan's main assumption holds: no change to the plan is needed.

### 1. Start and interrupt addresses ("vectors")

The 6502 family reads three 2-byte addresses from the last 6 bytes of memory. Script output:

```text
Raw bytes at $FFFA-$FFFF: 00 00 00 F0 00 00
NMI            $0000  (file offset $000, does NOT point into the cartridge)
RESET (start)  $F000  (file offset $000, points into the cartridge)
IRQ/BRK        $0000  (file offset $000, does NOT point into the cartridge)
```

- **RESET = $F000:** when the console is switched on, the program starts at the very first byte of the ROM.
- **NMI and IRQ = $0000:** unused. The 2600's 6507 processor has no interrupt pins, so these can never fire. A `BRK` instruction would use the IRQ address, but the trace found none in the code.
- **About the addresses:** the 6507 only sees 13 address lines, so the ROM shows up at $1000–$1FFF and at mirror copies such as $F000–$FFFF. This document uses the $F000 form, like most 2600 tools; the script's "$1Fxx" values mean the same places.

### 2. Bank switching

Script output:

```text
File size 4096 bytes = 4 KB, the largest size a 2600 can see without bank switching.
Traced code touching known switching addresses ($1FE0-$1FFB): $1FE9 from $FF43, $1FED from $FF4F, $1FF1 from $FFBC, $1FF5 from $FFC1
Traced code writing anywhere in cartridge space (needed for extra cartridge RAM or switching): none
Superchip pattern (first 256 bytes all one value, as in images of cartridges with extra RAM): no (91 different values)
Writes to $3F ('3F' switching scheme) in traced code: 0
```

The four "touching" lines looked worrying, so they were checked byte by byte:

```text
$FF43: B9 E9 FF    = LDA $FFE9,Y   (read table at $FFE9, entry Y)
$FF4F: B9 ED FF    = LDA $FFED,Y
$FFBC: BD F1 FF    = LDA $FFF1,X   (read table at $FFF1, entry X)
$FFC1: BD F5 FF    = LDA $FFF5,X
Bytes $FFE9-$FFFF: 07 03 01 00 09 18 10 30 08 04 0C 04 14 18 14 18 00 00 00 00 F0 00 00
```

These are ordinary reads from four 4-byte tables packed just before the vectors. Only 8 KB-and-larger schemes (such as Parker Bros "E0") react to those addresses, and a 4 KB cartridge has no switching circuitry. Together with "no writes to cartridge space" and "no Superchip pattern", the verdict is **no bank switching**. This agrees with Stella's database, which gives no special cartridge type for this ROM (A3), so Stella treats it as plain 4K.

### 3. Code versus data (an estimate)

The script followed the program from $F000 through every branch, jump and subroutine call (`JSR`) it could see, marking each reached byte as code.

```text
Bytes reached as code: 3557 of 4096 (86.8%)
Instructions found: 1818;  subroutines called with JSR: 33
Indirect jumps (cannot be followed): none
Stopped at unknown (non-official) opcodes: none

  $F000-$FD3E   3391  CODE
  $FD3F-$FF42    516  not reached (data or unfollowed code)
  $FF43-$FFE8    166  CODE
  $FFE9-$FFFF     23  not reached (data or unfollowed code)
```

- **Main data block, $FD3F–$FF42 (516 bytes).** Every table address the code reads directly is in this block or in the 16 table bytes at $FFE9 (37 table addresses in all, listed in the script output). So the code reads its data only from the regions the trace found to be "not code". The two results agree, which supports the split.
- **Last 23 bytes:** 16 bytes of tables, 1 zero byte, and the 6 vector bytes.
- **No unused filler.** Script output: `Longest run of one repeated byte anywhere in the ROM: 7 bytes of $00`. The 4 KB looks fully used.
- **No `BRK` in the traced code.** Script output: `BRK instructions in reached code: 0`. This supports the statement above that the IRQ/BRK address is unused.
- **Why it is only an estimate.** 2600 programmers often use a conditional branch that in practice always jumps. A simple tracer also follows the "never taken" path, so it may read some data as code. The true figure for data is therefore probably *higher* than 13%. Graphics reached through pointers (`(zp),Y` reads) are not in the table list above either. Stages F2–F6 produce the exact map.

### Bonus: is this really 2600 display code?

The most frequent writes go to these addresses. Their names come from the standard 2600 chip map, confirmed later in F3:

```text
$02 (WSYNC, "wait for the TV beam to finish this line"): 28 write instructions
$0D/$0E/$0F (PF0/PF1/PF2, the "playfield", i.e. the maze walls): 13 / 15 / 15
$1B/$1C (GRP0/GRP1, the two sprite shapes): 13 / 9
$B6, $BB, $E7, $D2, $D3, $D5 (game RAM): 9-17 each
```

That is the expected signature of beam-racing 2600 code, drawing a maze and sprites.

## Protection (Stage B4, 2026-10-23)

With your approval, three things were done:

1. A new folder, `F:\Ports\ROM-originals\`, was created outside the project.
2. A spare **copy** of the original was put in it, under the same name.
3. Windows' read-only flag was set on the original and on the spare.

The original's contents were never opened for writing successfully, and they are unchanged.

| File | SHA-1 | Size | Last written | Attributes |
| --- | --- | --- | --- | --- |
| `Pac-Man (1981) (Atari).a26` (project folder, **the original**) | `0940FEA7F04CDB6D4B90C5AD1A7E344E68F6DBB1` | 4096 | 1997-08-26 03:17:44 | ReadOnly, Archive |
| `F:\Ports\ROM-originals\Pac-Man (1981) (Atari).a26` (spare) | `0940FEA7F04CDB6D4B90C5AD1A7E344E68F6DBB1` | 4096 | 1997-08-26 03:17:44 | ReadOnly, Archive |
| `rom\pacman.a26` (working copy, A3) | `0940FEA7F04CDB6D4B90C5AD1A7E344E68F6DBB1` | 4096 | 1997-08-26 03:17:44 | Archive (left writable, T-B4-2) |

The protection test opened each file for writing with `FileMode.Open` and `FileAccess.Write`, writing nothing (T-B4-1):

```
REFUSED as expected: UnauthorizedAccessException: Access to the path 'F:\Ports\Pac-Man - Atari 2600\Pac-Man (1981) (Atari).a26' is denied.
REFUSED as expected: UnauthorizedAccessException: Access to the path 'F:\Ports\ROM-originals\Pac-Man (1981) (Atari).a26' is denied.
```

`attrib` view:

```
A    R               F:\Ports\Pac-Man - Atari 2600\Pac-Man (1981) (Atari).a26
A    R               F:\Ports\ROM-originals\Pac-Man (1981) (Atari).a26
```

- **`rom\EXPECTED_SHA1.txt`** holds `0940fea7f04cdb6d4b90c5ad1a7e344e68f6dbb1`. That is one line, lowercase, UTF-8 without a byte-order mark, and ending in a newline. Scripts compare against it case-insensitively. It lives in `rom\`, which stays local (B5 keeps it out of Git), so this file also records the value.
- **To undo the flag**, if ever needed: file Properties → untick "Read-only", or `attrib -R "<file>"`.
- **Not touched:** the other `Pac-Man (1981) (Atari).a26` directly in `F:\Ports` (T-A3-1).
