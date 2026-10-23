# Atari 2600 Pac-Man — Preservation & PC Port Roadmap

Version of October 23, 2026

## How to use this roadmap

This is the master plan for a new, separate project: a faithful PC version of the **Atari 2600** Pac-Man (1982), not the arcade game you already finished as PacArcadeComp. It runs in 10 phases (A to J) and 102 small stages, each sized for one 10–20 minute Claude Code prompt.

Read it in this order:

1. Sections 1–2 (feasibility and approach) once, so you know what is realistic.
2. Section 3 (rules and the stage template) once. It defines what every stage contains, so the stage list stays readable.
3. Section 4 onwards (the stages) one at a time, only when you reach them.
4. The last section tells you what to do tonight: the first milestone and the first prompt.

**What carries over from PacArcadeComp.** That project proved a workflow that suits you: a `PROJECT_STATUS.md` file, one resume prompt per session, a checkpoint after every step, the ROM never bundled, and a native rewrite that still reads its data from the player's own ROM file. This plan reuses all of it. Where the 2600 is different from the arcade machine, the plan says so.

**What I already know about your PC** (so you won't be asked again): Windows 11 version 25H2, a 2560×1440 display at 180 Hz, and a PS4 controller. You also have a GitHub account and have published a project before. Stage A2 only re-checks these, it does not ask.

**This plan is a living document.** Claude Code will copy it into the project folder as `PLAN.md` and keep it current. Where this document and `PLAN.md` disagree later, `PLAN.md` wins, because it holds what was discovered after today.

## 1. Feasibility assessment

All seven goals look achievable, and the 2600 version is in one important way easier to verify than the arcade game was. Nothing below is final until Phase A has inspected your actual ROM file.

### What we expect the ROM to be (to be confirmed, not assumed)

Atari 2600 Pac-Man is commonly documented as a 4 KB cartridge with no bank switching, written for the console's 6507 processor. If Stage A3 finds a different size or an unknown fingerprint, the plan pauses and adapts. The console has no screen memory: the program draws the picture line by line while the TV beam moves ("racing the beam"). It has only **128 bytes of RAM**, and that small number is the key to the whole plan.

### Goal by goal

| Goal | Difficulty | Why |
| --- | --- | --- |
| 1. PC executable (with an emulator inside) | Low | Mature, accurate 2600 emulators exist (Stella) and can be embedded in a small program of our own. |
| 2. Correct aspect ratio and scaling | Medium | The 2600's pixels are not square and the game picks its own visible line count, so the right shape must be measured, not guessed. Your 180 Hz screen also differs from the console's ~60 Hz. |
| 3. Readable source reconstruction | Medium–High | 4 KB of code is small, but much of it is timing-critical drawing code tied to the TV beam. Game logic is the easier half; the drawing "kernel" is the hard half. |
| 4. Removing the emulator | Medium–High | Logic converts to normal code. The picture must be rebuilt by a native renderer that produces the same pixels, including the famous ghost flicker. |
| 5. Proving behavioural equivalence | Medium (unusually good) | All game state fits in 128 bytes. We can run the original and the rewrite side by side and compare every byte on every frame. Few preservation projects have a test this strong. |
| 6. Modern features | Low–Medium | Easy once the native version exists; only after you approve each one. |
| 7. GitHub publication | Low technically, uncertain legally | The tooling is easy and you've done it. Publishing code derived from a disassembly of Atari's program is a legal question you must decide (Stage J1). |

### Main technical challenges

- **Beam-timed drawing.** The original changes graphics registers mid-line. A native renderer must reproduce the *result*, not the cycle timing, and prove it pixel by pixel.
- **Flicker.** The 2600 shows ghosts on alternate frames. That flicker is original behaviour and stays in the preservation version. Any "no flicker" option belongs in Phase I.
- **Randomness.** Ghost choices may depend on a frame counter, input timing, or uninitialised RAM. Stage F14 must find the true source so the rewrite stays in lockstep.
- **Sound.** The TIA chip makes sound from simple counters at roughly 31 kHz. A native sound generator is small but must match tone and volume exactly.
- **Frame pacing.** The NTSC console runs slightly under 60 frames per second; your monitor runs at 180 Hz. Stage D9 handles this without changing game speed.

### What cannot be promised yet

A perfect 1:1 rewrite is likely but not guaranteed. If any routine resists exact reconstruction, the plan documents the difference in `DISCREPANCIES.md` rather than hiding it, and the emulator build stays available as the reference forever.

## 2. Recommended technical approach

Build one small C++ program of our own (the "host") that first runs the ROM through an embedded Stella emulator, then gradually swaps the emulator for native code, while always being able to run both side by side.

### Phase 1 architecture: our host + an embedded emulator

| Part | Who wrote it | Job |
| --- | --- | --- |
| Host program (C++, SDL3 library) | Us, new code | Window, fullscreen, correct scaling, sound output, keyboard and PS4 controller, frame pacing, finding your ROM file. |
| Stella emulation core (libretro version) | Stella team, GPL licence | Pretends to be the 2600's chips: the 6507 processor, the TIA video/sound chip, the RIOT timer/RAM chip. |
| Your `.a26` ROM | Atari, 1982 | The actual game. All gameplay, graphics and sound decisions still come from here in Phase 1. |

In Phase 1 the executable is honestly an **emulator front-end**: native code for everything around the game, emulation for the game itself. The README will say exactly that.

**Why this option.** Three were compared:

- *Just configure Stella the app.* Fastest, but gives us nothing to build Phase 2 on. Rejected.
- *Our host + Stella as a library (recommended).* Accurate from day one, and the host (window, scaling, sound, input) is reused unchanged by the native game later.
- *Write our own emulator first.* Weeks of work that Stella already does better. Rejected for Phase 1.

**Why C++ and SDL3.** Stella and the libretro interface are C/C++, SDL3 is the standard small library for windows, sound and controllers (it handles the PS4 pad), and Microsoft's free compiler builds it on Windows. Stage A5 re-checks this choice and, if your PacArcadeComp tools are still installed, reuses them instead of installing again.

### Phase 2 approach: "Option A" again

The same split you chose for PacArcadeComp:

1. **Disassemble** the ROM into an annotated listing that reassembles into a byte-identical ROM. That proves nothing was missed. The listing stays private (it is Atari's code).
2. **Understand** it routine by routine, recording every finding in `docs/RE_NOTES.md` and a coverage map.
3. **Rewrite** each routine as readable C++ (the game rules), plus a native renderer and sound generator that produce the same pixels and samples.
4. **Read data from the player's ROM** at start-up: maze shape, sprite shapes, colours, sound tables. Our code then contains logic only, not Atari's art.
5. **Lockstep testing**: original and rewrite run with the same recorded inputs; all 128 RAM bytes, the picture and the sound are compared every frame.
6. When every frame matches across the test library, the emulator is removed from the release build. It stays in a separate test tool.

### Testing strategy in one sentence

Every behaviour is pinned by a recorded input file plus the original's output (RAM, picture, sound), and every later change must reproduce it exactly. Phase H lists the full suite.

### Distribution strategy

No ROM in the repository or the release. On first launch the game asks you to pick your `.a26` file once (a normal Windows file-picker), checks its fingerprint, and remembers the location. A `roms` folder next to the `.exe` and a command-line option also work. A `.gitignore` rule and a release-check script block any `.a26` file from being committed or packaged.

## 3. Working rules, safety and the stage template

The 12 engineering rules from your brief are written word for word into a file called `CLAUDE.md` in Stage B6. Claude Code reads that file automatically at the start of every session, so the rules apply even when you forget to repeat them.

### How every session works

1. Open the project folder in Windows Terminal and type `claude`.
2. Paste the **resume prompt** (section 5 of this document).
3. Claude Code reads `PROJECT_STATUS.md`, does exactly one stage, verifies it, makes a checkpoint, updates the status and plan, and tells you in plain words what happened and what's next.
4. You read the summary, look at anything it asks you to look at, and close the window or carry on.

### Permission prompts: what to approve

Claude Code stops and asks before it edits a file or runs a command. Your policy:

| Claude Code asks to… | Your answer |
| --- | --- |
| Read files inside the project folder | Yes. |
| Create or edit files inside the project folder | Yes, if it matches the stage it announced. |
| Run a build, a test, or a `git status`/`git diff`/`git log` | Yes. |
| Run any command you don't recognise | Press **Esc**, then type: `Explain that command in plain words before I approve it: what it does, what it changes, and whether it can be undone.` |
| Delete, move or rename files, or run `git reset`, `git push`, `git checkout -- .`, `rm`, `del` | Only after the plain-words explanation, and only if a checkpoint exists. |
| Install software, touch anything outside the project folder, or anything on GitHub | Never approve on the spot. These are always separate, announced stages. |
| "Yes, and don't ask again for…" | Only for reading files, builds and tests. Never for deletes, installs or Git pushes. |

Never start Claude Code with any "skip permissions" option.

### Three layers of safety

- **The ROM is read-only.** Stage B4 keeps an extra copy of the original outside the project in `F:\Ports\ROM-originals\`, sets Windows' read-only flag on your original, and records its fingerprint. Analysis uses a copy in `rom\` (ignored by Git).
- **Git commits** after every stage: a named snapshot you can return to.
- **Checkpoint zips** in `F:\Ports\Pac-Man - Atari 2600 checkpoints\` at the end of every phase: a full copy of the project, outside Git, in case anything goes badly wrong.

### The stage template

Every stage in `PLAN.md` carries all 16 headings from your brief (Objective, Why it matters, Prerequisites, What Claude Code will do, What I must do, Exact instructions, Required input, Claude Code prompt, Expected result, Verification, Completion criteria, Potential problems, Recovery, Documentation updates, Git checkpoint, Next stage).

To keep this roadmap readable, the stages below show only what is specific to each one. Everything else uses these shared defaults:

| Heading | Default (unless the stage says otherwise) |
| --- | --- |
| Prerequisites | The previous stage is marked Done in `PROJECT_STATUS.md`. |
| Claude Code prompt | The resume prompt. Stages with special prompts show them. |
| Required input | None. Claude Code investigates first and asks only real human decisions. |
| Verification | Claude Code shows evidence (command output, test result, screenshot, file listing). No evidence = not done. |
| Recovery | Stop. Explain what failed, whether anything changed, and whether the project is safe. Return to the last commit with `git restore` only after your approval. Never restart from scratch. |
| Documentation | Update `PROJECT_STATUS.md` and `PLAN.md`; add findings to the matching `docs\` file. |
| Git checkpoint | One commit per stage, message `Stage <ID>: <title>`. Claude Code shows the list of changed files first; you check that no `.a26` file is in it. |
| Next stage | The next one in the list. |

**Stage expansion rule.** At the start of each stage, Claude Code writes that stage's full 16-heading entry into `PLAN.md` using everything learned so far, shows you the short version, then does the work. That is how the plan stays exact without being written months in advance on guesses.

## 4. The roadmap

102 stages in 10 phases (F21 repeats once per routine, so the real session count is higher). Each phase ends at a gate: a checklist that must be fully met before the next phase starts. Stage IDs (A1, F17…) are what you and Claude Code use to talk about where you are.

### Phase A — Project discovery (6 stages)

Find out what we actually have before deciding anything. No game code is written in this phase.

1. **A1 — Install Claude Code and make the empty project folder.**
    - Goal: a working `claude` command and the folder `F:\Ports\Pac-Man - Atari 2600`.
    - You: follow the click-by-click steps in section 8 ("First milestone").
    - Done when: `claude --version` prints a version number, and Claude Code opens inside the new folder.
2. **A2 — Confirm the PC.**
    - Claude Code: reads Windows version, free space on F:, GPU name, display mode, and whether Git, a C++ compiler, CMake or Stella are already installed (from PacArcadeComp).
    - You: nothing, unless asked to plug in the PS4 controller.
    - Done when: `docs\ENVIRONMENT.md` lists each item with the command that found it.
3. **A3 — Identify the ROM without touching it.**
    - You: nothing. Your `.a26` file sits in the project folder's top level; Claude Code finds it there and never moves or edits it.
    - Claude Code: copies it into `rom\`, computes its size and its MD5 and SHA-1 fingerprints, checks the original's fingerprint is unchanged, and looks the fingerprint up in Stella's game database and in public 2600 ROM lists.
    - Done when: `docs\ROM_IDENTITY.md` states the size, fingerprints, the matched title/version (or "unidentified"), and the evidence. A non-4 KB size or an unknown fingerprint pauses the plan for a decision.
4. **A4 — Quick structural look.**
    - Claude Code: reads the last 6 bytes (where the 6502 family keeps its start and interrupt addresses), checks for bank-switching patterns, and estimates code vs data regions. Read-only.
    - Done when: `docs\ROM_IDENTITY.md` gains a "Structure" section with the start address and bank-switching verdict.
5. **A5 — Confirm tools and language.**
    - Claude Code: re-checks section 2's choices (C++, SDL3, Stella libretro core, CMake, Microsoft C++ compiler) against what A2 found and current versions online. Writes the comparison and the decision.
    - You: approve the tool list. This is one of the few real decisions, because it means installing software.
    - Done when: `DECISIONS.md` has entry D-001 "Toolchain" with versions and reasons.
6. **A6 — Discovery report.**
    - Claude Code: a one-page plain-words summary: what the ROM is, what is confirmed, what is still assumed, and any change to this plan.
    - **Gate A:** ROM identified (or the unidentified path agreed), structure known, toolchain approved, `PLAN.md` updated.

### Phase B — Development environment (9 stages)

Install only what A5 approved, verify each tool the moment it's installed, and put the safety nets in place before any game code exists. Installs are done by you, with Claude Code telling you each click, because installers need administrator approval that Claude Code should not have.

1. **B1 — Git for Windows** (skipped if A2 found it).
    - You: download the 64-bit installer from git-scm.com, accept every default screen, finish. No restart needed.
    - Verify: in a new Windows Terminal window type `git --version`. Expected: `git version 2.x.x.windows.x`.
    - If it says "not recognized": close and reopen Terminal; if it persists, sign out and in.
    - Also: set your name and email for commits (Claude Code gives the two `git config` lines; use the email from your GitHub account, or GitHub's private "noreply" address).
2. **B2 — Microsoft C++ compiler and CMake** (skipped if already present).
    - You: install Visual Studio Build Tools (the version A5 recorded). On the "Workloads" screen tick only **Desktop development with C++**; leave the default parts on the right, which include CMake. About 6–8 GB. Restart if asked.
    - Why: this is the program that turns C++ text into an `.exe`.
3. **B3 — Prove the compiler works.**
    - Claude Code: builds a 10-line "hello" program in a scratch folder with CMake, runs it, then deletes the scratch folder (with your OK).
    - Done when: the output shows the compiler version and `hello` running.
4. **B4 — Lock the original ROM.**
    - Claude Code: creates `F:\Ports\ROM-originals\`, asks you to copy (not move) the `.a26` there as a spare, sets your original read-only, writes its SHA-1 into `rom\EXPECTED_SHA1.txt`.
    - Done when: a test write to the original is refused by Windows and the fingerprint matches A3.
5. **B5 — Project skeleton and Git.**
    - Claude Code: creates folders `src\host`, `src\game`, `third_party`, `tools`, `tests`, `docs`, `rom` (local only), `reference` (local only); runs `git init`; writes `.gitignore` blocking `*.a26`, `*.bin`, `rom\`, `reference\`, `build\`.
    - Verify: Claude Code shows that `git status` does not list your `.a26` file or anything in `rom\`.
6. **B6 — The memory files.**
    - Claude Code: writes `CLAUDE.md` (your 12 rules + your working preferences), `PROJECT_STATUS.md`, `PLAN.md` (this roadmap), `DECISIONS.md`, `docs\GLOSSARY.md` (every new term in one line of plain English).
    - Done when: you open `PROJECT_STATUS.md` in Notepad and it says "Current stage: B7".
7. **B7 — Fetch the libraries.**
    - Claude Code: downloads pinned versions of SDL3 and the Stella libretro core source into `third_party\`, each with its licence file and the exact version noted in `docs\DEPENDENCIES.md`. This is a download, so you approve it after seeing the names, sources and sizes.
    - Done when: both compile as empty libraries.
8. **B8 — Checkpoint script and a restore test.**
    - Claude Code: writes `tools\make_checkpoint.ps1`, which zips the project (minus `build\`) into the checkpoints folder with the date and stage ID in the name, refusing to run if a `.a26` would be included in a release folder.
    - Verify: make one zip, unzip it to a temp folder, build it there, then remove the temp folder.
9. **B9 — Gate B.**
    - Checklist: all tools verified, ROM locked, Git ignoring ROMs (proved), memory files present, first commit made, first checkpoint zip exists.

### Phase C — ROM analysis and reference environment (8 stages)

Set up the standalone Stella app as the "ruler" we measure everything against, and write down how the real game looks, sounds and responds before we build anything.

1. **C1 — Install the reference emulator.**
    - You: download the Windows 64-bit **portable** zip of Stella from stella-emu.github.io (version recorded in D-002), unzip it to `F:\Ports\Tools\Stella`. Portable means nothing is installed into Windows.
    - Verify: Stella opens to its game list.
2. **C2 — First run of the original.**
    - You: in Stella, load the copy in `rom\`, play for one minute with the keyboard and with the PS4 pad, then tell Claude Code in a sentence whether it looked and sounded right.
    - Claude Code: reads Stella's game-properties entry for this ROM (cartridge type, TV format, display settings) and records it.
3. **C3 — Display facts, measured.**
    - Claude Code: using Stella's debugger and snapshots, records the number of scanlines per frame, the first and last visible line, the visible height, the 160-pixel width, the colour palette (NTSC), and whether the line count ever changes (during deaths, level changes, attract mode).
    - Done when: `docs\DISPLAY.md` lists each value with how it was measured. No value is copied from memory.
4. **C4 — Timing facts.**
    - Claude Code: measures frames per second in Stella's NTSC mode, the processor speed, and where in each frame the game logic runs. Writes `docs\TIMING.md`.
5. **C5 — Sound facts.**
    - Claude Code: records short WAV clips of each sound from Stella into `reference\audio\` (local only) and lists which sound plays when.
6. **C6 — Controls and game variations.**
    - You: with Claude Code guiding, press Game Select and watch the variation number; try the two difficulty switches.
    - Claude Code: documents joystick directions, fire button use (if any), Select/Reset, each game variation's settings, and what the difficulty switches change. Writes `docs\CONTROLS.md`.
7. **C7 — Reference captures.**
    - Claude Code: guides you to take numbered Stella snapshots of the title screen, start of game, a power pill in effect, a death, a fruit/vitamin, a level end and game over, saved to `reference\screens\`. If you want video too, you choose then whether to install OBS (optional).
8. **C8 — Gate C: reproducible reference.**
    - `docs\REFERENCE.md` explains how anyone can recreate this exact setup (Stella version, settings, ROM fingerprint). Gate checklist: display, timing, sound, controls and variations documented; snapshots present; nothing is "believed" without a measurement noted.

### Phase D — First working PC executable (16 stages)

Grow the host program one visible feature at a time, each checked against Stella, until it is a complete Phase 1 release.

1. **D1 — Architecture note.** `ARCHITECTURE.md` with the three-part table from section 2, file by file: which code is ours, which is Stella's, which behaviour comes from the ROM, and what Phase 2 will replace.
2. **D2 — An empty window.** A black 960×720 window titled with the project name that closes cleanly. You: double-click the `.exe` Claude Code points you to and close it.
3. **D3 — Finding your ROM.**
    - First launch with no ROM known: a Windows "Open file" dialog asks for the `.a26`. The path is remembered in `settings.ini` next to the `.exe`.
    - Also accepted: a `roms` folder next to the `.exe`, or a path on the command line.
    - Checks: file exists, size matches, fingerprint matches A3. A different fingerprint shows a clear warning naming the expected file, with "Use anyway" or "Choose another".
    - Verify: Claude Code runs the four failure cases (missing, empty, wrong size, unknown fingerprint) and shows each message.
4. **D4 — Emulator running blind.** The Stella core runs the ROM without a window for 600 frames and saves frame 300 as a PNG. Compared against C7's matching snapshot.
5. **D5 — The game on screen.** Frames shown in the window, unscaled and small. You: confirm the title screen appears.
6. **D6 — Correct shape and scaling.** See the dedicated section below.
7. **D7 — Windowed and fullscreen.** Borderless fullscreen on your main monitor; **Alt+Enter** toggles; window size and position remembered. Verify on your 2560×1440 screen and with the window dragged to odd sizes.
8. **D8 — Sound.** TIA sound from the core played through SDL with a small, steady buffer. Verify by ear against C5 clips and by Claude Code checking the sample rate and that no buffer underruns are logged in 5 minutes.
9. **D9 — Frame pacing on a 180 Hz screen.** The game advances at the console's measured rate (from C4), never at the monitor's rate. Each game frame is shown for about three refreshes; sound timing is the master clock so music never drifts. Verify: a 10-minute run logs game frames per second within 0.1% of C4's value and no audio gaps.
10. **D10 — Keyboard.** Arrows and WASD for the joystick, plus keys for Select, Reset and the difficulty switches. Listed on screen with **F1**.
11. **D11 — PS4 controller.** D-pad and left stick for the joystick; Options = Reset, Share = Select. Verify with your pad, including unplugging and replugging mid-game.
12. **D12 — Console switches.** The difficulty switches and Color/B&W switch available as settings, because the game may read them.
13. **D13 — Input recording and replay.** The host can record every frame's input to a small `.inputs` file and play it back. Replaying must produce identical frames, proved by comparing picture fingerprints of every frame across two runs. This is the foundation of all later testing.
14. **D14 — Side-by-side check.** Same input recording into our build and Stella; frames compared at 10 moments. Differences go to `DISCREPANCIES.md`.
15. **D15 — Release folder.** `tools\make_release.ps1` builds `PacMan2600-0.1.0-windows.zip` (name changes after J2) with the `.exe`, needed DLLs, licences, README and *no ROM*. The script scans the zip and fails if any `.a26` or 4 KB file of unknown origin is inside.
16. **D16 — Gate D: Phase 1 done.** You play a full game from the release zip, unzipped somewhere new. Checklist: correct shape, fullscreen and windowed, sound, keyboard, pad, ROM picker, replays identical, discrepancies list reviewed, version 0.1.0 tagged in Git.

#### Getting the shape right (stage D6 in detail)

The 2600 does not have square pixels, so the emulator's raw picture is the wrong shape. Stretching it to the monitor is also wrong.

- **Width.** Each line is 160 pixels, set by the TIA's colour clock.
- **Height.** Not fixed: the game decides how many lines to draw. C3 measures it for this ROM.
- **Pixel shape.** Standard NTSC broadcast arithmetic puts a 2600 pixel at about **12:7 (≈1.71) times as wide as it is tall**. D6 confirms this against Stella's own aspect correction and the reference snapshots before using it.
- **Monitor maths, example.** If C3 found 192 visible lines, a 7× vertical scale is 1344 screen lines tall, and 7 × 12/7 = exactly 12 screen pixels per game pixel horizontally (1920 wide). On your 1440-line screen that is both sharp and correctly proportioned, with even borders. D6 recomputes this from the real measured height.

Three modes are built; you pick the default at Gate D:

| Mode | What you see | Trade-off |
| --- | --- | --- |
| Sharp (recommended default) | Largest whole-number scale with correct proportions, centred, black borders | Not edge to edge; perfectly crisp |
| Fill | Largest correct-proportion size that fits, very lightly smoothed edges | Fills more of the screen; pixels slightly soft |
| TV frame | Adds the 2600's black overscan area around the picture, as a CRT showed it | Smaller game; most period-authentic framing |

No mode distorts the proportions. Letterboxing (bars top/bottom) or pillarboxing (bars left/right) is chosen automatically by the monitor's shape.

**Shape test procedure.** Claude Code writes an automatic test that computes the output rectangle for 1920×1080, 2560×1440, 3840×2160, 3440×1440 and a 1280×1024 window, and checks the width-to-height ratio of a game pixel is 12:7 (or the confirmed value) within 0.5%. You then compare a screenshot of the maze against a C7 snapshot shown in Stella with aspect correction on: the maze outline should match when the two are laid over each other.

### Phase E — The original game's behavioural baseline (8 stages)

Build a library of recorded play sessions, each paired with exactly what the original did, so Phase F has a precise target. Every recording is replayed by the emulator build and saved as a "golden" trace: the 128 RAM bytes, a picture fingerprint and a sound fingerprint for every frame. These traces stay local (they come from Atari's program).

1. **E1 — Baseline format.** `docs\BASELINE.md` defines a test entry: ID (e.g. `BL-012`), what it shows, the `.inputs` file, the golden trace, and plain-words observations (score, lives, timings in frames). Claude Code adds a "trace" switch to the host and proves a replay gives a byte-identical trace twice.
2. **E2 — Starting up.** Power-on, attract mode, each game variation chosen with Select, Reset to start. Scripted by Claude Code; no playing needed.
3. **E3 — Pac-Man's movement.** Speed in frames per maze step, turning at corners, pushing into walls, reversing, holding diagonals, wrapping through the side tunnel. Mostly scripted; you play one short recorded session.
4. **E4 — Ghosts.** Leaving the box, chasing, flicker order, speed, the blue/vulnerable state, eyes returning, and how they choose turns. Claude Code also records the same start position with different input timing to see whether ghost choices change (a clue to how randomness works).
5. **E5 — Eating and scoring.** Wafers, power pills, ghosts eaten in a row, the bonus item, extra lives. Every score value is read from RAM and from the screen and must agree.
6. **E6 — Dying and game over.** Collision timing, the death animation and sound, respawn, losing the last life, the game-over screen, starting again.
7. **E7 — Long play.** You play three full games with the recorder on, as far as you comfortably can (the more levels, the better). Claude Code extracts level transitions, speed-ups and any changes between levels.
8. **E8 — Edge cases and Gate E.** Pressing Reset mid-game, switching variation mid-game, flipping difficulty switches during play, very long idle, score near its maximum (reached by a memory-edit test in Stella's debugger, clearly labelled as artificial). Gate: at least 30 baseline entries covering every topic in your brief's Phase E list, each with a golden trace and a plain-words description you could check against the real game.

### Phase F — Reverse engineering and source reconstruction (26 stages)

Turn 4 KB of machine code into understood, documented behaviour, then into readable C++ that matches the original frame for frame. Expect this phase to take the most sessions; F21 alone repeats once per routine.

#### The words, in plain English

| Term | Meaning here |
| --- | --- |
| Disassembly | Turning ROM bytes back into processor instructions (`LDA`, `STA`…). Mechanical; no meaning added. |
| Decompilation | Turning instructions into a higher-level language automatically. Poor results for 2600 code; not relied on. |
| Reverse engineering | Working out *what* the code does and *why*, by reading and experimenting. |
| Source reconstruction | Writing that understanding down as readable code with good names. |
| Native reimplementation | That code running directly on the PC, with no emulated chip. |
| Emulation | Imitating the 2600's chips so the original bytes run unchanged. |
| Translation | A literal, line-by-line port of instructions into C++. Used only as a temporary, clearly-marked fallback. |

#### Tracking progress: the coverage map

`docs\COVERAGE.md` lists every routine, data table and RAM byte with one status each:

| Status | Meaning |
| --- | --- |
| Not investigated | Not looked at yet. |
| Identified | Its location and rough purpose are known. |
| Understood | Its behaviour is written in plain words in `RE_NOTES.md`, with evidence. |
| Reconstructed | Rewritten in C++. |
| Tested | Its own small test passes. |
| Verified | Lockstep-identical to the original across every golden trace. |

Findings are recorded the moment they are made, with the address, the evidence and the date, so no session repeats another's work.

#### The stages

1. **F1 — Analysis tools.** Stella's built-in debugger and disassembler (already installed) plus the portable `dasm` assembler (download approved by you). Python is installed only if A5 approved it; otherwise Claude Code's helper scripts are PowerShell or C++.
2. **F2 — Raw disassembly that rebuilds exactly.** Disassemble, reassemble with `dasm`, and compare fingerprints. Byte-identical rebuild = proof the listing is complete. Stored in `reference\disasm\` (local only).
3. **F3 — Hardware names.** Replace raw addresses of the TIA and RIOT chips with their standard names (`WSYNC`, `GRP0`, `INTIM`…).
4. **F4 — Code versus data.** Mark graphics shapes, maze data, colour and sound tables as data.
5. **F5 — The frame loop.** Find the start-up code and the four parts of every frame: vertical sync, vertical blank (logic time), the visible picture ("kernel"), overscan (more logic time).
6. **F6 — What actually runs.** Replay every golden trace with execution logging; every ROM byte is marked executed, read as data, or never touched.
7. **F7 — Naming all 128 RAM bytes.** Each byte watched across the baseline traces and named (`pacman_x`, `ghost_mode`…), with uncertain names marked "?".
8. **F8 — Routine list and call map.** Every subroutine listed, with who calls it.
9. **F9 — Input reading.**
10. **F10 — Pac-Man's movement and turning.**
11. **F11 — Maze and wafer storage.**
12. **F12 — Collisions** (the 2600 has hardware collision flags; find whether the game uses them or its own checks).
13. **F13 — Ghost movement and decisions.**
14. **F14 — Randomness: where it comes from.**
15. **F15 — Scoring, bonus item and lives.**
16. **F16 — Levels, speed-ups and timers.**
17. **F17 — Game states:** attract mode, start, death, level end, game over, variations, console switches.
18. **F18 — The drawing kernel and flicker schedule.** Exactly which object is drawn on which line in which frame.
19. **F19 — Sound routines and their tables.**
20. **F20 — Lockstep harness.** A test tool that runs the emulator and the native code together on the same inputs, compares all 128 RAM bytes every frame, and on the first difference stops and reports the frame, the byte, both values and the routine responsible.
21. **F21 — Rewrite one routine per session** (F21.1, F21.2…, one per item in F8's list, smallest and leaf routines first). Each rewrite must turn its part of the lockstep test green before the next starts.
22. **F22 — Data loader.** Reads maze, shapes, colours and sound tables from the player's ROM at start-up, by address, after checking the fingerprint. Our source contains addresses and logic, not Atari's data.
23. **F23 — Native renderer.** Produces each 160-pixel-wide frame from the game state; compared pixel for pixel against the emulator on every frame of every golden trace, flicker included.
24. **F24 — Native sound generator.** A small model of the TIA's two sound channels; output compared sample by sample with the emulator's.
25. **F25 — Whole-game lockstep.** All golden traces identical in RAM, picture and sound. Anything not identical is either fixed or entered in `DISCREPANCIES.md` with cause and impact, for your review.
26. **F26 — Gate F: emulator out.** The game build no longer contains Stella; it lives on only in the test tool. You play a full game on the native build. Tag `0.5.0-native`.

#### When a routine is hard to understand

1. Split it into blocks of 5–15 instructions and explain each block separately.
2. Watch it live: breakpoints in Stella's debugger on the golden trace frames where it matters.
3. Experiment: change one RAM byte in the debugger and record what changes on screen.
4. Check public references (Stella docs, 2600 programming guides, community notes), always recording the source.
5. Still unclear after two sessions: write a literal translation marked `// LITERAL: not yet understood`, so the game stays correct, mark it "Reconstructed (literal)" in the coverage map, and come back later. Gate G requires zero literal blocks remaining, or each one approved by you as documented.

### Phase G — Readable source and maintainable architecture (7 stages)

Reshape the verified native code so a stranger can read and mod it, without changing a single frame of output. The lockstep suite runs after every change in this phase; any red result means the change is undone.

1. **G1 — Choose the module layout from the real game.** Claude Code proposes files based on what Phase F found (likely along the lines of game state, maze, player, ghosts, scoring, game flow, renderer, sound, ROM data, platform). It explains each in one sentence; you approve or ask questions. Recorded as D-0xx in `DECISIONS.md`.
2. **G2 — Coding and comment conventions.** `docs\CONVENTIONS.md`: plain names (`ghostLeavesHouse()`, not `sub_F3A2()`), no file over ~400 lines, every function starting with a one-line "what and why", every original-behaviour quirk tagged `// ORIGINAL:` with a link to its `RE_NOTES.md` entry.
3. **G3 — Separate the state from its memory layout.** In Phase F the game state copied the original 128-byte layout so lockstep could compare it directly. Now give it named fields, and move the byte-for-byte mapping into the test tool only.
4. **G4 — Move code into the G1 modules,** one module per session, lockstep green after each.
5. **G5 — Remove leftovers.** Duplicated logic, unused helpers, literal blocks (or your documented approval of each remaining one), and any dependency the game no longer needs.
6. **G6 — "Stranger test".** A fresh Claude Code session, told nothing, is asked to explain how ghosts choose a direction and how scoring works using only the source and docs. Wrong or vague answers mark places that need better names or comments.
7. **G7 — Gate G.** Checklist: lockstep fully green; no file over the size limit; every `ORIGINAL:` tag links to evidence; `ARCHITECTURE.md` updated with a diagram of the modules; the 1.0.0 preservation build tagged. **This tag is the permanent baseline that modern features may never alter.**

Guarding against "generic Pac-Man": no rule may be simplified to what arcade Pac-Man or a tutorial does. If cleaner code would change one byte of RAM or one pixel, the original behaviour wins and the code gets a comment instead.

### Phase H — Automated testing and preservation verification (7 stages)

Testing starts in Phase D and grows every phase; Phase H turns it into a permanent, one-command safety net that guards the 1.0.0 baseline through modernization.

#### What can and can't be tested automatically

| Test kind | Practical? | Notes |
| --- | --- | --- |
| Lockstep RAM comparison (128 bytes per frame) | Yes, the strongest test | Needs your ROM, so it runs on your PC only. |
| Picture comparison, pixel-exact | Yes | Native and emulator frames must be identical, not "similar". |
| Sound comparison, sample-exact | Yes | Small tolerance only if D8's resampling makes exact impossible; documented if so. |
| Unit tests of single modules | Yes | Can use made-up data, so they also run on GitHub without a ROM. |
| ROM-loading and error tests | Yes | Uses fake files of wrong size and fingerprint. |
| Build-from-clean-copy test | Yes | Proves the README's build steps work. |
| Performance (frame time, start-up time) | Yes | Recorded per release. |
| Comparison against a real Atari console | No | No hardware and no capture setup. Stella's accuracy is the stated assumption, written in `TESTING.md`. |
| You playing | Supplementary | Catches "feel" problems; never the only proof. |

#### The stages

1. **H1 — `TESTING.md`.** Lists every test, what it proves, how long it takes, and whether it needs the ROM.
2. **H2 — Unit tests per module** (maze lookups, score table, speed tables, ROM loader, scaling maths).
3. **H3 — Two test commands.** `tools\test_quick.ps1` (unit tests + 5 key traces, a few minutes) after every stage; `tools\test_full.ps1` (everything) at gates and before releases. This keeps each session short, as you asked.
4. **H4 — ROM-free test set for GitHub.** The subset that runs without the ROM, so the public repository can check itself automatically on every change.
5. **H5 — Clean-build and release checks.** A fresh copy built following only `BUILDING.md`; the release zip scanned for ROM files.
6. **H6 — Performance and pacing checks.** Frame-time log on your 180 Hz screen and with the monitor set to 60 Hz; any frame-rate dependence is a failure.
7. **H7 — Gate H: freeze the preservation suite.** `tests\preservation\` is locked: its traces can be added to, never edited. Every later change must pass it.

### Phase I — Modernization discussion (4 stages, then one stage per approved feature)

Development stops here until you decide what, if anything, to add. Nothing in this phase is pre-decided.

1. **I1 — Status report.** What is preserved and verified, what isn't, open discrepancies, and which structural changes would make additions easier.
2. **I2 — Feature menu.** Claude Code presents candidate features one at a time, each with the eight answers your brief requires: what it does, why it's useful, whether it changes game behaviour, effect on fidelity, difficulty, new dependencies, whether it should be optional, and now / later / never. The list in your brief is the starting point; nothing is added because it is common.
3. **I3 — Your decisions.** Recorded in `DECISIONS.md` with your exact wording. Rejected features are kept in a "Considered and declined" list so they aren't proposed again.
4. **I4 — Plan the approved features** as new stages (I5, I6…), each small and each required to keep the preservation suite green.

Two rules for every feature:

- The **Original** mode stays the default and is byte-for-byte the 1.0.0 baseline.
- Anything that changes gameplay (for example removing flicker, or a new mode) is a separate, clearly labelled option, and is tested to prove it changes nothing when switched off.

### Phase J — GitHub and release (11 stages)

Publish only what you have decided is safe to publish, in a private repository first, and never with a ROM. Every `git push` and every GitHub action is approved by you at the moment it happens.

1. **J1 — Legal and licensing decision (yours).** Claude Code prepares a plain-words briefing; it is not legal advice. The points it must cover:
    - **The ROM** is Atari's copyrighted program. Never distributed. Settled.
    - **The reconstructed source** was written by studying Atari's code. It may be treated as a derivative of it. The EU software directive (which applies in Bulgaria) allows decompiling only in narrow cases, mainly for compatibility between programs, so this needs real legal advice before publishing.
    - **The name "Pac-Man"** is a Bandai Namco trademark. The project name and icon should not use it (J2).
    - **Stella's code** is GPL-licensed. Any release containing it (the Phase 1 build, the test tool) must offer its source under the GPL. The native game build no longer contains it. Package listings disagree on whether Stella is "GPL 2 only" or "GPL 2 or later"; Claude Code reads the licence file in the pinned source to settle it.
    - **Your options:** (a) publish full source and `.exe`; (b) publish the host, tools and docs but keep the reconstructed game logic private; (c) publish nothing. You choose after reading the briefing, ideally after asking a lawyer.
2. **J2 — Project name.** A name without the trademark, like you did with PacArcadeComp. Applied to the `.exe`, window title, zip names and repository.
3. **J3 — Content audit.** Every file classified as: our own code, our notes containing ROM addresses, ROM-derived material (must be none in public), emulator code, third-party library, documentation, build tool, generated file. Output: `docs\PROVENANCE.md`.
4. **J4 — Public documentation.** `README.md` (what it is, the ROM requirement in the first screen, how to play), `BUILDING.md`, `docs\HOW_IT_WORKS.md`, and the preservation write-up from `RE_NOTES.md` with any content J1 excluded removed.
5. **J5 — Licences and credits.** `LICENSE` for our code (your choice from J1, explained), `THIRD_PARTY_NOTICES.md` for SDL3 and Stella, and credits to Atari, the original programmer and the Stella team.
6. **J6 — Create the repository, private.** You create it on github.com while Claude Code tells you each click. Claude Code shows the exact file list for the first push; you check for `.a26` files; you approve the push. Sign-in happens in your browser, never by typing a password into Claude Code.
7. **J7 — Automatic checks on GitHub.** A GitHub Actions workflow builds the game and runs the ROM-free tests on every change.
8. **J8 — Versions and changelog.** Numbers like 1.2.0 (big.feature.fix), with `CHANGELOG.md` newest first.
9. **J9 — Release packaging.** The release zip built by script, scanned for ROMs, unzipped and played on your PC from a fresh folder. The README explains Windows SmartScreen's "unknown publisher" warning, since the `.exe` isn't signed.
10. **J10 — Issues and contributions.** A short issue template asking for version, Windows version and ROM fingerprint (never the ROM itself); `CONTRIBUTING.md` only if you want outside contributions.
11. **J11 — Final gate: go public.** You switch the repository to public and publish the release. Checklist: J1 decision recorded, no ROM anywhere in history (Claude Code scans all past commits), licences present, release verified, `PROJECT_STATUS.md` says Done.

## 5. Progress tracking and continuity

The project folder, not any chat, holds the project's memory. Four files carry it:

| File | Holds | When updated |
| --- | --- | --- |
| `PROJECT_STATUS.md` | Line 1: current phase and stage. Then: last stage done and its evidence, open problems, pending decisions, next stage, last session's handoff. | End of every stage |
| `PLAN.md` | This roadmap, each stage marked Not started / In progress / Done, full 16-heading entries for stages already expanded | Every stage |
| `DECISIONS.md` | Every decision (D-001, D-002…): options, recommendation, your choice, date, whether it can be reversed | When decided |
| `docs\` files | Findings: environment, ROM identity, display, timing, RE notes, coverage, baseline, discrepancies | As discovered |

**To see where you are:** open `PROJECT_STATUS.md` in Notepad and read the first line.

**If a session ends suddenly** (usage limit, crash, you close the window): nothing is lost except unfinished work in that stage. The next resume prompt makes Claude Code check `git status` first, tell you what was half-done, and either finish it or undo it with your OK. A stage is never marked Done without its evidence.

**Usage limits.** If you hit your plan's limit mid-stage, just stop; resume when it resets. Stages are kept to 10–20 minutes so this rarely costs much.

### The resume prompt (use at the start of every session)

```text
Read CLAUDE.md, PROJECT_STATUS.md and PLAN.md (if they exist). Do only the single next stage named in PROJECT_STATUS.md.
First run git status (once Git exists) and tell me in plain words if earlier work was left half-done, before touching it.
Then write that stage's full 16-heading entry into PLAN.md and show me a 5-line summary.
Do the work. Investigate and decide technical questions yourself, choosing the safest option, and record it in DECISIONS.md. Ask me only for real human decisions, files only I can give, or approvals.
Never modify, move or delete any .a26 file; only copy it where the stage says. Never install software, delete files, push to GitHub or work outside this folder without asking me first and explaining in plain words.
Verify with evidence and run tools\test_quick.ps1 once it exists. Never claim success without evidence.
If anything fails: stop, explain what happened, whether anything changed and whether the project is safe, and do not start the next stage.
At the end: show me the list of changed files, make the Git commit, update PROJECT_STATUS.md and PLAN.md, and finish with a plain-words handoff: done, proof, next stage, anything you need from me.
```

## 6. Known limitations

- **Claude Code makes mistakes.** It can misread a routine or claim too much. The lockstep tests are what make its work trustworthy, which is why "no evidence, not done" is a rule.
- **Emulator accuracy is an assumption.** Stella is very accurate, but without a real console we can't prove it on every detail. Stated in `TESTING.md`.
- **Flicker and beam-timing** are the hardest parts to rebuild natively (F18, F23). They may take several sessions each.
- **Randomness** may come from sources that are hard to reproduce (F14). If so, lockstep still works, because the emulator and native code get the same inputs.
- **Sound** might match only within a tiny tolerance because of resampling; any tolerance is documented.
- **Legal questions** (J1) can't be settled by Claude Code.
- **Usage limits and session length** slow the pace but don't endanger the project.

If an approach fails outright, the plan proposes the next-best one and says plainly how it differs from the original. The emulator build always remains as a faithful fallback.

## 7. What is needed from you

**Before the first session (all confirmed on October 23, 2026):**

1. Claude plan includes Claude Code: yes, Pro.
2. Project folder: `F:\Ports\Pac-Man - Atari 2600`.
3. Put the `.a26` file in the project folder's top level. It stays there untouched; Claude Code works on copies (A3, B4).

**Later, at the stage that needs it:** approving the tool list (A5), your one-minute play check (C2), recorded play sessions (E3, E7), the default screen mode (Gate D), any literal-code approvals (Gate G), modern features (Phase I), the legal decision and project name (J1, J2).

**Claude Code finds by itself:** your hardware and installed tools, the ROM's identity and structure, display and timing facts, and every technical choice inside a stage.

## 8. First milestone: Claude Code running in your project folder

This is stage A1. About 15 minutes. Nothing here touches your ROM.

1. **Open Windows Terminal.** Press the Windows key, type `Terminal`, press Enter. A dark window opens with a line ending in `>`. This is PowerShell, where you type commands.
2. **Install Claude Code.** Click in the window, type the line below exactly (or copy it, then right-click in the window to paste), and press Enter. Don't run Terminal as administrator; it isn't needed.

    ```text
    irm https://claude.ai/install.ps1 | iex
    ```

    Wait until it says installation is complete. If you see an error mentioning `&&`, you used a Command Prompt line in PowerShell; use the line above instead.
3. **Check it.** Close Terminal completely, open it again, type `claude --version` and press Enter. You should see a version number. If it says "not recognized", sign out of Windows and back in, then try again.
4. **Make the project folder.** Open File Explorer, go to drive F:, open `Ports`, right-click empty space → **New** → **Folder**, name it `Pac-Man - Atari 2600`.
5. **Put two files in it:** this `ROADMAP.md`, and your `.a26` ROM file (copy it in; keep your own copy elsewhere too).
6. **Open Terminal in the folder.** In File Explorer, open the folder, right-click empty space → **Open in Terminal**. The line before `>` should end with `Pac-Man - Atari 2600`.
7. **Start Claude Code.** Type `claude` and press Enter. The first time:
    - It opens your web browser to sign in with your Claude account. Sign in, then return to Terminal.
    - It may ask you to pick a colour theme: use the arrow keys and Enter.
    - It asks whether you trust the files in this folder. Choose **Yes**. This lets it work only inside this folder.
8. **Stage A1 is done when** Claude Code shows its prompt box inside the folder. Type `/exit` and Enter to close it, or carry on with the first prompt below.

### First Claude Code prompt (stage A2)

Paste this as your first message in Claude Code:

```text
We are starting a new long-term project in this folder: a faithful PC version of the Atari 2600 Pac-Man game, from a ROM file I legally own. Read ROADMAP.md in this folder first. It is the master plan. Stage A1 is done (you are running in the project folder). Today, do Stage A2 only.

I am not a programmer. Explain everything in plain words. Do not ask me technical questions; investigate them yourself.

Strict limits for this session: inspect only. Do not install anything. Do not change, move or delete anything outside this folder. My .a26 ROM file is in this folder: do not open, copy, move, rename or change it yet (that is Stage A3). Do not create a Git repository yet (that is Stage B5). If a check needs administrator rights, skip it and say so.

Tasks:
1. Find my Windows version, free space on drive F:, GPU name, and the current resolution and refresh rate of my main display.
2. Check whether these are installed and which versions: Git, CMake, a Microsoft C++ compiler (Visual Studio or Build Tools), Python, the Stella emulator. Check standard install locations, not only the PATH.
3. Write docs\ENVIRONMENT.md listing each finding next to the exact command that produced it. Mark anything you could not check as "not checked" with the reason.
4. Create PLAN.md as a copy of ROADMAP.md with a status marker next to every stage (A1 Done, A2 Done when you finish, all others Not started).
5. Create PROJECT_STATUS.md. Line 1: "Current: Phase A, next stage A3". Then: stages done with evidence, pending decisions, and a short handoff.
6. Finish by telling me in plain words: what you found, what is missing for later phases, and exactly what Stage A3 will need from me.

Never claim something works or exists without showing the evidence.
```

## Sources

- [Claude Code setup (official docs)](https://code.claude.com/docs/en/setup.md): Windows installer command; Git for Windows recommended, PowerShell used if absent; free plan excludes Claude Code.
- [Stella package listing, openSUSE](https://build.opensuse.org/projects/Emulators/packages/stella/files/stella.spec?expand=0): Stella 7.0, licence listed as GPL-2.0-only.
- [Stella package listing, RPM Fusion](https://download1.rpmfusion.org/free/fedora/releases/43/Everything/x86_64/os/repoview/stella.html): licence listed as GPLv2+ (hence the check in J1).
- [stella2014-libretro](https://github.com/libretro/stella2014-libretro): the libretro version of Stella, GPL-2.

Hardware facts in sections 1–2 and D6 (4 KB size, 128 bytes RAM, 160-pixel lines, 12:7 pixel shape) are from general knowledge and are confirmed by measurement in stages A3, A4, C3 and D6 before anything relies on them.
