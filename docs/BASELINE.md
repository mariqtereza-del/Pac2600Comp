# Baseline: what a test entry is (Stage E1, 2026-10-26)

The baseline is a library of recorded games, each paired with exactly what the original did. In Phase F the rebuilt game (our own code, no emulator) is run on the same inputs and must give the same memory, the same picture and the same sound on every frame. One entry is one such recorded game.

## An entry

| Part | Where | In Git? | What it is |
| --- | --- | --- | --- |
| ID | `BL-001`, `BL-002`, ... | yes | Counts up; never reused. |
| Entry text | `baseline\BL-001.entry.txt` | yes | `id`, `title`, `shows` (one sentence), `inputs` (the input file), `frames`, `rom_sha1`, `trace_sha256`, `trace_bytes`, then `observations:` in plain words. |
| Input | `baseline\BL-001.inputs` | yes | The recording file of Stage D13: what the joystick and switches did on every frame, from power-on. It holds no game data. |
| Golden trace | `reference\baseline\BL-001.trace` | **no, local only** | What the original did on every frame (below). It carries Atari's memory contents, so it stays on this PC (`reference\` is ignored by Git); only its SHA-256 is in the entry text. |

**Observations** are written so you could check them against the real game: frame numbers counted from power-on, what the screen shows, which memory byte changes (named by address, with the working labels of Stage C7, not yet confirmed), and how to see it yourself. Anything not measured is not written.

## The golden trace

Made by the host, with no window (`pacman2600.exe --headless <png> <rom> --replay <inputs> --trace <file>`), or in the window (`--replay` or `--record` with `--trace`). Plain text:

```
PacMan2600-trace 1
rom_sha1=<40 hex digits>
columns=frame,input,switches,picture,audio,ram
<frame>,<input>,<switches>,<picture>,<audio>,<ram>        one line per frame, frame 0 first, no gaps
```

| Column | Meaning |
| --- | --- |
| `frame` | Number of the frame, counted from power-on. |
| `input` | The console input given on that frame (`controls::Input::pack`, decimal): up 1, down 2, left 4, right 8, fire 16, select 32, reset 64, left difficulty A 128, right difficulty A 256, B&W 512. |
| `switches` | The console switch register as the game could read it (2 hex digits; `3f` = nothing pressed, Reset low gives `3e`). |
| `picture` | Fingerprint of the picture the chip drew: 160 x H palette numbers. The same fingerprint as Phase C. |
| `audio` | Fingerprint of the frame's sound: 16-bit stereo bytes exactly as the emulator made them. Frame 0 has none (the fingerprint of nothing, `14650fb0739d0383`). |
| `ram` | All 128 bytes of the console's memory after the frame, 256 hex digits. |

The fingerprint is 64-bit FNV-1a with the start value `1469598103934665603`, which is one digit short of the textbook value; every fingerprint since Phase C uses it, so it is kept (T-E1-1).

## Making and checking entries

```
python -I tools\baseline.py make  rom\pacman.a26 BL-001   (replays twice, requires byte-identical traces, stores the SHA-256 in the entry)
python -I tools\baseline.py check rom\pacman.a26 BL-001   (replays again and compares with the stored SHA-256)
python -I tools\baseline.py list
```

If `check` ever says DIFFERS, the program no longer behaves as it did when the entry was made: stop and find out why before changing anything else.

## How Phase F uses it

The lockstep tool (F20) runs the emulator and the rebuilt code on one `.inputs` file and compares, frame by frame, the 128 memory bytes, the picture and the sound with the golden trace. The first difference is reported with the frame, the byte and both values.

## Proof that the format is trustworthy (Stage E1)

- The same recording traced twice gives byte-identical files (BL-001: 700 frames, 210,016 bytes, SHA-256 in the entry).
- The window trace of BL-001 equals the no-window trace on all 700 frames (all five data columns, so picture and sound too).
- The trace of the 9,800-frame bot route equals the independent Phase C reference run of Stella's core on all 9,800 frames: memory (128 bytes), picture fingerprint, and the three switch positions.
- The sound column is the fingerprint of the same bytes the WAV file of Stage D8 holds, which is identical to the Phase C recording.

## Plan for the entries (Phase E)

E2 start-up and variations (done); E3 Pac-Man's movement (done); E4 ghosts; E5 eating and scoring; E6 dying and game over; E7 your three long games; E8 edge cases. Gate E needs at least 30 entries, each with a trace and a description you could check against the real game.

## Entries so far

