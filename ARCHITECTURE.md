# Architecture (rewritten in Stage G7, 2026-10-26)

How the finished preservation program is put together. The first version of this page (Stage D1) described a Phase D plan: a window around the Stella emulator core. That plan was carried out, and then in Phases F and G the emulator was replaced by our own game code, proved identical to the original frame by frame. This page describes what exists now.

## 1. Two programs, one game

| Program | What is inside | Used for |
| --- | --- | --- |
| `pacman2600.exe` | window, sound, controls, **our own game code** (`src\game`). No emulator, no cartridge code. | what a player runs |
| `pacman2600_ref.exe` | the same program with the Stella 7.0 emulator core running the original ROM instead of our game | the REFERENCE: golden traces are made with it; never shipped |

Both are built together by `tools\build_host.cmd`. The ROM is not an input of the game. At start-up the program asks for the player's own ROM file and checks only its size (4,096 bytes) and SHA-1 (D-009). A wrong or missing file means the program refuses to start. The game then runs from its own tables.

## 2. The modules

```
                         +--------------------------------------------------+
                         |  src\host   the Windows program                  |
                         |  main - options - rom_choice - rom_finder        |
                         |  play_window - headless - window_checks - pads   |
                         |  screen - layout - window_tracker - windowstate  |
                         |  audio - pacer - controls - inputlog - trace     |
                         |  menu - menu_session - menu_view - menu_check    |
                         |  settings - common - ntsc_palette                |
                         |  native_emulator (our game)  emulator (Stella,   |
                         |  reference program only)                         |
                         +----------------------+---------------------------+
                                                | one call per frame: Game::runFrame(inputs, chip)
                                                v
 +-----------------------------------------------------------------------------------------------+
 |  src\game   the game                                                                           |
 |                                                                                               |
 |   game            runs one frame:  top logic -> drawing code -> bottom logic -> frame start   |
 |                                                                                               |
 |   logic\   the rules                       picture\ + kernel   the drawing code               |
 |     top  bottom  frame_start                 kernel (drawPicture)  kernel_internal            |
 |     pacman  ghosts  objects  animation       kernel_positions  kernel_maze  kernel_score      |
 |     maze  scoring  collisions  flow          status_bar                                       |
 |     players                                                                                   |
 |   sound\sound_logic   which sound plays when                                                  |
 |                                                                                               |
 |   renderer   turns the drawing code's timed writes into the picture (tia_video)               |
 |   layers     the picture twice: as the game makes it, and without Pac-Man and the ghosts (I9.1) |
 |   objects_view  where each moving object is in the picture; lookahead  the next frames (I9.2)  |
 |   smooth     the moving objects glided between their places, with no delay (I9.3)         |
 |   tia_video  the video chip's behaviour:  writes in, 160 x 228 picture out                    |
 |   tia_audio  the sound chip's behaviour:  writes in, samples out                              |
 |   chips      the interfaces the game talks to      inputs   what the game is told each frame  |
 |                                                                                               |
 |   state\  state (the 128 bytes of the original, as named fields)   cycles (cy(n) bookkeeping) |
 |   data\   tables (508 bytes, 34 tables, generated)     rom_check (size and SHA-1, nothing else)|
 +-----------------------------------------------------------------------------------------------+
```

**The one-way rule.** A box may use the boxes below it and never those above: `data` and `state` use nothing of the game; `logic`, `sound_logic` and `picture` use `state` and `data`; `game` uses the logic and the drawing code; `renderer`, `tia_video` and `tia_audio` are the chip models that receive the writes; the host calls `game` and the chip models and knows nothing of the rules inside. Only `rom_check.cpp` opens a file (`tools\g7_gate_check.py` checks that).

## 3. What happens in one frame

`Game::runFrame(inputs, chip)` (`game.cpp`), 262 scanlines of the original:

1. **Top logic** (`top`): reads the joystick and switches, moves Pac-Man (`pacman`) and the ghosts (`ghosts`, `objects`, `animation`), starts or ends a game (`flow`, `players`).
2. **Drawing code** (`kernel`, `kernel_positions`, `kernel_maze`, `kernel_score`, `status_bar`): the original draws the picture line by line by writing to the video chip at exact moments. `drawPicture` makes the same timed writes from the state and the tables.
3. **Chip models**: `renderer` hands those writes to `tia_video`, which gives the 160 x 228 picture and the collision flags; the sound writes of the game go through `tia_audio`, which gives the samples.
4. **Bottom logic** (`bottom`, `collisions`, `scoring`, `maze`, `sound_logic`): reads the collision flags, eats wafers and ghosts, adds score, starts sounds, handles the timers (`flow`).
5. **Frame start** (`frame_start`): the next frame's counters and the video sync.

The functions are not in the order of the 6502 code. The order of every step that touches state or chip is the original's. The `cy(n)` calls (`cycles`) count the original's processor cycles along the path taken, because the sound writes must happen at the right moment inside the frame. Reading a rule, ignore them.

## 4. The state and the tables

