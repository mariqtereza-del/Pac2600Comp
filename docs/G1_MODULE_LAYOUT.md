# Stage G1: the module layout (2026-10-26)

This is a plan for moving code, not code. Nothing in `src\` changed in G1. It says which file each piece of the working game will live in, so that G2 (conventions), G3 (named state) and G4 (the moves) all aim at the same picture. The layout comes from what Phases F20-F26 built, not from a textbook Pac-Man.

## What is wrong today (measured 2026-10-26)

| File | Lines | Problem |
| --- | --- | --- |
| `src\game\routines.cpp` | 1,329 | Holds 28 unrelated things: scoring, ghosts, sound, the status line, the frame start, the whole bottom logic of the frame (one 480-line struct `Bottom`). |
| `src\game\routines.hpp` | 274 | Declares all of them, plus the cartridge view, the chip classes and the cycle counter. |
| `src\game\kernel.cpp` | 646 | The drawing code of the original in one function. |
| `src\host\main.cpp` | 1,601 | Options, window, pad handling, pacing, tracing and every self-check program in one file. |
| everything else in `src\game` | under 210 | Already small and single-purpose. |

The size limit planned for Phase G is about 400 lines a file. Four files break it.

## Rules the layout follows

1. **One job per file, named for what the game does** (ghosts, maze, scoring), not for where the original kept it (`$F9AA`). The original address stays in the comment of each function (the F8 name too), so the proof trail from `docs\F8_ROUTINES.md` is not lost.
2. **Dependencies point one way:** `data` and `state` at the bottom, then `logic`, then the frame (`game.cpp`), then `picture` and `sound` chips, then the platform (`src\host`). `src\game` never includes anything from `src\host`, and the logic never includes the chip models.
3. **The cycle counter stays where the code is.** The bottom logic counts the original's processor cycles with `cy()` between the lines (F24.2) because the sound samples depend on them. Moving a function must not move, drop or reorder its `cy()` calls. The counter itself gets its own small file.
4. **Original behaviour wins.** If a cleaner shape would change a byte of memory, a pixel or a sound sample, the original shape stays and gets a comment (Phase G guard in `PLAN.md`).

## The layout of `src\game`

```
src\game\
  game.hpp / game.cpp          the frame: top logic, drawing, bottom logic, frame start (exists, stays)
  data\
    cartridge.hpp              the Cartridge view class (moved out of routines.hpp)
    cartridge_data.*           the loader that checks size and SHA-1 (exists)
    rom_tables.hpp             NEW: named addresses of the cartridge tables (maze walls $FE13,
                               score amounts $FF06, speeds $FE6F ...) instead of raw numbers in the code
  state\
    state.hpp                  game::GameState: named fields (G3 done; it sits in src\game until G4 moves it here);
                               the byte mapping is in src\testsupport
    cycles.hpp / cycles.cpp    cy() and gCycles
  logic\
    players.*                  clearScores, swapPlayers, newGameSetup, lives, two-player turns
    maze.*                     newMaze, newMapCheck, wafer and power-pill eating, wall table lookups
    objects.*                  locateObject, moveObject, setDirection, tryDirection, tryPair, the two wraps
                               (what Pac-Man and the ghosts share)
    animation.*                animateObject (openings, leaving the pen, dying)
    pacman.*                   joystick to direction, pacmanKeepGoing, Pac-Man's move in the bottom logic
    ghosts.*                   ghostDecide, ghostUpdate, rngStep (the ghosts are its only user)
    collisions.*               the reactions to the chip's flags: ghost, bonus item, power pill
    scoring.*                  addScore, bcdAdd, bonus item timer and score of the bonus
    flow.*                     game states: title, freeze, eat pause, death, game over, event timer,
                               new game / Select / Reset (the top logic's start part), startUp
    frame_start.*              frameStart (sprite pictures, FRAME_COUNTER, tick counters)
    bottom.*                   bottomLogic: the order in which the parts above run in one frame
    top.*                      topLogic: the same for the first half of the frame
  sound\
    sound_logic.*              soundStart, soundEffects, soundMusic (what to play)
    tia_audio.*                the sound-chip model (exists)
  picture\
    status_bar.*               scoreDigits, livesMarkers (the line under the maze)
    kernel_*.cpp               drawPicture split along its stages (see below)
    renderer.*, tia_video.*    exist, stay
```

`routines.hpp` and `routines.cpp` disappear at the end of G4. Until then they shrink as each module is cut out, so the build never has two copies of a function.

## Where every current function goes

| Now (`routines.hpp`) | Original | Goes to |
| --- | --- | --- |
| `clearScores`, `swapPlayers`, `newGameSetup` | CLEAR_SCORES, SWAP_PLAYERS, NEW_GAME_SETUP | `logic\players` |
| `newMaze` | NEW_MAZE | `logic\maze` |
| `placeObjects` | PLACE_OBJECTS | `logic\flow` (it starts the freeze) |
| `rngStep` | RNG_STEP | `logic\ghosts` |
| `wrapToBottom`, `wrapToTop`, `wrapEnd` | WRAP_* | `logic\objects` |
| `locateObject`, `moveObject`, `setDirection` | LOCATE_OBJECT, MOVE_OBJECT, SET_DIRECTION | `logic\objects` |
| `tryDirection`, `tryPair` | TRY_*, TRY_PAIR_* | `logic\objects` |
| `animateObject` | ANIMATE_OBJECT | `logic\animation` |
| `addScore`, `bcdAdd` | ADD_SCORE | `logic\scoring` |
| `livesMarkers`, `scoreDigits` | LIVES_MARKERS, SCORE_DIGITS | `picture\status_bar` |
| `ghostDecide`, `ghostUpdate` | GHOST_DECIDE, GHOST_UPDATE | `logic\ghosts` |
| `soundStart`, `soundEffects`, `soundMusic` | SOUND_START, SOUND_EFFECTS, SOUND_MUSIC | `sound\sound_logic` |
| `pacmanKeepGoing` | PACMAN_KEEP_GOING | `logic\pacman` |
| `topLogic` | top logic ($F87E-$F9A7) | `logic\top` (new game / Select / Reset part to `logic\flow`) |
| `Bottom::sounds` | L_F7C3 | `logic\bottom` (calls `sound\sound_logic`) |
| `Bottom::newMapCheck` | L_F6AA | `logic\maze` |
| `Bottom::pacman` | L_F61E | `logic\pacman` |
| `Bottom::joystickAndMove` | L_F6CE | `logic\pacman` (the part that eats goes to `logic\maze`) |
| `Bottom::run` | L_F4E7 | split: event timer to `logic\flow`, collisions to `logic\collisions`, order of the parts stays in `logic\bottom` |
| `bottomLogic` | bottom logic | `logic\bottom` |
| `frameStart` | frame start ($F7D6) | `logic\frame_start` |
| `startUp` | start-up ($F000) | `logic\flow` |
| `Cartridge` class | - | `data\cartridge.hpp` |
| `TiaChip`, `SoundChip`, `chip::` names | - | `chips.hpp` (the interface the logic writes to; a small header in `src\game`) |
| `ConsoleInput`, `FrameInput` | - | `game.hpp` (they are what the platform hands the frame) |
| `gCycles`, `cy()` | - | `state\cycles` |

Size check: after the split each file is estimated at 40-300 lines. The largest are `logic\bottom` + `logic\collisions` + `logic\pacman` (the 480-line `Bottom` struct cut in three, about 160 lines each) and `logic\ghosts` (about 200). The kernel (646 lines) is cut along the stages the F18 document names (header and score lines, maze rows, ghost and Pac-Man rows, bottom lines) into 2-3 `picture\kernel_*.cpp` files plus `kernel.hpp`; G4 reads the code and records the exact cut points before moving.

## The layout of `src\host` (the platform)

`main.cpp` (1,601 lines) is cut at the boundaries it already has:

| New file | Takes |
| --- | --- |
| `main.cpp` | `main()` only: reads options, picks window program or a test mode, exits |
| `options.*` | `Options` and the command-line reading |
| `screen.*` | `Screen`, `View`, the window and the drawing of the picture |
| `pads.*` | `Pads` (keyboard and gamepad to the console inputs; `controls.*` stays) |
| `pacer.*` | `Pacer` (the frame pacing steered by the sound queue) |
| `checks.*` | the `--self-check`, `--layout-check`, `--fullscreen-check`, `--pad-check`, `--audio-check` and similar test programs |
| `tracker.*` | `Tracker` and `PickResult` (the trace and fingerprint writer, if its use is shown to be shared by the checks) |

The other host files (`audio`, `controls`, `inputlog`, `layout`, `rom_finder`, `settings`, `trace`, `windowstate`, `native_emulator`, `emulator`) are all under 200 lines and stay as they are.

## Tests and tools

- `src\lockstep` keeps its own copies of nothing: it links the same `src\game` files. Its CMake list changes in G4 with every move. The adapters in `natives.cpp` keep calling the moved functions by the same names, so `docs\F21_ROUTINES.md` (the ledger) stays valid; only its "file" column is updated.
- `ram_map.hpp` is generated by `tools\f20_gen_tables.py` into `src\testsupport` since G3 (the game does not use it).
- `tools\f21_routines.py` and the other proofs that name `src\game\routines.cpp` (a text in `f21_routines.py`) get their paths updated in the same session as each move.

## How G4 works with this layout

Order of the moves (smallest and most self-contained first; after each one: build both programs, `python -I tools\f21_native.py`, `f23_pictures.py`, `f24_sound.py` and `f25_games.py --games 20` must say `result: OK`, and the full 120-game run before the last move):

1. `data\` and `state\` (Cartridge, `rom_tables.hpp`, `cycles`): no logic moves.
2. `logic\players`, `logic\objects`, `logic\animation`, `logic\scoring`, `logic\maze` (leaf routines).
3. `picture\status_bar`, `sound\sound_logic`.
4. `logic\ghosts`, `logic\pacman`, `logic\collisions`, `logic\flow`.
5. `logic\bottom`, `logic\top`, `logic\frame_start`; `routines.*` deleted.
6. `picture\kernel_*`.
7. `src\host` split.

Any red result means the move is undone before going on.

## What G1 decided, and what it left open

- Decided: the folder tree and the home of every function (above), recorded as T-G1-1.
- Left to later stages: naming conventions and comment rules (G2), named fields instead of the 128-byte array (G3), exact cut lines inside the kernel and the `Bottom` struct (found while moving in G4), and whether tiny files (`players`, `scoring`) should be merged if G5 finds them too small to stand alone.
