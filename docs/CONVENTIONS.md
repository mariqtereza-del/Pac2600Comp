# Coding and comment conventions (Stage G2, 2026-10-26)

These rules apply to everything under `src\game` and, from G4, to the moved files and the split `src\host`. They describe the style the code already has, with the missing rules added (size, the `ORIGINAL:` tag). The layout they apply to is `docs\G1_MODULE_LAYOUT.md`. `python -I tools\g2_conventions_check.py` measures them; G2 only reports, G7 requires it clean.

## 1. Names

- **Functions and variables: `camelCase`, named for what the game does.** `ghostDecide`, `moveObject`, `addScore`, never `sub_F3A2` or `routine7`. Types: `PascalCase`. Constants and the memory names of `ram::`: `UPPER_SNAKE` (the names found in F7).
- **The original's address and F8 name go in the comment, not the name:** `// F21.8  MOVE_OBJECT ($F9EE): ...`. That is the trail back to `docs\F8_ROUTINES.md`.
- **Say the game word.** "wafer" (the small dot), "power pill", "bonus item", "eyes" (a ghost returning to the pen), "pen" (the ghost house), "scared". Words come from `docs\GLOSSARY.md`; a new word goes there.
- **No names that mean "this was cleaned up".** No `fixed`, `new`, `improved`, `v2`.
- **Cartridge tables get a name once** in `data\rom_tables.hpp` (from G4) with the address and size in a comment; code uses the name, not `0xFE13`.
- **Numbers with a meaning get a name or a one-line comment** (`0x58` is the pen door's x: say so). Bit masks of the state bytes get names when the same bit is tested in more than one place.

## 2. Size

- **No file over 400 lines**, no function over about 80 lines. A function the original wrote as one long routine may be longer only if it is cut by comments into labelled blocks that follow the listing (`// L_F6AA: the new map ...`, as `Bottom` does today). The limit is checked; the exception is listed in G5/G7 with its reason.
- **Lines up to 150 characters**; the checker reports lines over 160.
- **One job per file**, named for the job (`ghosts.cpp`, not `util.cpp`). A `.hpp` declares only what other files use.

## 3. Comments

1. **Every function starts with one comment line of what and why**, in the header or just above the definition, in plain words: `// Steps the game's only random number and says whether the low bits are zero (the ghosts ask).` After it, as many lines as the routine needs, in the style of `routines.hpp` today: original address, which bytes it reads and writes, what it returns for the adapters.
2. **Comments say why, not what the next line does.** "The pause stops the timer" is useful; "increment i" is noise. Do not narrate code that reads clearly.
3. **Every claim about the original cites its evidence** with the stage and document: `(F13, docs\F13_GHOSTS.md)`. A fact that is only believed gets `// LIKELY:`; a literal copy not yet understood gets `// LITERAL: not yet understood` (Phase F rule; Gate G wants none left).
4. **Cycle counts:** `cy(n)` lines keep a short comment naming the original instruction group (`// LDX ATTRACT_SHIFT`). They are never merged, moved or removed in a clean-up (F24.2): the sound depends on them.
5. **Comments are English, plain words**, no jokes, no names of people, no "TODO" without a stage number (`// TODO(G5): ...`).

## 4. The `ORIGINAL:` tag

A quirk is behaviour that a clean redesign would not have: an error, an odd order, a limit nobody chose. Each one carries a comment, on the line that does it, in this exact form:

```cpp
// ORIGINAL: <what it does that looks wrong>. Evidence: docs\F13_GHOSTS.md (F13). Do not fix.
```

- The `Evidence:` path must exist (the checker tests it). The evidence document is the `docs\F<n>_*.md` of the stage that proved it, or `DECISIONS.md` with a `T-…` number.
- `RE_NOTES.md`, named in the first plan text, does not exist. The F-stage documents are the notes, and each is written by its proof tool, so they are better evidence than a second file that could drift. (T-G2-1)
- **Fixing an `ORIGINAL:` quirk is a rule change** and needs the owner's approval (CLAUDE.md Rule 2). Preservation code never does it; a modern option would be a separate switch (Rule 3).

Known quirks and where they are tagged (Stage G5: 8 of the 9 carry an `ORIGINAL:` comment; the list is a start, not complete):

| Quirk | Where it lives | Evidence |
| --- | --- | --- |
| Scared ghosts aim at a mirrored x but at Pac-Man's own y | `ghostDecide` | `docs\F13_GHOSTS.md` |
| The four TRY routines and the TRY_PAIR helpers drop their caller's return | `tryDirection`, `tryPair` | `docs\F8_ROUTINES.md` (column "Drops caller's return"), `src\game\logic\objects.hpp` (F21.12) |
| Only five score digits are shown; the hundred-thousands digit never | `Kernel::scoreLine` | `DECISIONS.md` (T-F21-21) |
| One ghost per frame is drawn (flicker); its collision flag only on 4 frames of 8 | `top.cpp`, `collisions.cpp` | `docs\F18_KERNEL.md`, `docs\F12_COLLISIONS.md` |
| No extra life for a score; lives only at a new map | scoring | `docs\F15_SCORING.md` |
| A ghost that wanders can fall into a path no recording ever runs (`ghostDecide` returns false) | `ghostDecide` | `src\game\logic\ghosts.hpp` (F21.15) |
| The level changes only ghost speed and the chase timer; scared time does not depend on it | ghosts, flow | `docs\F16_LEVELS.md` |
| The fire button works only about 500 frames after game over | not yet located in the code, so **not tagged** (G5) | `DECISIONS.md` T-E6-1 |

## 5. Code style

- C++23, as in the build (`CLAUDE.md`, T-B3-2). Two-space indent, braces as in the surrounding code. No exceptions in `src\game`.
- **`src\game` has no I/O, no platform and no emulator**; it takes input structures and writes to chip interfaces. It never includes from `src\host` and never holds cartridge bytes as data (F22 rule, checked by `tools\f22_loader.py`).
- No new dependency without explaining it (Rule 5). No global state except the cycle counter, which moves to `state\cycles` and stays global because the original's timing is global.
- Every change to `src\game` must keep lockstep green: `tools\f21_native.py`, `f23_pictures.py`, `f24_sound.py` and `f25_games.py` say `result: OK`. A red result means undo (Phase G rule).
- Clean-ups never change a byte of memory, a pixel or a sound sample. If they would, keep the original shape and add a comment.

## 6. Checking

`python -I tools\g2_conventions_check.py` scans `src\game`, `src\host` and `src\lockstep` and prints: files over 400 lines, functions over 80 lines, lines over 160 characters, count of `ORIGINAL:` tags, and every `ORIGINAL:` tag whose evidence path does not exist. Exit 0 always in G2 (the code is not moved yet); `--strict` exits 1 on any finding and is used in G7.
