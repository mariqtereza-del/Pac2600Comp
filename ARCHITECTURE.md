# Architecture (Stage D1, 2026-10-24)

What the program is made of, who wrote each part, and what changes in Phase 2. It describes the **plan**; each part is marked with the stage that builds it. Facts about the game come from `docs\` (measured), not from memory.

## 1. The three parts of the Phase 1 program

| Part | Whose code | What it does | Where it lives |
| --- | --- | --- | --- |
| **Host** | Ours (new, Phase D) | Window, fullscreen, correct picture shape, sound output, keyboard and pad, frame pacing, ROM finding, recording of inputs | `src\host\` |
| **Emulation core** | Stella team, GPL v2 (vendored, never edited) | Pretends to be the 2600's chips: the 6507 processor, the TIA (picture and sound) and the RIOT (RAM, timer, switches, joystick ports) | `third_party\stella\src\` |
| **The game** | Atari, 1982 (the player's own ROM) | Every rule, picture and sound. Never inside our repository or release | `rom\pacman.a26` locally |

The program is honestly an **emulator front-end**: our code for everything around the game, emulation for the game itself.

## 2. How the parts connect

```
 keyboard / pad  -->  Host input  -->  switches + joystick events
                                              |
 your .a26 file  -->  Host ROM loader --> Stella core (6507 + TIA + RIOT) runs the ROM
                                              |
              one call per game frame ("run a frame")
                                              |
        picture (160 x 228 indexed pixels)  +  audio samples  +  128 bytes of RAM
                 |                                  |                      |
        Host renderer (SDL3)               Host sound (SDL3)       tests and recordings only
```

- **Link style (T-D1-1):** the host links Stella's core as a static library (`stella_core`, built by `third_party\CMakeLists.txt`) and calls Stella's own libretro wrapper class `StellaLIBRETRO` directly. The measuring tools of Phase C already work this way. The `stella_libretro.dll` also builds, but is not needed.
- **What the wrapper gives us** (the probes in `tools\stella_probe` already use these): load a ROM from memory (`setROM`), create the console, run one frame (`runFrame`), read the picture (`tia.frameBuffer()`, 160 wide), set joystick and switch events (`setInputEvent`), and read the 128 bytes of RAM (`m6532().getRAM()`). Saving and loading emulator state is used by `stella_bot`.
- **One direction only:** the host tells the core what the player does and asks for a frame. The core never calls into host code.

## 3. File by file (planned)

| File or folder | Owner | Stage | Job |
| --- | --- | --- | --- |
| `src\host\main.cpp` | Ours | D2 (built) | Starts SDL3, makes the window, runs the main loop, closes cleanly. Also `src\host\CMakeLists.txt` and `tools\build_host.cmd` |
| `src\host\rom_finder.*` | Ours | D3 (built) | Finds the ROM (command line, saved path, `roms` folder, file dialog) and checks size and SHA-1 `0940fea7…dbb1`. Tests: `tests\host\test_rom_finder.cpp` |
| `src\host\emulator.*` | Ours | D4 | Thin wrapper around `StellaLIBRETRO`: load, run frame, picture, RAM, state. The only host file that includes Stella headers |
| `src\host\video.*` | Ours | D5-D7 | Palette to colours, picture to window, correct shape (12:7 to be confirmed), whole-number scaling, fullscreen |
| `src\host\audio.*` | Ours | D8 | Core audio samples to an SDL3 audio stream with a small steady buffer |
| `src\host\pacing.*` | Ours | D9 | Advances the game at its own measured rate, not the monitor's (60.000 Hz vs 59.923 Hz is decided in D9) |
| `src\host\input.*` | Ours | D10-D12 | Keyboard, PS4 pad, Select/Reset, difficulty and Colour/B&W switches |
| `src\host\recorder.*` | Ours | D13 | Records every frame's input to an `.inputs` file and plays it back; picture fingerprint per frame |
| `tools\stella_probe\*.cpp` | Ours | C3-C7 | Measuring tools (already exist). Reused by tests |
| `third_party\SDL3\` | SDL team, zlib licence | B7 | Window, sound, controllers (vendored, never edited) |
| `third_party\stella\` | Stella team, GPL v2 | B7 | The emulation core (vendored, never edited; build-time fixes only in `third_party\CMakeLists.txt`, T-B7-4) |
| `rom\`, `*.a26` | Atari | A3 | **Never** committed, packaged or published (Rule 7) |

Stella parts that matter most (all under `third_party\stella\src\`): `emucore\M6502.cxx` (processor), `emucore\tia\TIA.cxx` and its Player, Missile, Ball, Playfield and Audio files (picture and sound), `emucore\M6532.cxx` (RAM, timer, ports), `emucore\Cart4K.cxx` (the plain 4 KB cartridge this game is), and `os\libretro\StellaLIBRETRO.*` (the wrapper we call).

## 4. Who decides what, in Phase 1

| Behaviour | Decided by | Evidence |
| --- | --- | --- |
| Rules, score, ghosts' behaviour, mazes, difficulty, bonus item, game variations | **The ROM** | `docs\CONTROLS.md`, `docs\CAPTURES.md` |
| 262 lines per frame, 19,912 CPU cycles per frame, drawn lines 34-222, 160 columns | **The ROM**, counted by the core | `docs\DISPLAY.md`, `docs\TIMING.md` |
| The three sounds (start tune, scoring blip, death) as TIA register writes | **The ROM** | `docs\SOUND.md` |
| What those register writes become as samples and pixels | **The Stella core** | `docs\SOUND.md` (audio format), `docs\DISPLAY.md` |
| Frame rate shown (60.000 vs 59.923 Hz) | **The host** (choice in D9) | `docs\TIMING.md` |
| Window, scaling, fullscreen, controls mapping, volume | **The host** | D5-D12 |
| Phosphor blending of flicker | **The host** (choice in D6) | `docs\DISPLAY.md` |

Nothing in the host may change a game rule (Rule 2). Host features that alter the experience are modern features and stay switchable and separate (Rule 3).

## 5. What Phase 2 replaces

| Today (Phase 1) | Phase 2 | Stays |
| --- | --- | --- |
| Stella core runs the ROM | Native C++ game logic (`src\game\`), written from the reverse-engineered ROM (Phases F-G) | The whole host |
| Stella's TIA draws the picture | A native renderer producing the same pixels | Window, scaling, fullscreen |
| Stella's TIA makes the sound | A native TIA-style sound generator producing the same samples | Audio output |
| The ROM supplies the game | The player's ROM is still read at start-up for maze shape, sprites, colours and sound tables, so our code contains logic only, not Atari's art | ROM finder and checks |
| - | Lockstep tests: original (core) and rewrite run on the same recorded inputs, comparing the 128 RAM bytes, picture and sound each frame | The Phase 1 build as a separate test tool and reference forever |

When every recorded frame matches, the core is removed from the release build. It remains in the test tool. Until then the program is the emulator front-end described above.

## 6. Licences in short

- Our code: licence chosen at J1.
- SDL3: zlib, free to use.
- Stella: GPL v2. Any release containing it must also offer complete source under the GPL (`docs\DEPENDENCIES.md`). The native build later will not contain Stella.

## 7. Not decided yet

- Exact picture pixel shape (12:7 is the standard value; D6 confirms against Stella and `reference\screens`).
- Pacing rate (D9) and flicker presentation (D6).
- Whether the `.inputs` format is shared with the test tools' script format (D13).