| ID | Frames | Title |
| --- | --- | --- |
| BL-001 | 700 | Game Reset starts a game; nobody plays; Pac-Man is caught |
| BL-002 | 3000 | Power-on and the attract mode, nothing touched |
| BL-003 | 1400 | Game Select steps through the 16 selections |
| BL-004 | 1400 | Game 1, 1 player: Select, then Reset starts it |
| BL-005 | 1400 | Game 1, 2 players: Select, then Reset starts it |
| BL-006 | 1400 | Game 2, 1 player: Select, then Reset starts it |
| BL-007 | 1400 | Game 2, 2 players: Select, then Reset starts it |
| BL-008 | 1400 | Game 3, 1 player: Select, then Reset starts it |
| BL-009 | 1400 | Game 3, 2 players: Select, then Reset starts it |
| BL-010 | 1400 | Game 4, 1 player: Select, then Reset starts it |
| BL-011 | 1400 | Game 4, 2 players: Select, then Reset starts it |
| BL-012 | 1400 | Game 5, 1 player: Select, then Reset starts it |
| BL-013 | 1400 | Game 5, 2 players: Select, then Reset starts it |
| BL-014 | 1400 | Game 6, 1 player: Select, then Reset starts it |
| BL-015 | 1400 | Game 6, 2 players: Select, then Reset starts it |
| BL-016 | 1400 | Game 7, 1 player: Select, then Reset starts it |
| BL-017 | 1400 | Game 7, 2 players: Select, then Reset starts it |
| BL-018 | 1400 | Game 8, 1 player: Select, then Reset starts it |
| BL-019 | 1400 | Game 8, 2 players: Select, then Reset starts it |
| BL-020 | 1400 | The fire button starts a game from the attract mode |
| BL-021 | 1500 | Game Reset pressed at frame 600 of the attract mode |
| BL-022 | 1900 | Game Reset pressed at frame 1000 of the attract mode |
| BL-023 | 2300 | Game Reset pressed at frame 1400 of the attract mode |
| BL-024 | 2700 | Game Reset pressed at frame 1800 of the attract mode |
| BL-025 | 3300 | Game Reset pressed at frame 2400 of the attract mode |
| BL-026 | 560 | Pac-Man walks left to the wall and back right to the other wall |
| BL-027 | 560 | Pac-Man turns down the first side corridor, then goes up and down it |
| BL-028 | 520 | A turn asked for early and held is taken at the corner |
| BL-029 | 480 | A turn asked for only briefly is not remembered |
| BL-030 | 480 | Pac-Man reverses at once |
| BL-031 | 500 | Joystick held diagonally: LEFT and DOWN together |
| BL-032 | 500 | Joystick held diagonally: LEFT and UP together |
| BL-033 | 500 | Joystick held diagonally: RIGHT and DOWN together |
| BL-034 | 760 | DOWN pressed in the bottom corridor under the middle of the maze, and held |
| BL-035 | 9800 | Computer play: the whole first level by the search bot |
| BL-036 | 760 | UP pressed just after Pac-Man has come out at the top of the maze |
| BL-037 | 760 | UP pressed while the pause flag of the bottom opening is set |

## Pac-Man's movement, in short (Stage E3)

Measured in BL-026 to BL-037 (`DECISIONS.md` T-E3-1; all numbers come from the traces by `tools\e3_entries.py`): Pac-Man (bytes $B1 = x, $B6 = y) moves one pixel at a time, never diagonally, along corridors at y = 0, 10, ... 70 and 11 vertical corridors; about 0.57 pixels per frame (2 frames per step, 1 frame about every fourth step). He stops at a wall, goes on after the stick is released, remembers a turn that is held until the corner but not a 2-frame tap, and reverses 2 frames after the press. The openings are in the top and bottom walls in the column x = 76 (there is no side tunnel); using one sets the pause flag for 84 frames without losing a life. What the game does inside those 84 frames is not yet understood.
| BL-038 | 800 | Ghosts from the start, Pac-Man stands still |
| BL-039 | 800 | The same start, Game Reset one frame later |
| BL-040 | 800 | The same start, Game Reset 60 frames later |
| BL-041 | 800 | Pac-Man walks left 12 pixels and stands still |
| BL-042 | 800 | The same walk left, one frame later |
| BL-043 | 800 | Pac-Man walks left, down and right along the bottom corridor |
| BL-044 | 4000 | Computer play: the first power pill and the first ghosts eaten |

## The ghosts, in short (Stage E4)

Measured in BL-038 to BL-044 (`DECISIONS.md` T-E4-1, computed by `tools\e4_entries.py`): ghost i has x in byte $B2+i and y in $B7+i; each changes place once every 4 frames (frame number mod 4 = i), by 1 or 2 pixels; all four start at (76,30). Their routes do not depend on Pac-Man for the first 126 frames of play, and do not change with the moment Game Reset is pressed or with the input being a frame late: nothing random was found. An idle Pac-Man is caught after 297 frames. Not yet identified: the scared state, the return of eaten ghosts, the drawing flicker order (Phase F).
| BL-045 | 600 | Eating wafers: left, down and right along the first corridors |
| BL-046 | 9800 | Every score value in a whole computer-played level, and the status bar |
| BL-047 | 7500 | The end of the first level: the last wafer, the extra life, the refilled maze |

## Eating and scoring, in short (Stage E5)

Measured in BL-045 to BL-047 (`DECISIONS.md` T-E5-1, computed by `tools\e5_entries.py`): score = bytes $CC (tens, ones) and $CE (hundreds); wafer +1 (one bit of $88-$97 cleared), power pill +5, ghosts eaten +20, +40, +80, +160 within one power pill; the last wafer of a level refills the map, adds a life and pauses 120 frames. The digits in the status bar equal the memory score (155 frames checked). Not yet covered: the bonus item eaten, an extra life by score (E7, E8).
| BL-048 | 3000 | An idle game: three lives lost, game over, the fire button starts a new game |
| BL-049 | 9800 | Deaths in a computer-played game and the game over |

## Dying and game over, in short (Stage E6)

Measured in BL-048 and BL-049 (`DECISIONS.md` T-E6-1, computed by `tools\e6_entries.py`): the pause flag comes on at the catch, the life is taken 125 or 126 frames later, the pause ends 245 or 246 frames after it began (126 for the last life), Pac-Man returns to (76,60) and the ghosts to (76,30). After game over the ghosts walk again from 120 frames later; the fire button starts a new game only after a delay (about 500 frames in the idle game).
