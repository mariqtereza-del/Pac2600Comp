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

E2 start-up and variations; E3 Pac-Man's movement; E4 ghosts; E5 eating and scoring; E6 dying and game over; E7 your three long games; E8 edge cases. Gate E needs at least 30 entries, each with a trace and a description you could check against the real game.

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
