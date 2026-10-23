# Atari 2600 Pac-Man — Preservation & PC Port Plan

> PLAN.md: the living copy of ROADMAP.md, created in Stage A2 on 2026-10-23. Every stage carries a status marker: [Done], [In progress] or [Not started]. Where this file and ROADMAP.md disagree, this file wins.

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
| Stella emulation core (libretro version; per D-001: Stella's own `src/os/libretro` port at the same version as the reference app, not stella2014) | Stella team, GPL licence | Pretends to be the 2600's chips: the 6507 processor, the TIA video/sound chip, the RIOT timer/RAM chip. |
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

1. **[Done] A1 — Install Claude Code and make the empty project folder.**
    - Goal: a working `claude` command and the folder `F:\Ports\Pac-Man - Atari 2600`.
    - You: follow the click-by-click steps in section 8 ("First milestone").
    - Done when: `claude --version` prints a version number, and Claude Code opens inside the new folder.
2. **[Done] A2 — Confirm the PC.**
    - Claude Code: reads Windows version, free space on F:, GPU name, display mode, and whether Git, a C++ compiler, CMake or Stella are already installed (from PacArcadeComp).
    - You: nothing, unless asked to plug in the PS4 controller.
    - Done when: `docs\ENVIRONMENT.md` lists each item with the command that found it.
3. **[Done] A3 — Identify the ROM without touching it.** *(expanded 2026-10-23; done 2026-10-23: Pac-Man (USA), 1982, CX2646, 4096 bytes, MD5 6e372f07…, SHA-1 0940fea7…; see docs\ROM_IDENTITY.md)*
    - **Objective:** Know exactly which ROM file we have: its size, its MD5 and SHA-1 fingerprints, and which known release of 2600 Pac-Man it matches.
    - **Why it matters:** Every later stage (disassembly, the ROM check in D3, the data loader in F22) depends on this exact file. If it were a hack, a bad dump or a different version, the plan would have to change.
    - **Prerequisites:** A2 marked Done in `PROJECT_STATUS.md` (yes).
    - **What Claude Code will do:** (1) fingerprint the original in the project folder (reading only); (2) create `rom\` and copy the file there as `rom\pacman.a26`; (3) fingerprint the copy and the original again and show all three agree; (4) look the MD5 up in Stella's built-in game database (its source file on GitHub, because Stella isn't installed yet) and in at least one other public 2600 ROM list; (5) write `docs\ROM_IDENTITY.md`.
    - **What I must do:** Approve the prompts for creating `rom\`, copying the file and the internet lookups. Nothing else.
    - **Exact instructions:** None for you beyond approving.
    - **Required input:** None. Only the `.a26` inside the project folder is used. The second file of the same name in `F:\Ports` is not touched.
    - **Claude Code prompt:** The resume prompt.
    - **Expected result:** `rom\pacman.a26` exists, 4096 bytes, with fingerprints identical to the original; `docs\ROM_IDENTITY.md` names the matched title/version.
    - **Verification:** Command output showing size, MD5 and SHA-1 of the original before the copy, of the copy, and of the original after the copy; the database entries quoted with their web addresses.
    - **Completion criteria:** `docs\ROM_IDENTITY.md` states the size, fingerprints, the matched title/version (or "unidentified"), and the evidence. A non-4 KB size or an unknown fingerprint pauses the plan for a decision.
    - **Potential problems:** Stella not installed (use its database source online instead); the website is unreachable (try another public list; if none works, mark the lookup "not checked"); fingerprint unknown (pause, ask you).
    - **Recovery:** Default. The only new things are `rom\` and its copy, which can be removed with your OK; the original is only ever read.
    - **Documentation updates:** `docs\ROM_IDENTITY.md` (new), `PROJECT_STATUS.md`, `PLAN.md`; minor technical choices go in `DECISIONS.md`.
    - **Git checkpoint:** Not possible yet: Git arrives in B5. The B5 first commit will include this stage's files (never the `.a26`).
    - **Next stage:** A4 — Quick structural look.
4. **[Done] A4 — Quick structural look.** *(expanded 2026-10-23; done 2026-10-23: start $F000, no bank switching (plain 4K), ~87% code / ~13% data (estimate); see docs\ROM_IDENTITY.md "Structure")*
    - **Objective:** Learn the ROM's basic layout: where the program starts, whether it uses bank switching, and roughly which parts are program code and which are data (graphics, maze, tables).
    - **Why it matters:** It confirms the plan's main assumption (a plain 4 KB cartridge with no bank switching) before tools are chosen in A5, and gives the disassembly in F2 a head start.
    - **Prerequisites:** A3 Done (yes): `rom\pacman.a26` exists and matches the original's fingerprint.
    - **What Claude Code will do:** Write a small read-only analysis script `tools\a4_structure.py` (run with the Python already installed). It: (1) reads the last 6 bytes, the three "vectors" (start address and two interrupt addresses); (2) looks for bank-switching signs: file size, reads/writes to the known switching addresses, Superchip RAM patterns; (3) follows the program from its start address, instruction by instruction, through every jump and branch it can see, marking reached bytes as code; everything else is "data or unreached"; (4) prints a map. Works only on `rom\pacman.a26`; fingerprint checked before and after.
    - **What I must do:** Approve creating the `tools` folder and the script, and running it.
    - **Exact instructions:** None beyond approving.
    - **Required input:** None.
    - **Claude Code prompt:** The resume prompt.
    - **Expected result:** A start address in the $F000–$FFFF range (the usual place for a 4 KB 2600 cartridge), a "no bank switching" verdict, and a rough code/data map.
    - **Verification:** The script's printed output quoted in the document; the copy's SHA-1 unchanged before and after; the original's SHA-1 unchanged.
    - **Completion criteria:** `docs\ROM_IDENTITY.md` gains a "Structure" section with the start address, the bank-switching verdict and the code/data estimate, each with its evidence.
    - **Potential problems:** Jumps through computed addresses can't be followed by a simple tracer, so some code may be wrongly counted as data. This is an estimate only; F2–F6 produce the exact answer. If bank-switching signs appear, the plan pauses.
    - **Recovery:** Default. The script only reads; deleting it (with your OK) undoes the stage.
    - **Documentation updates:** `docs\ROM_IDENTITY.md` ("Structure"), `DECISIONS.md` (Python for read-only helper scripts), `PROJECT_STATUS.md`, `PLAN.md`.
    - **Git checkpoint:** Not possible yet (Git arrives in B5).
    - **Next stage:** A5 — Confirm tools and language.
5. **[Done] A5 — Confirm tools and language.** *(expanded 2026-10-23; done 2026-10-23: D-001 approved: Microsoft C++ (VS 2026 Build Tools), Git for Windows, SDL3, Stella's own libretro core, Python 3.14)*
    - **Objective:** Fix the exact tool list for the whole project (language, compiler, build tool, libraries, emulator core, Git, Python, assembler) with versions and reasons, and get your approval.
    - **Why it matters:** Everything in Phases B–J is built with these tools. Changing them later is costly, and some mean installing software, which only you may approve.
    - **Prerequisites:** A4 Done (yes).
    - **What Claude Code will do:** (1) re-read A2's findings and check the existing MSYS2 tools' exact package versions (read-only, offline); (2) look up the current versions and status of SDL3, Stella and its libretro core, CMake, the Microsoft compiler, Git for Windows and dasm online; (3) compare "reuse what is on the PC" against "install what the roadmap assumed"; (4) write a recommendation in plain words; (5) after your answer, write D-001 "Toolchain" in `DECISIONS.md` and adjust stages B1/B2 in this plan.
    - **What I must do:** Read the short recommendation and approve it (or choose the alternative).
    - **Exact instructions:** Answer the one approval question Claude Code shows.
    - **Required input:** Your approval.
    - **Claude Code prompt:** The resume prompt.
    - **Expected result:** D-001 recorded with versions, sources and reasons; B1/B2 in this plan marked "skip" or "do" to match.
    - **Verification:** Local version commands shown; every online version quoted with its web address.
    - **Completion criteria:** `DECISIONS.md` has entry D-001 "Toolchain" with versions and reasons and your recorded choice.
    - **Potential problems:** Websites unreachable (mark "not checked"; versions are pinned again at B7); a blocker found with the existing tools (fall back to the roadmap's Microsoft compiler).
    - **Recovery:** Default. This stage installs nothing, so nothing on the PC changes.
    - **Documentation updates:** `DECISIONS.md` (D-001), `docs\ENVIRONMENT.md` (MSYS2 package versions), `PROJECT_STATUS.md`, `PLAN.md`.
    - **Git checkpoint:** Not possible yet (Git arrives in B5).
    - **Next stage:** A6 — Discovery report.
6. **[Done] A6 — Discovery report.** *(expanded 2026-10-23; done 2026-10-23: `docs\DISCOVERY_REPORT.md`; Gate A met, all 4 items with evidence)*
    - **Objective:** Close Phase A with a one-page, plain-words report, and check Gate A item by item.
    - **Why it matters:** Phase B starts installing software. Before that, everything learned so far should be in one place, with a clear line between "proved" and "still assumed".
    - **Prerequisites:** A5 Done (yes).
    - **What Claude Code will do:** (1) re-check each Gate A item against the files that prove it (fingerprints, D-001, the plan's markers); (2) write `docs\DISCOVERY_REPORT.md`: what the ROM is, what is confirmed (with the file that proves it), what is still assumed (with the stage that will check it), and the changes to the plan; (3) tick Gate A in this plan.
    - **What I must do:** Read the report (about one page). Nothing to install or decide.
    - **Exact instructions:** Open `docs\DISCOVERY_REPORT.md` in Notepad, or read Claude Code's summary.
    - **Required input:** None.
    - **Claude Code prompt:** The resume prompt.
    - **Expected result:** The report exists; every Gate A item is ticked with its evidence.
    - **Verification:** The gate check's command output (fingerprints, file contents found) shown in the session and summarised in the report.
    - **Completion criteria:** Gate A: ROM identified (or the unidentified path agreed), structure known, toolchain approved, `PLAN.md` updated.
    - **Potential problems:** A gate item without evidence. If so, Phase A stays open and the gap is named.
    - **Recovery:** Default. Writing a report changes nothing else.
    - **Documentation updates:** `docs\DISCOVERY_REPORT.md` (new), `PROJECT_STATUS.md`, `PLAN.md`.
    - **Git checkpoint:** Not possible yet (Git arrives in B5).
    - **Next stage:** B1 — Git for Windows.
    - **Gate A:** ROM identified (or the unidentified path agreed), structure known, toolchain approved, `PLAN.md` updated.

### Phase B — Development environment (9 stages)

Install only what A5 approved, verify each tool the moment it's installed, and put the safety nets in place before any game code exists. Installs are done by you, with Claude Code telling you each click, because installers need administrator approval that Claude Code should not have.

1. **[Done] B1 — Git for Windows** (needed: A2 found no Git for Windows; D-001 chose it). *(expanded 2026-10-23, done 2026-10-23)*
    - **Objective:** Git for Windows installed and working, with your name and email set for commits.
    - **Why it matters:** Git takes a named snapshot after every stage from B5 on. It is the main way to undo mistakes, and it is how the project reaches GitHub in Phase J.
    - **Prerequisites:** Gate A met (yes). Git not yet installed (checked 2026-10-23: not on PATH, `C:\Program Files\Git` absent). PC is x64 (Intel i5-12400F, `PROCESSOR_ARCHITECTURE` = AMD64), so the x64 installer is the right one.
    - **What Claude Code will do:** Give the exact download link and clicks; after you install, verify with `git --version` and `where.exe git`; once you give your name and choice of email, run the two `git config --global` lines and show they took effect; record the exact version in `docs\ENVIRONMENT.md`.
    - **What I must do:** Download and run the installer (it asks for administrator approval: answer Yes), accept every default screen, then tell Claude Code it's done and which name and email to put on your commits.
    - **Exact instructions:** (1) Open <https://git-scm.com/install/windows> in your browser. (2) Click **"Click here to download"**. The file is `Git-2.56.0.3-64-bit.exe` (current version 2.56.0(3), released 2026-10-22). (3) Open the downloaded file; Windows asks "Do you want to allow this app to make changes": click **Yes**. (4) On every screen click **Next** without changing anything, then **Install**. (5) On the last screen untick "View Release Notes", then click **Finish**. (6) **Close this Claude Code window and Terminal completely, open a new Terminal in the project folder, start `claude`, and paste the resume prompt.** A new window is needed so it can see Git.
    - **Required input:** The name to show on your commits; your GitHub email or GitHub's private "noreply" address (found on github.com → Settings → Emails, looks like `12345678+username@users.noreply.github.com`).
    - **Claude Code prompt:** The resume prompt (after reopening).
    - **Expected result:** `git --version` prints `git version 2.56.0.windows.3` (or newer); `git config --global user.name` and `user.email` print your chosen values.
    - **Verification:** Command output of the above, plus `where.exe git` showing `C:\Program Files\Git\cmd\git.exe`.
    - **Completion criteria:** All three commands show the expected values in a fresh session.
    - **Potential problems:** "git is not recognized" → close and reopen Terminal; if it persists, sign out of Windows and back in. SmartScreen warning → only accept if the file came from the git-scm.com link. Installer defaults: editor Vim (harmless: Claude Code always writes commit messages on the command line), line endings "Checkout Windows-style, commit Unix-style" (B5 adds a `.gitattributes` file so this doesn't matter).
    - **Recovery:** Uninstall via Windows Settings → Apps → Git → Uninstall; nothing in the project depends on Git until B5.
    - **Documentation updates:** `docs\ENVIRONMENT.md` (Git version and path), `DECISIONS.md` (D-001 note: installed version), `PROJECT_STATUS.md`, `PLAN.md`.
    - **Git checkpoint:** None yet (the repository is created in B5).
    - **Next stage:** B2 — Microsoft C++ compiler and CMake.
    - **Result (2026-10-23):** In a fresh session: `git --version` → `git version 2.56.0.windows.3`; `where.exe git` → `C:\Program Files\Git\cmd\git.exe` (the only one on PATH); `git config --global user.name` → `mariqtereza-del`; `user.email` → `mariqtereza@gmail.com`. These were already set on 2025-10-26; you chose "Keep the current ones", so no `git config` line was run. Recorded in `docs\ENVIRONMENT.md` and the D-001 note in `DECISIONS.md`. No commit (no repository until B5).
2. **[Done] B2 — Microsoft C++ compiler and CMake** (needed: A2 found none; D-001 chose it). *(expanded 2026-10-23, done 2026-10-23)*
    - **Objective:** Visual Studio 2026 Build Tools installed with the "Desktop development with C++" workload, so the PC has Microsoft's C++ compiler (`cl`), CMake and Ninja, with their exact versions recorded.
    - **Why it matters:** The compiler turns C++ text into an `.exe`. CMake and Ninja are the build tools that drive it. Nothing in Phase D can be built without them.
    - **Prerequisites:** B1 Done (yes). Checked 2026-10-23: no Visual Studio Installer (`vswhere.exe` absent), no `C:\Program Files\Microsoft Visual Studio`, and `cl`, `cmake`, `ninja` not on PATH. Free space: C: 678.8 GB, F: 86.7 GB. Current stable release per Microsoft's release history: **Visual Studio 2026 18.11.1, 20 October 2026**.
    - **What Claude Code will do:** Give the official download link and every click; after you install, check the downloaded file's digital signature is Microsoft's (optional extra safety, if you run that check before installing); then verify with `vswhere` (product, version, install path, workload present) and, inside a Developer environment started by script, `cl` (MSVC version), `cmake --version` and `ninja --version`; record everything in `docs\ENVIRONMENT.md` and the D-001 note.
    - **What I must do:** Download and run the installer (it asks for administrator approval: answer Yes), tick one workload, click Install, wait (roughly 15–40 minutes depending on internet speed), then reopen Terminal and Claude Code.
    - **Exact instructions:** (1) In your browser open <https://aka.ms/vs/stable/vs_buildtools.exe>. This is Microsoft's own short link from its release-history page; it downloads `vs_BuildTools.exe` (a small starter file, a few MB). (2) Open the downloaded file. Windows asks "Do you want to allow this app to make changes": the publisher shown must be **Microsoft Corporation**; click **Yes**. (3) A window says the Visual Studio Installer needs to set itself up: click **Continue** and wait. (4) The "Installing — Visual Studio Build Tools 2026" window opens on the **Workloads** tab. Tick **Desktop development with C++** (top-left area, under "Desktop & Mobile"). Tick nothing else. (5) Look at the "Installation details" list on the right. Leave its default ticks as they are; check that **C++ CMake tools for Windows** is among the ticked items (it is by default). (6) Leave the install location as it is (on C:). Bottom-right: leave "Install while downloading" and click **Install**. (7) Wait until it says the install succeeded. If it asks to restart Windows, restart. (8) Close the installer, **close this Claude Code window and Terminal completely**, open a new Terminal in the project folder, start `claude`, and paste the resume prompt.
    - **Required input:** None besides doing the install.
    - **Claude Code prompt:** The resume prompt (after reopening).
    - **Expected result:** `vswhere` reports "Visual Studio Build Tools 2026", version 18.11.x (or newer), with the workload `Microsoft.VisualStudio.Workload.VCTools`; `cl` reports "Microsoft (R) C/C++ Optimizing Compiler Version 19.5x… for x64" (MSVC 14.5x, D-001 asked for 14.51 or newer); `cmake --version` and `ninja --version` print versions from inside the Build Tools folder.
    - **Verification:** The command output above, shown in the session and copied into `docs\ENVIRONMENT.md`.
    - **Completion criteria:** All four checks (vswhere, cl, cmake, ninja) pass in a fresh session; exact versions recorded.
    - **Potential problems:** Installer stuck or fails on download → click Retry, check internet, or close and reopen `vs_BuildTools.exe` (it resumes). "Restart required" → restart, then reopen. `cl` "not recognized" in a normal Terminal → this is **normal**: the Microsoft compiler is only on PATH inside a "Developer" environment, which Claude Code starts by script (`VsDevCmd.bat`/`Launch-VsDevShell.ps1`). MSYS2's GCC/CMake (not used, D-001) are not on PATH, so they can't be picked up by mistake.
    - **Recovery:** Uninstall from Windows Settings → Apps → "Visual Studio Build Tools 2026", or modify via the "Visual Studio Installer" app. Nothing in the project depends on it until B3.
    - **Documentation updates:** `docs\ENVIRONMENT.md` (versions and paths), `DECISIONS.md` (D-001 note: installed versions), `PROJECT_STATUS.md`, `PLAN.md`.
    - **Git checkpoint:** None yet (the repository is created in B5).
    - **Next stage:** B3 — Prove the compiler works.
    - **Result (2026-10-23):** In a fresh session: `vswhere` → "Visual Studio Build Tools 2026", 18.11.1 (`18.11.12315.225`), `isComplete: 1`, no restart needed, at `C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools` (the installer's default is the `(x86)` folder). It has the workload `Microsoft.VisualStudio.Workload.VCTools` and the component `Microsoft.VisualStudio.Component.VC.CMake.Project`. Inside `VsDevCmd.bat -arch=x64 -host_arch=x64`: `cl` → "Version 19.51.36260 for x64" (toolset MSVC 14.51.36231); `cmake --version` → `4.3.1-msvc1`; `ninja --version` → `1.13.2`, all from inside the Build Tools folder. None of them are on the normal PATH (as intended). One harmless `'vswhere.exe' is not recognized` line comes from Microsoft's setup script (explained in `docs\ENVIRONMENT.md`). ROM SHA-1s unchanged. Recorded in `docs\ENVIRONMENT.md`, the D-001 note and T-B2-2 in `DECISIONS.md`. No commit (no repository until B5).
3. **[Done] B3 — Prove the compiler works.** *(expanded 2026-10-23, done 2026-10-23)*
    - **Objective:** Build and run a tiny C++23 "hello" program with the exact chain the port will use: CMake 4.3.1 + Ninja 1.13.2 + MSVC 14.51 (x64), inside the Developer environment.
    - **Why it matters:** B2 proved the tools exist. B3 proves they work together to produce a running `.exe`, before any real code depends on them. A broken chain found now costs minutes. Found in Phase D, it would cost hours.
    - **Prerequisites:** B2 Done (yes: Build Tools 18.11.1, `cl` 19.51.36260 x64, CMake 4.3.1-msvc1, Ninja 1.13.2).
    - **What Claude Code will do:** (1) Create a scratch folder **inside the project**, `scratch_b3\` (T-B3-1). (2) Write three small files: `main.cpp` (prints "hello" plus the compiler version, the C++ language level and whether the program is 64-bit), `CMakeLists.txt` (asks for C++23, no extensions) and `build_b3.cmd` (starts the Developer environment, finding its path with `vswhere`, then runs CMake configure, build and the program). (3) Run it and show the output. (4) Ask your OK, then delete `scratch_b3\`. (5) Record the result in `docs\ENVIRONMENT.md`.
    - **What I must do:** Approve deleting the scratch folder at the end (or say keep it).
    - **Exact instructions:** None beyond answering the deletion question.
    - **Required input:** None.
    - **Claude Code prompt:** The resume prompt.
    - **Expected result:** CMake reports "The CXX compiler identification is MSVC 19.51.36260.0" and the generator "Ninja". The build ends with "Linking CXX executable hello.exe". Running it prints `hello`, `_MSC_FULL_VER` 195136260, a C++ language level of at least 202302 (C++23) and "64-bit".
    - **Verification:** The full command output shown in the session. The exit code of `hello.exe` is 0. `scratch_b3\` is gone afterwards (if you approve), and the ROM SHA-1s are unchanged.
    - **Completion criteria:** All of the expected results shown. Scratch folder removed, or kept on your instruction.
    - **Potential problems:** `VsDevCmd.bat` prints the harmless "'vswhere.exe' is not recognized" line (known since B2). CMake picks up the wrong compiler → the script passes `-DCMAKE_CXX_COMPILER=cl` explicitly. MSVC reports `__cplusplus` as 199711 unless `/Zc:__cplusplus` is on → the program prints `_MSVC_LANG` instead, which is always correct. Antivirus slows the first build → harmless.
    - **Recovery:** Stop and show the error. Only files inside `scratch_b3\` were created, so removing that folder returns the project to its state at the end of B2.
    - **Documentation updates:** `docs\ENVIRONMENT.md` (B3 section with the output), `DECISIONS.md` (T-B3-1), `PROJECT_STATUS.md`, `PLAN.md`.
    - **Git checkpoint:** None yet (the repository is created in B5).
    - **Next stage:** B4 — Lock the original ROM.
    - **Result (2026-10-23):**
      - The chain works. CMake 4.3.1 + Ninja found "MSVC 19.51.36260.0", built `hello.exe`, and it printed `hello`, `_MSC_FULL_VER: 195136260`, `_MSVC_LANG: 202302` and `8 bytes (64-bit)`, then exited with code 0.
      - **Finding (T-B3-2):** CMake's own C++23 mapping is `/std:c++latest` (202400, moves with updates), and this compiler ignores `/std:c++23` (warning D9002, silently C++14). The project pins `/std:c++23preview`, proved in a clean rebuild (the build line shows the flag).
      - `scratch_b3\` was deleted with your OK.
      - ROM SHA-1s unchanged.
      - Recorded in `docs\ENVIRONMENT.md`, and T-B3-1 / T-B3-2 in `DECISIONS.md`.
      - No commit (no repository until B5).
4. **[Done] B4 — Lock the original ROM.** *(expanded 2026-10-23, done 2026-10-23)*
    - **Objective:** The original `Pac-Man (1981) (Atari).a26` has a spare copy outside the project in `F:\Ports\ROM-originals\` and is protected by Windows' read-only flag. Its SHA-1 is written in `rom\EXPECTED_SHA1.txt` so any later script can check it.
    - **Why it matters:** The ROM is the one thing in this project that can't be recreated. The read-only flag stops accidental overwrites (by a tool, a script or a slip). The spare copy outside the project survives even if the whole project folder is damaged. The written fingerprint lets every later stage prove the ROM is still the same.
    - **Prerequisites:** B3 Done (yes). Checked 2026-10-23:
      - The original's SHA-1 is `0940FEA7F04CDB6D4B90C5AD1A7E344E68F6DBB1`, the same as A3.
      - Its attributes are `Archive` only (not read-only), and it was last written 1997-08-26 03:17:44.
      - `F:\Ports\ROM-originals` does not exist, and F: has 86.7 GB free.
    - **What Claude Code will do:**
      1. Explain the outside-the-folder steps and get your approval.
      2. Create `F:\Ports\ROM-originals\`.
      3. **Copy** (never move) the original there under the same name, then check that the copy's SHA-1 matches.
      4. Set the read-only flag on the original and on the spare copy.
      5. Prove the protection with a **non-destructive** test: ask Windows to open the original for writing, without writing or truncating anything (T-B4-1). Windows must refuse.
      6. Re-check the SHA-1, size and last-written time of the original.
      7. Write `rom\EXPECTED_SHA1.txt`.
      8. Record everything in `docs\ROM_IDENTITY.md`.
    - **What I must do:** Approve the three outside-the-folder or attribute-changing steps: creating the folder, making the copy, and setting the read-only flags.
    - **Exact instructions:** Answer the approval question. Nothing else.
    - **Required input:** Your approval.
    - **Claude Code prompt:** The resume prompt.
    - **Expected result:**
      - `F:\Ports\ROM-originals\Pac-Man (1981) (Atari).a26` exists, 4096 bytes, with SHA-1 `0940FEA7…DBB1`.
      - Both the original and the spare show `ReadOnly`.
      - The open-for-writing test fails with "Access to the path … is denied".
      - The original's SHA-1, size and last-written time are unchanged.
      - `rom\EXPECTED_SHA1.txt` holds the SHA-1.
    - **Verification:** Command output for each of the above, shown in the session.
    - **Completion criteria:** All expected results shown. No `.a26` content changed (SHA-1 before = after).
    - **Potential problems:**
      - The write test unexpectedly succeeds in opening the file. It writes nothing and doesn't truncate (opened with "Open", not "Create"), so the ROM stays safe. Stop and investigate.
      - Antivirus or OneDrive locking the file: the step fails, nothing changes, and it is retried.
      - Later tools that legitimately need to read the ROM are not affected, because read-only still allows reading.
    - **Recovery:** Every step can be reversed:
      - The read-only flag is removed in Properties → untick "Read-only", or with `attrib -R`.
      - The spare copy and folder can be deleted (with your OK).
      - The original's contents are never written.
    - **Documentation updates:** `docs\ROM_IDENTITY.md` (B4 section), `DECISIONS.md` (T-B4-1, T-B4-2), `PROJECT_STATUS.md`, `PLAN.md`.
    - **Git checkpoint:** None yet (the repository is created in B5).
    - **Next stage:** B5 — Project skeleton and Git.
    - **Result (2026-10-23):**
      - With your approval ("Yes, do all three"), `F:\Ports\ROM-originals\` was created and the original was copied there (SHA-1 match, 4096 bytes, same date).
      - The read-only flag was set on both files (`attrib` shows `A R`).
      - The open-for-writing test was refused on both: `UnauthorizedAccessException: Access to the path … is denied`.
      - The original is unchanged afterwards: SHA-1 `0940FEA7…DBB1` (same as A3), 4096 bytes, written 1997-08-26 03:17:44.
      - `rom\EXPECTED_SHA1.txt` written, and its value matches.
      - Recorded in `docs\ROM_IDENTITY.md` ("Protection"), and T-B4-1 / T-B4-2 in `DECISIONS.md`.
      - No commit (no repository until B5).
5. **[Done] B5 — Project skeleton and Git.** *(expanded 2026-10-23, done 2026-10-23)*
    - **Objective:** The project has its folder layout, a Git repository with a `.gitignore` that keeps every ROM file out, a `.gitattributes` that fixes line endings, and its first commit holding all work from A1–B5.
    - **Why it matters:**
      - From now on every stage ends with a named snapshot you can return to.
      - The ignore list is the safety net that stops the copyrighted ROM ever reaching GitHub (Phase J).
      - It has to be proved before the first commit, because a file once committed stays in the history.
    - **Prerequisites:** B4 Done (yes). Checked 2026-10-23:
      - Git 2.56.0.windows.3; no repository yet.
      - F: is NTFS.
      - Git's system settings say `core.autocrlf=true` and `init.defaultbranch=master`. Global settings: your name and email, plus Git LFS filters, which are unused here.
      - The folder holds `docs\`, `rom\`, `tools\`, four `.md` files, the original `.a26`, `rom\pacman.a26`, `rom\EXPECTED_SHA1.txt` and `tools\a4_structure.py`.
    - **What Claude Code will do:**
      1. Create the folders `src\host`, `src\game`, `third_party`, `tests` and `reference` (`docs`, `tools` and `rom` exist already). Each new tracked folder gets a one-line `README.md` saying what goes there, because Git ignores empty folders.
      2. Write `.gitignore` (T-B5-1) and `.gitattributes` (T-B5-2).
      3. Run `git init -b main` (T-B5-3) inside the project folder only.
      4. **Before** any commit, prove:
         - `git status --ignored` and `git check-ignore -v` show every `.a26`, `rom\` and `reference\` as ignored.
         - A dry run `git add -A --dry-run` lists no `.a26`, `.bin` or `rom\` path.
      5. Stage, re-check the staged list by script, show you the list, then commit `Stage B5: Project skeleton and Git`.
      6. Show `git log` and a clean `git status`.
    - **What I must do:** Nothing, besides reading the file list.
    - **Exact instructions:** None.
    - **Required input:** None.
    - **Claude Code prompt:** The resume prompt.
    - **Expected result:**
      - `.git\` exists on branch `main`.
      - `git ls-files` lists only text files (docs, plan, scripts, READMEs, `.gitignore`, `.gitattributes`): no `.a26`, nothing under `rom\` or `reference\`.
      - `git status` is clean after the commit, and `git status --ignored` lists the `.a26` and `rom\` as ignored.
    - **Verification:** Command output of the above, plus a scripted check that `git ls-files` contains no `.a26`/`.bin` and no `rom/` path. The ROM SHA-1s are unchanged.
    - **Completion criteria:** The commit exists, the ignore check passes, and the ROMs are unchanged.
    - **Potential problems:**
      - Line-ending warnings ("LF will be replaced by CRLF") are harmless. `.gitattributes` makes them consistent.
      - "Dubious ownership" error → stop and explain. It would need a `safe.directory` setting, which changes Git's global config, so it needs your approval.
      - A ROM showing up in the staged list → stop and unstage before any commit. Nothing reaches history until the commit.
    - **Recovery:**
      - Before the commit: deleting `.git\` undoes `git init` completely (with your OK). The new folders and files are small text files.
      - After the commit: history can be amended only with your approval. Nothing is pushed anywhere.
    - **Documentation updates:** `DECISIONS.md` (T-B5-1, T-B5-2, T-B5-3), `PROJECT_STATUS.md`, `PLAN.md`.
    - **Git checkpoint:** The first commit, `Stage B5: Project skeleton and Git`, which includes all earlier stages' files.
    - **Next stage:** B6 — The memory files.
    - **Result (2026-10-23):**
      - **Folders and settings files:** `src\host`, `src\game`, `third_party` and `tests` were created, each with a README. `reference\` was created and is local-only. `.gitignore` (T-B5-1) and `.gitattributes` (T-B5-2) were written.
      - **Repository:** `git init -b main` (T-B5-3).
      - **Ignore check, before the commit:**
        - `git check-ignore -v` matched the original `.a26`, `rom/pacman.a26`, `rom/EXPECTED_SHA1.txt`, `reference/`, `build/`, `Test.A26` and `test.BIN`.
        - `git status --ignored` showed the `.a26` and `rom\` files as ignored (`!!`).
        - The dry-run add listed 14 text files and no ROM.
      - **First commit:** `Stage B5: Project skeleton and Git`. The staged list was checked by script for `.a26`/`.bin`/`rom/`/`reference/` (none), and `git status` was clean afterwards.
      - **ROMs:** SHA-1s unchanged.
6. **[Done] B6 — The memory files.** *(expanded 2026-10-23, done 2026-10-23)*
    - **Objective:** `CLAUDE.md` holds your 12 engineering rules word for word, plus the working preferences and hard-won technical facts. `docs\GLOSSARY.md` explains every project term in one plain line. `PROJECT_STATUS.md`, `PLAN.md` and `DECISIONS.md` are complete and consistent, with line 1 of `PROJECT_STATUS.md` reading "Current stage: B7".
    - **Why it matters:** Claude Code reads `CLAUDE.md` automatically at the start of every session, so the rules and the "how we work" apply even if a prompt forgets them. This is what keeps future sessions safe and consistent.
    - **Prerequisites:**
      - B5 Done (yes; commit `ffef5a0`, clean tree).
      - The 12 rules: you pasted "PART VII — Engineering rules and safeguards" on 2026-10-23.
      - `PROJECT_STATUS.md`, `PLAN.md` and `DECISIONS.md` already exist since Phase A, so B6 checks and tidies them rather than creating them.
    - **What Claude Code will do:**
      1. Write `CLAUDE.md`:
         - (a) the 12 rules, word for word, with only Markdown heading marks added;
         - (b) working preferences taken from the resume prompt and section 3 of this plan;
         - (c) technical facts later sessions must know (ROM protection, the Developer environment, the C++23 switch, PowerShell 5.1 quirks, the Git commit routine).
      2. Write `docs\GLOSSARY.md` with every term used so far.
      3. Check the three existing memory files against each other and set line 1 of `PROJECT_STATUS.md` to the agreed form.
      4. Prove the rules were copied word for word by comparing them with your pasted text by script.
      5. Commit.
    - **What I must do:** Nothing more (the rules are given). Optionally, open `PROJECT_STATUS.md` in Notepad afterwards and check line 1.
    - **Exact instructions:** None.
    - **Required input:** The 12 rules (received).
    - **Claude Code prompt:** The resume prompt.
    - **Expected result:**
      - `CLAUDE.md` exists, with 12 rule headings whose text matches your paste.
      - `docs\GLOSSARY.md` exists.
      - `PROJECT_STATUS.md` line 1 is `Current stage: B7 — Fetch the libraries (Phase B)`.
      - The commit is `Stage B6: The memory files`.
    - **Verification:**
      - A scripted comparison of the rules text, ignoring only blank lines and the added `#` marks.
      - The first line of `PROJECT_STATUS.md` shown.
      - `git status` clean after the commit, and no ROM tracked.
    - **Completion criteria:** All of the above shown.
    - **Potential problems:** Text that looks the same but uses different dashes or quote marks. The comparison script catches that.
    - **Recovery:** All new files are plain text in Git. Undoing them means returning to commit `ffef5a0`, with your OK.
    - **Documentation updates:** `CLAUDE.md`, `docs\GLOSSARY.md`, `PROJECT_STATUS.md`, `PLAN.md`, `DECISIONS.md` (T-B6-1).
    - **Git checkpoint:** `Stage B6: The memory files`.
    - **Next stage:** B7 — Fetch the libraries.
    - **Result (2026-10-23):**
      - **`CLAUDE.md` written.** Its rules section was compared by script with your pasted text (only blank lines and the added `#` marks ignored): 42 lines, identical byte for byte, 12 `Rule` headings. The reference copy was transcribed by Claude Code from your message, so a glance by you closes that gap.
      - **`docs\GLOSSARY.md` written:** about 70 terms in six groups.
      - **Memory files cross-checked:**
        - 102 stage markers.
        - A1–B5 Done in both `PLAN.md` and `PROJECT_STATUS.md`.
        - Every cited decision exists, apart from D-002, which is reserved for C1.
      - **Status line:** line 1 of `PROJECT_STATUS.md` is now "Current stage: B7 — Fetch the libraries (Phase B)" (T-B6-1).
      - **Commit:** `Stage B6: The memory files`.
7. **[Done] B7 — Fetch the libraries.** *(expanded 2026-10-23, done 2026-10-24)*
    - **Objective:** Pinned, fingerprinted source code of SDL3 and of Stella (including its libretro core) is in `third_party\`, each with its licence, recorded in `docs\DEPENDENCIES.md`. Both compile with our toolchain as stand-alone libraries that nothing uses yet.
    - **Why it matters:**
      - Phase D's program is built from exactly these two libraries.
      - Pinning them, and keeping the source in the project, means the build never changes behind our back and still works if a website disappears.
      - Compiling them now finds toolchain problems before any game code exists (Rule 9).
    - **Prerequisites:** B6 Done (yes; commit `d17cef6`, clean tree). Checked on GitHub's release pages, 2026-10-23:
      - **SDL3:** latest stable is `release-3.4.18` (2026-10-02), with a source archive `SDL3-3.4.18.tar.gz` of 15,651,004 bytes. GitHub's SHA-256 is `9c75cf16…34a3`. Licence: zlib.
      - **Stella:** latest release is **7.0** (2024-10-04), with a source archive `stella-7.0-src.tar.xz` of 13,685,208 bytes. No checksum is published. The libretro core is in `src/os/libretro` at tag 7.0. Licence: GNU GPL version 2 (`License.txt`).
      - **Stella's Windows app** is labelled "7.0c". Tags 7.0a/b/c differ from 7.0 only in two Windows installer files (`src/os/windows/Create_Builds.bat`, `stella.iss`), so the emulator code is identical.
      - **Stella 7.0's C++ level:** its build files ask for **C++20** (`-std=c++20`, MSVC `/std:c++20`), not C++23 (D-001's note was about Stella's later development code).
    - **What Claude Code will do:**
      1. Ask you for D-002 (the Stella version) and approval of the two downloads.
      2. Download both archives from their official GitHub release pages into a new folder `third_party\_downloads\`, kept out of Git.
      3. Check sizes and SHA-256 (SDL3 against GitHub's published value; Stella's value is recorded as ours).
      4. Unpack to `third_party\SDL3\` and `third_party\stella\`, and scan them for ROM-like files.
      5. Write `third_party\CMakeLists.txt`:
         - SDL3 as a static library, via its own CMake files.
         - Stella's libretro core as a static library, built from the source list in Stella's `Makefile.common`, using `/std:c++20`.
      6. Compile both into `build\b7\`.
      7. Write `docs\DEPENDENCIES.md` and commit.
    - **What I must do:** Choose the Stella version (D-002) and approve the downloads.
    - **Exact instructions:** Answer the question Claude Code shows.
    - **Required input:** D-002 and the download approval.
    - **Claude Code prompt:** The resume prompt.
    - **Expected result:**
      - Both archives match their recorded sizes and SHA-256 values.
      - Both licence files are present.
      - The build ends with `SDL3-static.lib` and `stella_libretro.lib` (or similar names), with no errors.
      - `docs\DEPENDENCIES.md` lists the versions, URLs, SHA-256 values and licences.
    - **Verification:**
      - Download checksums.
      - The build log tail and the list of built `.lib` files.
      - `git status` showing no archive, ROM or build file staged.
      - ROM SHA-1s unchanged.
    - **Completion criteria:** All of the above.
    - **Potential problems:**
      - Stella's core may need source files or settings that only its Makefile knows about. Claude Code reads the Makefile and copies them faithfully, without changing Stella's code.
      - The Stella source might contain `.bin`/`.a26` test files, which the ignore list hides from Git. The scan reports them.
      - Large sources make Git slower. Size is measured and the approach recorded (T-B7-x).
    - **Recovery:**
      - Before the commit: delete `third_party\SDL3`, `third_party\stella`, `third_party\_downloads` and `build\b7` (with your OK).
      - After the commit: return to `d17cef6` (with your OK).
      - Nothing is installed into Windows.
    - **Documentation updates:** `docs\DEPENDENCIES.md` (new), `DECISIONS.md` (D-002, T-B7-…), `docs\GLOSSARY.md` (new terms), `PROJECT_STATUS.md`, `PLAN.md`. C1 is adjusted to the D-002 version.
    - **Git checkpoint:** `Stage B7: Fetch the libraries`.
    - **Next stage:** B8 — Checkpoint script and a restore test.
    - **Result (2026-10-24):** D-002 = Stella 7.0, and you approved both downloads.
      - **SDL3:** `SDL3-3.4.18.tar.gz` SHA-256 equals GitHub's published value.
      - **Stella:** `stella-7.0-src.tar.xz` SHA-256 `d3fdd7a4…c0bc` recorded. All 1057 unpacked files are identical to tag 7.0.
      - **Left out (T-B7-1):**
        - Stella's `test/roms/`: 459 files, including 430 ROM images of other games.
        - SDL's `CLAUDE.md`/`AGENTS.md`, which are AI-tool instructions.
        - Result: 0 ROM-like files in `third_party\`.
      - **Build.** The build file is `third_party\CMakeLists.txt` (T-B7-3), and the build script is `tools\build_third_party.cmd`. Output goes to `build\third_party`, not `build\b7` as planned.
        - The first attempt failed for two reasons. My source-list parsing read only 1 file, and Stella's `snprintf`→`_snprintf` line broke `json.hpp`.
        - Both were fixed: exact parsing of 148 files, and a build-time patch on a copy (T-B7-4), with the vendored file untouched.
        - The clean build then took 62 s, with 0 errors and 0 warnings.
        - `stella_libretro.dll` is x64, built with `/std:c++20`, and exports all 25 `retro_*` functions. `SDL3.dll` is x64, version 3.4.18.0.
      - **Git (T-B7-2):**
        - The vendored sources are committed (3,238 files, 72.1 MB) with line-ending conversion off.
        - 8 files hidden by the projects' own `.gitignore` files were added explicitly.
        - The archives stay local.
      - **Records:** `docs\DEPENDENCIES.md` written, and C1 updated to Stella 7.0c.
8. **[Done] B8 — Checkpoint script and a restore test.** *(expanded 2026-10-24, done 2026-10-24)*
    - **Objective:** `tools\make_checkpoint.ps1` makes a dated, stage-named zip of the whole project, including its Git history, in `F:\Ports\Pac-Man - Atari 2600 checkpoints\`. It is proved by restoring one zip to a temporary folder and building it there.
    - **Why it matters:** Git protects against bad edits, but not against losing or damaging the folder itself (a disk fault, an accidental delete, a broken `.git`). A checkpoint zip outside the project is the third safety layer (section 3). It only counts once a restore has been shown to work.
    - **Prerequisites:**
      - B7 Done (yes; commit `1435e80`, clean tree).
      - Checked 2026-10-24:
        - The checkpoints folder doesn't exist yet.
        - F: has 86.5 GB free.
        - Project sizes: `.git` 27.1 MB, `third_party` 100.4 MB (of which `_downloads` 28.3 MB), and `build` 88.9 MB, which is excluded.
        - PowerShell is 5.1 (.NET Framework 4).
    - **What Claude Code will do:**
      1. Write `tools\make_checkpoint.ps1` (T-B8-1, T-B8-2):
         - It zips everything except `build\` and ROM files, with entry names using `/`.
         - It writes to a `.partial` file first.
         - Before keeping the zip, it re-reads it and refuses if any entry has a ROM extension or the same content as the ROM (SHA-1 match). On any problem it removes its own `.partial` and stops.
         - It never overwrites an existing checkpoint.
         - It refuses a destination inside the project folder.
         - It writes a small `.sha256` file next to the zip.
      2. After your approval, create the checkpoints folder and make checkpoint `B8`.
      3. Restore test, in a temporary folder `restore-test-B8` inside the checkpoints folder:
         - unzip;
         - prove `git status` is clean and `git fsck` finds no damage;
         - rebuild the libraries with `tools\build_third_party.cmd` from the restored copy;
         - compare the restored files with the project.
      4. With your OK, delete the temporary folder.
    - **What I must do:** Approve:
      - creating the checkpoints folder outside the project;
      - the temporary restore folder and its deletion afterwards;
      - (optionally) deleting `build_b7.log`, my B7 log.
    - **Exact instructions:** Answer the approval question.
    - **Required input:** Approvals.
    - **Claude Code prompt:** The resume prompt.
    - **Expected result:**
      - `pacman2600-<date>-B8.zip` and its `.sha256` file exist in the checkpoints folder.
      - The zip contains no `.a26`/`.bin`/`.rom` entry and no file whose SHA-1 equals the ROM's.
      - The restored copy shows a clean `git status`, `git fsck` reports no errors, and the libraries build with 0 errors.
      - The temporary folder is gone afterwards.
    - **Verification:** script output, zip size and entry count, the restored `git status`/`git fsck` output, the build log tail, and the before/after ROM SHA-1s.
    - **Completion criteria:** all expected results shown.
    - **Potential problems:**
      - Read-only files in `.git` make deleting the restore folder fail without `-Force`.
      - Windows path-length limits: the deepest path is checked first.
      - The zip creation fails part-way: the `.partial` file is removed and nothing else changes.
    - **Recovery:** checkpoints and the restore folder live outside the project and can be deleted (with your OK). The project itself is only read.
    - **Documentation updates:** `DECISIONS.md` (T-B8-1, T-B8-2), `docs\GLOSSARY.md`, `CLAUDE.md` (how to make a checkpoint), `PROJECT_STATUS.md`, `PLAN.md`.
    - **Git checkpoint:** `Stage B8: Checkpoint script and a restore test`.
    - **Next stage:** B9 — Gate B.
    - **Result (2026-10-24):** You approved all steps plus deleting `build_b7.log`.
      - **Safety refusals tested first, with nothing written:** destination inside the project, missing destination, invalid stage name.
      - **Checkpoint:** `pacman2600-2026-10-24-B8.zip`, 83,465,798 bytes, 7105 entries, made in 44 s. SHA-256 `eda9d7b1…bd25`, in the `.sha256` file. The original `.a26` and `rom\pacman.a26` were left out (T-B8-1), and the zip was re-checked for ROMs by name and content.
      - **Restore test** (`restore-test-B8`):
        - The SHA-256 matches.
        - All 7105 files are identical to the project (0 missing, 0 extra, 0 different), with 0 ROM-like files.
        - `git status` is identical to the project's at checkpoint time.
        - `git fsck --full` reports no errors.
        - `tools\build_third_party.cmd` built from the restored copy: 570 steps, 0 errors, 0 warnings, with the same DLL sizes as B7.
      - **Afterwards:** the restore folder was deleted (with your OK), and the ROM SHA-1s are unchanged.
      - **Note:** this checkpoint was taken before B8's commit (T-B8-2).
9. **[Not started] B9 — Gate B.**
    - Checklist: all tools verified, ROM locked, Git ignoring ROMs (proved), memory files present, first commit made, first checkpoint zip exists.

### Phase C — ROM analysis and reference environment (8 stages)

Set up the standalone Stella app as the "ruler" we measure everything against, and write down how the real game looks, sounds and responds before we build anything.

1. **[Not started] C1 — Install the reference emulator.**
    - You: download the Windows 64-bit **portable** zip of Stella from stella-emu.github.io (version recorded in D-002), unzip it to `F:\Ports\Tools\Stella`. Portable means nothing is installed into Windows.
    - *(Updated at B7, D-002.)* The file is `Stella-7.0c-windows.zip` (4,255,411 bytes) on the 7.0 release page, `https://github.com/stella-emu/stella/releases/tag/7.0`. "7.0c" has the same emulation code as the 7.0 source in `third_party\stella`. C1 records its SHA-256.
    - Verify: Stella opens to its game list.
2. **[Not started] C2 — First run of the original.**
    - You: in Stella, load the copy in `rom\`, play for one minute with the keyboard and with the PS4 pad, then tell Claude Code in a sentence whether it looked and sounded right.
    - Claude Code: reads Stella's game-properties entry for this ROM (cartridge type, TV format, display settings) and records it.
3. **[Not started] C3 — Display facts, measured.**
    - Claude Code: using Stella's debugger and snapshots, records the number of scanlines per frame, the first and last visible line, the visible height, the 160-pixel width, the colour palette (NTSC), and whether the line count ever changes (during deaths, level changes, attract mode).
    - Done when: `docs\DISPLAY.md` lists each value with how it was measured. No value is copied from memory.
4. **[Not started] C4 — Timing facts.**
    - Claude Code: measures frames per second in Stella's NTSC mode, the processor speed, and where in each frame the game logic runs. Writes `docs\TIMING.md`.
5. **[Not started] C5 — Sound facts.**
    - Claude Code: records short WAV clips of each sound from Stella into `reference\audio\` (local only) and lists which sound plays when.
6. **[Not started] C6 — Controls and game variations.**
    - You: with Claude Code guiding, press Game Select and watch the variation number; try the two difficulty switches.
    - Claude Code: documents joystick directions, fire button use (if any), Select/Reset, each game variation's settings, and what the difficulty switches change. Writes `docs\CONTROLS.md`.
7. **[Not started] C7 — Reference captures.**
    - Claude Code: guides you to take numbered Stella snapshots of the title screen, start of game, a power pill in effect, a death, a fruit/vitamin, a level end and game over, saved to `reference\screens\`. If you want video too, you choose then whether to install OBS (optional).
8. **[Not started] C8 — Gate C: reproducible reference.**
    - `docs\REFERENCE.md` explains how anyone can recreate this exact setup (Stella version, settings, ROM fingerprint). Gate checklist: display, timing, sound, controls and variations documented; snapshots present; nothing is "believed" without a measurement noted.

### Phase D — First working PC executable (16 stages)

Grow the host program one visible feature at a time, each checked against Stella, until it is a complete Phase 1 release.

1. **[Not started] D1 — Architecture note.** `ARCHITECTURE.md` with the three-part table from section 2, file by file: which code is ours, which is Stella's, which behaviour comes from the ROM, and what Phase 2 will replace.
2. **[Not started] D2 — An empty window.** A black 960×720 window titled with the project name that closes cleanly. You: double-click the `.exe` Claude Code points you to and close it.
3. **[Not started] D3 — Finding your ROM.**
    - First launch with no ROM known: a Windows "Open file" dialog asks for the `.a26`. The path is remembered in `settings.ini` next to the `.exe`.
    - Also accepted: a `roms` folder next to the `.exe`, or a path on the command line.
    - Checks: file exists, size matches, fingerprint matches A3. A different fingerprint shows a clear warning naming the expected file, with "Use anyway" or "Choose another".
    - Verify: Claude Code runs the four failure cases (missing, empty, wrong size, unknown fingerprint) and shows each message.
4. **[Not started] D4 — Emulator running blind.** The Stella core runs the ROM without a window for 600 frames and saves frame 300 as a PNG. Compared against C7's matching snapshot.
5. **[Not started] D5 — The game on screen.** Frames shown in the window, unscaled and small. You: confirm the title screen appears.
6. **[Not started] D6 — Correct shape and scaling.** See the dedicated section below.
7. **[Not started] D7 — Windowed and fullscreen.** Borderless fullscreen on your main monitor; **Alt+Enter** toggles; window size and position remembered. Verify on your 2560×1440 screen and with the window dragged to odd sizes.
8. **[Not started] D8 — Sound.** TIA sound from the core played through SDL with a small, steady buffer. Verify by ear against C5 clips and by Claude Code checking the sample rate and that no buffer underruns are logged in 5 minutes.
9. **[Not started] D9 — Frame pacing on a 180 Hz screen.** The game advances at the console's measured rate (from C4), never at the monitor's rate. Each game frame is shown for about three refreshes; sound timing is the master clock so music never drifts. Verify: a 10-minute run logs game frames per second within 0.1% of C4's value and no audio gaps.
10. **[Not started] D10 — Keyboard.** Arrows and WASD for the joystick, plus keys for Select, Reset and the difficulty switches. Listed on screen with **F1**.
11. **[Not started] D11 — PS4 controller.** D-pad and left stick for the joystick; Options = Reset, Share = Select. Verify with your pad, including unplugging and replugging mid-game.
12. **[Not started] D12 — Console switches.** The difficulty switches and Color/B&W switch available as settings, because the game may read them.
13. **[Not started] D13 — Input recording and replay.** The host can record every frame's input to a small `.inputs` file and play it back. Replaying must produce identical frames, proved by comparing picture fingerprints of every frame across two runs. This is the foundation of all later testing.
14. **[Not started] D14 — Side-by-side check.** Same input recording into our build and Stella; frames compared at 10 moments. Differences go to `DISCREPANCIES.md`.
15. **[Not started] D15 — Release folder.** `tools\make_release.ps1` builds `PacMan2600-0.1.0-windows.zip` (name changes after J2) with the `.exe`, needed DLLs, licences, README and *no ROM*. The script scans the zip and fails if any `.a26` or 4 KB file of unknown origin is inside.
16. **[Not started] D16 — Gate D: Phase 1 done.** You play a full game from the release zip, unzipped somewhere new. Checklist: correct shape, fullscreen and windowed, sound, keyboard, pad, ROM picker, replays identical, discrepancies list reviewed, version 0.1.0 tagged in Git.

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

1. **[Not started] E1 — Baseline format.** `docs\BASELINE.md` defines a test entry: ID (e.g. `BL-012`), what it shows, the `.inputs` file, the golden trace, and plain-words observations (score, lives, timings in frames). Claude Code adds a "trace" switch to the host and proves a replay gives a byte-identical trace twice.
2. **[Not started] E2 — Starting up.** Power-on, attract mode, each game variation chosen with Select, Reset to start. Scripted by Claude Code; no playing needed.
3. **[Not started] E3 — Pac-Man's movement.** Speed in frames per maze step, turning at corners, pushing into walls, reversing, holding diagonals, wrapping through the side tunnel. Mostly scripted; you play one short recorded session.
4. **[Not started] E4 — Ghosts.** Leaving the box, chasing, flicker order, speed, the blue/vulnerable state, eyes returning, and how they choose turns. Claude Code also records the same start position with different input timing to see whether ghost choices change (a clue to how randomness works).
5. **[Not started] E5 — Eating and scoring.** Wafers, power pills, ghosts eaten in a row, the bonus item, extra lives. Every score value is read from RAM and from the screen and must agree.
6. **[Not started] E6 — Dying and game over.** Collision timing, the death animation and sound, respawn, losing the last life, the game-over screen, starting again.
7. **[Not started] E7 — Long play.** You play three full games with the recorder on, as far as you comfortably can (the more levels, the better). Claude Code extracts level transitions, speed-ups and any changes between levels.
8. **[Not started] E8 — Edge cases and Gate E.** Pressing Reset mid-game, switching variation mid-game, flipping difficulty switches during play, very long idle, score near its maximum (reached by a memory-edit test in Stella's debugger, clearly labelled as artificial). Gate: at least 30 baseline entries covering every topic in your brief's Phase E list, each with a golden trace and a plain-words description you could check against the real game.

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

1. **[Not started] F1 — Analysis tools.** Stella's built-in debugger and disassembler (already installed) plus the portable `dasm` assembler (download approved by you). Python is installed only if A5 approved it; otherwise Claude Code's helper scripts are PowerShell or C++.
2. **[Not started] F2 — Raw disassembly that rebuilds exactly.** Disassemble, reassemble with `dasm`, and compare fingerprints. Byte-identical rebuild = proof the listing is complete. Stored in `reference\disasm\` (local only).
3. **[Not started] F3 — Hardware names.** Replace raw addresses of the TIA and RIOT chips with their standard names (`WSYNC`, `GRP0`, `INTIM`…).
4. **[Not started] F4 — Code versus data.** Mark graphics shapes, maze data, colour and sound tables as data.
5. **[Not started] F5 — The frame loop.** Find the start-up code and the four parts of every frame: vertical sync, vertical blank (logic time), the visible picture ("kernel"), overscan (more logic time).
6. **[Not started] F6 — What actually runs.** Replay every golden trace with execution logging; every ROM byte is marked executed, read as data, or never touched.
7. **[Not started] F7 — Naming all 128 RAM bytes.** Each byte watched across the baseline traces and named (`pacman_x`, `ghost_mode`…), with uncertain names marked "?".
8. **[Not started] F8 — Routine list and call map.** Every subroutine listed, with who calls it.
9. **[Not started] F9 — Input reading.**
10. **[Not started] F10 — Pac-Man's movement and turning.**
11. **[Not started] F11 — Maze and wafer storage.**
12. **[Not started] F12 — Collisions** (the 2600 has hardware collision flags; find whether the game uses them or its own checks).
13. **[Not started] F13 — Ghost movement and decisions.**
14. **[Not started] F14 — Randomness: where it comes from.**
15. **[Not started] F15 — Scoring, bonus item and lives.**
16. **[Not started] F16 — Levels, speed-ups and timers.**
17. **[Not started] F17 — Game states:** attract mode, start, death, level end, game over, variations, console switches.
18. **[Not started] F18 — The drawing kernel and flicker schedule.** Exactly which object is drawn on which line in which frame.
19. **[Not started] F19 — Sound routines and their tables.**
20. **[Not started] F20 — Lockstep harness.** A test tool that runs the emulator and the native code together on the same inputs, compares all 128 RAM bytes every frame, and on the first difference stops and reports the frame, the byte, both values and the routine responsible.
21. **[Not started] F21 — Rewrite one routine per session** (F21.1, F21.2…, one per item in F8's list, smallest and leaf routines first). Each rewrite must turn its part of the lockstep test green before the next starts.
22. **[Not started] F22 — Data loader.** Reads maze, shapes, colours and sound tables from the player's ROM at start-up, by address, after checking the fingerprint. Our source contains addresses and logic, not Atari's data.
23. **[Not started] F23 — Native renderer.** Produces each 160-pixel-wide frame from the game state; compared pixel for pixel against the emulator on every frame of every golden trace, flicker included.
24. **[Not started] F24 — Native sound generator.** A small model of the TIA's two sound channels; output compared sample by sample with the emulator's.
25. **[Not started] F25 — Whole-game lockstep.** All golden traces identical in RAM, picture and sound. Anything not identical is either fixed or entered in `DISCREPANCIES.md` with cause and impact, for your review.
26. **[Not started] F26 — Gate F: emulator out.** The game build no longer contains Stella; it lives on only in the test tool. You play a full game on the native build. Tag `0.5.0-native`.

#### When a routine is hard to understand

1. Split it into blocks of 5–15 instructions and explain each block separately.
2. Watch it live: breakpoints in Stella's debugger on the golden trace frames where it matters.
3. Experiment: change one RAM byte in the debugger and record what changes on screen.
4. Check public references (Stella docs, 2600 programming guides, community notes), always recording the source.
5. Still unclear after two sessions: write a literal translation marked `// LITERAL: not yet understood`, so the game stays correct, mark it "Reconstructed (literal)" in the coverage map, and come back later. Gate G requires zero literal blocks remaining, or each one approved by you as documented.

### Phase G — Readable source and maintainable architecture (7 stages)

Reshape the verified native code so a stranger can read and mod it, without changing a single frame of output. The lockstep suite runs after every change in this phase; any red result means the change is undone.

1. **[Not started] G1 — Choose the module layout from the real game.** Claude Code proposes files based on what Phase F found (likely along the lines of game state, maze, player, ghosts, scoring, game flow, renderer, sound, ROM data, platform). It explains each in one sentence; you approve or ask questions. Recorded as D-0xx in `DECISIONS.md`.
2. **[Not started] G2 — Coding and comment conventions.** `docs\CONVENTIONS.md`: plain names (`ghostLeavesHouse()`, not `sub_F3A2()`), no file over ~400 lines, every function starting with a one-line "what and why", every original-behaviour quirk tagged `// ORIGINAL:` with a link to its `RE_NOTES.md` entry.
3. **[Not started] G3 — Separate the state from its memory layout.** In Phase F the game state copied the original 128-byte layout so lockstep could compare it directly. Now give it named fields, and move the byte-for-byte mapping into the test tool only.
4. **[Not started] G4 — Move code into the G1 modules,** one module per session, lockstep green after each.
5. **[Not started] G5 — Remove leftovers.** Duplicated logic, unused helpers, literal blocks (or your documented approval of each remaining one), and any dependency the game no longer needs.
6. **[Not started] G6 — "Stranger test".** A fresh Claude Code session, told nothing, is asked to explain how ghosts choose a direction and how scoring works using only the source and docs. Wrong or vague answers mark places that need better names or comments.
7. **[Not started] G7 — Gate G.** Checklist: lockstep fully green; no file over the size limit; every `ORIGINAL:` tag links to evidence; `ARCHITECTURE.md` updated with a diagram of the modules; the 1.0.0 preservation build tagged. **This tag is the permanent baseline that modern features may never alter.**

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

1. **[Not started] H1 — `TESTING.md`.** Lists every test, what it proves, how long it takes, and whether it needs the ROM.
2. **[Not started] H2 — Unit tests per module** (maze lookups, score table, speed tables, ROM loader, scaling maths).
3. **[Not started] H3 — Two test commands.** `tools\test_quick.ps1` (unit tests + 5 key traces, a few minutes) after every stage; `tools\test_full.ps1` (everything) at gates and before releases. This keeps each session short, as you asked.
4. **[Not started] H4 — ROM-free test set for GitHub.** The subset that runs without the ROM, so the public repository can check itself automatically on every change.
5. **[Not started] H5 — Clean-build and release checks.** A fresh copy built following only `BUILDING.md`; the release zip scanned for ROM files.
6. **[Not started] H6 — Performance and pacing checks.** Frame-time log on your 180 Hz screen and with the monitor set to 60 Hz; any frame-rate dependence is a failure.
7. **[Not started] H7 — Gate H: freeze the preservation suite.** `tests\preservation\` is locked: its traces can be added to, never edited. Every later change must pass it.

### Phase I — Modernization discussion (4 stages, then one stage per approved feature)

Development stops here until you decide what, if anything, to add. Nothing in this phase is pre-decided.

1. **[Not started] I1 — Status report.** What is preserved and verified, what isn't, open discrepancies, and which structural changes would make additions easier.
2. **[Not started] I2 — Feature menu.** Claude Code presents candidate features one at a time, each with the eight answers your brief requires: what it does, why it's useful, whether it changes game behaviour, effect on fidelity, difficulty, new dependencies, whether it should be optional, and now / later / never. The list in your brief is the starting point; nothing is added because it is common.
3. **[Not started] I3 — Your decisions.** Recorded in `DECISIONS.md` with your exact wording. Rejected features are kept in a "Considered and declined" list so they aren't proposed again.
4. **[Not started] I4 — Plan the approved features** as new stages (I5, I6…), each small and each required to keep the preservation suite green.

Two rules for every feature:

- The **Original** mode stays the default and is byte-for-byte the 1.0.0 baseline.
- Anything that changes gameplay (for example removing flicker, or a new mode) is a separate, clearly labelled option, and is tested to prove it changes nothing when switched off.

### Phase J — GitHub and release (11 stages)

Publish only what you have decided is safe to publish, in a private repository first, and never with a ROM. Every `git push` and every GitHub action is approved by you at the moment it happens.

1. **[Not started] J1 — Legal and licensing decision (yours).** Claude Code prepares a plain-words briefing; it is not legal advice. The points it must cover:
    - **The ROM** is Atari's copyrighted program. Never distributed. Settled.
    - **The reconstructed source** was written by studying Atari's code. It may be treated as a derivative of it. The EU software directive (which applies in Bulgaria) allows decompiling only in narrow cases, mainly for compatibility between programs, so this needs real legal advice before publishing.
    - **The name "Pac-Man"** is a Bandai Namco trademark. The project name and icon should not use it (J2).
    - **Stella's code** is GPL-licensed. Any release containing it (the Phase 1 build, the test tool) must offer its source under the GPL. The native game build no longer contains it. Package listings disagree on whether Stella is "GPL 2 only" or "GPL 2 or later"; Claude Code reads the licence file in the pinned source to settle it.
    - **Your options:** (a) publish full source and `.exe`; (b) publish the host, tools and docs but keep the reconstructed game logic private; (c) publish nothing. You choose after reading the briefing, ideally after asking a lawyer.
2. **[Not started] J2 — Project name.** A name without the trademark, like you did with PacArcadeComp. Applied to the `.exe`, window title, zip names and repository.
3. **[Not started] J3 — Content audit.** Every file classified as: our own code, our notes containing ROM addresses, ROM-derived material (must be none in public), emulator code, third-party library, documentation, build tool, generated file. Output: `docs\PROVENANCE.md`.
4. **[Not started] J4 — Public documentation.** `README.md` (what it is, the ROM requirement in the first screen, how to play), `BUILDING.md`, `docs\HOW_IT_WORKS.md`, and the preservation write-up from `RE_NOTES.md` with any content J1 excluded removed.
5. **[Not started] J5 — Licences and credits.** `LICENSE` for our code (your choice from J1, explained), `THIRD_PARTY_NOTICES.md` for SDL3 and Stella, and credits to Atari, the original programmer and the Stella team.
6. **[Not started] J6 — Create the repository, private.** You create it on github.com while Claude Code tells you each click. Claude Code shows the exact file list for the first push; you check for `.a26` files; you approve the push. Sign-in happens in your browser, never by typing a password into Claude Code.
7. **[Not started] J7 — Automatic checks on GitHub.** A GitHub Actions workflow builds the game and runs the ROM-free tests on every change.
8. **[Not started] J8 — Versions and changelog.** Numbers like 1.2.0 (big.feature.fix), with `CHANGELOG.md` newest first.
9. **[Not started] J9 — Release packaging.** The release zip built by script, scanned for ROMs, unzipped and played on your PC from a fresh folder. The README explains Windows SmartScreen's "unknown publisher" warning, since the `.exe` isn't signed.
10. **[Not started] J10 — Issues and contributions.** A short issue template asking for version, Windows version and ROM fingerprint (never the ROM itself); `CONTRIBUTING.md` only if you want outside contributions.
11. **[Not started] J11 — Final gate: go public.** You switch the repository to public and publish the release. Checklist: J1 decision recorded, no ROM anywhere in history (Claude Code scans all past commits), licences present, release verified, `PROJECT_STATUS.md` says Done.

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
