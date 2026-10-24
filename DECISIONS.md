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

## Technical choices made by Claude Code

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
- **Mode 4, Game area (owner's choice):** like Fill, but only rows 11-199 of the picture are shown (`layout::kGameTop`, `kGameBottom`), so the maze and status bar run from the top edge to the bottom edge of a 16:9 window. The pixel shape stays Stella's 19:10 (computed from the whole picture). Nothing the game draws is cut off: rows 11-199 held every non-black pixel in 24,000 frames (C3) and in all 8 C7 pictures; `--layout-check` also counts non-black pixels in rows that are not shown (0). Key 4, `--mode game`.
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
