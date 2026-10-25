# Decisions

Numbered decisions (D-001, D-002…) are the ones you approve. As the plan says, D-001 is reserved for the toolchain (A5) and D-002 for the Stella version (C1).

Small technical choices Claude Code makes inside a stage go in the second list (T-…). Each one is low-risk and can be reversed.

## Approved decisions

### D-001 — Toolchain (2026-10-23, Stage A5)

**Your choice (answers given in the A5 session):**
- Compiler: "Install Microsoft's"
- Git: "Install Git for Windows (Recommended)"

**Claude Code's recommendation was** to reuse the MSYS2 compiler already on the PC and install Git for Windows. You chose Microsoft's compiler instead, which is the roadmap's original plan. Both options were workable.

**The approved tool list:**

| Part | Choice | Version (as of 2026-10-23) | Installed when | Source of the version |
| --- | --- | --- | --- | --- |
| Language | C++ (C++23 edition) | — | — | Stella's current libretro Makefile uses `-std=c++23` / `/std:c++23` |
| Compiler | Microsoft C++ (MSVC), via **Visual Studio 2026 Build Tools**, workload "Desktop development with C++" | MSVC 14.51 is the current release (GA 12 May 2026); B2 records the exact version installed | Stage B2, by you | [MSVC 14.51 announcement](https://devblogs.microsoft.com/cppblog/msvc-version-1451-available/) |
| Build tool | CMake + Ninja **as included in the Build Tools** | recorded at B2 | Stage B2 (comes with the workload) | — |
| Git | **Git for Windows**, 64-bit | 2.55.0 line (latest build recorded at B1) | Stage B1, by you | [git-scm.com Windows page](https://git-scm.com/install/windows) |
| Window/sound/controller library | **SDL3**, source kept in `third_party\` | 3.4.18 is the latest stable release; B7 pins the exact version | Stage B7 (download, approved by you) | [SDL releases](https://github.com/libsdl-org/SDL/releases) |
| Emulator core | **Stella's own libretro port** (`src/os/libretro` in the Stella source), at the **same version** as the reference Stella app (D-002, stage C1) | Latest Stella release: 7.0 | Stage B7 (download) | [Stella releases](https://github.com/stella-emu/stella/releases), [libretro folder](https://github.com/stella-emu/stella/tree/master/src/os/libretro) |
| Helper scripts | **Python 3.14.0** already installed, standard library only; any extra Python package needs your approval | 3.14.0 | already present | `docs\ENVIRONMENT.md` |
| Assembler (F1) | dasm, portable | chosen at F1 | Stage F1 (download, approved by you) | — |

**Reasons:**
- **Emulator core changed from the roadmap.** The roadmap named `stella2014-libretro`, a copy of Stella frozen in 2014. Stella's own source now contains a libretro version. Using it at the same version as the reference app means our program and the "ruler" we compare against run the same emulator. That makes the comparisons in D14 and Phase E trustworthy.
- **Microsoft compiler (your choice).** It is the most standard compiler on Windows, Stella's own Windows builds use it, and it comes with a debugger. It costs a one-time install of about 6–8 GB; F: has 86.7 GB free (A2).
- **Git for Windows.** Includes the browser sign-in for GitHub (needed in Phase J) and is what Claude Code expects on Windows.
- **MSYS2 is not used by this project.** It stays on the PC untouched, for whatever else uses it.

**Alternatives considered:** MSYS2's GCC 16.2.0 / CMake 4.4.3 / Ninja 1.13.2 (already installed, no download; risk: shared with other projects, updates can change the compiler) and MSYS2's Git 2.55.0. Not chosen.

**Installed versions (notes added as each tool arrives):**
- **Git (B1, 2026-10-23):** Git for Windows **2.56.0.windows.3**, 64-bit, at `C:\Program Files\Git\cmd\git.exe`. The table above says "2.55.0 line" because 2.56.0(3) came out on 2026-10-22, one day before the install. It is the same tool, one release newer.
- **Commit identity (B1, your choice: "Keep the current ones"):** `mariqtereza-del` / `mariqtereza@gmail.com`, already in your global Git settings since 2025-10-26. Nothing was changed. Note: if the project goes public on GitHub (Phase J), this email will be visible in the commit history. It can be switched to GitHub's private "noreply" address before then, but commits already made keep the old one.
- **Compiler and build tools (B2, 2026-10-23):** Visual Studio Build Tools 2026 **18.11.1** (`18.11.12315.225`), workload "Desktop development with C++", at `C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools`. Compiler: MSVC toolset **14.51.36231**, `cl` version **19.51.36260** for x64, which meets the "14.51 or newer" requirement. Build tools that come with it: **CMake 4.3.1-msvc1** and **Ninja 1.13.2**. They are used only inside the Developer environment (`VsDevCmd.bat -arch=x64 -host_arch=x64`), not on the normal PATH. Evidence: `docs\ENVIRONMENT.md`.

**Reversible:** Yes, before Phase D starts. Switching compilers later means rebuilding and re-running all tests, but no game code would be lost.

### D-002 — Stella version (2026-10-23, Stage B7, brought forward from C1)

**Your choice:** "Stella 7.0 (Recommended)".

- **What it means:**
  - The emulator core built into our program is Stella's libretro core from source tag **7.0**.
  - The reference app you run in C1 is the official Windows download of the same release, labelled **7.0c**.
- **Why they count as the same version:** tags 7.0a, 7.0b and 7.0c differ from 7.0 only in two Windows installer files (`Create_Builds.bat`, `stella.iss`), checked with GitHub's comparison on 2026-10-23. So the "ruler" and our program run identical emulation code (D-001's reason for using Stella's own core).
- **Alternatives:**
  - 6.7.1 (January 2024): older, with no advantage for this game.
  - Stella's current development code: newer, but not a release, and it changes often.
- **Why it was decided early:** B7 already needs it to download the matching source.
- **Reversible:** Yes, until Phase D builds on it. Changing it later means re-running every comparison made with the old version.

### D-003 — Disk space: keep all files (2026-10-24, after Stage C3)

**Your choice:** "Keep all files as they are."

- **Context (measured 2026-10-24):**
  - The project folder holds 467 MB: `build\` 291 MB, the library sources 72 MB, `.git` 27 MB, `reference\c3` 44 MB and the download archives 28 MB.
  - The backup zips outside it hold 159 MB.
  - Estimate if nothing is cleaned: about 1–2 GB by the end of the project. The released game itself is estimated at about 5 MB.
- **Options offered and declined:**
  1. A one-off clean-up of rebuildable or duplicate files (≈ 175 MB).
  2. Deleting build folders after each stage.
  3. Keeping only the two newest backup zips.
  4. Leaving the download archives out of backups.
- **What it means for future sessions:** don't propose deleting build folders, measurement data, downloads or old backups to save space. Only mention disk space again if free space on F: becomes a real problem (86 GB free today).
- **Reversible:** Yes. You can ask for any of the clean-ups at any time.

### D-004 — Authentic first: the port follows the 1981 console, not Stella (2026-10-25, Stage D14)

**Your words:** "It would be optimal to ensure the game ran as it would on an Atari 2600 in 1981. Any discrepancies between an authentic setup and the Stella emulator need to be fixed in favor of the authentic setup."

- **What it means for the work:** where Stella does something different from a real NTSC Atari 2600 of 1981, the port follows the real console, if that can be shown from the hardware's own numbers. Stella stays the reference for *what the game does* (its emulation core is proven equal to ours, D14); it is no longer the reference for *speed, sound pitch or picture shape*.
- **Applied so far:** the speed and sound pitch (T-D14-2). Power-on memory was examined and needs no change (T-D14-3). The picture shape is a question for you (see `DISCREPANCIES.md`).
- **Changes earlier choices:** T-D9-1 (60.000 frames a second, "match Stella") is replaced by T-D14-2.
- **Reversible:** Yes. `--stella-speed` brings Stella's speed back for comparison.

### D-005 — Default picture: 12:7 pixels, game area, fitted and centred (2026-10-25, Stage D14)

**Your words:** "The 12:7 aspect ratio centered within a 16:9 display (in my case and most common use-cases) should be the default option. Of course that 12:7 ratio should be scaled to the closest desktop resolution the user is using."

- **What changed:** the default pixel shape is **12:7** (was Stella's 19:10) and the default mode is **Game area** (key 4), which fits the picture to the window or screen and centres it (it was Sharp). Keys 1-4 still switch modes; `--pixel-aspect stella` brings back 19:10, `--pixel-aspect W:H` any shape.
- **Why Game area:** it is the only mode that scales to the display (any size, not whole steps) and shows only the lines the game draws, which fits "scaled to the closest desktop resolution". If you meant Fill (all 228 rows, with black bands), it is key 2.
- **Measured (real window, a game running):** 2560x1440: picture 2046x1440 centred (257 pixels of black each side), shape 1.714 (12:7 = 1.714); 1920x1080: 1535x1080; 1366x768: 1091x768; 1280x1024 (not 16:9): 1280x901, centred between black bands; fullscreen check OK. Nothing outside the picture is lit.
- **Replaces:** the default in T-D6-1 (19:10), and records the authenticity question left open in `DISCREPANCIES.md`.
- **Reversible:** Yes.

### D-008 — The owner's input is avoided unless absolutely necessary (2026-10-26, Stage E1)

**Your words:** "Make the change to Phase E in PLAN.md and commit. Remember, you are translating. My input should be avoided, unless absolutely necessary." (after asking why the plan wanted your play-throughs, since the goal is an exactly accurate port with no emulation layer.)

- **What it means:** the port is a translation of the ROM, and its accuracy is proved by machines: the original and our code run on the same inputs and are compared on every frame (lockstep). Nothing in that needs a person, so no stage may wait for the owner's play, reading or judgement of "feel". Owner input is kept for what only the owner can give: legal choices and the project name (J1, J2), approvals the rules require (installing, deleting, publishing, running Stella's app), choices of taste that are theirs (done at Gate D), and files only they have.
- **Changed in `PLAN.md` (Phase E and F26):** E3 is scripted (no recorded session); E7 is played by the bot (`stella_bot`, extended to later levels) and by seeded random input; E8's gate rests on traces and descriptions, not on anyone playing; the "you play" step of Gate F is optional (lockstep decides); the "needed from you" list loses the play sessions. E4's randomness probing and E2, E5, E6 were already automatic.
- **How I will work:** I decide technical questions myself and do not ask for human play, reading or checking as a step of a stage. At the end of a stage I say what was proved by measurement; if a person's look would add something, I say so as an option, never as a condition.
- **If a bot or script really cannot reach something:** that is written down as a limit, with what was tried; the owner is asked only if no automatic way exists.
- **Reversible:** Yes.

### D-007 — No phosphor glow by default; F7 turns it on (2026-10-26, Gate D)

**You noticed:** in black and white mode "Pac-Man has a ghosting effect when moving. Is that intended?" **Your choice:** option 1, "off by default (authentic), with a key to turn the soft blend on".

- **What it was:** Stella's game list switches its "phosphor" effect on for this ROM: each picture is blended 60 % with the one before, to soften the ghosts' flicker. It leaves a faint trail behind everything that moves, more visible in the greys of B&W mode. It is not in the game and not in the console's picture. Measured on your B&W recording: 74 to 249 pixels of a frame were blends of two pictures; with the glow off, 0.
- **Why off:** the real game draws each ghost on one frame in four, so on a real TV the flicker was there; the glow of a real picture tube faded much faster than Stella's 60 % per frame (my reasoning, not a measurement), and the eye does the smoothing on any screen. Authentic first (D-004).
- **Now:** off from the start. **F7** turns Stella's soft glow on and off (60 % blend, as before), remembered in `settings.ini` as `soft_glow=on|off`, shown in the F1 list as `[now off]` / `[now on]`. `--soft-glow` (tests) starts the no-window run with it on.
- **What does not change:** the game, the indexed picture, the fingerprints and the recordings (0 differences from C3 over 2,500 frames; audio identical to C5); the glow only touches the colours of the picture shown.
- **Proof:** `test_controls` 119 checks (the F7 line says off / on); the real window with an injected F7 wrote `soft_glow=on` to `settings.ini`, and a second F7 on the next start wrote `soft_glow=off`; raw against shown picture: 0 blended pixels by default at 3 moments of your recording, 74-249 with the glow on.
- **Reversible:** Yes.

### D-006 — Default picture: "Framed" (key 5), a border on every side, nothing stretched (2026-10-25, after Stage D14)

**Your words:** "I need the horizontal and vertical black margins to match the 12:7 aspect ratio. This is to ensure that any drawn graphics are not stretched in any way. This should be the default graphic setting, please ensure this is implemented and set to whatever key best fits this model."

- **How I read it:** the picture keeps the exact 12:7 pixel shape, and it has a black border on all four sides; the border is made of **game pixels**, the same number of them on each side (**8 columns left and right, 8 rows above and below**), so the margins have the same 12:7 shape as the picture itself. Everything (picture plus border) is scaled together to the largest size that fits the window, and centred. If you meant a different border size or rule, tell me and it is one number (`kFrameMargin`).
- **Key:** **5**, a new mode named **Framed**. Keys 1-4 keep what they did (Sharp, Fill, TV frame, Game area), so nothing you approved earlier moved. It is now the default (replaces Game area as the default of D-005).
- **What is shown:** the game area (rows 11-203, the lines the game draws plus the 4 black rows under the life markers), in 12:7 pixels, any scale (no whole-number steps), so it scales to whatever display it meets.
- **Measured in the real window, a game running:** 2560x1440: picture 1890x1330, border 335 left/right and 55 above/below (8 game columns and 8 game rows); 1920x1080: 1417x997 at 251/41; 1366x768: 1008x709 at 179/29; 1280x1024: 1164x819 at 58/102; fullscreen check OK; pixel shape 1.714 (12:7 = 1.714) everywhere; nothing outside the picture is lit.
- **Tests:** `test_layout` 231 checks (new: 12:7 within 0.5 %, inside, centred, border at least 8 game pixels on every side and exactly 8 on the limiting axis, in all five test windows), `test_controls` 117.
- **Not covered:** the pixel edges are still blended a little when the scale is not a whole number (SDL's pixel-art scaling); a perfectly crisp grid would need whole-number steps (key 1).
- **Reversible:** Yes.

## Technical choices made by Claude Code

### T-D14-2 — Speed and sound pitch follow the real console: 59.9227 Hz (2026-10-25, Stage D14, owner's rule D-004)

- **Facts:** a real NTSC 2600 runs from the 3,579,545.45 Hz colour clock: 228 clocks a line, 262 lines a frame = **59.9227 frames a second**; its sound chip makes 3,579,545.45 / 114 = **31,399.5 samples a second**. Stella's core plays 60.000 frames and 31,440 samples (524 pairs per frame), 0.13 % fast and 0.13 % sharp (`docs\TIMING.md`). The emulation counts CPU cycles, so only the playing speed changes, never the game.
- **Choice:** the game is paced at the console's frame rate, and the sound device is told the samples are 31,400 Hz (the nearest whole number; 14 ppm from the true value), so one frame still carries 524 sound pairs. The sound card steers the frame clock as before (T-D9-2). `--stella-speed` (60 Hz, 31,440 Hz) stays as a comparison switch. Code: `kConsoleFrameHz`, `frameRate`, `soundRate`, `Pacer` in `main.cpp`.
- **Proof:** a 9-minute muted run measured **59.9235 frames a second** (console 59.9228; the rest is the 31,400 rounding), 0 underruns, 0 dropped sound; 93.6 % of game frames were shown for exactly 3 refreshes of the 180 Hz screen (30,280 of 32,355). A 20 s audio check: 0 underruns, source format 31,400 Hz. With `--stella-speed`: 59.99 frames a second. The emulation is unchanged: 2,500 frames still equal the C3 fingerprints and the C5 audio bit for bit; the key replay and the side-by-side check still give 0 differences.
- **Not proved by me:** how it sounds and feels to the owner (the speed is 0.13 % lower, one frame in 13 seconds; the pitch drops by 2 cents).
- **Reversible:** Yes.

### T-F10-1 — Pac-Man's movement written as a model and proved on 202,473 frames (2026-10-26, Stage F10)

- **Method:** `tools\f10_pacman.py` is a model of the code (not an emulator): from the previous frame's memory, the frame's joystick and the ROM's wall table (`$FE13`) it predicts x, y, state, speed accumulator, JOY_DIRECTION and the idle counter, and compares with the recorded memory of every frame in which the normal road runs. Frames on other roads are counted by reason and skipped (title screen, frozen game, death, ghost-eaten pause, power pill eaten, change of turn, Select during play).
- **Result:** 202,473 of 202,473 predicted frames exactly right in all 70 games; every road of the rules was taken (turns, reversals, standing still against a wall, both openings, the turn-round inside an opening) except the x border (x onto 2 or $96), which Pac-Man cannot reach.
- **Found:** the maze is a grid of 4-pixel cells and 10-pixel rows; the wall table byte has bit 7 side wall, bit 6 up, bit 5 down; speed comes only from the accumulator (x17 mod 256, carry = a step): seed $09 for GAME_SELECT 2-7 (games 7, 8, 1) and $0B for 8-$11 (games 2-6), so Pac-Man is slower in games 1, 7 and 8 and never speeds up; with the stick neutral he stands still against a wall; the two openings are an animated path (`ANIMATE_OBJECT`) of about 85 frames (2,313 animation steps for 27 entries).
- **Correction:** the first draft of this stage said games 1-3 / 4-8 for the two speeds; reading `$84` (power-on value 6 = game 1) and the table of the 70 games gave the real split above.
- **Not claimed:** wafers, scoring, collisions, power pill, bonus item (F11, F12, F15); player 2's stick (never in the recordings; the model gives a neutral stick there, as the recordings did).
- **Reversible:** Yes.

### T-F9-1 — Input reading, and code that was listed as data (2026-10-26, Stage F9)

- **Reads:** exactly five instructions read inputs: `$F61E` SWCHA (joysticks), `$F857` SWCHB bit 3 (TV type), `$F886` INPT4 (left fire), `$F88A` SWCHB bits 0-1 (Reset, Select), `$F91C` SWCHB bit 6/7 (difficulty). Details and scanlines in `docs\F9_INPUT.md`.
- **Player 2 uses the RIGHT joystick and the RIGHT difficulty switch** (tested: right stick moves Pac-Man in player 2's turn, the left stick is ignored, bit 5 of GAME_FLAGS follows the right switch). All recorded games hold only the left joystick, so player 2 never moved in them.
- **Select held:** steps the game number on the press and again every 31 frames (tested: steps at frames 100, 131, 162, ...).
- **Owner decision for Phase H (not needed now):** keys/pad for player 2's joystick in the PC program. The faithful default would be a second key set or the second pad; to be asked when the host is revisited.
- **Correction to F2-F6:** `$F5C2-$F5C3` (`LDY #$50`), `$F832` (`DEY`) and `$F907-$F908` (`BEQ $F8C8`) were listed as data; they are code (the not-taken side of branches). Proved by three extra games `tools/f9_inputs/X-*.inputs` (difficulty A with power pill; difficulty A with the bonus item showing; Select held). They have no golden trace (only their instructions count) and are included in `f1_coverage.py` and `f6_coverage.py`. New numbers: C 1,810, c 1,735, D 509, untouched 42; F4: 539 data bytes in 61 segments. F2-F8 re-run: all byte-identical.
- **Rule for Phase F:** a byte no recording runs is not proof of data; read the code around table holes before trusting them.
- **Reversible:** Yes.

### T-F8-1 — Routines found by following the code, the frame loop kept whole (2026-10-26, Stage F8)

- **Method:** `tools\f8_routines.py` takes the start-up and every JSR target (33 targets, two of them only from JSRs that never ran) as entries, follows branches, jumps and fall-through, and makes a shared tail of any code that two routines reach by a jump. The pieces of the frame loop (cut by its jumps) are folded back into `MAIN`: an unnamed piece reached only from the main loop belongs to it.
- **Result:** 40 routines: `MAIN` and 39 subroutines, all named from reading the code (sure / likely / "?" in `docs/F8_ROUTINES.md`). Every instruction that ran belongs to exactly one; each routine entered only by JSR ran exactly as often as its JSR instructions (70 games); no recursion; deepest nesting 10 bytes of stack, as F7 found.
- **Found:** six routines drop the caller's return address as well (PLA PLA): the four TRY_PAIR helpers and `TRY_COMMIT` (the end of the four direction tries) end the decision of the caller at once; `PACMAN_KEEP_GOING` ends by continuing in the main loop. The C++ version needs an early exit at these points.
- **Not claimed:** names marked "?" (the pair-trying helpers, `PACMAN_KEEP_GOING`) are guesses from the shape of the code; F10-F13 settle them.
- **Reversible:** Yes.

### T-F7-1 — The 128 RAM bytes named from the code, with confidence marks (2026-10-26, Stage F7)

- **Method:** the whole listing was read; every byte of `$80-$FF` followed through the instructions that touch it (F6 code map) and measured over the 70 traces. The table lives in `tools\f7_ram.py`, which writes `docs\F7_RAM_MAP.md` and the named listing (`reference\disasm\pacman_ram.asm`, local only; byte-identical to the ROM).
- **Result:** 84 names sure, 32 likely, 12 uncertain ("?"). Structure found: two copies of a 20-byte player block (`$88-$9B` and `$9C-$AF`: wafer map, lives, level step, pill flags, score colour) exchanged at a change of turn; five-object arrays (Pac-Man then four ghosts) for x, y, state, speed accumulator; scores as decimal digits in `$CC-$D1`; stack in `$F6-$FF` (deepest call chain 10 bytes); `$EA` and `$EB` unused.
- **Correction:** Stage E4 recorded "no randomness found". The game does have a random source: `$CB` is a shift register stepped by the routine at `$FBE0` and used when a ghost chooses a direction. It is seeded the same way (`$FF`) at power-on and at every game start, so replays are still identical. F14 follows it.
- **Found while checking:** the frame counter reads 2, not 1, in the frame Select sets it (the same frame adds 1); the incoming player's lives are lowered by 1 in the frame of the swap. Both are in the checks of the tool.
- **Not claimed:** direction codes and state bits of `$BB-$BF`, the flag bits of `$E7`, the exact use of the scratch bytes; they belong to F9-F17.
- **Reversible:** Yes.

### T-F6-1 — What actually runs: all 70 recordings, and a read is data only when it fetches data (2026-10-26, Stage F6)

- **Method:** `stella_coverage` replays every baseline entry (each checked against its golden trace, 0 differences in all 70, 439,680 frames). BL-065/066 need their memory edits, so the tool got optional trailing arguments `<frame>:<hex address>:<hex value>` (test only, applied before that frame; same rule as the host's `--poke`).
- **Result:** C 1,807, c 1,733, D 509, n 3, untouched 44. No instruction runs that the 29 recordings of F1 had not already run: the F1 map was complete for these games.
- **Refinement:** the core logs the byte the processor looks at after an implied instruction; F1 counted it as data. F6 counts only fetches by instructions with data modes (3 bytes reclassified D to n; no executed byte is also data).
- **Not claimed:** that untouched bytes (12 bytes of code in 4 pieces, 32 others) can never be used.
- **Reversible:** Yes.

### T-F5-1 — The frame loop, found by tracing instructions with their scanlines (2026-10-26, Stage F5)

- **Tool:** `stella_pctrace.exe` (same method and same private-member trick as `stella_coverage`, Stage F1; checked against the golden trace on every frame of the recording: 0 differences in all 8 runs).
- **Finding:** in all 31 traced frames (attract mode, play, a death, game over, a new game, two players, Game Reset in play) the same six landmarks occur in the same order: `FRAME_START` `$F7D6` (scanline 258: wait for the line end, VBLANK and VSYNC on, clear collisions, work out values; `$F875` sets the vertical-blank timer to `$29`), `VSYNC_END` `$F87C` (scanline 0), `VBLANK_WAIT` `$F0C5` (the loop on INTIM, starting on scanline 10 to 22, ending on scanline 33), `KERNEL_START` `$F0CA` (scanline 33: VBLANK off; the picture is drawn by the code from `$F0CC` to about `$F389`), `OVERSCAN_TIMER` `$F389` (scanline 195, while the last picture lines are still being drawn), `OVERSCAN_WAIT` `$F7D1` (the loop on INTIM, starting on scanline 224 to 240, ending on scanline 258) and then the loop closes by falling into `FRAME_START`. The start-up code at `$F000` clears the whole page `$00-$FF`, sets first values and jumps into the loop at `FRAME_START` (not at the top).
- **Agrees with:** the C4 measurements (docs\TIMING.md) which were made from outside; this is the first time the lines are tied to code addresses.
- **In the listing:** labels `RESET`, `FRAME_START`, `VBLANK_TIMER_SETUP`, `VSYNC_END`, `VBLANK_WAIT`, `KERNEL_START`, `OVERSCAN_TIMER`, `OVERSCAN_WAIT` and a banner at each part. Still byte-identical (SHA-1 `0940fea7f04cdb6d4b90c5ad1a7e344e68f6dbb1`); local only.
- **Not claimed:** which routines belong to which part (F8), what the top and bottom logic compute (F9 onward).
- **Reversible:** Yes.

### T-F4-1 — Data tables cut and named by how they are read (2026-10-26, Stage F4)

- **Method:** from the instruction-level coverage of 29 recorded games (F1) each data byte knows which instructions read it; each reader is followed forward (within 14 instructions, stopping at a return or jump) to the chip register its value is stored into; ranges touched by readers are merged where they overlap or touch; the data is cut at old labels, range edges and kind changes. Kinds: GFX (read through a pointer or table and stored into GRP0/GRP1), COLOR, PF, SND, TBL (read but not stored straight into a chip register), UNREAD, VECTORS.
- **Result:** 544 data bytes in 66 segments: GFX 208 bytes (5 segments: `$FD8B-$FDAA`, `$FDAF-$FDC1`, `$FDC6-$FDF9`, `$FE89-$FEEB`, `$FEEC-$FEF1`), COLOR 20 (`$FF2B-$FF39`, `$FF3C-$FF40`), PF 10 (six small ones), SND 8 (`$FFF1-$FFF8`: two 4-entry tables stored into the sound frequency registers), TBL 266 (37 segments; the largest 80 bytes at `$FE13-$FE62` read by nine instructions), UNREAD 26 (13 pieces), VECTORS 6 (`$FFFA-$FFFF`: NMI `$0000`, RESET `$F000`, IRQ/BRK `$0000`). 77 instructions read data; 24 of them end in a chip register.
- **Not claimed:** what a TBL means in the game; where one shape ends and the next begins inside a GFX block; the last sprite pointer values are set by code and only the code (F9 onward) tells.
- **Proof:** `pacman_tables.asm` assembles byte-identical to the ROM (same SHA-1 `0940fea7f04cdb6d4b90c5ad1a7e344e68f6dbb1`); the rebuilt bytes replay BL-001 and BL-049 equal. The listing is local only.
- **Reversible:** Yes.

### T-F3-1 — Hardware names in the listing (2026-10-26, Stage F3)

- **What:** 178 operands now carry a standard hardware name (47 different registers, 51 equates): e.g. 28 uses of the wait-for-line register, 13 of the player-0 graphics register, 15 each of the playfield registers PF1 and PF2, the sound registers (AUDC/AUDF/AUDV, 16), the collision reads (CXM1P, CXP1FB, CXPPMM), the fire button (INPT4), the joystick and console switches (SWCHA, SWCHB) and the timer (TIM64T set, INTIM read). `STA $00,X` (a sweep over the whole page at the start) stays a number. No chip address was left without a name.
- **Rules:** a store writes, every other instruction reads (the same address has two names). Chip copies: the video chip ignores some address bits, so `$31`, `$33`, `$37`, `$3C` are copies of the collision/input read registers `CXM1P`, `CXP1FB`, `CXPPMM`, `INPT4`; they are written with an equate `CXM1P_AT31 = $31`, so the exact address survives and the rebuild stays byte-identical. The same for the timer-chip mirrors.
- **Check:** every run compares the name tables (45 write names, 14 read names, the RIOT ports and timers) with the tables in Stella's debugger source (`CartDebug.cxx`, vendored, read only): equal. The assembler of F1 got equates, with one more self-test (9 checks, 0 failed).
- **Proof:** the named listing assembles byte-identical to the ROM (same SHA-1 `0940fea7f04cdb6d4b90c5ad1a7e344e68f6dbb1`); the bytes replay golden traces BL-001, BL-049 and BL-067 equal (the bytes are the ROM's, so this is a formality). The listing is local only (`reference\disasm\pacman_named.asm`).
- **Not claimed:** the names say what the chip does; what the game means by each use (for example which write is the sprite of Pac-Man) is for F5 onward.
- **Reversible:** Yes.

### T-F2-1 — The raw listing; why no second assembler was needed (2026-10-26, Stage F2)

- **The listing:** 2,184 lines: 1,815 instructions (8 never run in the recorded games: two calls to `$FB36` and `$FB5C` and two `PLA PLA RTS` pieces, recognised because they decode exactly into official instructions), 544 data bytes, 279 labels (242 code, 37 data). Every branch, jump, call and absolute reference into the ROM is written with its label; no reference used the mirror `$1000-$1FFF`. No names or comments yet (F3 onwards). It stays in `reference\disasm\` (local only: it is the original program in readable form; Rule 7).
- **Proof:** the assembled listing is byte-identical to the ROM (SHA-1 `0940fea7f04cdb6d4b90c5ad1a7e344e68f6dbb1`, same SHA-256). Run through all 70 baseline entries as the ROM, every golden trace is reproduced (70 of 70 in a full run; one single unexplained mismatch, see T-F2-2).
- **The owner's remark (a `dasm` second opinion "if you are uncertain"):** I am not uncertain, for three reasons: (1) the test is against the original bytes, which are fixed: an assembler that disagreed with the real one in a way that matters would produce different bytes and fail; a pair of mistakes that cancel out cannot give the exact original on 4,096 bytes with 1,815 instructions; (2) the opcode table is Stella's own and the CPU run (F1) found no executed instruction starting inside another, which checks every instruction length; (3) the rebuilt ROM replays all 70 recorded baseline games (hundreds of thousands of frames) exactly like the original. Nothing was downloaded. If the owner wants `dasm` anyway, it is a small approved download and one more run; I will ask then.
- **Reversible:** Yes.

### T-F2-2 — One unexplained mismatch in a repeat check (2026-10-26, Stage F2)

- **What happened:** in the first full run of `baseline.py check` with the rebuilt ROM, one entry (BL-051, 30,000 frames) reported a different SHA-256 (`78c309e2...` instead of `9dc5da28...`). Nothing else differed.
- **What I did:** repeated it. BL-051 six more times through `baseline.py` (with the rebuilt ROM and with the original), a second full check of all 70 entries (70 of 70), 12 further runs of BL-051 and 60 runs of BL-049 straight with the host: every one equal to the stored value (72 repeats of long entries and 70+ entries, 0 differences).
- **What it means:** I could not reproduce it and found no cause. A one-off disturbance of the test run (an outside program touching the 9 MB trace file as it was written is the likeliest) is possible but not shown; a rare real nondeterminism in the host or the Stella core is not excluded. It is recorded here so that any future mismatch is taken seriously: **if a check ever says DIFFERS, run it again at once, keep the traces, and look for a pattern before going on.** Nothing in the project changed because of it.
- **Reversible:** n/a.

### T-F1-1 — Our own assembler instead of downloading dasm; Stella's debugger not used (2026-10-26, Stage F1)

- **Choice:** `tools\f1_6502.py` holds a 6502 disassembler and assembler written for this project; the opcode table (mnemonic, addressing mode, cycles, length of all 256 opcodes) is read at run time from Stella's own disassembler source `third_party\stella\src\debugger\DiStella.cxx`. Only the 151 official opcodes are assembled; the others are written `.byte`. No `dasm` was downloaded.
- **Why:** Rule 5 (no dependency that is not needed) and the standing rule that downloads need the owner's approval: a 6502 assembler for one 4 KB program is a few hundred lines, and it can be proved right on this very program (disassemble, assemble, byte-identical) and on all 151 opcodes. The plan's table named `dasm` as "chosen at F1"; this is the safer choice and the owner can still ask for `dasm` as a second opinion at any time.
- **Stella's debugger:** not used: it is a window that cannot be scripted, and its table is already used (above). The Phase C measuring tools (the Stella core without a window) do the job better.
- **Tests (8, all pass):** the table has 256 entries and 151 official ones; eleven opcodes known from the 6502 data sheet agree; all 151 official opcodes survive disassemble-then-assemble; the written width of a number decides zero page or absolute; labels and branches; five wrong lines are refused; the whole ROM (2,190 lines, 127 bytes written `.byte`) assembles back byte-identical; the same listing with a label on every line too.
- **Reversible:** Yes.

### T-F1-2 — A tool that records which ROM bytes the CPU runs and reads (2026-10-26, Stage F1)

- **Choice:** `tools\stella_probe\coverage.cpp` (`stella_coverage.exe`, built with `tools\build_stella_stella_probe.cmd`... see `tools\build_stella_probe.cmd`) runs the ROM on a baseline recording one instruction at a time (the same method as `stella_timing`, Stage C4), noting each instruction's start address and the last address it read. Stella's program counter is private; it is read by naming the private member in an explicit template instantiation (allowed by the C++ standard); no Stella file is changed.
- **Proof that it plays exactly like the normal loop:** each run is compared with the recording's golden trace on every frame (picture fingerprint and all 128 memory bytes): 29 recordings, 365,780 frames, 0 differences.
- **Result (`reference\f1\coverage_union.txt`, local):** of the 4,096 ROM bytes, 3,540 are part of instructions the CPU executed (1,807 first bytes, 1,733 others), 512 are read as data and never executed, 44 were neither: the last 7 bytes ($FFF9-$FFFF: the vectors are read by the CPU itself), four pieces of exactly 3 bytes in the code area ($FACF, $FAF9, $FB2A, $FB33: the size of one JMP or JSR, probably code that none of the 29 games reaches), two single bytes in the code area ($F5C3, $F908) and 23 bytes inside the data table. Every executed first byte is an official opcode, no instruction starts inside another, none was executed outside the ROM. The data lie in `$FD3F-$FF5F` (and a few bytes inside the code area); this is the first hard evidence for the "about 87 % code" estimate of Stage A4.
- **Reversible:** Yes.

### T-E8-1 — A test-only memory edit, and what the edge cases showed (2026-10-26, Stage E8)

- **The edit:** `pacman2600.exe --headless ... --poke <frame>:<hex address>:<hex value>` (repeatable) writes one byte of the console's memory before that frame; `tools\baseline.py` passes a `pokes:` line of an entry as `--poke`. Test only, headless only, never used in play (Rule 3). Why: a score near 9,999 cannot be reached by playing in reasonable time, and Stella's debugger is a window that cannot be scripted. Entries that use it (BL-065, BL-066) say ARTIFICIAL in their title and text and are not compared with Stella's script runner (which has no memory edit); all earlier traces still equal their stored SHA-256 after the change.
- **Findings:** (1) Game Reset during play restarts at once: the pause flag comes on the same frame, score 0, 126 wafers, three lives, the start tune again (127 frames). (2) Game Select during play ends the game at once (lives 0, Pac-Man moved to (64,96)) and advances the selection; Game Reset then starts that selection. (3) During play the left difficulty switch sets bit 5 of `$E7` and changes nothing else; the right one changes nothing at all; black-and-white changes `$EF` high nibble and the colour bytes `$F2 $F3 $F5`; none of them changes any ghost or Pac-Man place. (4) A 40,000-frame idle: the whole memory state never repeats and nothing breaks. (5) The score has four digits (`$CC` tens/ones, `$CE` hundreds/thousands): it goes from 9,999 to 0 and no life is given at any multiple of 1,000 (artificial test), and in the three long bot games (scores to about 4,000) the lives counter rose only at level clears. (6) Two players: `$E6` bit 6 = two players, bit 7 = player two's turn; the lives counter `$98` and the wafer map belong to the player in turn and change at every death (the score bytes read the same in every turn of this idle game, so a score swap was not shown); the game was over right after player two's third death. (7) Eaten ghosts walk back to the box: bit 3 of byte `$BC+i` is set on the frame the ghost is eaten and cleared when it is back at (76-78,30) (14 of 14 cases, 148 to 648 frames); this corrects the E4 note that an eaten ghost's bytes "show no jump", which was true but incomplete. (8) Random controls (Reset, Select, Fire, switches) over 24,000 frames keep every movement rule.
- **Proof:** 13 new entries, each traced twice byte-identical; fresh `baseline.py check` of all 70 equals the stored SHA-256 values; seven new inputs (55,500 frames) equal Stella's script runner: 0 fingerprint differences, 70 pictures, 0 different.
- **Reversible:** Yes.

### T-E8-2 — Gate E: two ghost topics moved to Phase F (2026-10-26, Stage E8)

- **Decision:** the brief's Phase E list names for the ghosts "flicker order" and "the blue/vulnerable state" and "how they choose turns". The baseline holds every frame's whole memory and picture, so a rebuilt game is tested on them anyway, but I did not isolate them as separate facts: the byte that holds the scared state, the drawing order of the ghost flicker and the rule for turn choices (only shown to depend on Pac-Man's place after 126 frames and not on timing or chance) are properties of the program that come out of reading its code (Phase F) far more cheaply than out of more play. They are listed in `docs\GATE_E.md` as "moved to Phase F", not as passed.
- **Why:** Rule 12 (the plan adapts) and D-008 (no waiting for people): more entries would only repeat what the lockstep test will check on every frame.
- **Reversible:** Yes: they can be added as entries at any time (Phase F tools make this easy).

### T-E7-1 — Long play: the bot, the bonus item, levels, speeds (2026-10-26, Stage E7)

- **The bot:** `stella_bot ... long` (fifth argument) keeps steering after a level ends and rewards the bonus item. Without the argument it is unchanged (the default run with seed 1 gave the same 279 moves as the C7 route; only its header line differs, because the C7 run used a longer frame limit). Rebuilt with the existing `tools\build_stella_probe.cmd`.
- **Games:** seeds 1, 2, 3, 36,000 frames each: 5 levels cleared in each (a 6th level reached), lives 4, 3, 5 at the end, bonus items eaten 4, 4, 7, scores 3,221, 3,558, 4,094. None reached game over (limit of the run, not of the game). Random input (seeds 11-14, 30,000 frames each): many deaths and games, the fire button restarting after each game over, highest score 22-35 (nobody plays well), the movement rules held on all.
- **Findings:** (1) The bonus item is on the board while `$E7` bit 1 is set; a window lasts up to 768 frames if not eaten; it appears at very different moments (from the start of a level to 5,868 frames into it, listed in each entry; the first one 1,144 frames into level 1 after 39 wafers eaten); eating it gives **+100** with Pac-Man at x 72-80, y 40 (the middle of the row under the ghost pen). (2) Pac-Man's speed does not change with the level (1-frame and 2-frame gaps between steps in the same proportion in all six levels). (3) The ghosts' speed rises with the level: still one change of place every 4 frames, but the change grows: on average 1.41 pixels per change in level 1, 1.59, 1.78, 1.97, 2.16 and 2.34 in level 6 (first game; changes of 3 pixels first appear in level 4); the other two games give 1.42-1.61-1.78-1.97-2.16-2.37 and 1.40-1.59-1.79-1.97-2.15-2.34. (4) Clearing a level added one life every time (15 of 15 level clears in the three bot games). (5) Ghosts also go through the openings at the top and bottom of the maze (jump of more than 3 pixels in column 76); Pac-Man's jumps through them are seen in the random games too. (6) Over all long games no step of Pac-Man other than one pixel or an opening passage, and no ghost change of more than 3 pixels other than an opening passage.
- **Open:** the Gate E list still needs an extra life by score (E8, memory-edit test) and the other topics listed there.
- **Proof:** 8 new entries, each traced twice byte-identical; a fresh `baseline.py check` of all 57 equals the stored SHA-256 values; BL-050, 053, 054, 056 and 057 (136,600 frames) equal Stella's script runner: 0 fingerprint differences, 50 pictures, 0 different.
- **Reversible:** Yes.

### T-E6-1 — Dying and game over entries; what was measured (2026-10-26, Stage E6)

- **A death:** the pause flag ($D2 bit 6) comes on; the lives counter drops 125 or 126 frames later; the flag clears 245 or 246 frames after it came on (126 frames for the last life, so the game over is reached at once). Sound is on from 1 frame after the flag comes on to about the last frame (about 183 to 185 of 245 frames; 121 to 123 of 126 for the last life); 74 to 131 different pictures are drawn. At the clearing Pac-Man is back at (76,60) and all four ghosts at (76,30); score and wafer map are unchanged by a death.
- **Catch timing:** an idle Pac-Man is caught 297, 254, 256 frames after play began (lives 3, 2, 1); in the bot route ghosts were 4 to 17 pixels away in the position bytes at the moment (the game most likely uses the chip's collision flags; not read here).
- **Game over:** the lives counter reads 0 and the pause flag clears a frame later (the third death's pause is 127 frames). Ghosts stand at (76,30), start walking again 120 frames after the flag cleared, and keep walking. The fire button does nothing for the first hundreds of frames (pressed 109 frames after game over: ignored); Pac-Man moves to (76,95) on frame 2295 (504 frames after the game over) in the idle game; a fire press after that starts a new game on the same frame (lives 3, start tune 119 frames, score and map as at the first start); in the bot route Stage C7 saw the same at frame 9864.
- **Not covered:** the Reset button during a game and the two-player change of player at a death (E8); the first life taken after a level clear in a later level (E7).
- **Proof:** 2 new entries, each traced twice byte-identical; a fresh `baseline.py check` of all 49 equals the stored SHA-256 values; BL-048 equals Stella's script runner (3,000 frames, 0 differences).
- **Reversible:** Yes.

### T-E5-1 — Eating and scoring entries; what was measured (2026-10-26, Stage E5)

- **Score bytes (working labels):** `$CC` = tens and ones and `$CE` = hundreds, both as decimal digits in each half-byte (found by matching the score shown in the status bar at 7 known frames). Earlier notes said `$CC` was the game number: that holds only while Select is pressed (T-E2-1). A third byte for larger scores was never non-zero and is not used.
- **Findings:** (1) A wafer is +1 and clears one bit of the wafer map `$88-$97` (126 wafers in a full map; the wafer at the start place is counted when play begins). (2) A power pill is +5 with no wafer bit cleared; five were eaten in the route, at places near the four corners. (3) Ghosts eaten during one power pill give +20, +40, +80, +160 in turn (eight eaten in the route: two chains of 20 and 40 and one chain of all four values); Pac-Man stopped for 32 frames at each of the four checked in Stage E4. (4) The last wafer of a level is +1, refills the map (126 wafers), adds one life (`$98` 2 to 3) and starts a pause of 120 frames. (5) The digits drawn at the right of the status bar equal the memory score on all 155 sampled frames; ten digit shapes, each for one digit.
- **Not covered (must be added before Gate E):** the bonus item eaten, and an extra life by score. The bot never ate the item; the score never passed 597.
- **Proof:** 3 new entries, each traced twice byte-identical; a fresh `baseline.py check` of all 47 equals the stored SHA-256 values; BL-045 equals Stella's script runner (600 frames, 0 differences, 10 pictures).
- **Reversible:** Yes.

### T-E4-1 — Ghost entries; what was measured (2026-10-26, Stage E4)

- **Ghost bytes (working labels, found by observation):** ghost i (0 to 3) has x in `$B2+i` and y in `$B7+i`; all four stand at (76,30) when play begins. On the 9,800-frame bot route every ghost position is on a corridor row or one of the 11 vertical corridors.
- **Findings:** (1) Each ghost changes place only once every 4 frames, ghost i on the frames whose number (counted from power-on) mod 4 is i; the change is 1 or 2 pixels. This rhythm is tied to the frame counter, not to the start of play (BL-039, Reset one frame later). It stays the same during a power pill, and while Pac-Man's pause flag is set the ghosts keep moving. (2) For the first 126 frames of play the four ghost routes do not depend on Pac-Man: with Pac-Man standing or walking left (also one frame later), every ghost place is identical frame by frame up to frame 126 of play; after that they differ (BL-041/042 from frame 126, BL-043 from frame 214), when Pac-Man's place matters. (3) Randomness: the ghosts visit the same places in the same order when Game Reset is pressed one frame later (BL-039) or 60 frames later (BL-040, frame-by-frame identical too because 60 is a multiple of 4), and BL-041 and BL-042 (input one frame later) diverge from BL-038 at the same frame: no sign of anything random; the game looks fully deterministic given the inputs. (4) An idle Pac-Man is caught 297 frames after play begins (BL-038). (5) Power pill windows in the bot route: frames 2226-2673 and 3124-3985; ghosts keep the same step sizes and rhythm inside them. When Pac-Man stops for 32 frames during a power pill, a ghost was within 3 to 6 pixels of him (two stops had no ghost near and are not explained); the eaten ghost's place bytes show no jump to the start place; it is next in the start area 123 to 459 frames later.
- **Not identified:** which byte holds each ghost's blue (scared) state and its return as eyes; what $BC-$BF and $C1-$C8 mean (they change with the ghosts' turns); the drawing flicker order. Left for Phase F; the entries do not state them.
- **Proof:** 7 new entries, each traced twice byte-identical; a fresh `baseline.py check` of all 44 entries equals the stored SHA-256 values; BL-038, 039, 041, 043 and 044 (7,200 frames) equal Stella's own script runner: 0 fingerprint differences, 50 pictures, 0 different.
- **Reversible:** Yes.

### T-E3-1 — Movement entries; what was measured (2026-10-26, Stage E3)

- **Entries:** BL-026 (left to the wall and back right), BL-027 (down, up, down the first side corridor), BL-028 (a turn asked for early and held), BL-029 (a brief request), BL-030 (reversal), BL-031 to BL-033 (stick held diagonally), BL-034, BL-036, BL-037 (the opening at the bottom and top), BL-035 (the whole bot route). Made by `tools\e3_entries.py`; every number in the observations is computed from the trace.
- **Findings:** (1) one pixel step every 2 frames, with a 1-frame gap about every fourth step: 0.57-0.59 pixels per frame, the same in all four directions; over the bot route 1 frame: 475 times, 2 frames: 1,661 times. (2) A wall: the position stops although the direction is held; it stays still. (3) Pac-Man goes on in its last direction when the stick is released. (4) A turn that is held is taken at the next corner (BL-028: 2 frames after the last step); a request of 2 frames is not remembered (BL-029). (5) A reversal in the middle of a corridor is taken 2 frames after the last step in the old direction (BL-030, at (71,60)). (6) With two directions held, Pac-Man follows whichever is open and changes at each corner (BL-031: left to the corner (64,60), down to (64,70), left again; BL-032 the same going up; BL-033 mirrored to the right). (7) Positions are only on corridor rows (y = 0, 10, ... 70) or on 11 vertical corridors; steps are always one pixel, never diagonal (2,178 steps in BL-035). (8) The openings at the top and bottom of the maze are in the column x = 76 (not the sides): pressing down at (76,70) sets the pause flag for 84 frames with no life lost; $B6 reads 81 and then other values, and Pac-Man is next at (76,0); up from there gives the mirror event (85 frames) back to (76,70); a change to UP during the event turns it round (168 frames in all).
- **Corrects:** the plan and Stage C3's idea of a "side tunnel": there is none on the left or right edge; the maze wraps at the top and bottom.
- **Not understood (left for Phase F):** what $B6/$B1 hold during the 84 frames and what the status-bar digit (4, then 5) is; the entries state only what was measured. Not covered: the ghosts' effect on Pac-Man's path (E4), what happens in the opening when ghosts are near, higher levels (E7).
- **Proof:** 12 new entries, each traced twice byte-identical; a fresh `baseline.py check` of all 37 entries equals the stored SHA-256 values; BL-026, 028, 031, 034, 036 and 037 (3,860 frames) equal the independent Phase C reference core: 0 fingerprint differences, 60 pictures, 0 different.
- **Reversible:** Yes.

### T-E2-1 — Starting-up entries; what the traces corrected (2026-10-26, Stage E2)

- **Entries:** BL-002 (power-on and attract, 3,000 frames), BL-003 (Select 17 times), BL-004 to BL-019 (each of the 16 selections, then Reset), BL-020 (fire starts a game), BL-021 to BL-025 (Reset at five moments of the attract mode). Made by `tools\e2_entries.py`; every number in the plain-words observations is computed from the trace.
- **Findings** (written into `docs\CONTROLS.md`, "Correction"): (1) $81 and $EC are an attract-mode timer (frames since power-on or the last Select press / 256, and 0c minus that), not a variation setting as C6's table said; only $99/$C2 depend on the selection and they equal C6's table for all eight games; (2) $CC holds the game number only while Select is pressed and $E6 bit 6 means two players; (3) the attract mode is silent and its "power pill" bit toggles first at frames 1 and 256, then every 512; (4) Game Reset and the fire button each start a game on the frame they are first seen (lives byte $98 reads 3 at once), the start tune pauses play for 127-128 frames, and an untouched Pac-Man is caught about 5 seconds after play begins.
- **Why it matters for Phase F:** the rebuilt game must reproduce the timer and the selection bytes exactly, including that the attract state depends on frames since the last Select.
- **Proof:** 24 entries, each traced twice byte-identical; a fresh `baseline.py check` of all 25 entries equals the stored SHA-256 values; BL-002 (1,800 frames) and BL-024 (2,700 frames) equal the independent Phase C reference run in memory and picture on every frame (0 differences); the generator's own checks passed (selection after every press, start of every game, the timer rule on all six runs).
- **Not covered:** a second player's turn (only that the two-player bit is set and a game starts; the player-switch behaviour is E6), what the variations change (Phase F), a Select press while a game runs (E8).
- **Reversible:** Yes.

### T-E1-1 — Baseline entries and frame traces (2026-10-26, Stage E1)

- **Choice:** an entry is a plain-words text, an `.inputs` file (both in Git, `baseline\`) and a golden trace (local, `reference\baseline\`; its SHA-256 is in the entry text). A trace is one text line per frame: frame, input, switch register, picture fingerprint, sound fingerprint, 128 bytes of memory (`docs\BASELINE.md`, `src\host\trace.*`). Text, not binary: it can be compared with ordinary tools and read by a person; about 300 bytes a frame, 11 MB for ten minutes of play.
- **Why local:** the memory contents are Atari's program state (Rule 7, J3's content audit). Only inputs, words and a SHA-256 are shared; anyone with their own ROM can recreate a trace and check it against the SHA-256.
- **The fingerprint:** a finding of this stage. The "FNV-1a" of Phase C uses the start value `1469598103934665603`, one digit short of the textbook `14695981039346656037` (a typing slip in `tools\stella_probe` that every Phase C fingerprint inherited). It is still a fine fingerprint; changing it now would break every stored comparison, so it is kept and written down (`test_trace` pins its values, computed independently in Python).
- **Where the trace is written:** by the same code in the headless replay and in the window (`writeTrace` in `main.cpp`, from the game thread, after each frame), so the two can be compared; the frame counter, not the clock, is recorded, so the pace of the window does not matter.
- **Proof:** see `docs\BASELINE.md`: byte-identical on repeat (BL-001); window = no-window on 700 frames; the 9,800-frame route equals Phase C's independent reference on memory, picture and switches for every frame; `test_trace` 18 checks; earlier checks unchanged (2,500 frames equal C3 fingerprints and C5 audio, 350,460 frames of key replay, 1,860-frame side-by-side, unit tests).
- **Not proved:** that the sound column equals an independent sound record frame by frame (the reference sound files are one WAV, not per frame); it is the fingerprint of the bytes the proven-identical WAV holds.
- **Reversible:** Yes.

### T-D15-1 — The release zip: what is in it, the ROM scan, the GPL duties (2026-10-26, Stage D15)

- **Contents** (`tools\make_release.ps1`, output `out\PacMan2600-0.1.0-windows.zip`, ignored by Git): `pacman2600.exe`, `SDL3.dll`, the three Microsoft runtime files the program imports (`msvcp140.dll`, `vcruntime140.dll`, `vcruntime140_1.dll`; found with `dumpbin /dependents`; the rest are Windows' own `api-ms-win-crt-*` and system DLLs), `README.txt` (for the player), `BUILD-INFO.txt` (Git commit, date, SHA-256 of every file), `licences\` (Stella's GPL v2 text and copyright note, SDL's zlib licence), `roms\PUT_YOUR_ROM_HERE.txt`, and `source\` (the source zip and `BUILDING.txt`).
- **Why the source is inside:** the program contains Stella (GPL v2), so it may be passed on only with its complete source (or a written offer). Putting the source in the zip needs no offer and no web page. It is `git archive HEAD` of `src`, `tests`, `third_party` and the two build scripts, so it holds no project notes (the notes name the owner's e-mail address and PC paths, which a download must not carry) and is exactly what Git holds. It makes the zip about 27 MB (25 MB of it is the libraries' source).
- **The Microsoft runtime files:** copied (read only) from the Build Tools' `VC\Redist\MSVC\...\x64\Microsoft.VC145.CRT` folder, which Microsoft provides for shipping next to a program ("app-local"). So a PC without Visual Studio needs nothing installed.
- **The scan** (run on the finished zip and the source zip inside it; a failure leaves only a `.partial` file): refuses file endings `.a26 .bin .rom .rar .7z .tar .gz`, any other zip, a file whose SHA-1 is the Pac-Man ROM's, and any file of exactly 4096 bytes that holds a zero byte (text and source files of that size have none; a ROM does). One exception, pinned by its SHA-256: Stella's `os/macos/template.dmg.gz`, which is part of its unchanged source. `-TestScan` proves the scan on invented files (never the ROM): a 4 KB binary, `.a26`, `.bin`, a stray zip and a file with the ROM's fingerprint are refused; a 4096-byte text file is accepted.
- **Safety rules in the script:** it refuses to run with uncommitted changes (the zip must be exactly what Git holds) and when the program is older than its sources; `-AllowDirty` makes a `...-test.zip` that says so inside. It deletes nothing (output files are overwritten).
- **Result of the first run** (test zip, 3,281 files scanned): nothing that could be a ROM; unzipped into a fresh folder under `out\` and run from there: `--self-check` OK, `--find-rom` with no ROM anywhere exit 5, with the ROM given by path (the ROM was never copied) exit 0, 4 s muted run 59.97 frames a second, default Framed picture, 13 files, no `.a26` or `.bin`.
- **Not proved by me:** that it runs on a PC that never had Visual Studio (this PC has it; the DLL list says it should); the owner's try.
- **Our own code's licence:** not chosen (J1). The README says only that the program as a whole is distributed under GPL v2 because of Stella, and does not choose for the owner.
- **Added at Gate D (2026-10-26, found by the owner):** the first release only told people to use `--record` / `--replay` on a command line, which a double-click cannot do. The zip now also holds `Record a game.cmd` (starts the game recording into a `recordings` folder next to the program, named by date and time) and `Replay a game.cmd` (plays the file dropped on it, else the newest recording), and the README says where a recording goes.
- **Reversible:** Yes.

### T-D14-3 — Power-on memory: random or zero makes no difference to this game (2026-10-25, Stage D14, owner's rule D-004)

- **Facts:** a real console's RAM holds random bytes when it is switched on; Stella's app imitates that (`plr.ramrandom`), but its libretro core, which we use, overwrites the RAM with zeros before every frame, so ours starts with zeros (`docs\DISPLAY.md`).
- **Test:** `--ram-seed N` (new) fills the RAM with pseudo-random bytes before the first frame. Seeds 0, 1, 2, 3, 7 and 99, 3,000 frames each (attract mode, Reset at frame 1800, a game starting): **0 differences** in any picture against the zero start. After the first frame the RAM is byte for byte the same for seed 5 and for zeros: the game's own start-up code rewrites everything.
- **Choice:** keep the zero start (recordings stay exactly repeatable); keep `--ram-seed` as a tool. If Phase E/F ever finds a place where the game reads RAM before writing it, this is the first thing to revisit (F14: where randomness comes from).
- **Not proved:** that the random bytes reached the core's RAM copy by anything but the stock path (they are written the way Stella's own libretro wrapper does it); the proof is that nothing differs, not that something changed.
- **Reversible:** Yes.

### T-D14-1 — The side-by-side partner is Stella's core run by the Phase C tool, not the Stella app (2026-10-25, Stage D14)

- **Choice:** the reference is `stella_script.exe` (the Stella 7.0 core, the same code `third_party\stella` that our program uses, driven frame by frame, zeroed memory at power-on). The same input goes into both: a recording is turned into a Stella input script, and a Phase C script into a recording (`tools\d14_side_by_side.py`).
- **Why not the Stella app:** it cannot play our recordings, starts with random memory (`docs\DISPLAY.md`) and runs on its own clock, so no frame of it can be paired with a frame of ours; it would give a picture to look at, not a measurement. Starting it also needs the owner's OK. An eye comparison stays possible whenever the owner wants it.
- **What is compared:** the fingerprint of every frame (the same FNV hash of the 160 x H palette numbers that Phase C wrote) and, at ten chosen frames, the whole picture byte for byte. The window is compared as well: the real window replays the recording and writes its own fingerprints.
- **Result:** 0 differences everywhere. Inputs: the owner's recording (1,860 frames, with Colour/B&W and the left difficulty switch changed, fire, all directions), a short test recording (219 frames), and the bot route of C7 (9,800 frames: ghost eaten, power pills, death, bonus item, level end, game over). Headless and window replay both: 0 of 9,800, 0 of 1,860. Pictures: 10 of 10 identical on the route (including the eight C7 moments and the game-over picture) and on each recording.
- **The tool can see a difference:** the owner's recording with one key press removed (frame 840) gave 963 differing frames, the first at frame 897.
- **Not covered:** sound (D8 proved it identical to Phase C), the Stella app itself, anything after the last frame of an input.
- **Reversible:** Yes.

### T-D13-1 — Input recordings: a small text file of input changes, starting from power-on (2026-10-25, Stage D13)

- **Format** (`src\host\inputlog.hpp`): line 1 `PacMan2600-inputs 1`; `rom_sha1=<hex>`; `frames=<n>`; then `<frame> <bits>` each time the input changes (frame 0 always; bits = `controls::Input::pack`, which includes the three console switches). Plain text, because it can be read, diffed and fixed by hand, and a 40-second game is about 30 lines. The reader refuses anything else with a plain-words error (wrong first line, no ROM line, bad numbers, frames out of order, a change after the last frame).
- **What is recorded:** the input the console was really given on each frame (after the keyboard/pad merge, after `--reset-at`), taken inside the game thread, so the recording is exactly what was applied, whatever the window thread did meanwhile.
- **Start state:** power-on, empty memory (our emulator, proven equal to the Phase C tool in D4). A recording therefore always replays from the start. The three switches in the recording override the remembered ones.
- **Replay:** `--replay` in the window feeds the recorded input instead of keyboard and pads; when the recording ends the keyboard and pads take over again (the title shows REPLAY / RECORDING). Headless `--replay` runs the whole recording unless `--frames` is given. A recording made with another ROM (SHA-1) is refused.
- **Pacing does not matter:** the input is tied to the frame number, not to time, so a slow or fast window gives the same game (checked: window record, window replay and headless replay agree).
- **Proof:** `test_inputlog` 25 checks. Window recording of 720 frames (injected keys: Reset, four directions, F5/F6/F4) vs headless replay vs window replay (481 frames, a shorter run): **0 differences** in every pair, headless twice identical, 526 different pictures in the run; the replayed window's own recording equals the first one's changes; a 2,399-frame recording (window vs headless) 0 differences. Earlier checks unchanged: key replay 350,460 frames 0 differences, 2,500 frames = C3 fingerprints and = C5 audio, controls 117, layout 173, window state 29, pad-check 31, ROM finder, self-check.
- **Not proved:** a bad file given to the **window** shows a message box (it cannot be answered automatically); headless refusals were checked (wrong ROM, garbage, missing file: exit 6 with the reason).
- **Reversible:** Yes.

### T-D12-1 — The Colour / B&W switch on F4; all three switches remembered (2026-10-25, Stage D12)

- **Choice:** F4 flips the TV type switch (latched like F5/F6, Colour at the very first start). The three switches are saved in `settings.ini` (`left_difficulty=a|b`, `right_difficulty=a|b`, `tv_type=colour|bw`) the moment one changes and restored at start, so a game is played the way it was left. Pads cannot change them. Stella's own events are used (`ConsoleColor` / `ConsoleBlackWhite`, sent every frame like the difficulty events).
- **The picture is not tinted by us:** B&W is what the game does when it reads the switch (C6: it uses grey colours itself).
- **Proof:** `test_controls` 117 checks (9 new); the replay of all recorded Phase C inputs now includes `switches\bw` through the real key code: **350,460 frames, 0 differences** (6,000 more than D10); injected F4/F5/F6 in the real window: switch register values 3f, 7f, ff, f7 (B&W clears bit 3) and the ini file held `left_difficulty=a right_difficulty=a tv_type=bw`; a second run started from that file saw f7 at once (restored); `--headless` 2,500 frames: fingerprints 0 differences from C3 and audio identical to the C5 reference (with Reset at 1800 as in C3); `--pad-check` 31 OK; self-check, layout-check, fullscreen-check, F1 screenshot OK.
- **Not proved by me:** how it looks to the owner's eyes (their try).
- **Reversible:** Yes.

### T-D11-2 — Game area (key 4): four black rows of margin below the life markers (2026-10-25, Stage D11)

- **Found by the owner:** in fullscreen, key 4 "cuts off a small part of the bottom".
- **Measured:** nothing is cut. The game draws to row 199 and the three life markers (6 rows tall) end exactly there; mode 4 used to end on that same row, so the markers touched the screen edge and looked clipped. `--layout-check` in game mode (now with `--reset-at 60` so a game is running) agreed at 2560x1440, 1920x1080 and 1366x768, and in the real fullscreen window (`--fullscreen-check --mode game --reset-at 60`): 0 non-black pixels outside the rows shown.
- **Choice:** `kGameBottom` 200 -> 204: rows 11-203 (193 rows) are shown, so 4 black rows (about 2 %) sit under the markers. The picture is about 2 % smaller sideways. Row 199 is still the last row the game draws.
- **Reversible:** Yes (one number).

### T-D11-1 — PS4 pad through SDL's gamepad layer: buttons, stick rules, plug and unplug (2026-10-24, Stage D11)

- **Choice:** SDL's gamepad layer (already part of the SDL we vendor; no new dependency). Pads are opened when SDL reports them and closed when they go; **what they are doing is read from the pad each time round the window loop**, not remembered from button events, so an unplugged pad can never leave a direction "held" and a pad plugged in with a button already down is right at once.
- **Buttons** (SDL's PS4 driver delivers Share as "back" and Options as "start", checked in `SDL_hidapi_ps4.c` lines 1114-1115): D-pad and left stick = joystick; **Cross or Square = fire**; **Options = Game Reset; Share = Game Select** (held while pressed, like F3 and F2); the difficulty switches stay on the keyboard (F5, F6). Several pads at once: any of them counts. Pad and keyboard work together; opposite directions cancel.
- **The stick** (pure rules in `controls::StickDirection`, tested): a push counts once it passes **50 %** and stays counted until it falls under **30 %** (dead zone with a little hysteresis); **one direction at a time** (the stronger axis wins, so a crooked push of right 92 % and up 55 % is just "right"); a direction already held is kept unless the other axis is more than 25 % stronger, so a stick resting near 45 degrees does not flicker; a full reversal switches at once. Pac-Man is a four-direction game, so no diagonals are sent.
- **Window focus:** while the window is not in front the pad is ignored (like the keyboard being released); when it comes back the pad counts at once.
- **If the PS4 pad is used through Steam or DS4Windows** it looks like an Xbox pad: Menu and View then take the place of Options and Share (SDL's names for them are the same: start and back).
- **Proof (my side):**
  - `test_controls`: 108 checks (70 before; now also buttons, D-pad, the stick's four directions, dead zone, crooked pushes, 45 degrees, reversals, two pads, pad plus keyboard, the F1 list).
  - **`--pad-check`** with a **virtual gamepad** (SDL's own test device, which goes through the same gamepad code as a real pad) playing the real game through the same `Pads` code as the window: **31 of 31 checks OK**: Share = Select (switch register 3d), Options = Reset (3e), both released (3f), D-pad up/down/left/right and left stick up/down/left/right each give the game's own joystick byte (RAM $ED, C6: 1, 2, 4, 8) and return to 0 in the middle, a 25 % push does nothing, a crooked push gives only right, Cross and Square give fire, **unplugging with the stick held right: the pad is closed, $ED returns to 0 and stays 0**, plugging in again works (left gives 4), a pad is ignored while the window is not in front and counts at once when it is.
  - **A real pad was not available to this program at the time** (`real_gamepads_found=0`): the owner's PS4 pad was not connected.
  - **Earlier checks still pass:** the key replay (344,460 recorded frames, 0 differences), injected keys in the real window, audio for 2,500 frames identical to C5, picture fingerprints 0 differences from C3, window state (29), ROM finder, layout (173), self-check, layout-check, fullscreen-check, F1 list screenshot (18 lines, readable); ROM SHA-1s unchanged.
- **Not proved by me:** the real PS4 pad (its radio or cable, its real stick centre and drift, the real button numbering on this PC); the owner's test is the check. A worn stick that does not return to the centre could need a bigger dead zone.
- **Reversible:** Yes.

### T-D10-1 — The keys, and how they become console input (2026-10-24, Stage D10)

- **Keys chosen by Claude Code** (the owner may ask for others; they are one table, `controls::bindings()`):

| Key | Does |
| --- | --- |
| Arrows or W A S D | Joystick (opposite directions at once cancel: a real stick cannot do both) |
| Space or Z | Fire button |
| F2 (held) | Game Select |
| F3 (held) | Game Reset (replaces the temporary R key) |
| F5, F6 | Left, right difficulty switch A / B (real switches: they flip on a fresh press and stay) |
| 1, 2, 3, 4 | Picture mode (D6) |
| Alt+Enter | Fullscreen (D7) |
| F1 | Shows the list of keys over the picture |
| Esc | Quit |

- **Why these:** the game keys are far from anything that does damage by accident (Reset abandons a game, so it is an F-key, not a letter next to WASD). Select and Reset are "held while pressed" because they are the console's momentary switches; the difficulty switches latch because the console's are real switches. Keys are read by **position** (SDL scancodes), so WASD is right on any keyboard layout. When the window loses focus, every held key is released (a key let go while another window had the focus must not stay pressed); the difficulty switches stay where they are. Colour/B&W is D12.
- **How it works:** the window thread turns key events into a small number (nine on/off values); the game thread reads it and gives it to the console at the start of every frame (`applyInput`). The game thread keeps running while the window is dragged (T-D8-2).
- **Proof (my side):**
  - `test_controls`: 70 checks (table, no key used twice and none of the window's keys, arrows and letters, two keys for one direction, opposite keys cancel, held and latched switches, key repeat, focus lost, unrelated keys, the F1 list).
  - **Replay of recorded Phase C inputs through the real key code** (`tools\d10_replay_compare.py`, `--keys-replay`): 55 experiments (`reference\c6\*`: directions, turns, select, fire, left/right/both difficulty A, game variations 1-8, two players, mid-game Reset and Select, and more) **plus** 6,000 frames of random joystick play (`reference\audio\play1`): **344,460 frames compared, 0 differences** in the picture fingerprint of every frame (`reference\d10\replay_compare.txt`). The one experiment that uses Colour/B&W (`switches\bw`) is skipped (D12).
  - **The real window's event loop** (`--timed 11 ... --push-keys`: a helper thread puts F3, Right, A, Up, S, F5 and F6 key events into the window's queue): the game's own joystick byte (RAM $ED, C6: up 1, down 2, left 4, right 8) took the values 0, 1, 2, 4 and 8, and the console switch register took the values 3e (Reset held), 3f, 7f (left difficulty A) and ff (both A). Underruns 0.
  - **F1 list:** `--help-check` in the real window at 1280x960 and 2560x1440, screenshot looked at: all 13 lines readable (`reference\d10\help.png`).
  - **Earlier checks still pass:** audio for 2,500 frames still byte-identical to C5, picture fingerprints 0 differences from C3, window state (29), ROM finder, layout (173), self-check, layout-check, fullscreen-check; 59.99 frames a second, 0 underruns; ROM SHA-1s unchanged.
- **Not proved by me:** that a physical key press arrives (my events are injected into the same queue, not typed); a keyboard that cannot report three keys at once (ghosting) may drop a combination such as two directions plus Fire; how the keys feel.
- **Reversible:** Yes.

### T-D9-1 — Game speed is 60.000 frames a second, set by the sound card's clock (2026-10-24, Stage D9)

- **The question (left open by C4):** pace at **60.000 Hz** (what Stella plays at and what C4 measured: 1,194,720 cycles a second) or **59.923 Hz** (what the console's own crystal gives)?
- **Choice:** **60.000 Hz.** It is the reference the whole project is compared against (Stella, C3 to C7), it keeps the sound exactly 524 sample pairs per frame (31,440 / 60), and the side-by-side checks of D14 compare frames, not seconds. 59.923 Hz stays a possible optional setting later (the real hardware's speed is 0.13 % slower, one frame in 13 seconds).
- **The monitor is never the clock.** The window shows the newest finished picture on every monitor refresh (here 180 Hz, measured 179.94 to 180.06 presents a second), whatever the game speed is.
- **Proof (my side):** see T-D9-2.
- **Reversible:** Yes (one constant, `Pacer::kFrameNs`, plus the sound rate).

### T-D9-2 — Frames are started from a steady timer whose speed is steered by the sound card (2026-10-24, Stage D9)

- **First try (rejected):** run a frame whenever less than the target sound is waiting (the sound card as a pure trigger). Speed was right (60.026 frames a second in 30 s, 0 shortages) but the evenness was poor: because the sound card takes sound in 10 ms chunks, frames were started in an irregular rhythm and on the 180 Hz screen game frames got 2, 3 or 4 refreshes in nearly equal numbers (572, 576 and 616 of 1,801), which would look like judder.
- **Choice:** a **steady PC timer** (evenly spaced frames, precise sleep) whose period is lengthened or shortened by a very small amount depending on how much sound is waiting: `period = 1/60 s x (1 + 4e-6 x (average queue - target))`, never more than 0.1 %. The queue is averaged over about 50 frames to smooth out the sound card's chunks. The result: on average the game runs exactly as fast as the sound card plays; frames are evenly spaced. With no sound device, or `--timer-clock`, the steering is off and the PC's clock is used. If the sound card is very far off, the sound code still pads or drops (counted).
- **Proof (real window, 2560x1440 at 180 Hz, Focusrite USB sound at 44.1 kHz, muted):**
  - **10 minutes** (`--timed 600`): **59.9993 frames a second** (36,000 frames, 0.001 % from 60.000; the limit was 0.1 %), **0 underruns, 0 dropped sound pairs**, 31,438.8 sound pairs pushed per second, queue 30.1 to 70.1 ms, the game thread's longest wait 21.2 ms, no wait over 30 ms. The average steering was +21 ppm (the PC clock and the sound card's clock differ by about 0.002 %).
  - **Refreshes per game frame** (10 minutes, 107,964 presents, 179.94 a second): 3 refreshes: **33,635 (93.4 %)**; 2 refreshes: 1,183; 4 refreshes: 1,166; 1: 9; 5 or more: 3; frames never shown: 3 of 36,000.
  - **Why not 100 % threes:** the sound card's clock, the PC's clock and the monitor's clock all differ by a few thousandths of a percent, so the game's frame time slowly slides across the monitor's refresh boundary (about once every 15 seconds) and for a short stretch frames flip between 2 and 4 refreshes (a timing shift of 1/180 s, about 5.6 ms, for that frame). Removing it would need the picture to be tied to the monitor's refresh (a repeated or skipped game frame now and then). Left as a possible later polish.
  - **Comparison, PC timer only** (`--timer-clock`, 30 s): 3 refreshes for 99 % of frames, but 60.028 frames a second (this PC's clock against the sound card's) and the music would slowly drift against the picture over time; the sound-card steering is what the stage asks for.
  - **Freezes:** an 800 ms freeze of the window thread: 0 underruns, 0 dropped (the game and its sound are on their own thread, T-D8-2); a 300 ms freeze of the game loop itself in `--audio-check`: 24 counted underruns (shows the counter still works).
  - **Earlier checks still pass:** headless audio for 2,500 frames byte-identical to C5, picture fingerprints 0 differences from C3, window-state (29), ROM finder, layout (173), `--self-check`, `--layout-check` and `--fullscreen-check` all OK; ROM SHA-1s unchanged.
- **Not proved by me:** how the speed and smoothness look and feel. The owner's play is the check. Also not tested: a monitor other than this 180 Hz one, and a PC with no sound device (the code falls back to the timer; I could not remove the device to try it).
- **Reversible:** Yes.

### T-D8-3 — The remaining crackle is the original sound: no smoothing (2026-10-24, Stage D8)

- **What the owner reported (second listen, after T-D8-2):** the crackle is still there; the owner thinks it may be inherent to how the 2600 plays sound and asked whether it should be resolved.
- **Evidence it is not our program:** the program's own log for that session (`settings.ini.audio.txt`): 91.8 s, 0 underruns, 0 dropped, 0 shortfall bytes, queue 60.1 to 76.8 ms, game thread's longest wait 17.1 ms. The audio stream is byte-identical to the Stella 7.0 core's output for 2,500 frames (T-D8-1), so any crackle in the samples is also in the reference.
- **Likely source (from `docs\SOUND.md`, C5):** every frame with sound, the game writes volume 0 at scanline 226 and the real volume at scanline 227: a gap of about 64 microseconds 60 times a second. That is audible as a buzz or crackle on the tune and the wail. Rapid pitch steps and the 2600's square-wave tones add to the harshness.
- **Choice:** leave it. Rule 2 (preserve the original behaviour) and Rule 3 (modernization separate): smoothing or filtering would change the original sound. If wanted, it can be an optional, off-by-default setting in a later modernization stage.
- **Not proved by me:** that it sounds the same as Stella's own app by ear. That check (start Stella with `tools\run_stella.cmd`, with the owner's OK, and listen to the same tune and wail) was offered; it is optional.
- **Reversible:** Yes.

### T-D8-2 — The game and its sound run on their own thread (2026-10-24, Stage D8, after the owner's first listen)

- **What the owner heard:** the start tune and the death wail play, no delay, but **crackling**. The program's own note from that run (`settings.ini.audio.txt`): 11.4 s, 40 underruns (the device asked for sound the stream did not have), 49,832 bytes short, queue down to 0.2 ms, 657 pushes instead of about 684, i.e. the loop stopped for roughly half a second once. Runs without anyone touching the window (10 s, 20 s, 30 s, 40 s, 5 minutes) had 0 underruns, and the longest gap of the loop was 25 ms.
- **Cause (likely, not seen directly):** the sound was fed by the same loop that draws the window. On Windows a window that is being moved or resized freezes that loop for as long as the mouse button is held, so no sound is made and the cushion of 67 ms runs dry. The owner's saved window size (1650x1360 at 640,31) shows the window had been moved or resized in that session.
- **Choice:** the emulator, the 60 Hz clock and the sound push now run on a **game thread**; the window thread (events, drawing, Alt+Enter, keys) only copies the latest finished picture under a lock. The R key is passed on as an atomic flag. The picture may freeze while the window is dragged, but the sound goes on. The game thread's longest wait is logged in `<settings>.audio.txt` (`game_thread_longest_gap_ms`).
- **Proof:** `--timed 15 ... --stall-ms 800` freezes the window thread for 0.8 s (like a drag): **0 underruns**, queue 50.1 to 66.8 ms, game thread's longest gap 13.5 ms. Before the change, the same kind of freeze in the sound loop gave 24 underruns for 0.3 s (T-D8-1).
- **Five minutes with the new structure** (real window, muted, `--timed 300`): 0 underruns, 0 dropped, 2,100 padded (the start-up cushion only), queue 46.8 to 66.8 ms, 31,439.8 sound pairs pushed per second (nominal 31,440), 60.003 frames per second, game thread's longest gap 20.0 ms. The sample-for-sample comparison with the C5 audio is still byte-identical (2,500 frames), headless fingerprints 0 differences from C3, and all earlier tests pass.
- **The wafer blip:** the owner has no controls yet (D10), so its sound could not be tried by ear. It is in the byte-identical stream (the blips of C5 are in frames 1928 onward of the compared audio); its ear check moves to D10, when Pac-Man can be steered.
- **Unexplained, noted:** one earlier `--timed 30` window run ended after 8.3 s although nothing in the program stops it; the same command then ran the full 30 s twice. Possibly a key press or window event from the owner's session reached the test window. Not seen again.
- **Still for D9:** the game thread is still driven by the temporary 60 Hz clock, not by the sound card's clock; the cushion and drop rules absorb the small difference (0 dropped, 0 padded after start-up in 5 minutes).
- **Not proved by me:** that the crackling the owner heard was this cause; the owner's second listen is the check. If crackling remains, the next suspects are the sound device's own buffer size or the 31,440 to 44,100 Hz conversion.
- **Reversible:** Yes.

### T-D8-1 — Sound: SDL audio stream at the emulator's own rate, 4-frame cushion, counted shortages, temporary R key (2026-10-24, Stage D8)

- **Choice:** the core's sound (31,440 Hz, 16-bit stereo, about 524 pairs per frame; `docs\SOUND.md`) goes into **one SDL audio stream opened at 31,440 Hz** on the default playback device. SDL converts it to the device's format (here 44,100 Hz, 32-bit float) itself, so no resampling code of ours is needed.
- **Buffer:** before playing, the stream is given a cushion of silence of 4 TV frames (66.8 ms), then the device is started. A frame's sound is dropped if more than 8 frames (about 133 ms) are already waiting, so a slowly drifting clock cannot build up delay; after a shortage a new cushion of silence is added. Every case is counted (`AudioOutput::Stats`: pushed, padded, dropped, underruns, smallest and largest queue).
- **How a shortage is detected:** SDL's "get" callback reports how many bytes the device wanted beyond what the stream had; any value above 0 counts as an underrun. Proof that the counter works: `--audio-check ... --stall-ms 300` freezes the loop once for 0.3 s and the counter reports 24 underruns (28900 bytes short), 4,193 pairs of padding and 1,572 dropped.
- **No sound device:** the game goes on without sound and says so on the error output (no crash).
- **Why not make the sound the master clock now:** that is D9's job. For now the temporary 60 Hz clock still drives the game, so the cushion and drop rules absorb the small difference between that clock and the sound card's.
- **Temporary key R** = the console's Reset switch (held while pressed), because attract mode is silent and the real keyboard is D10. `--reset-at N` does the same from frame N for tests.
- **Test options:** `--mute` plays the stream at zero volume (the whole path runs, nothing is heard); `--audio-check <seconds> <report>` runs with no window; the game window writes `<settings file>.audio.txt` at exit with the same numbers.
- **Proof (my side):**
  - **Sample for sample:** `--headless ... --frames 2500 --reset-at 1800 --audio-wav` gives 5,237,904 bytes of audio (2,500 frames, 31,440 Hz) that are **byte-identical** to the first 5,237,904 bytes of the data in `reference\audio\idle\audio.wav` (the C5 reference made by the Phase C tool). It contains 164,110 non-zero samples: the start tune, the wafer blips and a death sound.
  - **Real device:** the default playback device here is "Speakers (2- Focusrite USB Audio)" at 44,100 Hz, 2 channels, 32-bit float; SDL accepted 31,440 Hz input.
  - **20 s muted run:** 0 underruns, queue 46.8 to 66.8 ms.
  - **5-minute muted run (300.0 s):** 0 underruns, 0 dropped pairs, 2100 padded pairs (start-up cushion), queue 46.8 to 66.8 ms, 60 frames per second, 31438.3 pairs pushed per second (nominal 31,440).
- **Not proved by me:** how it sounds. I cannot hear; the byte-identical stream and the counters are the evidence, and the owner's ear is the check. The owner's first listen (see T-D8-2) heard crackling.
- **Reversible:** Yes.

### T-D7-1 — Borderless fullscreen with Alt+Enter, remembered window state in settings.ini (2026-10-24, Stage D7)

- **Choice:** Alt+Enter calls `SDL_SetWindowFullscreen` with no fullscreen mode set, which in SDL3 means *borderless fullscreen at the desktop's own size* on the monitor the window is on (the primary one at first). No video mode is changed, so switching is instant and other windows are not disturbed. The cursor is hidden in fullscreen. Only Alt+Enter toggles (the plan's key); Esc still quits for now (D10 decides the final keys).
- **Remembered:** `settings.ini` (next to the program, local) gets `window_x`, `window_y`, `window_w`, `window_h` (the windowed rectangle) and `fullscreen` (0 or 1), saved when the window is closed. The rectangle is tracked only while the window is free (not fullscreen, maximized or minimized), so quitting from fullscreen still remembers the windowed place. A maximized window is not remembered as maximized (it is remembered as its last normal rectangle).
- **Safety:** `windowstate::fitToMonitors` corrects what was remembered before it is used: size limited to between 320x240 and the biggest monitor; if the middle of the top edge is not on any monitor (a screen was unplugged, a resolution changed) the window is centred on the primary monitor. Anything unreadable, missing or tiny means "nothing remembered" and the normal 1280x960 window is used.
- **settings.ini is now shared:** the new `settings` module changes one `key=value` line and keeps all other lines. Before, saving the ROM path rewrote the whole file, which would have erased the window state. `rom_finder` uses it (its tests still pass).
- **Not remembered when:** `--window` is given, or for `--self-check`, `--layout-check` and `--fullscreen-check`, or for `--timed` runs (test runs must not change the settings).
- **Proof (my side):** `test_windowstate` 29 checks (settings keep other lines, round trip including negative positions of a monitor on the left, bad data, unplugged monitor, huge/tiny/off-screen windows). `--fullscreen-check` in the real window on this 2560x1440 screen: odd size 1011x757 at 123,87; Alt+Enter (the same handler the key uses) gives borderless fullscreen covering exactly the monitor (2560x1440 at 0,0); the windowed place is remembered; the D6 picture check passes in fullscreen (outline difference 0 px, nothing outside the picture, screenshot looked at); leaving fullscreen brings back exactly 1011x757 at 123,87; the state is saved; `--state-report` starting from that file starts fullscreen and after leaving it gets 1011x757 at 123,87; a windowed state (900x700 at 300,200) is restored exactly; an off-screen/huge state is corrected (2560 wide, centred on the usable area); no saved state gives the normal 1280x960. Files are in `reference\d7\` (local).
- **Not proved by me:** a real physical Alt+Enter key press (my check sends a made-up key event to the same handler), dragging the window with the mouse, a second monitor, and Windows display scaling other than the one in use. The owner's own try is the check for these.
- **Reversible:** Yes. Deleting `settings.ini` resets the window.

### T-D6-1 — Pixel shape from Stella's own aspect correction (19:10 here, not 12:7), three layout modes, resizable window (2026-10-24, Stage D6)

- **Finding:** Stella's source shows that with aspect correction on (default `tia.correct_aspect`), the whole picture (`width` x `TIA height`) is stretched to fill a 4:3 box: `TIASurface.cxx` sets the source to `(width, myTIA->height())` and the destination to the video mode's image rectangle; `Console.cxx` line 734 sizes the display `viewableWidth x viewableHeight` = 320 x 240, and the default window is zoom 3 = 960 x 720. For this ROM the picture is 160 x 228 (`docs\DISPLAY.md`), so a game pixel is `(4/3) x 228 / 160` = **19:10** wide-to-tall. The plan's 12:7 assumed a 192-line picture (4/3 x 192 / 160 = 1.6, and the broadcast figure 12:7 = 1.714 is for ~210 lines).
- **Choice:**
  - The default shape is Stella's, computed from the measured picture size (`layout::stellaShape`), because Stella with aspect correction is the project's reference ("ruler") and the plan's own acceptance test is to match it. `--pixel-aspect W:H` selects another shape (for example `12:7`) for comparison.
  - `src\host\layout.*` (pure arithmetic, no window code) gives the picture rectangle for any window size in three modes: **Sharp** (largest whole-number *vertical* scale; the width follows from the shape), **Fill** (largest size that fits), **TV frame** (like Sharp inside 90 % of the window, leaving a border). Bars at the sides or top and bottom are whatever is left over, so no mode distorts the shape.
  - The picture is drawn with SDL's pixel-art scaling (`SDL_SCALEMODE_PIXELART`, SDL 3.4): pixels stay sharp; only their edges are blended where the horizontal factor is not whole.
  - The window is resizable and opens at 1280 x 960 (4:3, fits a 1440-line screen). Keys 1, 2, 3 choose the mode for now (temporary, D10 gives keys their home); `--mode`, `--window WxH`, `--pixel-aspect W:H` exist for tests. The default mode is chosen at Gate D; Sharp is the starting default. Nothing is remembered between runs yet (D7).
- **Why a whole-number vertical scale only:** with 228 rows, a whole-number scale *in both directions* at the exact shape would need 19 x 10 per pixel (3040 x 2280), larger than any normal screen. Whole vertical scale plus pixel-art scaling horizontally is the nearest crisp result.
- **Proof:** `tests\host\test_layout.cpp` (148 checks: five window sizes x three modes x two shapes, shape within 0.5 %, inside and centred, Sharp whole-number, Fill touches an edge, TV smaller; Fill in 960 x 720 equals Stella's rectangle exactly). `--layout-check` on the real window at 11 size/mode combinations: outline of the drawn maze equals the prediction (largest difference 0 px), nothing non-black outside the picture's rectangle; screenshots looked at.
- **Compared with Stella itself (owner's OK to start it, 2026-10-24):** the Stella 7.0c app started with `tools\run_stella.cmd` and `rom\pacman.a26` showed a 960 x 720 client area. Its window was captured with `PrintWindow` (that window only) and our Fill picture at 960 x 720 was measured against it: first lit row 34, last lit row 609, lit columns 0..959 and the status bar on rows 574..609 in **both**. The black above and below the maze is therefore part of Stella's own picture, not an error of ours. (Colours differ because the Stella app starts from random memory and its attract mode cycles colour schemes, `docs\DISPLAY.md`.) Captures: `reference\d6\stella_printwindow.png`, `reference\d6\960x720_fill.png` (local only).
- **Owner's test of the first version:** key 1 (Sharp) is the starting mode so it changes nothing; key 2 (Fill) enlarged the picture by only about 5 % in the 1280 x 960 window (barely visible); key 3 made it smaller, as designed. The owner also saw that the maze's top and bottom edges do not meet the window's edges: that is the black rows described above.
- **Mode 4, Game area (owner's choice):** like Fill, but only rows 11-199 of the picture are shown (D11: 11-203, see T-D11-2) (`layout::kGameTop`, `kGameBottom`), so the maze and status bar run from the top edge to the bottom edge of a 16:9 window. The pixel shape stays Stella's 19:10 (computed from the whole picture). Nothing the game draws is cut off: rows 11-199 held every non-black pixel in 24,000 frames (C3) and in all 8 C7 pictures; `--layout-check` also counts non-black pixels in rows that are not shown (0). Key 4, `--mode game`.
- **Not proved by me:** the number keys (a simulated key press did not change the mode); the owner's own key presses are the check. The picture while the window is dragged is also not measured.
- **Open for Gate D:** which mode is the default.
- **Reversible:** Yes.

### T-D5-1 — The picture in the window: streaming texture, unscaled, temporary 60 Hz clock, pixel-exact check (2026-10-24, Stage D5)

- **Choice:**
  - Each frame the emulator's colour picture (160 x 228, `Emulator::colours()`) is uploaded to a streaming SDL texture (`XRGB8888`) and drawn **unscaled** (one game pixel = one screen pixel) with nearest-pixel sampling, centred in the 960 x 720 window; the rest stays black. The window is cleared and redrawn on every present, because a presented buffer is not kept.
  - **Temporary pacing:** the emulator is stepped from a simple 60.000 Hz clock (`SDL_GetTicksNS`), at most 5 frames to catch up, then it gives up catching up. 60.000 Hz is what Stella itself plays at (`docs\TIMING.md`). Stage D9 replaces this with the real pacing, so the choice between 60.000 and 59.923 Hz is **not** made here.
  - **Vsync is asked for explicitly** (`SDL_SetRenderVSync`). The D2 code's comment assumed present waits for the screen; that was not verified, and this now makes it so.
  - `--screen-check <png> [rom]` runs 300 frames with no clock, reads the window's own pixels back and compares them with the emulator's colours: 0 pixels differing, 0 non-black pixels outside the picture, and saves a screenshot. `--timed <seconds> <file> [rom]` plays for that long and writes the frames per second it achieved.
- **Why:** unscaled first keeps the picture path simple and exactly checkable; shape and scaling are D6, fullscreen D7. A pixel-exact readback replaces "it looks right".
- **Measured:** 60.046 frames per second over 10 s on this PC (180 Hz screen).
- **Not decided here:** scaling, pixel shape, colour/phosphor presentation (D6), pacing against the monitor (D9).
- **Reversible:** Yes.

### T-D4-1 — Emulator wrapper, headless mode, and how "same as the reference" is proved (2026-10-24, Stage D4)

- **Choice:**
  - `src\host\emulator.*` wraps `StellaLIBRETRO` behind a pointer-to-implementation class, so no Stella header reaches any other host file (T-D1-1). `stella_core` is linked into the host through it. It defines `libretro_read_rom`, which Stella's file layer calls to get the ROM bytes (same as the Phase C tools).
  - The wrapper gives the picture two ways: palette **indexes** (`tia.frameBuffer()`, 160 x 228), which are exact, and **colours** (the core's own display buffer, as Stella shows it, with its palette and phosphor settings). Proofs use the indexes; the PNG uses the colours.
  - `pacman2600 --headless <png> [rom] [--frames N] [--save-frame N] [--raw f] [--fingerprints csv]` runs with no window or dialog (defaults 600 and 300) and writes the PNG and `<png>.txt`. Exit codes: 0 done, 4 ROM problem, 5 no ROM, 6 other failure. The PNG is written with SDL3's own `SDL_SavePNG`: no new library.
  - The picture fingerprint is the same FNV-1a over the indexed picture that the Phase C tools record, so the host's frames can be compared directly with `reference\c3\dump1\frames.csv`.
- **Proof of equivalence:** all 600 per-frame fingerprints equal C3's (0 differences); a run to frame 1500 equals C7's `01_title` raw picture byte for byte; two runs are identical. The host does not send the "Reset released" input every frame as the C3 probe did; the fingerprints show it makes no difference.
- **Not decided here:** how colours, phosphor blending and scaling are presented (D5-D7); the colour PNG is for viewing.
- **Reversible:** Yes.

### T-D3-1 — ROM finding: own SHA-1, search order, what is remembered, what "Use anyway" means (2026-10-24, Stage D3)

- **Choice:**
  - `src\host\rom_finder.*` (no window code) holds the logic. SHA-1 is a small piece of our own code, checked against the three published test vectors ("abc", empty, a million "a") and against the ROM. No library was added (Rule 5).
  - **Search order:** a path given on the command line (used as it is, good or bad, so its problem can be explained); then the saved path; then a `roms` folder next to the `.exe` (any `.a26` that passes, either letter case); then SDL3's Open file dialog.
  - **Checks:** the file exists and is a file; not empty; exactly 4096 bytes (a file of any other size is **not read**, so a huge file can't stall the program); SHA-1 equals `0940fea7…dbb1`.
  - **Remembering:** only a ROM that passed every check is saved, as an absolute path, in `settings.ini` next to the `.exe` (one line, `rom_path=`).
  - **"Use anyway"** appears only when the size is right but the fingerprint differs. It applies to that run only and is never saved. Every other problem offers only "Choose another".
  - Closing the dialog without choosing ends the program (exit code 3) with nothing run.
  - The ROM file is only ever opened for reading.
- **Test hooks:** `--find-rom <file>` (with `--settings`, `--roms-dir` and an optional ROM path) runs the quiet part of the search with no window; `tests\host\test_rom_finder.cpp` builds `build\host\test_rom_finder.exe`. The test never copies or deletes anything.
- **Why:** the ROM is never shipped (Rule 7), so a wrong file must give a clear message; later stages (D4+) need a checked ROM. The 4096-byte rule fits this ROM (plain 4 KB, A4); other games are outside this project.
- **Reversible:** Yes.

### T-D2-1 — Host skeleton: SDL3 renderer window, working name as title, self-check flag (2026-10-24, Stage D2)

- **Choice:**
  - `src\host\main.cpp` + `src\host\CMakeLists.txt`, built by `tools\build_host.cmd` into `build\host`. A Windows-subsystem program (no console window), C++23 (T-B3-2), warnings `/W4`. `SDL3.dll` is copied next to the `.exe` after the build.
  - Window 960x720 created with `SDL_CreateWindowAndRenderer` and cleared to black every frame (the present call waits for the screen refresh, so the loop doesn't spin). It closes on the close button, Alt+F4 or Escape.
  - Title: **"pacman2600 (working name)"**. "Pac-Man" is a Bandai Namco trademark, so the real project name is left to J2; `pacman2600` is already the name of our checkpoint files.
  - `--self-check <file>` makes the program open the window, write its title, pixel size and centre-pixel colour to the file, and quit with exit code 0 (OK) or 2 (wrong). It exists only so the proof doesn't depend on someone looking at a screen.
- **Why:** SDL's renderer is the simplest way to a window that D5-D7 can reuse for pictures and scaling; no extra library. A GUI-subsystem program is what a double-click expects.
- **Not decided here:** high-DPI handling and scaling (D6, D7).
- **Reversible:** Yes.

### T-D1-1 — The host links Stella's core as a static library and calls its libretro wrapper class (2026-10-24, Stage D1)

- **Choice:** the Phase 1 host links `stella_core` (the static library built by `third_party\CMakeLists.txt`) and calls `StellaLIBRETRO` directly. It does not load `stella_libretro.dll` through the `retro_*` functions.
- **Why:**
  - D-001 names Stella's own libretro port as the core. `StellaLIBRETRO` is that port's class, so the emulation code is the same.
  - All five Phase C tools already work this way, and they gave reproducible frames, RAM reads, input events and save/load (C3-C7). The direct class also gives access to the console's internals (the TIA picture buffer, the RIOT's RAM) that tests will compare.
  - One program, no DLL to ship, simpler licence packaging (GPL source offer covers one binary).
- **Limits:** only `src\host\emulator.*` (D4) may include Stella headers, so a later change of link style stays local.
- **Reversible:** Yes.

### T-C7-1 — Reference captures from a search-based bot, proved by replay (2026-10-24, Stage C7)

- **Choice:**
  - `tools\stella_probe\bot.cpp` (`stella_bot.exe`) plays the game by trying short move plans ahead with Stella's own save/load and writes its moves as an input script. The pictures are taken only from a replay of that script from power-on with `stella_script.exe` (new `region` line, which also records the colours under the ghost pen to spot the bonus item).
  - `tools\c7_captures.py` finds the moments from RAM; the committed script is `tools\captures\c7_captures.txt`.
- **Why:**
  - You can't be asked to play an exact sequence, and a human session is not reproducible. A script gives the same pictures on any PC and is reusable for Phase D lockstep tests.
  - The replay is the evidence: it reproduced the bot's outcome, two replays are byte-identical, so save/load is not part of the result.
  - No new dependency; same toolchain as C3–C6.
- **Limits:** the RAM meanings are working labels (`docs\CAPTURES.md`), to be confirmed in Phase F. The fallback (a play session by you) was not needed.
- **Reversible:** Yes. New files only.

### T-C6-1 — A general input-script tool for controls and variations (2026-10-24, Stage C6)

- **Choice:**
  - `tools\stella_probe\script.cpp` builds `stella_script.exe`. It runs Stella's normal `runFrame()` with inputs from a small text script. Each line holds an input over a frame range: directions, fire, Select, Reset, and switch positions, which stay where they're set, as in Stella's `Switches::update()`.
  - Per frame it records the picture and status-bar fingerprints, the colours, the switch positions and the 128 bytes of RAM, plus pictures on request.
  - `tools\c6_experiments.py` generates the scripts and compares runs, and `tools\raw_to_png.py` makes viewable pictures.
- **Why:**
  - Every control can be tried in isolation and compared frame by frame with an otherwise identical run, which shows exactly when and where it makes a difference.
  - The plan had you pressing Select and the switches by hand. The script does it repeatably, and you only did the PS4 pad test, which needs real hardware.
  - The same tool can drive C7's reference captures and Phase E's input recordings.
- **Proof it doesn't change the emulation:** the idle script reproduced all 24,000 of C3's picture fingerprints.
- **Reversible:** Yes. It's a separate program.

### T-C5-1 — Sound measured with a third tool and scripted joystick play (2026-10-24, Stage C5)

- **Choice:**
  - `tools\stella_probe\sound.cpp` builds `stella_sound.exe`, the same instruction-precise frame stepping as T-C4-1. It records:
    - every change of AUDC0/1, AUDF0/1 and AUDV0/1 ($15–$1A);
    - Stella's audio output (`getAudioBuffer()`) as a WAV;
    - per-frame picture and status-bar fingerprints, and the colours used;
    - pictures of chosen frames.
  - **Scripted play** instead of a human session: after Game Reset, the joystick holds a pseudo-random direction for 30–119 frames (a fixed generator, so a seed gives the same run every time). Optionally Game Reset is pressed again every 4,000 frames to start new games.
  - `tools\c5_report.py` groups register data into sounds (effective bits only), cuts WAV clips and makes PNGs.
- **Why:**
  - Repeatable, needs nobody at the keyboard, and records the exact register values a port must reproduce.
  - A human play session in the Stella app couldn't easily be recorded at register level.
  - The names of sounds are checked against evidence: pictures, plus a status-bar change for every scoring blip (376 of 376).
- **Limits:** random steering never ate a power pill or a ghost, or cleared a level, so those sounds are "not yet captured" (`docs\SOUND.md`).
- **Proof it doesn't change the emulation:** the idle run's fingerprints equal C3's, and every re-run reproduced its data exactly.
- **Reversible:** Yes. It's a separate program.

### T-C4-1 — Timing measured at CPU-instruction precision with a second tool (2026-10-24, Stage C4)

- **Choice:**
  - `tools\stella_probe\timing.cpp` builds `stella_timing.exe`. It subclasses Stella's `StellaLIBRETRO` to call its protected frame steps in the same order as `runFrame()`.
  - It unrolls `TIA::updateScanline()` into the identical sequence of `M6502::execute(1)` calls.
  - After each instruction it reads only public, side-effect-free values: `TIA::cycles()`, `scanlines()`, `clocksThisLine()`, `registerValue()` for VSYNC/VBLANK, and `M6502::lastWriteAddress()` / `lastReadAddress()` to spot RIOT timer writes ($294–$297) and timer reads ($284/$286, any mirror).
  - A report script, `tools\c4_report.py`, summarises the output.
- **Why:**
  - **A first version sampled once per scanline, and its numbers were wrong.** Stella's `updateScanline()` stops at the first instruction boundary after a line ends, so samples landed a few cycles late. A "last write was WSYNC" flag also missed most WSYNCs. Instruction-level sampling gives exact positions.
  - **Stella's own WSYNC and timer counters only exist in debugger builds.** Compiling the debugger in would change which code paths run, so it was avoided.
- **Proof it doesn't change the emulation:** all 24,000 picture fingerprints and line counts are identical to C3.
- **Reversible:** Yes. It's a separate program; nothing else depends on it.

### T-C3-1 — Measuring with a probe built on the Stella 7.0 core instead of the debugger by hand (2026-10-24, Stage C3)

- **Choice (a plan change, Rule 12):**
  - The plan said "using Stella's debugger and snapshots". Instead, display facts are measured by `tools\stella_probe`, a small program built from the **same Stella 7.0 code** as the reference app.
  - It drives Stella's own `StellaLIBRETRO` class with default settings and records, for every frame, what the TIA produced: line count, visible window, drawn area, colours and a picture fingerprint.
  - A report script (`tools\c3_report.py`) reads the palette from Stella's source and turns saved frames into PNGs.
  - To build it, `third_party\CMakeLists.txt` gained a `stella_core` static library: the same sources and settings as `stella_libretro`, minus the entry file `libretro.cxx`. The probe supplies the one function from that file it needs, `libretro_read_rom`, with the same three-line body.
- **Why:**
  - It measures **every** frame of a long run, including attract mode, game start, deaths and game over, instead of a few frames inspected by hand.
  - It repeats exactly, and needs no clicking by you.
  - It uses identical emulation code, so the numbers are Stella's own.
  - The same tool can grow into the reference-trace recorder Phase D/E need.
- **Proof that `stella_libretro` didn't change:** after the CMake change, `tools\build_third_party.cmd` reported nothing to rebuild. Ninja rebuilds whenever any compile setting changes.
- **Limits:**
  - The probe reads the raw TIA picture, before Stella's display effects (phosphor).
  - Its emulator starts with zeroed RAM, unlike the app (see `docs\DISPLAY.md`, "Things learned").
- **Reversible:** Yes. The probe and the `stella_core` target can be removed without affecting anything else.

### T-C2-1 — Which ROM file Stella opens, and how its view is read (2026-10-24, Stage C2)

- **Choice:**
  - Stella opens the working copy `rom\pacman.a26`, never the read-only original.
  - What Stella detects (cartridge type, TV format) is read from its in-app "Game Properties" → "Emulation" tab, because `-rominfo` prints nothing on Windows (tried in C2: empty output).
  - The database entry is read from Stella 7.0's own source (`DefProps.hxx`), the same version as the app.
- **Why:**
  - The working copy is the one meant for analysis (T-A3-2), and the original stays untouched even by reading tools.
  - The in-app dialog is Stella's own report.
  - The source entry is exact text that can be checked again at any time.
- **Reversible:** Yes.

### T-C1-1 — Proving the reference app is genuine; keeping your folder name (2026-10-24, Stage C1)

- **Choice:**
  - You had already unzipped Stella to `F:\Ports\Tools\Stella-7.0c`. That folder is kept as it is (not renamed to the planned `F:\Ports\Tools\Stella`).
  - Its identity was proved by downloading the official `Stella-7.0c-windows.zip` again (with your approval) into `reference\_downloads\` (local only), recording its SHA-256, and comparing all 96 files byte for byte. All 96 were identical.
- **Why:**
  - Renaming a folder outside the project needs your permission and gains nothing; the path is simply recorded.
  - `Stella.exe` isn't digitally signed and the original download was gone, so a file-by-file comparison with the official release is the only solid proof. It was done before the app was first run.
- **Reversible:** Yes.

### T-C1-2 — Stella always starts with `-baseinappdir` (2026-10-24, Stage C1)

- **Choice:** Stella is started only through `tools\run_stella.cmd`, which passes Stella's own `-baseinappdir` option. All its settings and saved data then live in `F:\Ports\Tools\Stella-7.0c` (`stella.sqlite3`, `cfg\`, `nvram\`, `state\`), never in `%APPDATA%\Stella`.
- **Why:**
  - It is the "portable" setup the plan asked for: nothing is written into your Windows profile, and the reference app and its settings stay together.
  - It is easy to back up, or to reset by re-unzipping.
  - Proved in C1: after the first start, no `%APPDATA%\Stella` existed and the settings appeared in the app folder.
- **Note:** starting `Stella.exe` directly (by double-click) would use `%APPDATA%\Stella` instead, with separate, default settings. Always use the script for project work.
- **Reversible:** Yes.

### T-B8-1 — What a checkpoint zip contains (2026-10-24, Stage B8)

- **Choice:**
  - Everything in the project folder, including the hidden `.git` history, `rom\EXPECTED_SHA1.txt`, `reference\` (your future recordings) and the downloaded library archives.
  - Two exceptions: `build\`, and every ROM file. ROMs are left out by extension (`.a26`, `.bin`, `.rom`) and also by content: any 4096-byte file whose SHA-1 equals the ROM's makes the script stop.
  - The zip is written with .NET's own zip library and entry names using `/`. (PowerShell 5.1's `Compress-Archive` skips hidden files such as `.git`.)
- **Why:**
  - **Git history:** a checkpoint must be able to replace a damaged project completely, history included.
  - **`build\`:** it is re-created by building (89 MB saved).
  - **ROMs:** they are left out so a backup zip can never spread the ROM by accident, if it is ever copied or shared (Rule 7). The ROM already has its own protected spare in `F:\Ports\ROM-originals\` (B4), so nothing is lost.
- **How to restore:**
  1. Unzip.
  2. Copy the ROM back from `F:\Ports\ROM-originals\`, as the original (then set read-only) and as `rom\pacman.a26`.
  3. Check it against `rom\EXPECTED_SHA1.txt`.
- **Reversible:** Yes.

### T-B8-2 — Checkpoint safety rules and naming (2026-10-24, Stage B8)

- **Choice:**
  - **Location and name:** zips go to `F:\Ports\Pac-Man - Atari 2600 checkpoints\` (the plan's folder), named `pacman2600-<yyyy-MM-dd>-<stage>.zip`, with a `.sha256` file next to each.
  - **Refusals:** the script refuses a destination inside the project or a folder that doesn't exist, and it never overwrites a checkpoint.
  - **Safe writing:** it writes to `.partial` first. It re-opens the finished zip to check the entry count and to confirm no entry is a ROM (by name and content), and only then renames it. On any failure it removes only its own `.partial`.
  - **Restore tests:** they unzip into `restore-test-<stage>` inside the checkpoints folder, which is deleted afterwards with your OK.
- **Why:**
  - A checkpoint stored inside the project would be lost with it, and would end up inside the next checkpoint.
  - The `.partial` step means a half-written zip can never look like a good one.
  - The `.sha256` file lets anyone check later that a zip hasn't been damaged.
- **When to run:** at the end of each phase (the plan), and before any risky step.
  - A checkpoint taken mid-stage includes uncommitted changes. That is fine, because the restore test shows Git restores to exactly the same state.
  - The B8 checkpoint was taken before B8's commit.
- **Reversible:** Yes.

### T-B7-1 — What is left out when unpacking (2026-10-23, Stage B7)

- **Choice:**
  - Stella's `test/roms/` (459 files, 49.4 MB) is not unpacked.
  - SDL3's `CLAUDE.md` and `AGENTS.md` are not unpacked.
  - Everything else is unpacked unchanged, using Python's safe unpacking mode (`filter='data'`), which refuses links and paths that point outside the target folder.
- **Why:**
  - `test/roms/` contains 430 ROM images of other games (bankswitching and controller tests). The project must never store or publish ROMs (Rules 1 and 7). Leaving them out is safer than relying on `.gitignore`, and nothing in the build uses them.
  - SDL's two files are instructions for AI tools working on SDL itself. Claude Code could load them automatically in later sessions, as if they were project instructions. They play no part in building SDL.
- **Proof:** after unpacking, `third_party\` held 0 files with ROM-like extensions. Stella's remaining 1057 files match tag 7.0 exactly.
- **Reversible:** Yes. The archives are kept in `third_party\_downloads\`.

### T-B7-2 — The library sources are kept in Git, byte-identical (2026-10-23, Stage B7)

- **Choice:**
  - The unpacked SDL3 and Stella sources are committed: 3,238 files, 72.1 MB.
  - The archives themselves stay local in `third_party\_downloads\`, which is ignored by Git.
  - `.gitattributes` switches off line-ending conversion for `third_party/SDL3/**` and `third_party/stella/**`.
  - The 8 files the projects' own `.gitignore` files hid (7 SDL `.hlsl` shaders and Stella's `.vscode/settings.json`) are added with `git add -f`.
- **Why:**
  - The project then builds without internet, and its history records exactly which code was used. That suits a preservation project.
  - With your Git's `core.autocrlf=true`, 256 vendored files would otherwise get altered line endings, and `libretro.cxx` would get Windows endings on checkout. That would break the "identical to the release" check and the T-B7-4 text match.
- **Alternatives:**
  - Download at build time (CMake FetchContent): this needs internet, and the site could change.
  - Git submodules: these are more complex for you to manage.
- **Cost:** a larger repository, still far below GitHub's limits. The biggest single file is Stella's bundled `sqlite3.c` at 9.1 MB, against GitHub's 100 MB per-file limit.
- **Reversible:** Yes.

### T-B7-3 — How Stella's libretro core is built (2026-10-23, Stage B7)

- **Choice:**
  - `third_party\CMakeLists.txt` reads the source list from Stella's own `Makefile.common` (148 files).
  - It uses exactly the compiler settings of Stella's Makefile for Microsoft's compiler.
  - It builds `stella_libretro.dll`, the same kind of file Stella's Makefile produces.
  - SDL3 is built by its own CMake files as `SDL3.dll`. Whether to use static or DLL forms in the final program is left to Phase D.
- **Why:**
  - Copying Stella's own recipe keeps the emulation identical to Stella's.
  - Building the DLL, rather than only compiling, also proves everything links (all 25 `retro_*` functions exported).
- **Reversible:** Yes.

### T-B7-4 — Build-time fix for `snprintf` in Stella's `libretro.cxx` (2026-10-23, Stage B7)

- **Problem:**
  - Lines 10–12 of `src/os/libretro/libretro.cxx` say `#ifdef _MSC_VER` / `#define snprintf _snprintf`.
  - On today's compiler this turns `std::snprintf` in Stella's bundled `json.hpp` into `std::_snprintf`, which doesn't exist (error C2039, six places).
  - The same line is still in Stella's development code (checked 2026-10-23).
- **Choice:**
  - The vendored file is left untouched.
  - At configure time, CMake writes a copy to `build\third_party\stella_patched\libretro.cxx` with the condition changed to `#if defined(_MSC_VER) && _MSC_VER < 1900` (compilers before Visual Studio 2015, the condition zlib uses for the same workaround), and compiles that copy.
  - If the original text isn't found exactly, the build stops with an error that points here.
- **Why:**
  - Modern Microsoft compilers have a correct `snprintf`. `_snprintf` is the older, less safe version: it may not end the text properly when it is cut short.
  - The change only affects text formatting in settings and JSON, never the emulation.
  - Keeping the vendored file pristine means the "identical to tag 7.0" check stays true.
- **Reversible:** Yes. It is one block in `third_party\CMakeLists.txt`.

### T-B2-1 — Build Tools download link and how the compiler is checked (2026-10-23, Stage B2)

- **Choice:** Download from `https://aka.ms/vs/stable/vs_buildtools.exe`, Microsoft's own link from its [release history page](https://learn.microsoft.com/en-us/visualstudio/releases/2026/release-history). On 2026-10-23 it redirected to `download.visualstudio.microsoft.com/.../e07fb79d-.../vs_BuildTools.exe` (5,691,480 bytes), the release folder of **18.11.1** (20 October 2026), the current stable release. After the install, Claude Code checks the tools inside a Developer environment it starts by script, not by adding the compiler to the normal PATH.
- **Why:** An official Microsoft link avoids third-party mirrors. Keeping `cl` off the normal PATH is how Microsoft sets it up, and it means nothing else on the PC changes.
- **Reversible:** Yes.

### T-B2-2 — How the Developer environment is started (2026-10-23, Stage B2)

- **Choice:** Start it with `VsDevCmd.bat -arch=x64 -host_arch=x64 -no_logo`, called from a short `cmd` script. The B2 check typed in the install path that `vswhere` had just reported. Scripts from B3 on will ask `vswhere` for the path each time instead of typing it in. The PowerShell `Launch-VsDevShell.ps1` route is not used.
- **Why:** `VsDevCmd.bat` is the most widely used way to start the environment. It needs no change to PowerShell's script-running policy, and the `-arch=x64` options make sure we always get the 64-bit compiler for a 64-bit program, even if Microsoft's default changes. The environment lasts only as long as that one script, so the normal Terminal and PATH stay untouched.
- **Reversible:** Yes.

### T-B3-1 — Where B3's scratch folder goes (2026-10-23, Stage B3)

- **Choice:** `scratch_b3\` inside the project folder, deleted at the end of B3 with your OK.
- **Why:** Working outside the project folder needs your permission, and there's no reason to. No Git repository exists yet, so the folder can't end up in a commit by accident.
- **Reversible:** Yes.

### T-B3-2 — Which C++23 compiler switch the project uses (2026-10-23, Stage B3)

- **Finding:** With MSVC 14.51 (`cl` 19.51.36260):
  - `/std:c++23` is **not recognised**. The compiler only prints warning D9002 and silently builds as **C++14** (`_MSVC_LANG` = 201402).
  - `/std:c++23preview` gives C++23 (202302).
  - `/std:c++latest` gives 202400, which includes post-C++23 features and changes with each compiler update.
  - With `CMAKE_CXX_STANDARD 23`, CMake 4.3.1 picks `/std:c++latest` by itself.
- **Choice:** Pin `/std:c++23preview` for MSVC by setting `CMAKE_CXX23_STANDARD_COMPILE_OPTION` (and the `_EXTENSION_` one) in `CMakeLists.txt`, together with `CMAKE_CXX_STANDARD 23`, `CMAKE_CXX_STANDARD_REQUIRED ON` and `CMAKE_CXX_EXTENSIONS OFF`. B3 proved this works: the build line shows `/std:c++23preview`, and the program reports 202302.
- **Why:** A fixed language level means the same source builds the same way after a compiler update, which matters for a preservation project. `/std:c++latest` can change meaning under us. Microsoft labels the fixed mode "preview" only because the C++23 standard library isn't finished in this compiler.
- **To watch:**
  - D-001 says Stella's libretro Makefile uses `/std:c++23`. On this compiler that flag would silently mean C++14. B7 (and D-stage builds of the core) must use our CMake setup, not that flag, and must check the actual language level.
  - When a future MSVC accepts `/std:c++23`, switch to it.
- **Reversible:** Yes. It is one setting in `CMakeLists.txt`.

### T-B4-1 — How the read-only protection is tested (2026-10-23, Stage B4)

- **Choice:** Open the file with `[System.IO.File]::Open(path, FileMode.Open, FileAccess.Write)` and expect Windows to refuse ("Access … is denied"). No bytes are ever written. `FileMode.Open` never creates or empties a file, so even an unexpected success would leave the ROM untouched. The SHA-1, size and last-written time are checked again afterwards.
- **Why:** The plan asks for "a test write to the original is refused". Actually writing to the irreplaceable file to see if it fails would put the ROM at risk if the protection were missing. Asking for write access proves the same thing with no risk.
- **Reversible:** Not applicable (nothing is changed).

### T-B4-2 — What gets the read-only flag (2026-10-23, Stage B4)

- **Choice:**
  - The read-only flag is set on the original and on the spare in `F:\Ports\ROM-originals\`.
  - The working copy `rom\pacman.a26` is left as it was (writable).
  - The spare keeps the original's file name.
  - `rom\EXPECTED_SHA1.txt` holds the SHA-1 in lowercase on one line.
- **Why:**
  - The plan only names the original. Locking the spare as well protects the backup in the same way, at no cost.
  - The working copy can always be re-made from the original. Leaving it alone keeps B4 to what was approved.
  - The same file name keeps it obvious which file the spare is a copy of.
  - Lowercase matches A3's records and how most tools print SHA-1.
- **Reversible:** Yes (`attrib -R`).

### T-B5-1 — What `.gitignore` blocks (2026-10-23, Stage B5)

- **Choice:**
  - Everything the plan named (`*.a26`, `*.bin`, `rom/`, `reference/`, `build/`), plus:
    - other ROM and image formats and archives that could hold one (`*.rom`, `*.zip`, `*.7z`, `*.rar`);
    - build leftovers (`/out/`, `/scratch*/`, `.vs/`, `CMakeUserPresets.json`, `*.obj`, `*.pdb`, `*.ilk`, `*.exe`);
    - Python caches.
  - Folders are anchored to the project's top level (`/rom/`), so a folder named `rom` deeper in a library isn't hidden by mistake.
- **Why:**
  - A ROM can reach GitHub inside a zip as easily as on its own, and anything once committed stays in history.
  - Git on Windows matches names case-insensitively here (proved: `Test.A26` and `test.BIN` are ignored).
  - `.exe` files are build output, never source. Releases (Phase J) are packaged separately.
- **Trade-off:** if a later stage needs to commit a legitimate `.zip` or `.bin` (for example test data), that one file gets a named exception (`!path`) and a note here.
- **Reversible:** Yes.

### T-B5-2 — Line endings (`.gitattributes`) (2026-10-23, Stage B5)

- **Choice:**
  - `* text=auto`: text is stored with Unix endings inside Git and checked out to suit Windows.
  - `.cmd`, `.bat` and `.ps1` are always Windows endings, and `.sh` is always Unix endings.
  - `.a26`, `.bin`, `.png`, `.wav` and `.zip` are marked binary, so Git never alters them.
- **Why:** Your Git has `core.autocrlf=true` (the installer default). `.gitattributes` makes the result the same on any PC, whatever its settings. `.cmd` files can misbehave with Unix endings, so they are pinned.
- **Reversible:** Yes.

### T-B5-3 — Repository details (2026-10-23, Stage B5)

- **Choice:**
  - The branch is called `main` (`git init -b main`), although Git's system default here is `master`.
  - Empty tracked folders get a one-line `README.md` saying what goes there.
  - `ROADMAP.md`, your original roadmap, is committed as it is.
  - The repository lives only on this PC until Phase J.
- **Why:** `main` is GitHub's default, so Phase J needs no rename. A README is more useful than an empty placeholder file. Committing the roadmap keeps a record of where `PLAN.md` came from.
- **No global settings changed:** the branch name was given on the command line, and no `safe.directory` was needed (F: is NTFS and owned by you).
- **Reversible:** Yes.

### T-B6-1 — How `CLAUDE.md` and the status line are laid out (2026-10-23, Stage B6)

- **Choice:**
  - `CLAUDE.md` holds your rules between `<!-- RULES START -->` and `<!-- RULES END -->` markers. Every sentence is copied unchanged, and only Markdown heading marks (`##`, `###`) are added, so Claude Code sees them as headings.
  - Working preferences and technical facts follow in separate sections, kept short.
  - Line 1 of `PROJECT_STATUS.md` now always has the form `Current stage: <ID> — <title> (Phase <X>)`.
- **Why:**
  - The markers let any later session prove by script that the rules are still word for word (B6 did this: identical, 42 lines).
  - A short `CLAUDE.md` is read in full every session, whereas a long one would crowd out work. Details stay in `PLAN.md`, `DECISIONS.md` and `docs\`.
  - The line-1 form matches B6's completion check ("Current stage: B7").
- **Reversible:** Yes.

### T-A3-1 — Which ROM file is "the original" (2026-10-23, Stage A3)

- **Choice:** The original is `Pac-Man (1981) (Atari).a26` in the project folder's top level. The file with the same name in `F:\Ports` is ignored.
- **Why:** The roadmap names the project-folder copy, and working outside the folder needs your permission.
- **Reversible:** Yes.

### T-A3-2 — Name and place of the working copy (2026-10-23, Stage A3)

- **Choice:** `rom\pacman.a26`.
- **Why:** A short name with no spaces or brackets is safer for build tools and scripts. Its identity is proved by fingerprint, not by name (`docs\ROM_IDENTITY.md`).
- **Reversible:** Yes.

### T-A4-1 — Python for read-only helper scripts; `tools\` folder created early (2026-10-23, Stage A4)

- **Choice:** Stage A4's analysis is a Python script, `tools\a4_structure.py`, run with the Python 3.14.0 already installed (`python -I`, which ignores outside settings). The `tools` folder is created now rather than in B5.
- **Why:** Nothing is installed. A 6502 tracer is much clearer in Python than in PowerShell 5.1, which showed number-handling problems in A3. The script only reads the copy in `rom\`, and B5 would create `tools\` anyway.
- **Limits:** This doesn't decide Python's long-term role. That is still A5's job. If A5 rejects Python, this script can be rewritten or kept as a record.
- **Reversible:** Yes (delete one file).

### T-A3-3 — Which fingerprints to record (2026-10-23, Stage A3)

- **Choice:** MD5, SHA-1, SHA-256 and CRC32.
- **Why:** Stella looks games up by MD5, No-Intro lists CRC32/MD5/SHA-1, and the roadmap uses SHA-1 for the lock in B4. SHA-256 is the strongest, kept for the future.
- **Reversible:** Yes.