- `state` holds the original's 128 bytes of memory as named fields (`GameState`): exactly the original's layout, because the proofs compare it byte for byte after every frame. The names, bit meanings (`kObj...`, `kPill...`) and the two player blocks are described in the header.
- `tables` (generated by `tools\g5b_make_tables.py`) holds the 34 tables (508 bytes) of maze, shapes, colours, sounds, speeds and scores. It is the list in `docs\EMBEDDED_DATA.md`, and Rule 7 allows exactly these bytes. No program code of the cartridge is in the program, the repository or a release. A read outside a table stops the program with a message.
- `rom_check` is the only code that reads a ROM file, to check size and SHA-1 (an indirect copy protection: no legal dump, no start).

## 5. The host program

| Part | Job |
| --- | --- |
| `main`, `options` | command line, what to run |
| `rom_choice`, `rom_finder` | find the player's ROM and run the check (D3, D-009) |
| `play_window`, `screen`, `layout`, `window_tracker`, `windowstate`, `settings` | the window, the picture modes, fullscreen, remembered position, `settings.ini` |
| `audio`, `pacer` | sound output; the game runs at the console's 59.9227 frames a second, steered by the sound card |
| `controls`, `pads`, `inputlog` | keyboard, game pad, recordings and replays |
| `gpu_pass`, `filter_check` | our own pixel shaders on SDL's Direct3D 11 renderer (Stage I11, D-017): reading the renderer's device and the textures behind SDL's textures, running an HLSL pixel pass from one SDL texture into another, and putting SDL's Direct3D state back afterwards (`gpu_pass`); the check on the real renderers (`filter_check`) |
| `hdr`, `hdr_check`, `render_setup` | HDR output (Stage I10, D-016): when HDR is used, the ceiling of the highlights and the slider arithmetic, no window code (`hdr`); making the renderer in the standard or the HDR colour space and asking Windows about HDR (`render_setup`); the report and the picture check on the real screen (`hdr_check`) |
| `framerate`, `smooth_check`, `key_pusher` | the frame rate of the picture (Stage I9.4, D-015): the six choices, the moments pictures are made and how far through a game step they are, with no window code (`framerate`); the check that shows the smooth picture in the real window (`smooth_check`); the test-only helper that presses keys in the window's event queue, cut out of `play_window` (`key_pusher`) |
| `background`, `backdrop`, `backdrop_check` | what fills the space around the picture (Stage I7, D-012): the choices, colours and small calculations with no window code (`background`); the drawing of the glow, mirror, gradient, arcade panel and TV bezel (`backdrop`); the check that draws every one in the real window (`backdrop_check`) |
| `menu`, `menu_session`, `menu_view`, `menu_check` (with its header `menu_checks`) | the Escape menu (Stage I5): its choices and explanations, the Select and Reset presses that start a chosen game (`menu`, no window code); the part the window thread and the game thread share, which stops the game while the menu is open (`menu_session`); drawing and the picture test (`menu_view`); the menu-and-game check (`menu_check`; Stage I6 added the two-player scenarios and the turns played on one set of controls) |
| `headless`, `window_checks`, `trace` | window-less runs and the self-checks used by the tests; the per-frame trace |
| `native_emulator` | the adapter between the host and `game::Game` |
| `emulator` | the same adapter over Stella (reference program only) |
| `common`, `ntsc_palette` | shared helpers; the colour table made by `tools\f26_palette.py` |

## 6. The test side

| Where | What |
| --- | --- |
| `src\lockstep` | the lockstep tool: runs the original 6502 code and our game side by side and compares all 128 bytes, the picture and the sound after every frame (`lockstep.exe`, `lockstep_native.exe`) |
| `src\testsupport` | the original's address names and the byte conversion of the state, used only by tests |
| `tests\game`, `tests\host` | unit tests (tables, ROM check, chip models, drawing code, controls, layout, recordings, window state); `tests\game\data` holds fingerprint files that let a test replay whole games with no ROM |
| `tools` | the proof scripts of every stage (`f..._*.py`, `g..._*.py`), build scripts, the gate checks |
| `baseline` | the 70 recorded games (inputs and descriptions; the traces stay local) |

The proofs run against the REFERENCE program and the local ROM; they are test tools and are never part of a release.

## 7. Licences in short

SDL3 (zlib) is in `third_party` and linked into the window program. Stella 7.0 (GPL-2.0-or-later) is in `third_party` and linked only into the reference program; the release holds the source (see `docs\DEPENDENCIES.md`). The Microsoft runtime files in a release come from the Visual Studio Build Tools. Our own code carries the project's licence.

## 8. Where the details are

`docs\GATE_G.md` (the gate checklist), `docs\EMBEDDED_DATA.md` (the tables), `docs\CONVENTIONS.md` (the rules for the source), `docs\F20_LOCKSTEP.md` and `docs\F25_LOCKSTEP.md` (the proofs), `docs\G1_MODULE_LAYOUT.md` (why the modules are cut this way), `DECISIONS.md` (every choice), `docs\GLOSSARY.md` (terms).
