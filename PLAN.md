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
9. **[Done] B9 — Gate B.** *(expanded 2026-10-24; done 2026-10-24: `docs\GATE_B.md`; Gate B met, all 6 items with evidence)*
    - **Objective:** Check every Gate B item afresh, with new evidence rather than earlier notes. Record the result in `docs\GATE_B.md`. With your OK, make an end-of-phase checkpoint `Gate-B`, and close Phase B.
    - **Why it matters:** Rule 10 says Phase C (the first work with the reference emulator) must not start until Phase B has met its completion criteria. A gate re-checked from scratch also catches anything that quietly changed since it was first done.
    - **Prerequisites:**
      - B8 Done (yes; commit `1bb3e97`, clean tree).
      - Checkpoint `pacman2600-2026-10-24-B8.zip` exists.
    - **What Claude Code will do:** run one read-only check per gate item:
      1. **All tools verified:** Git, Build Tools (`vswhere`), `cl`/CMake/Ninja versions in the Developer environment, Python; then rebuild the libraries with `tools\build_third_party.cmd` to show the whole chain still works.
      2. **ROM locked:**
         - the original's SHA-1 equals `rom\EXPECTED_SHA1.txt` and A3's value;
         - it is read-only;
         - the open-for-writing test is refused;
         - the spare in `ROM-originals` matches and is read-only.
      3. **Git ignoring ROMs (proved):**
         - `git check-ignore` on the ROM paths;
         - every commit in the history scanned for ROM file names;
         - the ROM's own Git fingerprint searched for in all stored objects.
      4. **Memory files present:**
         - `CLAUDE.md`, `PROJECT_STATUS.md`, `PLAN.md`, `DECISIONS.md` and `docs\GLOSSARY.md` all exist;
         - the 12 rules are still word for word;
         - the plan's 102 markers are intact, with B1–B8 Done;
         - every cited decision exists.
      5. **First commit made:** `git log` and `git fsck`.
      6. **First checkpoint zip exists:** its SHA-256 matches its `.sha256` file and it contains no ROM.

      Then: write `docs\GATE_B.md`; with your OK, make checkpoint `Gate-B`; commit.
    - **What I must do:** Approve the `Gate-B` checkpoint, which writes a zip outside the project.
    - **Exact instructions:** Answer the approval question.
    - **Required input:** That approval.
    - **Claude Code prompt:** The resume prompt.
    - **Expected result:** all six items PASS with evidence; `docs\GATE_B.md` written; checkpoint `pacman2600-<date>-Gate-B.zip` made; Phase B marked complete.
    - **Verification:** the command output for each item, shown in the session and summarised in `docs\GATE_B.md`.
    - **Completion criteria:** Gate B (the six items) all PASS.
    - **Potential problems:** any item failing. Phase B then stays open, the gap is named, and no Phase C work starts.
    - **Recovery:** default. The checks only read. The checkpoint is a new file outside the project.
    - **Documentation updates:** `docs\GATE_B.md` (new), `PROJECT_STATUS.md`, `PLAN.md`.
    - **Git checkpoint:** `Stage B9: Gate B`.
    - **Next stage:** C1 — Install the reference emulator.
    - **Gate B:** all tools verified, ROM locked, Git ignoring ROMs (proved), memory files present, first commit made, first checkpoint zip exists.
    - **Result (2026-10-24): Gate B met. All 6 items PASS**, each re-checked this session (details in `docs\GATE_B.md`):
      1. **Tools:**
         - Git 2.56.0, Python 3.14.0, Build Tools 18.11.1, `cl` 19.51.36260 x64, CMake 4.3.1, Ninja 1.13.2.
         - A fresh library build into `build\gate_b`: 570 steps, 0 errors, 0 warnings.
      2. **ROM:** original and spare match the expected SHA-1, are read-only, and refuse writing.
      3. **Git:**
         - ROM paths are ignored;
         - 0 ROM-like names in any commit;
         - the ROM's Git fingerprint `3a7ed122…2361` is absent from all 3,829 objects.
      4. **Memory files:**
         - all five exist and are tracked;
         - the rules are identical to your text;
         - 102 markers;
         - all cited decisions exist.
      5. **Commits:** four commits from `ffef5a0`, and `git fsck --full` is clean.
      6. **B8 checkpoint:** SHA-256 matches, 7,105 entries, no ROM by name or content.

      **End-of-phase checkpoint** (your approval): `pacman2600-2026-10-24-Gate-B.zip`, 83,537,815 bytes, 7,116 entries, no ROM, SHA-256 `da29d7bd…fe47`.

      **Phase B complete.**

### Phase C — ROM analysis and reference environment (8 stages)

Set up the standalone Stella app as the "ruler" we measure everything against, and write down how the real game looks, sounds and responds before we build anything.

1. **[Done] C1 — Install the reference emulator.** *(expanded 2026-10-24, done 2026-10-24)*
    - **Objective:** The standalone Stella 7.0 app (Windows build "7.0c", D-002) is ready as the project's reference "ruler". It is proved to be the official release file by file, keeps all its settings inside its own folder, starts, and shows its game list. Its location and the way it is launched are recorded.
    - **Why it matters:** Every later comparison (display, timing, sound, baselines, lockstep tests) trusts this app. If it were a tampered or wrong version, or its settings were scattered or changed, every measurement built on it would be doubtful.
    - **Prerequisites:**
      - Gate B met (yes; commit `bd3d30a`, clean tree).
      - Checked 2026-10-24 (read-only):
        - **What you set up:** you already unzipped Stella to **`F:\Ports\Tools\Stella-7.0c`** (folder created 2026-10-24 00:45). The plan had said `F:\Ports\Tools\Stella`.
        - **Contents:** 96 files (counted in the C1 comparison; an earlier rough "103" was wrong). `Stella.exe` is 6,692,352 bytes, file version 7.0, "The Stella Team", and not digitally signed. Alongside it: `SDL2.dll`, `msvcp140.dll`, `vcruntime140.dll` and `docs\`.
        - **The zip:** the downloaded zip is no longer in Downloads, so its SHA-256 can't be taken from your copy.
        - **Never run:** Stella has never been started (no `%APPDATA%\Stella`).
        - **Settings location:** Stella's manual says settings normally go to `%APPDATA%\Stella`. The `-baseinappdir` option keeps them in the app's own folder.
    - **What Claude Code will do:**
      1. With your approval, download the official `Stella-7.0c-windows.zip` again into `reference\_downloads\` (local only, never in Git). Record its SHA-256, and compare every file inside it with your unpacked folder (T-C1-1).
      2. Write `tools\run_stella.cmd`, which always starts Stella with `-baseinappdir`, so settings stay in `F:\Ports\Tools\Stella-7.0c` and nothing is written to your Windows profile (T-C1-2).
      3. With your approval, start Stella once through that script. Check the process and its window title, then check that its settings appeared in the app folder and not in `%APPDATA%`.
      4. Record everything in `docs\REFERENCE_EMULATOR.md`.
    - **What I must do:**
      - Approve the download and the first start of Stella.
      - When Stella's window appears, look at it and tell Claude Code what you see (the game list), then close Stella yourself.
    - **Exact instructions:** Given in the session when Stella opens: look, answer the question, close the Stella window with its X button.
    - **Required input:** Approvals and your one-line description of the window.
    - **Claude Code prompt:** The resume prompt.
    - **Expected result:**
      - The official zip's SHA-256 is recorded, and all files in it are identical to your folder (none missing or changed).
      - Stella starts, and its window title names Stella 7.0.
      - Its settings file appears inside `F:\Ports\Tools\Stella-7.0c`, and no `%APPDATA%\Stella` is created.
      - You confirm the game list is showing.
    - **Verification:** comparison output, process and window-title output, a file listing of the new settings files, your confirmation, and the ROM SHA-1s unchanged.
    - **Completion criteria:** all expected results shown.
    - **Potential problems:**
      - **Windows SmartScreen** may warn because `Stella.exe` isn't signed. Accept only once the file comparison has proved it is the official release.
      - **A file differs from the official zip:** stop; don't run it, and re-unzip with your OK.
      - **The window doesn't appear:** check the process, close it, and record what happened.
      - **Settings still go to `%APPDATA%`:** record it, and discuss before deleting anything.
    - **Recovery:**
      - The Stella folder and `reference\_downloads\` can be deleted (with your OK).
      - Stella installs nothing into Windows.
      - The project is only read; no ROM is opened in this stage.
    - **Documentation updates:** `docs\REFERENCE_EMULATOR.md` (new), `DECISIONS.md` (T-C1-1, T-C1-2), `docs\GLOSSARY.md`, `CLAUDE.md` (where the reference app lives and how it's launched), `.gitignore` (if needed), `PROJECT_STATUS.md`, `PLAN.md`.
    - **Git checkpoint:** `Stage C1: Install the reference emulator`.
    - **Next stage:** C2 — First run of the original.
    - **Result (2026-10-24):** you approved the download and the first start.
      - **Download:** the official `Stella-7.0c-windows.zip`, 4,255,411 bytes, SHA-256 `44102ccf…4653`, into `reference\_downloads\`.
      - **Identity:** 96 of 96 files identical to your folder `F:\Ports\Tools\Stella-7.0c`, with 0 different, missing or extra (T-C1-1).
      - **First start:** through `tools\run_stella.cmd` (`-baseinappdir`, T-C1-2).
        - The process ran from the app folder, with window title "Stella 7.0", responding.
        - You saw "the game list / file browser" and closed it normally.
      - **Settings:** `stella.sqlite3` (57,344 bytes) and empty `cfg\`, `nvram\` and `state\` were created in the app folder. No `%APPDATA%\Stella` was created.
      - **Afterwards:** the official files are still identical, no ROM was opened, and the ROM SHA-1s are unchanged.
      - **Recorded:** `docs\REFERENCE_EMULATOR.md`.
2. **[Done] C2 — First run of the original.** *(expanded 2026-10-24, done 2026-10-24)*
    - **Objective:** The original game runs in the reference Stella app from the working copy `rom\pacman.a26`. You confirm in a sentence that it looks, sounds and plays right. Stella's view of this ROM (database entry and what it detects: cartridge type, TV format, display settings) is recorded. Nothing is written next to the ROM.
    - **Why it matters:**
      - This is the first time the game itself runs.
      - Your judgement is the human check on the "ruler" (Stella) before anything is measured with it (C3–C8).
      - The detected properties are the settings every later measurement must reproduce.
    - **Prerequisites:** C1 Done (yes; commit `764720d`, clean tree). Checked 2026-10-24:
      - **Database entry:** Stella 7.0's own database (`third_party\stella\src\emucore\DefProps.hxx`, line 1625) has the same entry for MD5 `6e372f07…f1cb` that A3 read from Stella's development version: name "Pac-Man (1982) (Atari)", cartridge type and TV format empty (auto-detect), `Display_Phosphor` YES.
      - **Default keys (Stella's manual):** arrow keys move; Space or Left Ctrl is fire; F1 is Game Select, F2 Game Reset, F5/F6 the left player's difficulty A/B, Pause pauses, Escape goes back to the game list.
      - **Controller:** Windows currently lists no PS4 controller, only the "Nefarius Virtual Gamepad Emulation Bus" (usually from DS4Windows).
    - **What Claude Code will do:**
      1. Take a fingerprint snapshot of `rom\`, then the ROM SHA-1s.
      2. With your approval, run Stella's `-rominfo` on `rom\pacman.a26`, which prints what Stella detects and exits (through the same `-baseinappdir` setup, T-C1-2).
      3. With your approval, start the game with `tools\run_stella.cmd rom\pacman.a26`, and tell you the keys.
      4. Afterwards, check that `rom\` is unchanged and nothing new appeared next to the ROM, list what Stella wrote in its own folder, and record everything in `docs\FIRST_RUN.md`.
    - **What I must do:**
      - Approve the two Stella starts.
      - Play for about a minute with the keyboard, then with your PS4 controller if you connect it.
      - Close Stella.
      - Tell Claude Code in one sentence how it looked, sounded and felt.
    - **Exact instructions:** given in the session (keys, and how to connect the pad).
    - **Required input:** your sentence, and whether the pad worked.
    - **Claude Code prompt:** The resume prompt.
    - **Expected result:**
      - `-rominfo` shows the name, MD5 `6e372f07…f1cb`, a 4K cartridge type and NTSC.
      - The game runs; you report it looks and sounds right (or describe what doesn't).
      - `rom\` is unchanged, and the ROM SHA-1s are unchanged.
    - **Verification:** the `-rominfo` output, process and window-title output, the before/after listing of `rom\`, the list of Stella's own new files, and your sentence quoted.
    - **Completion criteria:** all expected results shown, and your sentence recorded.
    - **Potential problems:**
      - `-rominfo` prints nothing, because Windows programs with a window often have no text output. In that case: read Stella's in-app "Game Properties" instead, with your help, or its log.
      - The PS4 pad isn't seen: with DS4Windows running, it may appear as an Xbox controller. Note what happens; controller details are C6's job.
      - Sound missing: check the Windows volume, and note it.
      - Anything looks wrong: describe it. Nothing is changed in Stella's settings to "fix" it without recording it.
    - **Recovery:** Stella only reads the ROM. Its own new files live in its folder and can be removed with your OK.
    - **Documentation updates:** `docs\FIRST_RUN.md` (new), `docs\GLOSSARY.md`, `DECISIONS.md` (if a choice is needed), `PROJECT_STATUS.md`, `PLAN.md`.
    - **Git checkpoint:** `Stage C2: First run of the original`.
    - **Next stage:** C3 — Display facts, measured.
    - **Result (2026-10-24):** you approved both Stella starts and chose keyboard only.
      - **Info run:** `-rominfo` gave empty output (a windowed program on Windows), as foreseen.
      - **Game run:** Stella started with `rom\pacman.a26`, window title `Stella 7.0: "Pac-Man (1982) (Atari)"`. It was restarted once at your request.
      - **Your verdict:** "Looks, sounds and plays right".
      - **Game Properties → Emulation**, read by you: "4K detected" and "NTSC detected". I had first pointed you to the wrong tab; the fields' location was then checked in Stella's source.
      - **Database:** Stella 7.0's entry (`DefProps.hxx` line 1625) is the same as A3's.
      - **Files:** `rom\` unchanged (fingerprint snapshot before = after) and ROM SHA-1s unchanged. Only Stella's own `stella.sqlite3` was updated, and nothing went to `%APPDATA%`.
      - **Recorded:** `docs\FIRST_RUN.md` and T-C2-1. The PS4 pad is left for C6.
3. **[Done] C3 — Display facts, measured.** *(expanded 2026-10-24, done 2026-10-24)*
    - **Objective:** `docs\DISPLAY.md` lists the game's display facts, each with how it was measured and none copied from memory:
      - scanlines per frame;
      - the visible window Stella uses (first line, height);
      - the lines and columns the game actually draws on;
      - the 160-pixel width;
      - the colours used and their NTSC palette values;
      - whether the line count changes in attract mode, at game start and during deaths;
      - whether level changes were reached, stated honestly either way.
    - **Why it matters:** the PC port must draw exactly the same picture (D6 "getting the shape right"). These numbers define the size and shape of its screen and which colours it needs. A frame with a different line count is exactly the kind of detail a port gets wrong.
    - **Prerequisites:** C2 Done (yes; commit `47e24ad`, clean tree). Checked 2026-10-24 in the Stella 7.0 source:
      - `StellaLIBRETRO` (Stella's own libretro wrapper) can be driven directly, with no other libretro code.
      - Its TIA (video chip) gives `scanlinesLastFrame()`, `startLine()`, `height()` and the raw `frameBuffer()`: 160 pixels per line, TIA colour numbers, before Stella's display effects.
      - The palette table `ourNTSCPalette` is in `src/common/PaletteHandler.cxx`.
      - Defaults are the same as the app: palette "standard", format "AUTO", phosphor "byrom". `plr.ramrandom` is true (random memory at power-on, like a real console).
    - **What Claude Code will do (plan change, T-C3-1):** instead of you operating Stella's debugger by hand,
      1. Build a small measuring program, `tools\stella_probe\`, from the **same Stella 7.0 code** as the reference app. This adds a `stella_core` static library to `third_party\CMakeLists.txt`, made from the same sources and settings as the core.
      2. Run the game for a long stretch: about 30 s of attract mode, then Game Reset, then no input, so the ghosts catch Pac-Man until the game ends. For every frame it records the line count, visible window, drawn area, colours used and a picture fingerprint.
      3. Run it a second time to see whether runs repeat exactly.
      4. Save a few frames as pictures (BMP) in `reference\c3\` (local only), for you to look at.
      5. A report script (`tools\c3_report.py`) reads the palette from Stella's source and summarises everything.
      6. Write `docs\DISPLAY.md`.
    - **What I must do:** optionally, look at the saved pictures and say whether they look like the game you played in C2.
    - **Exact instructions:** none beyond that optional look.
    - **Required input:** none.
    - **Claude Code prompt:** The resume prompt.
    - **Expected result:**
      - The probe reports "NTSC" and "4K", like C2.
      - Per-frame data covers attract mode, game start and the deaths up to game over.
      - The line counts are summarised: the normal value, plus any frames that differ and when.
      - The drawn area and the colours used are listed with RGB values from Stella's palette.
      - `docs\DISPLAY.md` is written.
    - **Verification:** probe and report output shown in the session; the summary numbers in `docs\DISPLAY.md` traceable to the CSV in `reference\c3\`; ROM SHA-1s unchanged.
    - **Completion criteria:** every display fact listed with its measurement method. Anything not reached (for example a level change) is marked "not yet measured", with the stage that will measure it.
    - **Potential problems:**
      - **Build problems** linking Stella's code into a new program: fixed in our CMake file only, never in Stella's sources.
      - **Runs not repeating:** expected with random power-on memory; recorded, not "fixed".
      - **Idle play never reaches a new level:** stated plainly.
    - **Recovery:** only new project files plus local output in `reference\c3\` and `build\`. Undo by returning to `47e24ad` (with your OK). Nothing outside the project is touched, and Stella isn't started.
    - **Documentation updates:** `docs\DISPLAY.md` (new), `DECISIONS.md` (T-C3-1), `docs\GLOSSARY.md`, `CLAUDE.md` (probe tool), `PROJECT_STATUS.md`, `PLAN.md`.
    - **Git checkpoint:** `Stage C3: Display facts, measured`.
    - **Next stage:** C4 — Timing facts.
    - **Result (2026-10-24):** `tools\stella_probe` (T-C3-1) ran the working copy for 24,000 frames: attract, Game Reset at 1800, then no input until game over. Details in `docs\DISPLAY.md`.
      - **Detected:** NTSC* and 4K* (as in C2).
      - **Lines per frame:** **262** on all frames except frame 0 (45, the partial power-on frame). The count never changed through attract, reset, the deaths and game over.
      - **Visible window:** line 23, 228 lines.
      - **Drawn area:** scanlines 34–222, all 160 columns.
      - **Colours:** 9 in active play (4 ghost colours each in about ¼ of frames, i.e. the flicker). 87 overall, because of the idle colour shifting.
      - **Pictures:** 80 in `reference\c3\dump1\png\` were checked by eye: attract, play with 3 lives, 1 life left, game over.
      - **Not measured:** the line count at a level change (none was reached).
      - **Findings for D/E:**
        - The libretro wrapper zeroes RAM before the first frame (the app randomises it), so probe runs are deterministic (13 identical full runs).
        - "File open/read error" explained as harmless.
        - **One unexplained crash** in the first run, not reproduced in 13 more. Its data before the crash was identical to the good runs.
      - **Build:** the `stella_core` target was added, and the `stella_libretro` build was proved unchanged.
4. **[Done] C4 — Timing facts.** *(expanded 2026-10-24, done 2026-10-24)*
    - **Objective:** `docs\TIMING.md` records, with how each was measured:
      - CPU cycles per scanline and per frame;
      - the processor clock and the resulting frame rate, both the console's (per Stella's constant) and the rate Stella actually plays at;
      - the layout of each frame: VSYNC lines, blanked lines at top and bottom, drawn lines;
      - where the game's logic can run, meaning the lines where the CPU is not locked to the beam (not ending each line with WSYNC);
      - whether all of this stays constant across attract mode, play, deaths and game over.
    - **Why it matters:**
      - The PC port must run the game at the right speed and keep sound in step. A 0.13% difference in frame rate adds up to a whole second over 13 minutes.
      - The frame layout and the time available for logic are what the native rebuild (Phase F) must reproduce. Lockstep tests need the exact cycle counts.
    - **Prerequisites:** C3 Done (yes; commits `09988f4`, `874d917`, clean tree). Found in Stella 7.0's source on 2026-10-24:
      - **Stella's playback rate:** for NTSC it times emulation at 262 × 76 × 60 = 1,194,720 cycles/s, i.e. exactly 60.000 frames/s (`EmulationTiming.cxx` lines 174–188).
      - **Stella's console clock:** "NTSC 6507 clock rate" = 1,193,191.67 Hz (`CartARM.cxx` line 59, also `CartCTY.cxx` and `CartDPC.cxx`).
      - **One CPU cycle per system cycle** (`M6502.hxx` `SYSTEM_CYCLES_PER_CPU = 1`), and 76 cycles per line (`TIAConstants.hxx` `H_CYCLES`).
      - **Public, read-only measuring points:** `TIA::cycles()`, `TIA::registerValue()` (last value written to VSYNC/VBLANK) and `M6502::lastWriteAddress()`.
      - **Not available in this build:** Stella's WSYNC-cycle and timer counters exist only when its debugger is compiled in.
    - **What Claude Code will do:**
      1. Add a second tool, `tools\stella_probe\timing.cpp` → `stella_timing.exe` (T-C4-1). It repeats Stella's own `runFrame()` steps exactly, but stops after every scanline to record, without disturbing anything:
         - the cycle count;
         - the VSYNC and VBLANK state;
         - whether the line ended with a WSYNC write.
      2. Prove it doesn't change the emulation: its picture fingerprints must equal C3's for all 24,000 frames.
      3. Run the same 24,000-frame scenario as C3 and summarise per frame.
      4. Keep full per-line detail for three sample frames (attract, play, game over).
      5. Write `docs\TIMING.md`.
    - **What I must do:** Nothing.
    - **Exact instructions:** None.
    - **Required input:** None.
    - **Claude Code prompt:** The resume prompt.
    - **Expected result:**
      - 76 cycles per line and 19,912 per frame (262 × 76), confirmed by measurement.
      - Frame rate: 59.92 Hz on Stella's console clock and 60.000 Hz as Stella plays, both stated.
      - A frame layout table, and the lines available for logic.
      - Fingerprints identical to C3.
    - **Verification:** tool output, the fingerprint comparison with C3, and the summary in `reference\c4\` traceable to `docs\TIMING.md`. ROM SHA-1s unchanged.
    - **Completion criteria:** all of the above shown. What can't be measured without the debugger (CPU busy versus waiting inside the blank areas) is marked "not yet measured", with the stage that will measure it.
    - **Potential problems:**
      - Replicating `runFrame()` could subtly change the emulation. The C3 fingerprint comparison catches that.
      - A "last write" isn't a perfect busy/idle signal, so it is only interpreted for what it proves (whether the line ended locked to the beam).
    - **Recovery:** new files only, plus local output in `reference\c4\`. Undo by returning to `874d917` (with your OK).
    - **Documentation updates:** `docs\TIMING.md` (new), `DECISIONS.md` (T-C4-1), `docs\GLOSSARY.md`, `CLAUDE.md`, `PROJECT_STATUS.md`, `PLAN.md`.
    - **Git checkpoint:** `Stage C4: Timing facts`.
    - **Next stage:** C5 — Sound facts.
    - **Result (2026-10-24):** `stella_timing.exe` (T-C4-1) ran C3's 24,000-frame scenario. All picture fingerprints and line counts are identical to C3. Details in `docs\TIMING.md`.
      - **Cycles:** 19,912 per frame on all full frames, 76 per line.
      - **Frame rate:** Stella plays at **60.000000 Hz** (1,194,720 cycles/s). Stella's NTSC console clock (1,193,191.67 Hz, `CartARM.cxx`) gives **59.923246 Hz**, a +0.128 % difference (one frame per 13 s). Phase D must choose.
      - **Frame pattern, identical in all 23,999 full frames:**
        - VSYNC lines 259–261, with the timer set at L261;
        - top logic from L0, then a wait until L33;
        - VBLANK off at L34;
        - a second timer set at L195;
        - bottom logic after drawing, then a wait until L258;
        - VSYNC and VBLANK on at L259.
      - **Spare time:** at least 6 lines (456 cycles) at the top and 24 lines at the bottom. No overruns.
      - **Not measured:** WSYNC waits during drawing and the timer values (debugger-only), and busy moments of real play (Phase E).
      - **Fixed during the stage:** a first per-scanline version proved imprecise and was replaced.
5. **[Done] C5 — Sound facts.** *(expanded 2026-10-24, done 2026-10-24)*
    - **Objective:** `docs\SOUND.md` lists every distinct sound the game made in the measured runs. For each one:
      - when it plays (with a picture of the moment);
      - which TIA channel it uses;
      - its register values over time (tone type AUDC, pitch AUDF, volume AUDV);
      - how long it lasts, and how often it occurred.

      It also gives the audio format Stella produces. A WAV clip of each sound and full-run WAVs go in `reference\audio\` (local only). Sounds that never occurred are listed as "not yet captured".
    - **Why it matters:** the port must make exactly the same sounds at the same moments. The register sequences are what the native rebuild must reproduce, and the clips are what you and later tests compare by ear and by number.
    - **Prerequisites:** C4 Done (yes; commit `1912511`, clean tree). Checked in the Stella 7.0 source on 2026-10-24:
      - **Audio format:** the libretro core delivers 16-bit stereo at 262 × 76 × 60 ÷ 38 = 31,440 Hz (`StellaLIBRETRO.hxx` `getAudioRate()`), via `getAudioBuffer()`/`getAudioSize()`. Mono games are copied to both sides (`SoundLIBRETRO::dequeue`).
      - **Sound registers:** AUDC0 $15, AUDC1 $16, AUDF0 $17, AUDF1 $18, AUDV0 $19 and AUDV1 $1A (`TIAConstants.hxx`). Their last-written values are readable with `TIA::registerValue()`.
      - **Joystick input:** Stella events `LeftJoystickUp/Down/Left/Right`.
    - **What Claude Code will do (T-C5-1):**
      1. Add `tools\stella_probe\sound.cpp` → `stella_sound.exe`, built the same way as C4's tool: identical frame steps, checked after every CPU instruction. It records:
         - every change of the six sound registers, with its frame, line and cycle;
         - the audio stream as a WAV;
         - the picture at chosen frames.
      2. Prove it doesn't change the emulation: its picture fingerprints for the idle run must equal C3's.
      3. Run two scenarios:
         - (a) **idle**, as in C3/C4;
         - (b) **scripted play**: Game Reset, then the joystick changes direction on a fixed pseudo-random schedule, so Pac-Man moves and eats. It is repeatable, with nobody at the keyboard.
      4. A report script (`tools\c5_report.py`) groups the register changes into distinct sounds, cuts a WAV clip of each and makes a PNG of the moment each starts.
      5. Claude Code looks at those pictures to say what was happening, and records everything in `docs\SOUND.md`.
    - **What I must do:** Nothing required. Optionally, listen to the clips in `reference\audio\` and say whether they sound like the game.
    - **Exact instructions:** None.
    - **Required input:** None.
    - **Claude Code prompt:** The resume prompt.
    - **Expected result:**
      - The idle run's fingerprints equal C3's.
      - A list of distinct sounds, each with its channel, register sequence, duration, occurrences, the moment it starts and a WAV clip.
      - Sounds not triggered are named as "not yet captured", with where they will be.
    - **Verification:** tool and report output, the fingerprint comparison, the pictures checked by eye, and the ROM SHA-1s unchanged.
    - **Completion criteria:** all of the above. Every sound named in `docs\SOUND.md` is traceable to register data and a clip.
    - **Potential problems:**
      - Scripted input may never trigger some events (for example eating a ghost). They are reported as not captured; nothing is invented.
      - The names of sounds are interpretations of the pictures, and are labelled as such.
      - If a register changes several times within a frame, the per-instruction record keeps every change.
    - **Recovery:** new files only, plus local output in `reference\audio\`. Undo by returning to `1912511` (with your OK).
    - **Documentation updates:** `docs\SOUND.md` (new), `DECISIONS.md` (T-C5-1), `docs\GLOSSARY.md`, `CLAUDE.md`, `PROJECT_STATUS.md`, `PLAN.md`.
    - **Git checkpoint:** `Stage C5: Sound facts`.
    - **Next stage:** C6 — Controls and game variations.
    - **Result (2026-10-24):** `stella_sound.exe` (T-C5-1). Idle fingerprints equal C3's, and every re-run reproduced its data exactly. Runs: `idle`, `play1` and `seed1`–`seed6` (scripted joystick), 24,000 frames each. Details in `docs\SOUND.md`.
      - **Audio format:** 31,440 Hz, 16-bit stereo (identical sides), 524 samples per frame.
      - **Only 3 sounds occurred:**
        1. **Start tune:** 2 channels, 63 frames, at game start and after each lost life.
        2. **Scoring blip:** C9 F9, volume 9→2, 8 frames. All 376 occurrences coincide with a status-bar change.
        3. **Death sound:** C4, AUDF 31→0 (rising note) one step per 4 frames, volume ramping 15→0 four times, ≈ 2.1 s.
      - **Silences:** attract mode is silent, and there is no background sound during play.
      - **Each frame:** the game writes AUDV = 0 at line 226 and the real volume at line 227.
      - **Not captured:** power pill, ghost eaten, level clear (never reached by scripted play).
      - **Corrected during the stage:**
        - The death sound's note rises, not falls (checked in Stella's `AudioChannel.cxx`).
        - The count of idle sound frames is 738.
6. **[Done] C6 — Controls and game variations.** *(expanded 2026-10-24, done 2026-10-24)*
    - **Objective:** `docs\CONTROLS.md` records, each with how it was measured:
      - what each joystick direction does;
      - whether the fire button does anything;
      - what Game Select and Game Reset do;
      - how many game variations there are, and what is observably different in each;
      - what the two difficulty switches and the Colour/B&W switch change;
      - the switch positions Stella uses by default.
    - **Why it matters:** the port must offer exactly the same controls and options, mapped sensibly to keyboard and game pad. A missed variation or switch effect would be a fidelity gap.
    - **Prerequisites:** C5 Done (yes; commit `9004fef`, clean tree). Checked in the Stella 7.0 source on 2026-10-24:
      - **Momentary:** `ConsoleSelect` and `ConsoleReset` are pressed only while the event is held.
      - **Switches that stay put:** `ConsoleLeftDiffA/B`, `ConsoleRightDiffA/B` and `ConsoleColor/BlackWhite` set a position that remains (`Switches::update()`).
      - **Defaults:** they come from the game's properties (`Switches.cxx` line 37). The positions can be read with `Switches::leftDifficultyA()`, `rightDifficultyA()` and `tvColor()`.
    - **What Claude Code will do (T-C6-1):**
      1. Add a general input-script tool, `tools\stella_probe\script.cpp` → `stella_script.exe`. It reads a text file of timed inputs (directions, fire, Select, Reset, switch positions) and records per frame:
         - the picture fingerprint and status-bar fingerprint, and the colours used;
         - the switch positions;
         - the 128 bytes of console RAM;
         - pictures at chosen frames.

         It's built the same way as C3–C5, and its idle script must reproduce C3's fingerprints.
      2. Run experiments:
         - (a) press Game Select repeatedly and read the variation shown on screen until it wraps around;
         - (b) for each variation, start a game and compare it with variation 1 (lives, picture, RAM, and Pac-Man's movement under the same joystick script);
         - (c) the same scripted game with each difficulty switch on A versus B;
         - (d) with versus without the fire button;
         - (e) each direction held from the start;
         - (f) Colour versus B&W;
         - (g) Select and Reset pressed during play.
      3. Look at the pictures where runs differ, and write `docs\CONTROLS.md`.
      4. **Optional, needs you:** the PS4 controller test in the Stella app, left over from C2. It is offered, not required. The port's own controller support is a Phase D matter.
    - **What I must do:** Nothing required. Optionally, test the PS4 pad in Stella if Claude Code asks and you have it to hand.
    - **Exact instructions:** Given in the session if you take the optional pad test.
    - **Required input:** None.
    - **Claude Code prompt:** The resume prompt.
    - **Expected result:**
      - The number of variations and their on-screen labels.
      - Per variation, the observed differences (or "no difference found in this test").
      - Per switch and the fire button, the effect or "no effect found", with the frame where runs first differ.
      - The direction mapping and the default switch positions.
    - **Verification:**
      - Tool output.
      - Fingerprint comparisons between runs. Identical fingerprints mean "no visible effect in this scenario".
      - Pictures checked by eye.
      - The idle-script check against C3.
      - ROM SHA-1s unchanged.
    - **Completion criteria:** every item above answered with evidence, or marked "not determined" with the reason and the stage that will determine it.
    - **Potential problems:**
      - An effect may only show in situations the script doesn't reach, for example a later level. "No effect found" is then stated as limited to the tested scenario, never as "no effect".
      - The meaning of RAM bytes is a Phase F job. C6 only notes which bytes differ.
    - **Recovery:** new files only, plus local output in `reference\c6\`. Undo by returning to `9004fef` (with your OK).
    - **Documentation updates:** `docs\CONTROLS.md` (new), `DECISIONS.md` (T-C6-1), `docs\GLOSSARY.md`, `CLAUDE.md`, `PROJECT_STATUS.md`, `PLAN.md`.
    - **Git checkpoint:** `Stage C6: Controls and game variations`.
    - **Next stage:** C7 — Reference captures.
    - **Result (2026-10-24):** `stella_script.exe` (T-C6-1); its idle script reproduced C3 exactly. Details in `docs\CONTROLS.md`.
      - **Defaults:** both difficulty switches on B, TV type Colour.
      - **Select:** 16 selections, games 1–8 × 1 or 2 players, starting at 1 (1P). During play it ends the game and moves to the next selection.
      - **Reset and fire:** Reset starts a new game. **Fire starts a new game from attract mode or after game over; no effect found during play.**
      - **Directions:** all 4 read (RAM $ED = 1/2/4/8). Left/right move Pac-Man; up/down are blocked at the start by walls but work elsewhere.
      - **B&W:** the game itself uses grey colours.
      - **Left difficulty:** changes when the bonus item under the ghost pen is shown (player 1).
      - **Right difficulty:** the same, for player 2 only; no effect in one-player games.
      - **Variations:** no game number shown in play. Idle ghost-catch timings repeat every 4 games, and Pac-Man's speed over the same 12 px is 20 frames (games 1, 7, 8) versus 16 (2–6). What each variation means is not determined.
      - **Two players:** a red bar for player 2, turns alternate.
      - **PS4 pad in Stella (your test):** the left stick moves Pac-Man and a pad button starts the game. The d-pad was not confirmed.
7. **[Done] C7 — Reference captures.** *(expanded and done 2026-10-24)*
    - **Objective:** a numbered, **reproducible** set of reference pictures in `reference\screens\` (local only), each recreatable from a committed input script:
      1. title (attract) screen;
      2. start of a game;
      3. a power pill in effect;
      4. a ghost being eaten, if reached;
      5. a death;
      6. the bonus item;
      7. a level end;
      8. game over.

      Plus `docs\CAPTURES.md` listing each picture with its frame, the script that makes it, and what it shows.
    - **Why it matters:** these are the fixed "this is how the original looks at that moment" references for the port (Phase D lockstep, D6 shape check) and for Phase E. Being script-based, anyone can recreate them byte-for-byte.
    - **Prerequisites:** C6 Done (yes; commit `090aa6d`, clean tree). Found on 2026-10-24 in C6's recorded runs:
      - **Lives:** RAM $98 counts lives remaining (3 at start, then 2/1/0 exactly at the life-marker changes).
      - **Dying:** $D2 = 64 while Pac-Man is dying (set at each death, cleared when the next life starts).
      - **Wafer map:** player 1's is $88–$97, with 126 one-bits at game start. Each bit clears exactly when the score goes up. Player 2's identical map is at $9C–$AB.
      - These are working labels, to be confirmed in Phase F.
    - **What Claude Code will do (T-C7-1):**
      1. Add a search-based player, `tools\stella_probe\bot.cpp` → `stella_bot.exe`. At regular decision points it saves the emulator state, tries short move sequences ahead using Stella's own save/load, and picks one that eats wafers ($88–$97) without dying ($D2).
      2. It writes its moves as an ordinary **input script**.
      3. **Replay that script from power-on with `stella_script.exe`.** Only the replay counts: it must reproduce the outcome, proving the captures don't depend on save/load.
      4. Find the moments in the replay (power pill in effect, ghost eaten, death, bonus item, level end, game over) from RAM and pictures.
      5. Save numbered pictures and write `docs\CAPTURES.md`.
      6. **Fallback:** if the bot can't reach a power pill or a level end, Claude Code asks you for a short play session in Stella with F12 snapshots of the missing moments, marked "not reproducible".
    - **What I must do:** Nothing, unless the fallback is needed. Optionally, look at the pictures.
    - **Exact instructions:** only if the fallback is needed (given then).
    - **Required input:** None.
    - **Claude Code prompt:** The resume prompt.
    - **Expected result:**
      - Committed scripts in `tools\captures\` that recreate every picture.
      - Up to 8 numbered pictures in `reference\screens\`, each checked by eye.
      - `docs\CAPTURES.md`.
      - Anything not reached is listed as such.
    - **Verification:**
      - The replay outcome (wafers left, lives) equals the bot's search.
      - Picture fingerprints listed in `docs\CAPTURES.md`, and a second replay giving identical fingerprints.
      - ROM SHA-1s unchanged.
    - **Completion criteria:** at least the scriptable moments (title, start, death, bonus item, game over) captured and reproducible. Power pill and level end either captured the same way, or covered by the fallback, or listed as not captured with your agreement.
    - **Potential problems:**
      - The search may get stuck (long empty corridors) or keep dying. Search depth and horizon are tuned and recorded.
      - Save/load might not be perfectly identical to continuous play. The replay check catches that.
    - **Recovery:** new files only, plus local output in `reference\c7\` and `reference\screens\`. Undo by returning to `090aa6d` (with your OK).
    - **Documentation updates:** `docs\CAPTURES.md` (new), `DECISIONS.md` (T-C7-1), `docs\GLOSSARY.md`, `CLAUDE.md`, `PROJECT_STATUS.md`, `PLAN.md`.
    - **Git checkpoint:** `Stage C7: Reference captures`.
    - **Next stage:** C8 — Gate C: reproducible reference.
8. **[Done] C8 — Gate C: reproducible reference.** *(expanded and done 2026-10-24)*
    - **Objective:** prove Phase C is complete: write `docs\REFERENCE.md` (how anyone recreates this exact reference setup) and `docs\GATE_C.md` (the gate checklist, each item re-checked against the files and tools, not against earlier notes).
    - **Why it matters:** Phase D builds the port and compares it with this reference. The gate makes sure the reference is complete, repeatable and honest about what is still unknown.
    - **Prerequisites:** C1–C7 Done (commit `85e4630`, clean tree).
    - **What Claude Code will do:**
      1. Re-check the reference emulator folder against the official zip's recorded hashes, the three ROM copies, and Git (no ROM committed).
      2. Re-run the C7 replay and compare its pictures with the fingerprints in `docs\CAPTURES.md`; re-run an idle script and compare it with C3's fingerprints.
      3. Cross-check each fact document (display, timing, sound, controls/variations, captures) for its measurement source and for an explicit "not yet measured" list.
      4. Write `docs\REFERENCE.md` and `docs\GATE_C.md`; record anything open as an open item with the stage that will settle it.
    - **What I must do:** nothing, unless a check fails.
    - **Exact instructions:** none.
    - **Required input:** None.
    - **Claude Code prompt:** The resume prompt.
    - **Expected result:** every Gate C item PASS with evidence, or a clear list of what fails.
    - **Verification:**
      - Gate items: display, timing, sound, controls and variations documented; snapshots present; nothing "believed" without a note of how it was measured; reference setup recreatable from `docs\REFERENCE.md`.
      - ROM SHA-1s unchanged; staged list free of ROMs.
    - **Completion criteria:** all Gate C items PASS. Otherwise Phase D does not start (Rule 10).
    - **Potential problems:** a document may cite a measurement that cannot be reproduced now (fix the document or the tool); Stella's folder may have changed (re-compare with the zip).
    - **Recovery:** new documents only plus local output in `reference\c8\`. Undo by returning to `85e4630` (with your OK).
    - **Documentation updates:** `docs\REFERENCE.md`, `docs\GATE_C.md` (new), `DECISIONS.md` (T-C8-n if any), `CLAUDE.md` if facts change, `PROJECT_STATUS.md`, `PLAN.md`.
    - **Git checkpoint:** `Stage C8: Gate C reproducible reference`. An end-of-phase checkpoint zip (as for Gate B) is offered; it writes outside the project, so it needs your OK.
    - **Next stage:** D1 — Architecture note (only if Gate C passes).

### Phase D — First working PC executable (16 stages)

Grow the host program one visible feature at a time, each checked against Stella, until it is a complete Phase 1 release.

1. **[Done] D1 — Architecture note.** *(expanded and done 2026-10-24)*
    - **Objective:** `ARCHITECTURE.md` (project top level) with the three-part table from section 2, file by file: which code is ours, which is Stella's, which behaviour comes from the ROM, and what Phase 2 will replace.
    - **Why it matters:** every Phase D stage adds a host file; this note fixes where each goes and which part owns each behaviour, so the host never starts changing game rules (Rule 2) and Phase 2 knows what to swap.
    - **Prerequisites:** Gate C passed (`docs\GATE_C.md`), commit `8867c7b`, clean tree.
    - **What Claude Code will do:** read what exists (plan section 2, D-001/D-002, `third_party\CMakeLists.txt`, the probes' use of Stella's wrapper, the fact documents), write the note, and record the link style as T-D1-1. No code is written or built.
    - **What I must do:** read the note if you wish and say if anything reads wrong.
    - **Exact instructions:** none.
    - **Required input:** None.
    - **Claude Code prompt:** The resume prompt.
    - **Expected result:** `ARCHITECTURE.md` with: the three parts, a data-flow sketch, a planned file table with owning stage (D2-D13), a "who decides what" table citing measured documents, the Phase 2 replacement table, licences, and open choices.
    - **Verification:** every file path named in the note exists or is marked planned (checked by script); every `docs\` citation exists; every D-stage cited matches `PLAN.md`; no ROM or `reference\` file staged.
    - **Completion criteria:** the checks above pass and the note's open choices point to the stage that settles them.
    - **Potential problems:** a path or API name written from memory could be wrong (hence the script check); the plan's "libretro core" wording could be read as the DLL (T-D1-1 states the actual link style).
    - **Recovery:** new file only. Undo by returning to `8867c7b` (with your OK).
    - **Documentation updates:** `ARCHITECTURE.md` (new), `DECISIONS.md` (T-D1-1), `docs\GLOSSARY.md`, `PROJECT_STATUS.md`, `PLAN.md`.
    - **Git checkpoint:** `Stage D1: Architecture note`.
    - **Next stage:** D2 — An empty window.
2. **[Done] D2 — An empty window.** *(expanded and done 2026-10-24)*
    - **Objective:** the first host program: `pacman2600.exe` opens a black 960×720 window titled with the project's working name and closes cleanly (close button, Alt+F4 or Escape), exit code 0.
    - **Why it matters:** it proves our own code, SDL3 and the compiler work together end to end, and it is the base every later Phase D stage grows from.
    - **Prerequisites:** D1 Done (`ARCHITECTURE.md`, commit `a2ca7f3`); SDL3 builds (B7).
    - **What Claude Code will do (T-D2-1):**
      1. Add `src\host\main.cpp` and `src\host\CMakeLists.txt` (C++23 settings as in T-B3-2, links `SDL3::SDL3`), and `tools\build_host.cmd` (output in `build\host`; `SDL3.dll` copied next to the `.exe`).
      2. The window is created with SDL3 and cleared to black each frame by SDL's renderer. The program is a Windows-subsystem program (no console window) and reports errors in a message box.
      3. A command-line check, `--self-check <file>`, opens the window, reads back its title, size and the colour of a pixel, writes them to the file and quits by itself. Used for the automatic proof; a normal start has no such behaviour.
      4. Build with 0 errors and 0 warnings, run the self-check, then ask you to double-click.
    - **What I must do:** double-click `build\host\pacman2600.exe` (path given at the end), see a black window, close it, and tell me what you saw.
    - **Exact instructions:** Open the folder `F:\Ports\Pac-Man - Atari 2600\build\host` in File Explorer, double-click `pacman2600.exe`. A black window should appear. Close it with its X button. Nothing should be left running, and no error box should appear.
    - **Required input:** Your report of what you saw.
    - **Claude Code prompt:** The resume prompt.
    - **Expected result:** a black 960×720 window; clean exit.
    - **Verification:** build log (0 errors, 0 warnings); self-check output (title, size 960×720, pixel black, exit code 0); process list shows no leftover process; your double-click report; ROM SHA-1s unchanged; no ROM staged.
    - **Completion criteria:** all of the above, including your confirmation.
    - **Potential problems:** `SDL3.dll` not found on a double-click (the copy next to the `.exe` prevents it); Windows display scaling can make the window look larger or smaller than 960×720 pixels (the check reports the real pixel size); a security prompt for an unsigned program may appear (tell me).
    - **Recovery:** new files only plus `build\host`. Undo by returning to `a2ca7f3` (with your OK).
    - **Documentation updates:** `DECISIONS.md` (T-D2-1), `ARCHITECTURE.md` (file table: D2 file built), `docs\GLOSSARY.md`, `CLAUDE.md` (build command), `PROJECT_STATUS.md`, `PLAN.md`.
    - **Git checkpoint:** `Stage D2: An empty window`.
    - **Next stage:** D3 — Finding your ROM.
3. **[Done] D3 — Finding your ROM.** *(expanded and done 2026-10-24)*
    - **Objective:** the program finds the player's own ROM, checks it is the right file, remembers where it is, and says clearly what is wrong when it is not.
    - **Why it matters:** the ROM is never shipped (Rule 7). Every later stage needs a checked ROM, and a wrong or broken file must give a plain message, not a crash or a strange picture.
    - **Prerequisites:** D2 Done (commit `154bf84`).
    - **What Claude Code will do (T-D3-1):**
      1. Add `src\host\rom_finder.*` (no window code): size and SHA-1 check (own small SHA-1, no library), `settings.ini` next to the `.exe` (key `rom_path`), and the search order: path on the command line, then the saved path, then a `roms` folder next to the `.exe`, then a Windows "Open file" dialog (SDL3's).
      2. In `main.cpp`: show a plain-words message for every problem, with "Choose another" and, only when the size is right but the fingerprint differs, "Use anyway" (used for this run only, never remembered). Remember only a ROM that passed all checks.
      3. Add `--find-rom <out_file>` (with optional `--settings <file>`, `--roms-dir <dir>`, and a ROM path) which runs the search without any window or dialog and writes the result, and a test program `tests\host\test_rom_finder.cpp`.
      4. Run the four failure cases plus the good case and show each message.
    - **What I must do:** start `pacman2600.exe` once, pick `rom\pacman.a26` in the dialog, close the window, start it again and say whether it opened without asking.
    - **Exact instructions:** (given at the end of the stage).
    - **Required input:** Your report of the two starts.
    - **Claude Code prompt:** The resume prompt.
    - **Expected result:** first start asks for the file; second start does not; wrong files give clear messages.
    - **Verification:** build with 0 errors/warnings; test program passes all cases (good ROM, missing, empty, 100-byte, 4096-byte wrong content, settings round-trip, search order, a huge-size file is rejected without reading it); `--find-rom` output for each case; your two-start report; the ROM and its SHA-1 unchanged (only read); no ROM staged.
    - **Completion criteria:** all of the above.
    - **Potential problems:** the dialog's callback may arrive on another thread (handled with a lock); Windows paths with non-English letters (handled as UTF-8); the saved path may point at a moved file (then the search continues).
    - **Recovery:** new files plus a local `settings.ini` in `build\host`. Undo by returning to `154bf84` (with your OK).
    - **Documentation updates:** `DECISIONS.md` (T-D3-1), `ARCHITECTURE.md`, `docs\GLOSSARY.md`, `CLAUDE.md`, `PROJECT_STATUS.md`, `PLAN.md`.
    - **Git checkpoint:** `Stage D3: Finding your ROM`.
    - **Next stage:** D4 — Emulator running blind.
4. **[Done] D4 — Emulator running blind.** *(expanded and done 2026-10-24)*
    - **Objective:** the host runs the checked ROM in Stella's core with no window for 600 frames and saves frame 300 as a PNG, and that picture is proved identical to the reference.
    - **Why it matters:** it is the first time our own program runs the original game. Every later stage shows, hears or controls what this step produces, and the proof against Phase C's reference starts here.
    - **Prerequisites:** D3 Done (commit `666b60d`); Gate C reference data in `reference\` (C3 fingerprints, C7 pictures).
    - **What Claude Code will do (T-D4-1):**
      1. Add `src\host\emulator.*`: the only host file that includes Stella headers (ARCHITECTURE.md, T-D1-1). It loads the ROM bytes, runs one frame at a time, and returns the picture as palette indexes (160 x 228) and as colours, plus the 128 bytes of RAM. `stella_core` is linked into the host.
      2. Add `pacman2600 --headless <out_png> [rom_file] [--frames N] [--save-frame N] [--raw <file>] [--settings <ini>] [--roms-dir <dir>]` (defaults 600 and 300). No window, no dialog. It writes the PNG, an optional raw picture, and `<out_png>.txt` (ROM SHA-1, frames, picture size, fingerprint of every frame to a `.csv` option).
      3. Compare with the reference: frame 300's fingerprint and all 600 frames' fingerprints against C3's `reference\c3\dump1\frames.csv`; a run to frame 1500 against C7's `01_title` raw picture, byte for byte.
      4. Look at the PNG by eye.
    - **What I must do:** nothing.
    - **Exact instructions:** none.
    - **Required input:** None.
    - **Claude Code prompt:** The resume prompt.
    - **Expected result:** `reference\d4\frame300.png` showing the attract-mode maze, with fingerprints equal to C3's and the frame-1500 picture equal to C7's.
    - **Verification:**
      - Build 0 errors, 0 warnings; ROM only read (SHA-1 unchanged).
      - All 600 per-frame fingerprints equal C3's; frame-1500 raw picture equal to C7's `frame_001500_160x228.raw`; a second run gives identical output.
      - The program ends with exit code 0 and no window opens; no leftover process.
      - The earlier tests (`test_rom_finder`) and window mode still work.
    - **Completion criteria:** all of the above.
    - **Potential problems:** Stella's headers may clash with Windows or SDL headers (kept apart: only `emulator.cpp` sees Stella); C3's probe sets the Reset input to "off" every frame (checked: the fingerprints must still match without it); the colour picture depends on Stella's palette and phosphor settings (the exact comparison uses the palette indexes).
    - **Recovery:** new files and `build\host` only. Undo by returning to `666b60d` (with your OK).
    - **Documentation updates:** `DECISIONS.md` (T-D4-1), `ARCHITECTURE.md`, `CLAUDE.md`, `docs\GLOSSARY.md`, `PROJECT_STATUS.md`, `PLAN.md`.
    - **Git checkpoint:** `Stage D4: Emulator running blind`.
    - **Next stage:** D5 — The game on screen.
5. **[Done] D5 — The game on screen.** *(expanded and done 2026-10-24)*
    - **Objective:** the window shows the original game's picture, live: the emulator's frames drawn unscaled (one game pixel = one screen pixel, 160 x 228) in the middle of the black window, at normal game speed.
    - **Why it matters:** the first time the game is visible in our own program; D6 (shape and scaling), D7 (fullscreen) and D9 (proper pacing) build on this picture path.
    - **Prerequisites:** D4 Done (commit `68af2f2`).
    - **What Claude Code will do (T-D5-1):**
      1. Create a streaming texture of the emulator's colour picture and draw it each frame, nearest-pixel, unscaled, centred.
      2. Step the emulator from a simple 60.000 Hz clock (temporary; D9 replaces it with real pacing) and ask for vsync explicitly.
      3. Add `--screen-check <png> [rom_file]`: run 300 frames with no clock, read the window's pixels back, compare them with the emulator's colour picture, save the screenshot, write `<png>.txt`.
      4. Check by script, then by eye (the screenshot), then ask you.
    - **What I must do:** start `pacman2600.exe` (it remembers the ROM), look at the window, and say whether the title screen with the maze appears and moves normally (ghosts drifting), then close it.
    - **Exact instructions:** (given at the end of the stage).
    - **Required input:** Your report of what you saw.
    - **Claude Code prompt:** The resume prompt.
    - **Expected result:** a small, unscaled maze picture in the middle of the black window; attract mode runs; no controls yet.
    - **Verification:**
      - Build 0 errors/0 warnings; earlier checks (`test_rom_finder`, `--self-check`, `--headless` fingerprints) still pass.
      - `--screen-check`: pixels read back from the window equal the emulator's colours at the picture's place; the rest of the window is black; screenshot looked at.
      - Window mode runs for several seconds, responds, closes with exit code 0, no leftover process.
      - Speed: frames run per second measured at the clock's rate (about 60), not the monitor's.
      - Your report.
    - **Completion criteria:** all of the above.
    - **Potential problems:** the screen may draw slightly off if Windows scales the window (the check reports real pixel sizes); the temporary clock may drift against the monitor (acceptable until D9); phosphor-blended colours may look dimmer than in Stella (D6).
    - **Recovery:** edits to `src\host\main.cpp` and `CMakeLists.txt` only. Undo by returning to `68af2f2` (with your OK).
    - **Documentation updates:** `DECISIONS.md` (T-D5-1), `ARCHITECTURE.md`, `CLAUDE.md`, `PROJECT_STATUS.md`, `PLAN.md`.
    - **Git checkpoint:** `Stage D5: The game on screen`.
    - **Next stage:** D6 — Correct shape and scaling.
6. **[Done] D6 — Correct shape and scaling.** *(expanded 2026-10-24; see also "Getting the shape right" below)*
    - **Objective:** the game picture is enlarged with the correct pixel shape, in three modes (Sharp, Fill, TV frame), centred with black bars chosen automatically by the window's shape, and the shape is proved by an automatic test on five screen sizes.
    - **Why it matters:** the 2600's pixels are not square, so the raw picture is the wrong shape; the unscaled picture of D5 is also too small to judge. Everything the player sees from here on goes through this layout.
    - **Prerequisites:** D5 Done (commit `9550dde`, follow-up `e5c2d30`).
    - **Finding made at the start (T-D6-1):** Stella's own source (`emucore\TIASurface.cxx` lines 97-98 and 449, `Console.cxx` line 734, `TIAConstants.hxx`) shows that with aspect correction on (its default) the whole picture, `width` x `height` = 160 x 228 for this ROM, is stretched to fill a 4:3 box (its default window is 320 x 240 at zoom 3 = 960 x 720). So a game pixel is `(4/3) x 228 / 160` = **19:10** wide-to-tall here. The 12:7 in the plan assumed a 192-line picture. D6 uses the value derived from the measured height and keeps 12:7 as a selectable alternative for comparison.
    - **What Claude Code will do:**
      1. `src\host\layout.*` (no window code): pixel shape, the three modes and the picture rectangle for any window size.
      2. `tests\host\test_layout.cpp`: five window sizes (1920x1080, 2560x1440, 3840x2160, 3440x1440, 1280x1024) x three modes x two pixel shapes; checks shape within 0.5 %, inside the window, centred, Sharp has a whole-number vertical scale, Fill touches the window on one axis, TV frame is smaller; and that Fill in a 960 x 720 window equals Stella's rectangle exactly.
      3. Window mode: resizable, opens at 1280 x 960; picture drawn with SDL's pixel-art scaling; keys 1, 2, 3 choose Sharp, Fill, TV frame (temporary; D10 gives the keys their final home); `--mode`, `--window WxH` and `--pixel-aspect W:H` options.
      4. `--layout-check <png> [rom]`: opens the real window at the given size/mode, runs 300 frames, reads the pixels back and checks that the drawn picture's outline is where the layout says it should be.
    - **What I must do:** start the program, try keys 1/2/3 and resize the window, and say whether the maze looks right (not stretched or squashed). Optionally compare with Stella (needs your OK to start it; the command is given then).
    - **Exact instructions:** (given at the end of the stage).
    - **Required input:** Your report; the default mode is chosen later, at Gate D.
    - **Claude Code prompt:** The resume prompt.
    - **Expected result:** a clearly larger, correctly proportioned picture; three modes; no distortion at any window size.
    - **Verification:** build 0 errors/0 warnings; `test_layout` all pass; `--layout-check` at several sizes and modes (outline within 2 px of the prediction) with screenshots looked at; earlier checks still pass (`test_rom_finder`, headless fingerprints = C3, `--screen-check` updated); your report.
    - **Completion criteria:** all of the above.
    - **Potential problems:** integer scales rarely give the exact shape on common screens (so Sharp keeps a whole-number vertical scale and takes the horizontal size from the shape); Windows display scaling can change real pixel sizes (the checks read real pixels); the pixel-art filter slightly blends column edges when the horizontal factor is not a whole number.
    - **Recovery:** new files plus edits to `main.cpp`/`CMakeLists.txt`; undo by returning to `e5c2d30` (with your OK).
    - **Documentation updates:** `DECISIONS.md` (T-D6-1), `ARCHITECTURE.md`, `CLAUDE.md`, `docs\GLOSSARY.md`, `PROJECT_STATUS.md`, `PLAN.md`.
    - **Git checkpoint:** `Stage D6: Correct shape and scaling`.
    - **Next stage:** D7 — Windowed and fullscreen.
7. **[Done] D7 — Windowed and fullscreen.** *(expanded 2026-10-24)*
    - **Objective:** borderless fullscreen on the monitor the window is on (your main one at first), toggled with Alt+Enter; the windowed size and position, and whether you were in fullscreen, are remembered between runs; a remembered position that is no longer on any monitor is corrected.
    - **Why it matters:** the game is meant to be played big and uninterrupted; and a port that forgets its window every time feels unfinished. Fullscreen also exercises the layout of D6 at the real 2560x1440 size.
    - **Prerequisites:** D6 Done (commit `ae6bfee`).
    - **What Claude Code will do:**
      1. `src\host\settings.*`: read and write single `key=value` lines of `settings.ini` without losing the other lines (today saving the ROM path would erase anything else). `rom_finder` uses it.
      2. `src\host\windowstate.*` (no window code): the remembered state (position, size, fullscreen), reading/writing it, and a correction step that keeps the window on a real monitor and at a sensible size. Tested by `tests\host\test_windowstate.cpp`.
      3. `main.cpp`: window created from the remembered state; Alt+Enter toggles SDL's borderless desktop fullscreen; the windowed rectangle is tracked while not fullscreen and saved on quit.
      4. `--fullscreen-check <report> [rom]`: in the real window, moves it to an odd size and place, enters fullscreen with the same key handler Alt+Enter uses, checks the window covers the whole monitor and the picture is laid out right (reusing the D6 pixel check), leaves fullscreen and checks the odd size and place came back, then saves. `--state-report <report>` restarts from the saved file and reports what was restored.
    - **What I must do:** start the program, press Alt+Enter, drag and resize the window, close it, start it again, and say what you saw (details at the end of the stage).
    - **Exact instructions:** given at the end of the stage.
    - **Required input:** your report.
    - **Claude Code prompt:** the resume prompt.
    - **Expected result:** Alt+Enter switches between a normal window and a borderless full-screen picture; the window reopens where and how big you left it.
    - **Verification:** build 0 errors/0 warnings; `test_windowstate` and `test_rom_finder` pass; `--fullscreen-check` says OK on this 2560x1440 screen; `--state-report` shows the saved state restored; earlier checks still pass (`test_layout`, `--self-check`, headless fingerprints = C3, `--timed`); your report.
    - **Completion criteria:** all of the above.
    - **Potential problems:** the program cannot press Alt+Enter on a real keyboard for me (the check uses the same handler, your key press is the proof); a second monitor that is unplugged leaves a saved position off screen (the correction handles it, tested with made-up monitor lists); Windows display scaling may change reported sizes (the checks read real pixels); a maximized window is not remembered as maximized.
    - **Recovery:** new files plus edits to `main.cpp`, `rom_finder.cpp` and `CMakeLists.txt`; undo by returning to `ae6bfee` (with your OK). Deleting `settings.ini` resets the remembered state.
    - **Documentation updates:** `DECISIONS.md` (T-D7-n), `ARCHITECTURE.md`, `CLAUDE.md`, `docs\GLOSSARY.md`, `PROJECT_STATUS.md`, `PLAN.md`.
    - **Git checkpoint:** `Stage D7: Windowed and fullscreen`.
    - **Next stage:** D8 — Sound.
8. **[Done] D8 — Sound.** *(expanded 2026-10-24)*
    - **Objective:** the sound the Stella core makes is played through the PC's speakers by SDL, with a small steady buffer (about 4 frames, roughly 67 ms), and the program counts every moment the sound ran short or ran long.
    - **Why it matters:** the music and effects are half of the game's feel; a buffer that is too big adds delay, too small makes crackles. D9 (frame pacing) will build on this buffer, with sound as the master clock.
    - **Prerequisites:** D7 Done (commit `4660834`). The sound facts in `docs\SOUND.md` (31,440 Hz, 16-bit stereo, 524 samples per frame, silent attract mode).
    - **What Claude Code will do:**
      1. `Emulator`: hand out each frame's audio (`audio()`, `audioRate()`), and a console Reset input (`setConsoleReset`) so sound can be produced and tested.
      2. `src\host\audio.*`: an SDL audio stream at 31,440 Hz (SDL converts it to the device's format), prefilled with silence, kept near the target by dropping data that would make the delay too long, and a counter for every time the device asked for sound the stream did not have (an underrun). `--mute` plays the stream with the volume at zero, for tests that must not make noise.
      3. The game window pushes each frame's audio; temporary key **R** = Game Reset (held while pressed) so the three sounds can be heard before the real keyboard of D10.
      4. `--headless ... --audio-wav <file> [--reset-at N]`: writes the audio as a WAV so it can be compared with the C5 reference (`reference\audio\idle\audio.wav`) sample for sample.
      5. `--audio-check <seconds> <report> [rom] [--mute]`: plays for that long on the real sound device and writes the sample rate, the device format, the buffer statistics and the number of underruns.
    - **What I must do:** start the program, press **R** to start a game, listen (the start tune, a blip for each wafer, the death sound) and say whether it sounds right (no crackling, no delay you notice). Needs your speakers on at a safe volume.
    - **Exact instructions:** given at the end of the stage.
    - **Required input:** your report.
    - **Claude Code prompt:** the resume prompt.
    - **Expected result:** the sounds of C5 come out of the speakers in time with the picture, with no crackle.
    - **Verification:** build 0 errors/0 warnings; the audio of the host (2,500 frames with Reset at 1800) equals `reference\audio\idle\audio.wav` byte for byte for those frames; a muted 5-minute run on the real device logs 0 underruns (or a counted, explained number); the report shows the device accepted 31,440 Hz and the queue stayed near the target; earlier checks still pass; your ear test.
    - **Completion criteria:** all of the above.
    - **Potential problems:** I cannot hear, so "sounds right" rests on the sample-for-sample comparison and on you; with the temporary 60 Hz clock the game and the sound card clocks differ slightly, so the buffer slowly drifts (the program trims it and counts it; D9 removes the cause); dragging the window freezes the loop and can cause an underrun; a PC with no sound device must not crash the game (it plays silently and says so).
    - **Recovery:** new files plus edits to `emulator.*`, `main.cpp`, `CMakeLists.txt`; undo by returning to `e9629ab` (with your OK).
    - **Documentation updates:** `DECISIONS.md` (T-D8-n), `ARCHITECTURE.md`, `CLAUDE.md`, `docs\GLOSSARY.md`, `docs\SOUND.md` (what D8 proved), `PROJECT_STATUS.md`, `PLAN.md`.
    - **Git checkpoint:** `Stage D8: Sound`.
    - **Next stage:** D9 — Frame pacing on a 180 Hz screen.
9. **[Done] D9 — Frame pacing on a 180 Hz screen.** *(expanded 2026-10-24)*
    - **Objective:** the game advances at the console's rate (60.000 frames a second, the rate Stella plays at and C4 measured), whatever the monitor's rate is. The sound card's clock is the master clock: a game frame is run only when the sound card has used up enough sound, so the music can never drift against the picture. The window shows the newest game picture on every monitor refresh (about three refreshes per game frame on a 180 Hz screen), and the program counts how many refreshes each game frame really got.
    - **Why it matters:** this is what makes the game feel right on any monitor. Pacing from the monitor would run the game 3 times too fast at 180 Hz; pacing from a separate PC clock slowly drifts from the sound.
    - **Prerequisites:** D8 Done (commit `72926b1`); `docs\TIMING.md` (C4).
    - **Technical decision (T-D9-1, made by Claude Code):** pace at **60.000 Hz** (Stella's own rate, which C4 measured and the reference recordings use), not 59.923 Hz (the real console's crystal rate). It keeps every side-by-side check with Stella exact, and the sound already has exactly 524 sample pairs per frame (31,440 / 60). A 59.923 Hz option stays a possible later setting.
    - **What Claude Code will do:**
      1. `AudioOutput`: report how much sound is waiting and the target.
      2. Game thread: run a frame when less than the target (4 frames, 67 ms) is waiting, one at a time and at most 5 per turn; fall back to the old timer clock when there is no sound device, or if the sound device has not used any sound for 100 ms (so the picture never freezes). `--timer-clock` forces the timer for comparison.
      3. Count, on the window thread, how many presents (monitor refreshes) each game frame was on screen (1, 2, 3, 4, 5 or more), the presents per second, and write them with the sound numbers to `<settings>.audio.txt`.
      4. `--audio-check` uses the same pacing.
      5. A 10-minute muted run of the real window; shorter runs with the timer clock for comparison.
    - **What I must do:** play for a minute or two in the window and in fullscreen and say whether the speed and smoothness feel right (no stutter, same speed as before); optionally say which refresh rate your monitor is set to.
    - **Exact instructions:** given at the end of the stage.
    - **Required input:** your report.
    - **Claude Code prompt:** the resume prompt.
    - **Expected result:** the game runs at the same speed as before (60 frames a second) and the sound has no gaps; frames are shown evenly.
    - **Verification:** build 0 errors/0 warnings; a 10-minute muted window run shows frames per second within 0.1 % of 60.000, 0 underruns and 0 dropped sound; the refresh counts per game frame are reported (on 180 Hz mostly 3); an 800 ms window freeze and the timer-clock fallback behave; earlier checks still pass (headless audio and fingerprints, layout, window state, fullscreen); your report.
    - **Completion criteria:** all of the above.
    - **Potential problems:** a monitor whose rate is not a whole multiple of 60 Hz (for example 144 Hz) cannot show every game frame for the same number of refreshes, so a slight unevenness is unavoidable without variable refresh; the sound card's clock differs a little from the PC's clock, so the game runs a few parts per million off the PC clock (harmless, and what keeps sound and picture together); a sound device that is unplugged mid-game triggers the timer fallback.
    - **Recovery:** edits to `audio.*`, `main.cpp` only; undo by returning to `72926b1` (with your OK).
    - **Documentation updates:** `DECISIONS.md` (T-D9-n), `ARCHITECTURE.md`, `CLAUDE.md`, `docs\TIMING.md` (what D9 chose), `PROJECT_STATUS.md`, `PLAN.md`.
    - **Git checkpoint:** `Stage D9: Frame pacing`.
    - **Next stage:** D10 — Keyboard.
10. **[Done] D10 — Keyboard.** *(expanded 2026-10-24)*
    - **Objective:** the game can be played from the keyboard: arrows or WASD steer, Space is the fire button, F2 is Game Select, F3 is Game Reset (both held while pressed, like the console's switches), F5 and F6 flip the left and right difficulty switches. F1 shows the list of keys on top of the picture. The keys of the earlier stages get their final homes (1-4 picture modes, Alt+Enter fullscreen, Esc quits); the temporary R key is replaced by F3.
    - **Why it matters:** until now nobody could steer Pac-Man; the wafer-eating blip could not even be heard (D8). Everything after this (controller, recordings, side-by-side checks) builds on one shared way of turning input into console events.
    - **Prerequisites:** D9 Done (commit `988a2e1`); `docs\CONTROLS.md` (C6: how the game reads the joystick, the switches and the fire button).
    - **What Claude Code will do:**
      1. `src\host\controls.*` (no window code, only SDL's key-code names): the key table, a keyboard state (keys down, opposite directions cancel, everything released when the window loses focus, difficulty switches flip on a fresh press only), and the text of the F1 list. Tested by `tests\host\test_controls.cpp`.
      2. `Emulator`: joystick, Select, difficulty switches and a read-back of the console switch register (for tests).
      3. Window: the window thread turns key events into an input state and hands it to the game thread through one atomic number; the game thread applies it at the start of every frame.
      4. F1 overlay drawn with SDL's built-in text; the window title says "F1 = keys".
      5. `--keys-replay <script> <csv> [rom]`: no window; replays a Phase C input script (`from to NAME`, the format of `tools\stella_script`) **by pressing the real keys in the real keyboard code**, and writes the picture fingerprint of every frame. The fingerprints are compared with the recorded Phase C runs (`reference\c6\*` experiments and `reference\audio\play1`, 6,000 frames of random joystick play).
      6. `--help-check <png> [rom]`: real window with the F1 list up; reads the pixels back and saves a screenshot.
    - **What I must do:** start the program, press F3 to start a game, steer with the arrows or WASD, listen for the wafer blip, press F1, try F2/F5/F6 if you like, and tell me whether the keys feel right and whether anything is awkward (a key you would rather have elsewhere is a fine answer).
    - **Exact instructions:** given at the end of the stage.
    - **Required input:** your report.
    - **Claude Code prompt:** the resume prompt.
    - **Expected result:** Pac-Man moves with the keys; Game Select and Reset behave as in Stella; the F1 list names every key.
    - **Verification:** build 0 errors/0 warnings; `test_controls` passes; the replays reproduce the recorded fingerprints with 0 differences (select, difficulty switches, fire, directions, turns, and the 6,000-frame random play); the F1 screenshot looked at; earlier checks still pass; your play test.
    - **Completion criteria:** all of the above.
    - **Potential problems:** the replay proves the keys reach the right console events, not that a physical keyboard key arrives (a keyboard with ghosting can drop three-key combinations); two opposite directions at once cancel (a real joystick cannot do both); Colour/B&W is D12; the game's own use of the difficulty switches is only partly known (`docs\CONTROLS.md`).
    - **Recovery:** new files plus edits to `emulator.*`, `main.cpp` and `CMakeLists.txt`; undo by returning to `988a2e1` (with your OK).
    - **Documentation updates:** `DECISIONS.md` (T-D10-n), `ARCHITECTURE.md`, `CLAUDE.md`, `docs\GLOSSARY.md`, `docs\CONTROLS.md` (the port's keys), `PROJECT_STATUS.md`, `PLAN.md`.
    - **Git checkpoint:** `Stage D10: Keyboard`.
    - **Next stage:** D11 — PS4 controller.
11. **[Done] D11 — PS4 controller.** *(expanded 2026-10-24)*
    - **Objective:** a PS4 pad (and any pad SDL knows as a gamepad) plays the game: the D-pad and the left stick steer, Cross or Square is the fire button, **Options = Game Reset, Share = Game Select**. Plugging in, unplugging and plugging in again during a game is safe: nothing stays pressed, nothing crashes, the pad works again when it is back. The keyboard keeps working at the same time.
    - **Why it matters:** this is the way the owner will most likely play; the C6 test showed that Stella itself needs the pad's stick and a button, and the port should do at least that, on purpose.
    - **Prerequisites:** D10 Done (commit `2ceb865`); the owner's PS4 pad, connected as "Wireless Controller" with DS4Windows **not** running (as in C6).
    - **What Claude Code will do:**
      1. `src\host\controls.*` (no window code): pure rules for pad input. Stick: a dead zone (a push must pass 50 % to count and stays counted until it falls under 30 %), one direction at a time (the stronger axis wins, so a slightly crooked push is not a diagonal), with a little stickiness so the direction does not flicker at 45 degrees; D-pad; buttons; the merge with the keyboard (opposite directions cancel). Tested in `tests\host\test_controls.cpp`.
      2. `main.cpp`: open pads when they appear and close them when they go (SDL's gamepad events), read the state of every open pad each time round the window loop, ignore the pads while the window does not have the focus, add the pad lines and "Controller: <name> / none" to the F1 list.
      3. `--pad-check <report> <rom>`: attaches a **virtual** gamepad (SDL's own test device, which goes through the same gamepad code as a real one), runs the real game, and checks each button and the stick against the game's own memory and the console switches, then unplugs the pad while the stick is held, checks that nothing stays pressed, plugs it in again and checks it works. Also lists the real pads found.
    - **What I must do:** connect the PS4 pad (DS4Windows not running), start the program, play with the D-pad and the stick, press Options and Share, and tell me what you see; then pull the cable (or turn the pad off) while moving, and plug it back in. Tell me which buttons feel wrong.
    - **Exact instructions:** given at the end of the stage.
    - **Required input:** the PS4 pad; your report.
    - **Claude Code prompt:** the resume prompt.
    - **Expected result:** Pac-Man follows the stick and D-pad; Options starts a new game; Share steps through the variations; unplugging does not crash and does not leave Pac-Man running into a wall forever; replugging works.
    - **Verification:** build 0 errors/0 warnings; `test_controls` passes with the new pad checks; `--pad-check` says OK for every button, the stick (all four directions), unplug and replug; the F1 list screenshot looked at; earlier checks still pass (key replay: 0 differences, audio, fingerprints, window state, layout, fullscreen); your play test with the real pad.
    - **Completion criteria:** all of the above.
    - **Potential problems:** the virtual pad proves our code and SDL's gamepad layer, not the pad's own radio or cable; a PS4 pad used through Steam or DS4Windows appears as an Xbox pad (the buttons are then in Xbox positions: Menu/View instead of Options/Share, and the mapping is still sensible); Bluetooth pads sleep after a while; the fire button has no effect during play (C6), so it only matters on the title screen.
    - **Recovery:** edits to `controls.*`, `main.cpp`, `CMakeLists.txt` and the test only; undo by returning to `2ceb865` (with your OK).
    - **Documentation updates:** `DECISIONS.md` (T-D11-n), `ARCHITECTURE.md`, `CLAUDE.md`, `docs\GLOSSARY.md`, `docs\CONTROLS.md`, `PROJECT_STATUS.md`, `PLAN.md`.
    - **Git checkpoint:** `Stage D11: PS4 controller`.
    - **Next stage:** D12 — Console switches.
12. **[Done] D12 — Console switches.** *(expanded 2026-10-25)*
    - **Objective:** the three real switches of the console (left difficulty, right difficulty, Colour / B&W) are all available on the keyboard (F5, F6, **F4**), start where they were left last time (remembered in `settings.ini`), and the F1 list says where each one is now.
    - **Why it matters:** the game reads all three (C6: B&W makes the game itself switch to grey; the difficulty switches change the bonus item and the two-player game). Phase E/F need to flip them to find out the rules, and the owner needs to see them.
    - **Prerequisites:** D11 Done (commit `67bf4f4`).
    - **What Claude Code will do:**
      1. `controls.*`: a tenth action, TV type on F4, as a latched switch like F5/F6 (Colour at the start); `Input.blackWhite`; `Keyboard::setSwitches` to restore remembered positions; the F1 line shows the position; pads never change it.
      2. `emulator.*`: `setBlackWhite` (Stella's own Colour and Black/White events). `main.cpp`: all three switches are read from `settings.ini` at start and written whenever one of them changes; `--keys-replay` now plays COLOR and BW.
      3. `tools\d10_replay_compare.py` no longer skips the B&W experiment.
    - **What I must do:** start the program, press F4 and watch the picture go grey and back, press F1 to see the list; close and start again: the switches must be where you left them.
    - **Exact instructions:** given at the end of the stage.
    - **Required input:** your report.
    - **Claude Code prompt:** the resume prompt.
    - **Expected result:** F4 turns the picture grey (the game itself changes its colours) and back; F5/F6 as before; the three positions come back after a restart.
    - **Verification:** build 0 errors; `test_controls` (new F4 checks); replay of all recorded Phase C inputs including `switches\bw` with 0 differences; injected F4 in the real window reaches the switch register and the ini file; a second run restores the switches; F1 list screenshot; earlier checks still pass.
    - **Completion criteria:** all of the above plus the owner's try.
    - **Potential problems:** B&W only changes what the game asks for; this port does not tint the picture itself. The difficulty switches' rules are still unknown (Phase E/F).
    - **Recovery:** edits to `controls.*`, `emulator.*`, `main.cpp`, one test and one tool; undo by returning to `67bf4f4` (with your OK).
    - **Documentation updates:** `DECISIONS.md` (T-D12-1), `docs\CONTROLS.md`, `docs\GLOSSARY.md`, `ARCHITECTURE.md`, `CLAUDE.md`, `PROJECT_STATUS.md`, `PLAN.md`.
    - **Git checkpoint:** `Stage D12: Console switches`.
    - **Next stage:** D13 — Input recording and replay.
13. **[Done] D13 — Input recording and replay.** *(expanded 2026-10-25)*
    - **Objective:** the program can record what the console was given on every frame to a small `.inputs` text file and play it back, in the window and with no window. Replaying gives identical pictures, proved by the picture fingerprint of every frame.
    - **Why it matters:** this is the foundation of all later testing (D14 side-by-side check, Phase E rule-finding, Phase H regression tests): a played game becomes a repeatable test.
    - **Prerequisites:** D12 Done (commit `7e4e560`).
    - **What Claude Code will do:**
      1. `src\host\inputlog.*` (no window code): the file format (header with the ROM's SHA-1 and the number of frames, then one line per change of input), a recorder, a reader that refuses bad files in plain words; `tests\host\test_inputlog.cpp`.
      2. `main.cpp`: `--record <file>` (window; written when the window closes), `--replay <file>` (window and `--headless`; refuses a recording made with another ROM; when it runs out the keyboard takes over), `--fingerprints <csv>` also in the window (tests).
      3. `tools\record_inputs.cmd` (plays and records into `recordings\`, which Git ignores) and `tools\replay_inputs.cmd` (drop a file on it).
    - **What I must do:** run `tools\record_inputs.cmd`, play for a minute (steer, eat, die, change a switch), close the window with Esc; then drop the new file from `recordings\` on `tools\replay_inputs.cmd` and watch: it must play out the same game.
    - **Exact instructions:** given at the end of the stage.
    - **Required input:** your report.
    - **Claude Code prompt:** the resume prompt.
    - **Expected result:** the replay is the same game, move for move, ghosts included.
    - **Verification:** `test_inputlog` passes; a window recording made with injected keys, replayed in the window and headless, gives identical fingerprints on every frame (720 frames, and a longer 2,399-frame one); the replayed window writes a recording equal to the one it played; bad, foreign-ROM and missing files are refused; earlier checks still pass.
    - **Completion criteria:** all of the above plus the owner's try.
    - **Potential problems:** the game starts from power-on with empty memory, so a recording always starts there (the real Atari's memory at power-on is random, Stella's app too; ours is not). Replaying a recording in a different version of the program only works while the game logic stays the same. A bad file in the window is shown as a message box (not checked automatically).
    - **Recovery:** new files plus edits to `main.cpp`, `CMakeLists.txt`, `build_host.cmd` and `.gitignore`; undo by returning to `7e4e560` (with your OK).
    - **Documentation updates:** `DECISIONS.md` (T-D13-1), `ARCHITECTURE.md`, `CLAUDE.md`, `docs\GLOSSARY.md`, `PROJECT_STATUS.md`, `PLAN.md`.
    - **Git checkpoint:** `Stage D13: Input recording and replay`.
    - **Next stage:** D14 — Side-by-side check.
14. **[Done] D14 — Side-by-side check.** *(expanded 2026-10-25)*
    - **Objective:** the same input is run in our program and in Stella (7.0, the same emulation), and the pictures are compared: every frame's fingerprint, and ten whole pictures byte for byte. Any difference is written to `DISCREPANCIES.md`.
    - **Why it matters:** it proves that the port, with its window, recording, switches and replay, shows what Stella shows, before the release (D15) and before Phase E builds on it.
    - **Prerequisites:** D13 Done (commit `ca88e59`); the Phase C reference tool `build\stella_probe\stella_script.exe`.
    - **What Claude Code will do:**
      1. `tools\d14_side_by_side.py`: turns a recording (`.inputs`) or a Phase C script into a Stella input script and back; runs the reference (`stella_script.exe`), our headless replay and, with `--window`, our real window replaying the recording; compares the fingerprint of every frame and ten whole pictures.
      2. Runs it on: your own recordings (human play, with switches changed) and the bot's 9,800-frame route (C7: ghost eaten, power pills, death, bonus item, level end, game over).
      3. Proves the tool can see a difference (a recording with one key press removed must differ).
      4. `DISCREPANCIES.md`: the result and the known, intended differences from the Stella app.
    - **What I must do:** read the result; optionally (only with your OK, because it starts Stella) look at the real Stella app next to our window.
    - **Exact instructions:** given at the end of the stage.
    - **Required input:** your OK of the result.
    - **Claude Code prompt:** the resume prompt.
    - **Expected result:** 0 differences, or every difference written down and explained.
    - **Verification:** the numbers printed by the tool (frames, pictures, differences) for every input, for the headless and the window replay; the negative test.
    - **Completion criteria:** all of the above and the owner's OK.
    - **Potential problems:** "Stella" here is its emulation core as built in Phase C (zeroed memory at power-on), not the Stella app, which cannot read our recordings and starts with random memory, so its frames cannot be compared one by one. Sound is not part of this comparison (audio was proved identical to the Phase C audio in D8).
    - **Recovery:** one new tool and two documents; undo by returning to `ca88e59` (with your OK).
    - **Documentation updates:** `DECISIONS.md` (T-D14-1), `DISCREPANCIES.md`, `CLAUDE.md`, `PROJECT_STATUS.md`, `PLAN.md`.
    - **Git checkpoint:** `Stage D14: Side-by-side check`.
    - **Added during the stage (owner's rule D-004, authentic first):** speed and sound pitch changed to the real console's (T-D14-2); power-on memory examined (T-D14-3); `DISCREPANCIES.md` now holds an authenticity table; open: the picture shape.
    - **Next stage:** D15 — Release folder.
15. **[Done] D15 — Release folder.** *(expanded 2026-10-26)*
    - **Objective:** `tools\make_release.ps1` builds `out\PacMan2600-0.1.0-windows.zip`: the program, the DLLs it needs, the licences, a README for the player, the source code the licences require, and **no ROM**. The script itself scans the finished zip, and fails (keeping nothing as the finished file) if a ROM or anything that could be one is inside.
    - **Why it matters:** this is the first thing that can be handed to another person; Rule 7 says the ROM must never be in a public release, and the program contains Stella (GPL v2), which brings duties (licence text, source).
    - **Prerequisites:** D14 Done and the D-006 picture mode committed (`73f946c`); a clean Git tree; the program built from it (`tools\build_host.cmd`).
    - **What Claude Code will do:**
      1. Find out what the program needs on a PC that has no Visual Studio (`dumpbin /dependents`): `SDL3.dll`, and three Microsoft runtime files (`msvcp140.dll`, `vcruntime140.dll`, `vcruntime140_1.dll`) that Microsoft allows to be shipped next to a program.
      2. Write `tools\make_release.ps1`: refuses if the Git tree is not clean or the program is older than its sources; builds the zip with .NET's zip classes (nothing is deleted; the zip is written as `.partial`, scanned, then renamed); contents: `pacman2600.exe`, DLLs, `README.txt`, `BUILD-INFO.txt` (commit, date, SHA-256 of every file), `licences\`, `roms\PUT_YOUR_ROM_HERE.txt`, `source\...-source.zip` made by `git archive` from the tracked source folders only.
      3. The scan: every file in the zip, and inside the nested source zip, is checked for: file endings `.a26 .bin .rom`; the SHA-1 of the known ROM; any 4,096-byte file that is not a known text or source file type. Also a test that the scan really fails (a bad zip made on purpose in `out\` with a fake 4 KB file, never the ROM).
      4. Unzip the result into a fresh folder under `out\` and run the program from there (`--self-check`, `--find-rom` with no ROM = exit 5, and with the ROM given by path from `rom\` — the ROM is never copied).
    - **What I must do:** unzip the release somewhere new (for example your desktop), start `pacman2600.exe`, pick your ROM when asked (or put it in `roms\`), and play for a minute; say if anything is missing or unclear in the README.
    - **Exact instructions:** given at the end of the stage.
    - **Required input:** your try of the unzipped release.
    - **Claude Code prompt:** the resume prompt.
    - **Expected result:** a zip of a few megabytes (the source adds more) that runs from any folder, asks for the ROM, and contains none.
    - **Verification:** the script's own report (files, sizes, scan result); the failing-scan test; the unzipped program runs; `dumpbin` shows every needed DLL is in the zip or part of Windows; Git status clean afterwards; the zip is not in Git (`/out/` is ignored).
    - **Completion criteria:** all of the above and the owner's try.
    - **Potential problems:** the Microsoft runtime files come from the Visual Studio Build Tools' `Redist` folder on this PC (read only; their licence allows shipping them with a program); the owner chooses the licence of their own code before any public release (the program as a whole must be GPL v2 compatible because Stella is GPL v2); the README says so and does not choose for them.
    - **Recovery:** new script and generated files in `out\` only (ignored by Git); undo by returning to `73f946c` (with your OK).
    - **Documentation updates:** `DECISIONS.md` (T-D15-n), `CLAUDE.md`, `docs\GLOSSARY.md`, `PROJECT_STATUS.md`, `PLAN.md`.
    - **Git checkpoint:** `Stage D15: Release folder`.
    - **Next stage:** D16 — Gate D: Phase 1 done.
16. **[Done] D16 — Gate D: Phase 1 done.** *(expanded 2026-10-26)*
    - **Objective:** decide, with evidence, whether Phase D is complete: the release zip, unzipped in a new place, plays Pac-Man correctly. Items: correct shape, fullscreen and windowed, sound, keyboard, pad, ROM picker, replays identical, discrepancies list reviewed, version `0.1.0` tagged in Git.
    - **Why it matters:** Rule 10: no new major phase (E: play recordings, F: the native rebuild) until the previous one meets its completion criteria. This is also the first version that is a finished thing.
    - **Prerequisites:** D15 Done (commit `72d8ca7`); the release zip `out\PacMan2600-0.1.0-windows.zip`.
    - **What Claude Code will do:**
      1. `tools\d16_gate_check.ps1 <unzipped folder>`: runs the checks that need no human on the program **from the unzipped release**: the picture shape and border in the window at four sizes, the real fullscreen round trip, sound with 0 underruns, injected keys reaching the game and the switches, the virtual pad, recording then replaying (window and no window, identical fingerprints), the ROM search (nothing found, wrong file, right file by path; the ROM is never copied), and the release's own files (no ROM, SHA-256 equal to `BUILD-INFO.txt`).
      2. Re-runs the side-by-side check against Stella (`tools\d14_side_by_side.py`) and the key and unit tests, so the gate rests on fresh runs, not on earlier notes.
      3. `docs\GATE_D.md`: the checklist with a verdict and the evidence for each item, and what only you can judge.
    - **What I must do:** unzip the release somewhere new, start `pacman2600.exe`, use the real ROM picker once (or the `roms` folder), play a **full game** (all three lives): sound, keys, the PS4 pad, fullscreen (Alt+Enter), a recording and its replay (`pacman2600.exe --record my.inputs`, then `--replay my.inputs`), and read `DISCREPANCIES.md`. Then tell me yes or what is wrong.
    - **Exact instructions:** given at the end of the stage.
    - **Required input:** your full play-through and your verdict on `DISCREPANCIES.md`.
    - **Claude Code prompt:** the resume prompt.
    - **Expected result:** every item PASS; tag `v0.1.0` made in Git (local; nothing is pushed).
    - **Verification:** the gate script's report; fresh side-by-side run; your play.
    - **Completion criteria:** all items PASS with evidence, your OK, the tag.
    - **Potential problems:** the ROM picker is a Windows dialog that I cannot click for you (it is only checked by you); the pad needs the real pad; sound quality is judged by ear.
    - **Recovery:** new files only (a script, a document, a tag); undo by returning to `72d8ca7` and removing the tag (with your OK).
    - **Documentation updates:** `docs\GATE_D.md`, `PROJECT_STATUS.md`, `PLAN.md`, `DECISIONS.md` if anything is decided.
    - **Git checkpoint:** `Stage D16: Gate D` and the tag `v0.1.0`.
    - **Next stage:** E1 — see Phase E.

#### Getting the shape right (stage D6 in detail)

**Result of D6 (2026-10-24):** the 12:7 below assumed a 192-line picture. Stella's own aspect correction, read from its source, stretches the whole 160 x 228 picture of this ROM to a 4:3 box, which makes a game pixel 19:10. That is the default; 12:7 stays selectable. See `DECISIONS.md` T-D6-1.

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

Build a library of recorded games, each paired with exactly what the original did, so Phase F has a precise target. **No stage waits for the owner's play (D-008, 2026-10-26):** every input comes from Claude Code's own scripts, from the search-playing bot `stella_bot` (C7, which already cleared level 1 and played to game over), and from seeded random input. A recording made by the owner is welcome but never needed. Every recording is replayed by the emulator build and saved as a "golden" trace: the 128 RAM bytes, a picture fingerprint and a sound fingerprint for every frame. These traces stay local (they come from Atari's program).

1. **[Done] E1 — Baseline format.** *(expanded 2026-10-26)*
    - **Objective:** fix what one baseline test is (`docs\BASELINE.md`), and give the host a trace switch (`--trace <file>`) that writes, for every frame of a replay, exactly what the original did: the input, the switch register, the picture fingerprint, the sound fingerprint and all 128 bytes of memory. Prove that a replay gives a byte-identical trace every time.
    - **Why it matters:** Phase F rebuilds the game in our own code and tests it against these traces frame by frame (lockstep, F20); a trace that is not exactly repeatable would make that test meaningless.
    - **Prerequisites:** D16 Done (Gate D passed, tag `v0.1.0`, commit `c9509dc`).
    - **What Claude Code will do:**
      1. `src\host\trace.*` (no emulator code): the trace file format and the fingerprint function; `tests\host\test_trace.cpp`.
      2. `--trace <file>` in the host, for `--headless --replay` and for the window with `--replay` or `--record`, all from the same code, so a window trace and a no-window trace of one recording can be compared.
      3. `baseline\` (in Git: only inputs and plain-words entries) and `reference\baseline\` (local: the traces, which carry Atari's memory contents): the first entry `BL-001` as the worked example, made by `tools\baseline.py`, which also checks an entry (replay twice, compare byte for byte, compare with the recorded SHA-256).
      4. Cross-check the trace against the independent Phase C reference run of Stella's core (RAM and picture of every frame of the 9,800-frame bot route).
      5. `docs\BASELINE.md`: the entry format, the trace columns, where things live, what is committed and what is not, how Phase F uses it.
    - **What I must do:** nothing in this stage except read `docs\BASELINE.md` and say if the plain-words description of an entry is something you could check against the real game.
    - **Exact instructions:** given at the end of the stage.
    - **Required input:** none.
    - **Claude Code prompt:** the resume prompt.
    - **Expected result:** `docs\BASELINE.md`; `--trace` working; `BL-001` with a trace that replays identically.
    - **Verification:** `test_trace` passes; the same recording traced twice no-window gives equal files; the window trace equals the no-window trace; the trace's RAM and picture columns equal the Phase C reference on all 9,800 route frames; trace SHA-256 stored in the entry equals a fresh run; the earlier checks (fingerprints = C3, audio = C5, key replay) still pass.
    - **Completion criteria:** all of the above.
    - **Potential problems:** a trace is large (about 300 bytes a frame: a 10-minute game is 11 MB), so traces stay local and only their SHA-256 goes into Git; a window run can drop or repeat a frame only if the program is stalled (the frame counter, not the clock, is what is recorded).
    - **Recovery:** new files plus a small edit to `main.cpp`, `CMakeLists.txt`, `build_host.cmd`, `.gitignore`; undo by returning to `3dd32c4` (with your OK).
    - **Documentation updates:** `docs\BASELINE.md`, `DECISIONS.md` (T-E1-n), `CLAUDE.md`, `docs\GLOSSARY.md`, `PROJECT_STATUS.md`, `PLAN.md`.
    - **Git checkpoint:** `Stage E1: Baseline format`.
    - **Next stage:** E2 — Starting up.
2. **[Done] E2 — Starting up.** *(expanded and done 2026-10-26)*
    - **Objective:** baseline entries for everything that happens before and as a game starts: power-on, the attract mode, Game Select through all 16 selections, Game Reset and the fire button starting a game.
    - **Why it matters:** these are the first behaviours the rebuilt game must reproduce, and the cheapest to test (no playing needed); they also fix the attract-mode timer and the selection bytes that later entries depend on.
    - **Prerequisites:** E1 Done (`59ba6e1`); `tools\baseline.py`.
    - **What Claude Code will do:**
      1. `tools\e2_entries.py`: writes the inputs for BL-002 to BL-025 (power-on and attract; the Select cycle with 17 presses; each of the 16 selections followed by Reset; the fire button from the attract mode; Reset at five different moments of the attract mode), makes each trace twice through `tools\baseline.py` (byte-identical required), and writes the plain-words observations **from the traces** (no number is typed by hand).
      2. Cross-check against the independent Phase C reference run (`reference\c6\idle`).
      3. Correct what Stage C6 recorded if the traces disagree.
    - **What I must do:** nothing (D-008).
    - **Exact instructions:** none.
    - **Required input:** none.
    - **Claude Code prompt:** the resume prompt.
    - **Expected result:** 24 new entries (BL-002 to BL-025), every one repeating byte for byte.
    - **Verification:** each entry's two traces identical and its SHA-256 stored; a fresh `baseline.py check` on all 25 entries; BL-002 (first 1,800 frames) and BL-024 (all 2,700) equal the Phase C reference run in memory and picture; the generator's own checks (the selection reached after each Select press; the start; the attract timer rule).
    - **Completion criteria:** all of the above.
    - **Potential problems:** a number written into an observation could be wrong or over-claimed; that is why they are computed, and why two over-claims were caught and removed while writing this stage (byte `$CC` is the game number only while Select is pressed; `$81`/`$EC` are an attract timer, not a variation setting).
    - **Recovery:** new files only (`baseline\`, a tool, documents); undo by returning to `59ba6e1` (with your OK).
    - **Documentation updates:** `docs\BASELINE.md`, `docs\CONTROLS.md` (a correction), `DECISIONS.md` (T-E2-1), `CLAUDE.md`, `PROJECT_STATUS.md`, `PLAN.md`.
    - **Git checkpoint:** `Stage E2: Starting up`.
    - **Next stage:** E3 — Pac-Man's movement.
3. **[Done] E3 — Pac-Man's movement.** *(expanded and done 2026-10-26)*
    - **Objective:** baseline entries for how Pac-Man moves: speed, stopping at walls, turning at corners (also when the turn is asked for early), reversing, a stick held diagonally, the openings at the top and bottom of the maze, and bot play over a whole level.
    - **Why it matters:** the rebuilt game must move Pac-Man pixel for pixel like the original, including the odd frame pattern of its steps and the way a held turn is remembered.
    - **Prerequisites:** E2 Done (`8daa0e8`); `tools\baseline.py`, `tools\e2_entries.py`.
    - **What Claude Code will do:**
      1. Explore with throw-away scripts which situations show different behaviour (turn early, brief request, reversal, diagonals, the opening).
      2. `tools\e3_entries.py`: writes BL-026 to BL-037 (BL-035 is the Stage C7 bot route), makes each trace twice (byte-identical required), and writes the observations from the traces (steps, legs, frames between steps, direction changes, wall stops, pauses).
      3. Cross-check the new inputs against the independent Phase C reference core (`tools\d14_side_by_side.py`).
    - **What I must do:** nothing (D-008).
    - **Exact instructions:** none.
    - **Required input:** none.
    - **Claude Code prompt:** the resume prompt.
    - **Expected result:** 12 new entries (BL-026 to BL-037); 37 entries in all.
    - **Verification:** every new entry's two traces identical; a fresh `baseline.py check` of all 37 equals the stored SHA-256 values; six new inputs (BL-026, 028, 031, 034, 036, 037) give 0 fingerprint differences and 0 different pictures against Stella's own script runner; BL-035 shows no step larger than one pixel in 9,800 frames.
    - **Completion criteria:** all of the above.
    - **Potential problems:** the opening at the bottom and top of the maze starts a pause of 84 frames in which bytes $B1/$B6 hold other values than Pac-Man's place; what the game is doing in those frames (and what the status-bar number means) is not understood yet and is written as measured only (Phase F). Scripted entries last only about 150 frames of play before a ghost catches Pac-Man, so long cases come from the bot.
    - **Recovery:** new files only; undo by returning to `8daa0e8` (with your OK).
    - **Documentation updates:** `docs\BASELINE.md`, `docs\CONTROLS.md`, `DECISIONS.md` (T-E3-1), `CLAUDE.md`, `docs\GLOSSARY.md`, `PROJECT_STATUS.md`, `PLAN.md`.
    - **Git checkpoint:** `Stage E3: Pac-Man's movement`.
    - **Next stage:** E4 — Ghosts.
4. **[Done] E4 — Ghosts.** *(expanded and done 2026-10-26)*
    - **Objective:** baseline entries for the four ghosts: where they start, when and how fast they move, how their path depends on Pac-Man and on input timing (the question of randomness), and what a power pill and an eaten ghost do to them.
    - **Why it matters:** the ghosts' choices decide almost everything in a game; the rebuilt game must reproduce each ghost's place on every frame.
    - **Prerequisites:** E3 Done (`cb6ea6b`); `tools\baseline.py`, `tools\e2_entries.py`, `tools\e3_entries.py`.
    - **What Claude Code will do:**
      1. Find the ghost bytes by watching which memory bytes move in an idle game, then check on the 9,800-frame bot route that they stay on corridors.
      2. `tools\e4_entries.py`: BL-038 to BL-043 (the same start with Pac-Man idle, Reset one frame and 60 frames later, Pac-Man walking, the walk one frame later, a longer walk) and BL-044 (the first 4,000 frames of the bot route, with the first power pills), traced twice each, observations computed from the traces.
      3. Cross-check the new inputs against the independent Phase C reference core.
    - **What I must do:** nothing (D-008).
    - **Exact instructions:** none.
    - **Required input:** none.
    - **Claude Code prompt:** the resume prompt.
    - **Expected result:** 7 new entries (BL-038 to BL-044); 44 in all.
    - **Verification:** every new entry's two traces identical; a fresh `baseline.py check` of all 44 equals the stored SHA-256 values; five new inputs (7,200 frames) give 0 fingerprint differences and 0 different pictures against Stella's own script runner.
    - **Completion criteria:** all of the above.
    - **Potential problems:** the ghost bytes are working labels found by observation; the blue (scared) state of each ghost and the way eaten ghosts return were not identified from bytes (the entries say only what was measured); the flicker order of the drawing is not read from the picture here.
    - **Recovery:** new files only; undo by returning to `cb6ea6b` (with your OK).
    - **Documentation updates:** `docs\BASELINE.md`, `docs\GLOSSARY.md`, `DECISIONS.md` (T-E4-1), `CLAUDE.md`, `PROJECT_STATUS.md`, `PLAN.md`.
    - **Git checkpoint:** `Stage E4: Ghosts`.
    - **Next stage:** E5 — Eating and scoring.
5. **[Done] E5 — Eating and scoring.** *(expanded and done 2026-10-26; partly: see Potential problems)*
    - **Objective:** baseline entries for what eating does: wafers, power pills, ghosts eaten in a row, the end of a level and its extra life, with every score read from memory and compared with the digits drawn in the status bar.
    - **Why it matters:** the score and the wafer map are the game's bookkeeping; the rebuilt game must change them on exactly the same frames.
    - **Prerequisites:** E4 Done (`9455ad3`); `tools\baseline.py`, `tools\e2_entries.py`, `tools\e3_entries.py`.
    - **What Claude Code will do:**
      1. Find the score bytes by matching the score shown in the status bar at known frames with memory (`$CC` tens and ones, `$CE` hundreds).
      2. `tools\e5_entries.py`: BL-045 (wafers, scripted), BL-046 (every score value over the whole bot route, plus the status-bar check), BL-047 (the end of level 1), traced twice each, observations computed from the traces.
      3. The status-bar check: save the picture on 155 frames with a steady score, read the digits at the right of the status bar and check they are the digits of the memory score, each digit shape standing for one digit only.
    - **What I must do:** nothing (D-008).
    - **Exact instructions:** none.
    - **Required input:** none.
    - **Claude Code prompt:** the resume prompt.
    - **Expected result:** 3 new entries (BL-045 to BL-047); 47 in all.
    - **Verification:** every new entry's two traces identical; a fresh `baseline.py check` of all 47 equals the stored SHA-256 values; BL-045's input equals Stella's own script runner (0 differences); BL-046/047 use the route already proved equal in Stage D14; status bar and memory agree on all 155 frames.
    - **Completion criteria:** all of the above.
    - **Potential problems:** NOT reached in this stage: the bonus item eaten (the bot never eats it; the item is only seen sitting in the box) and an extra life by score (the best score is 597). Both go to E7 (the bot is to be taught to take the bonus item) and E8 (a memory-edit test for a score near the limit); the Gate E list must contain them.
    - **Recovery:** new files only; undo by returning to `9455ad3` (with your OK).
    - **Documentation updates:** `docs\BASELINE.md`, `docs\GLOSSARY.md`, `DECISIONS.md` (T-E5-1), `CLAUDE.md`, `PROJECT_STATUS.md`, `PLAN.md`.
    - **Git checkpoint:** `Stage E5: Eating and scoring`.
    - **Next stage:** E6 — Dying and game over.
6. **[Done] E6 — Dying and game over.** *(expanded and done 2026-10-26)*
    - **Objective:** baseline entries for a death and what follows: when the pause flag comes on, when the life is taken, how long the pause lasts, the sound and picture during it, the respawn, the loss of the last life, the game over, the ghosts after it, and starting again with the fire button.
    - **Why it matters:** the rebuilt game must match every one of these frames, including the 125-frame delay before the life counter drops.
    - **Prerequisites:** E5 Done (`2aa5595`); `tools\baseline.py`, `tools\e2_entries.py` to `tools\e5_entries.py`.
    - **What Claude Code will do:**
      1. Run an idle game to find the timeline (three deaths, game over, what the fire button does and when).
      2. `tools\e6_entries.py`: BL-048 (an idle game to game over, a fire press during the game over and one later that starts a new game) and BL-049 (the four deaths of the bot route and its game over), traced twice each, observations computed from the traces.
      3. Cross-check BL-048 against the independent Phase C reference core.
    - **What I must do:** nothing (D-008).
    - **Exact instructions:** none.
    - **Required input:** none.
    - **Claude Code prompt:** the resume prompt.
    - **Expected result:** 2 new entries (BL-048, BL-049); 49 in all.
    - **Verification:** every new entry's two traces identical; a fresh `baseline.py check` of all 49 equals the stored SHA-256 values; BL-048 equals Stella's script runner (3,000 frames, 0 fingerprint differences, 10 pictures, 0 different); BL-049 uses the route already proved in Stage D14.
    - **Completion criteria:** all of the above.
    - **Potential problems:** the distance between a ghost and Pac-Man at the catch ranges from 4 to 17 pixels in these coordinates, so the catch is not "same place": the game most likely uses the chip's own collision flags, which are not read here (Phase F). Two-player turn change at a death is left to E8.
    - **Recovery:** new files only; undo by returning to `2aa5595` (with your OK).
    - **Documentation updates:** `docs\BASELINE.md`, `docs\CONTROLS.md`, `DECISIONS.md` (T-E6-1), `CLAUDE.md`, `PROJECT_STATUS.md`, `PLAN.md`.
    - **Git checkpoint:** `Stage E6: Dying and game over`.
    - **Next stage:** E7 — Long play.
7. **[Done] E7 — Long play.** *(expanded and done 2026-10-26)*
    - **Objective:** long games without a human: the bot taught to play on into later levels and to eat the bonus item; three long bot games with different seeds; four seeded random-input games of 30,000 frames; baseline entries that record levels, speed changes between levels and the bonus item.
    - **Why it matters:** the rebuilt game must be right in level 6 as well as level 1, and the movement rules must hold over tens of thousands of frames nobody planned.
    - **Prerequisites:** E6 Done (`876692b`); `tools\stella_probe\bot.cpp` (Stage C7), `tools\baseline.py`, `tools\e2_entries.py` to `tools\e6_entries.py`.
    - **What Claude Code will do:**
      1. Add an optional fifth argument `long` to `stella_bot` (bonus-item bit `$E7` bit 1 found by comparing frames; the item is rewarded; steering goes on after a level ends). Without it the bot behaves as in Stage C7: the default run gave the same 279 moves as `tools\captures\c7_route.txt`.
      2. Rebuild with `tools\build_stella_probe.cmd`; run seeds 1, 2, 3 for 36,000 frames each.
      3. `tools\e7_entries.py`: BL-050 to BL-053 (seeded random input, 30,000 frames each, seeds 11-14), BL-054 to BL-056 (the three bot games), BL-057 (the first 4,000 frames of the first bot game, with the first bonus item); observations computed from the traces.
      4. Cross-check the new inputs against the independent Phase C reference core.
    - **What I must do:** nothing (D-008).
    - **Exact instructions:** none.
    - **Required input:** none.
    - **Claude Code prompt:** the resume prompt.
    - **Expected result:** 8 new entries (BL-050 to BL-057); 57 in all.
    - **Verification:** every new entry's two traces identical; a fresh `baseline.py check` of all 57 equals the stored SHA-256 values; the default bot run reproduces the C7 route; five new inputs equal Stella's own script runner on every frame.
    - **Completion criteria:** all of the above.
    - **Potential problems:** the bot did not reach a game over in 36,000 frames (it cleared five levels in each game and was still alive), so the long games stop at the frame limit; game over is covered by E6. The bot's own play is not "the original's behaviour"; only the replay by the host counts as evidence. Each long bot run took about 45 minutes.
    - **Recovery:** new files and one optional bot mode; undo by returning to `876692b` (with your OK).
    - **Documentation updates:** `docs\BASELINE.md`, `DECISIONS.md` (T-E7-1), `CLAUDE.md`, `PROJECT_STATUS.md`, `PLAN.md`.
    - **Git checkpoint:** `Stage E7: Long play`.
    - **Next stage:** E8 — Edge cases and Gate E.
8. **[Done] E8 — Edge cases and Gate E.** *(expanded and done 2026-10-26)*
    - **Objective:** baseline entries for the edge cases (Reset and Select in the middle of a game, the three console switches during play, a very long idle, a score at its limit, two players, random controls), and Gate E: is the baseline complete enough to be the target of Phase F?
    - **Why it matters:** Rule 10: Phase F (reading the code and rebuilding it) must not start on a baseline that has holes nobody knows about.
    - **Prerequisites:** E7 Done (`3dd3899`).
    - **What Claude Code will do:**
      1. A test-only memory edit for the host (`--poke <frame>:<hex address>:<hex value>`, headless only; `pokes:` line in an entry; `Emulator::pokeRam`), because a score near its limit cannot be played to. Entries that use it are labelled ARTIFICIAL.
      2. `tools\e8_entries.py`: BL-058 to BL-070 (Reset in play, Select in play, a control walk and three switch entries, a 40,000-frame idle, two ARTIFICIAL score entries, two players, two seeded random games with random console buttons, and the eyes of eaten ghosts).
      3. `docs\GATE_E.md`: the checklist with a verdict and the evidence; fresh runs of every earlier check.
    - **What I must do:** nothing (D-008).
    - **Exact instructions:** none.
    - **Required input:** none.
    - **Claude Code prompt:** the resume prompt.
    - **Expected result:** 13 new entries (BL-058 to BL-070); 70 in all; `docs\GATE_E.md`.
    - **Verification:** every new entry's two traces identical; a fresh `baseline.py check` of all 70 equals the stored SHA-256 values; seven new inputs (55,500 frames) equal Stella's own script runner; the unit tests, the key replay (350,460 frames) and the Git file check all pass again after the host change.
    - **Completion criteria:** Gate E items in `docs\GATE_E.md` all PASS or moved to Phase F with a reason.
    - **Potential problems:** the two ARTIFICIAL entries cannot be compared with Stella's script runner (it has no memory edit) and are not evidence about real play; the scared state of the ghosts, the order of the ghost flicker and the exact meaning of most bytes are code-reading questions and are moved to Phase F (T-E8-2).
    - **Recovery:** new files plus a small test-only addition to the host; undo by returning to `3dd3899` (with your OK).
    - **Documentation updates:** `docs\GATE_E.md`, `docs\BASELINE.md`, `DECISIONS.md` (T-E8-1, T-E8-2), `CLAUDE.md`, `docs\GLOSSARY.md`, `PROJECT_STATUS.md`, `PLAN.md`.
    - **Git checkpoint:** `Stage E8: Edge cases and Gate E`.
    - **Next stage:** F1 — Analysis tools (Phase F).

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

1. **[Done] F1 — Analysis tools.** *(expanded and done 2026-10-26; the tools differ from the first idea, see T-F1-1)*
    - **Objective:** the tools that Phase F needs to read the 4 KB program: a disassembler and an assembler for the 6502 processor, a way to tell which bytes are code and which are data, and tests that prove the tools right.
    - **Why it matters:** F2 to F26 rest on these: a listing that rebuilds the ROM byte for byte is the proof that nothing was lost, and the list of what the CPU really runs is the proof of what is code.
    - **Prerequisites:** Gate E passed (`327a5ba`); `tools\stella_probe` (C-phase measuring tools), the baseline entries.
    - **What Claude Code will do:**
      1. `tools\f1_6502.py`: the opcode table read from Stella's own disassembler source (vendored, never edited), a disassembler and an assembler of our own (no download: T-F1-1), and 8 self-tests including "disassemble the whole ROM, assemble it, byte-identical".
      2. `tools\stella_probe\coverage.cpp` (`stella_coverage.exe`): runs a recording one CPU instruction at a time and records every ROM address executed and every ROM address read as data; checked frame by frame against the golden trace (T-F1-2).
      3. `tools\f1_coverage.py`: runs it on 29 baseline recordings (365,780 frames), unions them and writes the map `reference\f1\coverage_union.txt`.
      4. `docs\F1_TOOLS.md`: what each tool is, how to run it, the results.
    - **What I must do:** nothing. (Optional: if you want a second, independent assembler, `dasm` can still be downloaded later; I did not need it.)
    - **Exact instructions:** none.
    - **Required input:** none.
    - **Claude Code prompt:** the resume prompt.
    - **Expected result:** the three tools, the code/data map, and the document.
    - **Verification:** `python -I tools\f1_6502.py selftest rom\pacman.a26` (8 checks, 0 failed); every coverage run equals its golden trace on all frames (0 differences in 365,780 frames); the map has no executed illegal opcode, no overlapping instructions, no instruction outside the ROM.
    - **Completion criteria:** all of the above.
    - **Potential problems:** the map is only as complete as the games played: 44 ROM bytes were never executed or read in 29 recordings (listed in the map), and code that runs only in situations not yet recorded could be hiding among the "D" bytes (data) or the "." bytes; the vectors at the end are read by the CPU's reset, not by an instruction, so they show as "."; four 3-byte pieces of the code area ($FACF, $FAF9, $FB2A, $FB33) were never executed and are the first candidates for code that needs a situation not yet recorded.
    - **Recovery:** new files only (tools, a document); undo by returning to `327a5ba` (with your OK).
    - **Documentation updates:** `docs\F1_TOOLS.md`, `DECISIONS.md` (T-F1-1, T-F1-2), `CLAUDE.md`, `docs\GLOSSARY.md`, `PROJECT_STATUS.md`, `PLAN.md`.
    - **Git checkpoint:** `Stage F1: Analysis tools`.
    - **Next stage:** F2 — Raw disassembly that rebuilds exactly.
2. **[Done] F2 — Raw disassembly that rebuilds exactly.** *(expanded and done 2026-10-26)*
    - **Objective:** a readable listing of the whole 4 KB program, split into instructions and data, with a label on every address that is jumped to or read, that assembles back to exactly the original bytes.
    - **Why it matters:** a byte-identical rebuild is the proof that the listing is complete and that nothing is hiding: every later stage (names, routines, the C++ translation) edits this listing and must keep it rebuilding.
    - **Prerequisites:** F1 Done (`10b53f4`): `tools\f1_6502.py`, the code/data map.
    - **What Claude Code will do:**
      1. `tools\f2_disasm.py`: reads the map, splits the ROM, labels every branch, jump, call and absolute reference (`L_Fxxx` for instruction starts, `D_Fxxx` for data), writes `reference\disasm\pacman_raw.asm` (local only: it is the original program in readable form, Rule 7), assembles it and compares.
      2. Run the rebuilt ROM bytes through every baseline entry (all 70 golden traces) as the independent proof that the rebuilt bytes behave exactly like the original.
      3. `docs\F2_RAW_DISASSEMBLY.md`: what the listing is made of (numbers only; no program text).
    - **What I must do:** nothing. (You said a `dasm` second opinion would help if I were uncertain: I am not, for the reasons in T-F2-1, so nothing was downloaded; say so if you want it anyway.)
    - **Exact instructions:** none.
    - **Required input:** none.
    - **Claude Code prompt:** the resume prompt.
    - **Expected result:** the listing, a byte-identical rebuild (same SHA-1 and SHA-256), the statistics.
    - **Verification:** `python -I tools\f2_disasm.py rom\pacman.a26` says BYTE-IDENTICAL; `baseline.py check reference\disasm\pacman_rebuilt.bin` of all 70 entries equals every stored SHA-256.
    - **Completion criteria:** all of the above.
    - **Potential problems:** the split into code and data comes from the games played (F1): eight never-run instructions were recognised by their shape (two calls and two `PLA PLA RTS` pieces); a data byte that is really code is impossible to see from a rebuild alone (a rebuild is byte-exact either way), so F4 onward reads each routine.
    - **Recovery:** new tool and local files; undo by returning to `10b53f4` (with your OK).
    - **Documentation updates:** `docs\F2_RAW_DISASSEMBLY.md`, `DECISIONS.md` (T-F2-1, T-F2-2), `CLAUDE.md`, `docs\GLOSSARY.md`, `PROJECT_STATUS.md`, `PLAN.md`.
    - **Git checkpoint:** `Stage F2: Raw disassembly`.
    - **Next stage:** F3 — Hardware names.
3. **[Done] F3 — Hardware names.** *(expanded and done 2026-10-26)*
    - **Objective:** in the listing, replace the numbers of the video chip (TIA) and the timer/input chip (RIOT) with their standard names (`WSYNC`, `GRP0`, `INTIM`...), without changing a single byte of the rebuilt ROM.
    - **Why it matters:** every line that touches the screen, the sound or the controls becomes readable at once, and a wrong name would show up as a wrong guess later, so the names are checked against an independent source.
    - **Prerequisites:** F2 Done (`8f0b4d1`): `reference\disasm\pacman_raw.asm`.
    - **What Claude Code will do:**
      1. Give the assembler of F1 equates (`NAME = value`), with a test.
      2. `tools\f3_names.py`: names every chip operand (a store writes, everything else reads; chip copies at other addresses get an `_AT<address>` name), writes `reference\disasm\pacman_named.asm` (local only), assembles it and requires byte-identical.
      3. Check the name tables against the tables in Stella's debugger source (vendored, read only) on every run.
      4. `docs\F3_HARDWARE_NAMES.md`.
    - **What I must do:** nothing.
    - **Exact instructions:** none.
    - **Required input:** none.
    - **Claude Code prompt:** the resume prompt.
    - **Expected result:** the named listing, a byte-identical rebuild, the statistics.
    - **Verification:** `python -I tools\f3_names.py rom\pacman.a26` says the tables equal Stella's and BYTE-IDENTICAL; `python -I tools\f1_6502.py selftest rom\pacman.a26` (9 checks); the rebuilt bytes replay golden traces.
    - **Completion criteria:** all of the above.
    - **Potential problems:** a read-modify-write instruction on a chip address would be ambiguous (none exists); `STA $00,X` is a sweep over the whole page, not a register, and stays a number; the names say what the hardware does, not what the game does with it (later stages).
    - **Recovery:** new tool and local files; undo by returning to `8f0b4d1` (with your OK).
    - **Documentation updates:** `docs\F3_HARDWARE_NAMES.md`, `DECISIONS.md` (T-F3-1), `CLAUDE.md`, `PROJECT_STATUS.md`, `PLAN.md`.
    - **Git checkpoint:** `Stage F3: Hardware names`.
    - **Next stage:** F4 — Code versus data.
4. **[Done] F4 — Code versus data.** *(expanded and done 2026-10-26)*
    - **Objective:** in the listing, mark every data byte as data, cut it into tables, and name each table by how the program uses it (sprite shapes, colours, playfield, sound, other tables, never read), with its size and its readers, without changing a byte of the rebuilt ROM.
    - **Why it matters:** the graphics, maze and sound tables are the "content" of the game; the C++ version needs them cut at the right places, and the cut must come from evidence, not guesses.
    - **Prerequisites:** F3 Done (`83b6b0b`): the named listing; the instruction-level coverage files of F1.
    - **What Claude Code will do:**
      1. `tools\f4_tables.py`: from the 29 coverage recordings, which instruction read which data byte; follow each read forward to the chip register the value ends in (GRP0/GRP1 = shape, COLUPx = colour, PFx = playfield, AUDxx = sound); merge the ranges each reader touched; cut at old labels and range edges; name each segment `<KIND>_Fxxx`; the three CPU vectors written as `.word`.
      2. Write `reference\disasm\pacman_tables.asm` (local only), assemble, require byte-identical.
      3. `docs\F4_DATA_TABLES.md`.
    - **What I must do:** nothing.
    - **Exact instructions:** none.
    - **Required input:** none.
    - **Claude Code prompt:** the resume prompt.
    - **Expected result:** 544 data bytes in 66 named segments; a byte-identical rebuild.
    - **Verification:** `python -I tools\f4_tables.py rom\pacman.a26` says BYTE-IDENTICAL; the rebuilt bytes replay golden traces.
    - **Completion criteria:** all of the above.
    - **Potential problems:** the kind is how the bytes are used, not what they mean in the game: 37 segments (266 bytes) are read but not stored straight into a chip register (`TBL`) and need the routines (F5 onward) to be understood; sprite tables are cut by the reads of the games played, so the cut between two shapes inside a `GFX` block is not yet known (it comes with the kernel stage); 26 bytes were never read.
    - **Recovery:** new tool and local files; undo by returning to `83b6b0b` (with your OK).
    - **Documentation updates:** `docs\F4_DATA_TABLES.md`, `DECISIONS.md` (T-F4-1), `CLAUDE.md`, `PROJECT_STATUS.md`, `PLAN.md`.
    - **Git checkpoint:** `Stage F4: Code versus data`.
    - **Next stage:** F5 — The frame loop.
5. **[Done] F5 — The frame loop.** *(expanded and done 2026-10-26)*
    - **Objective:** find, in the code and by running it, the start-up and the six parts of every frame (vertical sync, top logic, wait, the picture "kernel", bottom logic, wait), mark them in the listing, and prove that every frame has the same shape.
    - **Why it matters:** the rebuilt game must do the same work in the same order on every frame, and the kernel must be reproduced line by line; this is the skeleton everything else hangs on.
    - **Prerequisites:** F4 Done (`1627c57`); the timing facts of Stage C4.
    - **What Claude Code will do:**
      1. A new measuring tool `stella_pctrace.exe` (`tools\stella_probe\pctrace.cpp`): one line per executed instruction with its scanline and colour clock, for chosen frames of a recording, checked against the golden trace.
      2. `tools\f5_frame.py`: traces 8 sets of frames (attract, play, a death, game over, a new game, two players, Reset in play), checks that the six landmarks occur in the same order in every frame, measures the scanlines of each part, writes `reference\disasm\pacman_frame.asm` (local only) with the landmarks as labels and a banner for each part, and requires a byte-identical rebuild.
      3. `docs\F5_FRAME_LOOP.md`.
    - **What I must do:** nothing.
    - **Exact instructions:** none.
    - **Required input:** none.
    - **Claude Code prompt:** the resume prompt.
    - **Expected result:** the frame map, the labelled listing, a byte-identical rebuild.
    - **Verification:** `python -I tools\f5_frame.py rom\pacman.a26` says "problems: none" for 31 traced frames and BYTE-IDENTICAL; every traced run equals its golden trace on every frame.
    - **Completion criteria:** all of the above.
    - **Potential problems:** the banners mark where each part begins; subroutines shared between parts (the kernel's helpers at `$F053-$F06A`, the sound and input routines) sit between them and are sorted out in F8 (routine list); the exact line on which the top and bottom logic end varies with what the game does that frame (measured ranges are in the document).
    - **Recovery:** new tool and local files; undo by returning to `1627c57` (with your OK).
    - **Documentation updates:** `docs\F5_FRAME_LOOP.md`, `DECISIONS.md` (T-F5-1), `CLAUDE.md`, `docs\GLOSSARY.md`, `PROJECT_STATUS.md`, `PLAN.md`.
    - **Git checkpoint:** `Stage F5: The frame loop`.
    - **Next stage:** F6 — What actually runs.
6. **[Done] F6 — What actually runs.** *(expanded and done 2026-10-26)*
    - **Objective:** replay every one of the 70 golden recordings with execution logging, and mark every one of the 4096 ROM bytes as executed, read as data, or never touched.
    - **Why it matters:** the rebuild must contain every instruction that can run and must know which bytes are tables; the bytes no recording ever touches are where the rebuilt game could differ unseen.
    - **Prerequisites:** F5 Done (`ef2f2a2`); the coverage tool and map of F1 (29 recordings); the 70 baseline entries of Phase E.
    - **What Claude Code will do:**
      1. Give `stella_coverage` the memory edits of BL-065/066 (`<frame>:<hex addr>:<hex value>` arguments) so all 70 entries can be replayed; rebuild the measuring tool.
      2. `tools\f6_coverage.py`: replay all 70 entries, each checked frame by frame against its golden trace; union the results; classify every byte (C/c executed, D data read, n only touched by the processor's look-ahead, . untouched); compare with F1; list the untouched pieces and the rarely run instructions; write `reference\f6\` (local only).
      3. `docs\F6_COVERAGE.md`.
    - **What I must do:** nothing.
    - **Exact instructions:** none.
    - **Required input:** none.
    - **Claude Code prompt:** the resume prompt.
    - **Expected result:** a map of all 4096 bytes from 439,680 replayed frames, every run equal to its golden trace.
    - **Verification:** `python -I tools\f6_coverage.py rom\pacman.a26` shows 70 lines "0 differences", "problems: none" and the counts.
    - **Completion criteria:** all of the above; every byte has exactly one class.
    - **Potential problems:** "never run" means never run by these 70 recordings, not "cannot run"; the 44 untouched bytes and the 4 unexecuted code pieces are listed so that F7-F19 can look at them.
    - **Recovery:** one test-only option added to a measuring tool, a new script and local files; undo by returning to `ef2f2a2` (with your OK).
    - **Documentation updates:** `docs\F6_COVERAGE.md`, `DECISIONS.md` (T-F6-1), `CLAUDE.md`, `docs\GLOSSARY.md`, `PROJECT_STATUS.md`, `PLAN.md`.
    - **Git checkpoint:** `Stage F6: What actually runs`.
    - **Next stage:** F7 — Naming all 128 RAM bytes.
7. **[Done] F7 — Naming all 128 RAM bytes.** *(expanded and done 2026-10-26)*
    - **Objective:** give every one of the 128 bytes of the console's memory (`$80-$FF`) a name and a stated meaning, each marked sure, likely or "?", and put the names into the listing without changing a byte of the rebuilt ROM.
    - **Why it matters:** every later stage (input, movement, maze, ghosts, scoring, states) reads and rewrites these bytes; the C++ rebuild's state structure and the lockstep test of F20 are built from this map.
    - **Prerequisites:** F6 Done (`067528b`): the code map of what runs; F5 listing; the 70 baseline traces.
    - **What Claude Code will do:**
      1. Read the whole listing (about 1,800 instructions) and follow each RAM byte through every instruction that touches it.
      2. `tools\f7_ram.py`: holds the table (address, name, confidence, meaning), counts static users and measures every byte over the 70 traces (439,680 frames), runs checks that test the claims of the names (frame counter, player-block swap, decimal score digits, position ranges, stack, unused bytes), writes `reference\disasm\pacman_ram.asm` (local only) with every zero-page RAM operand replaced by its name, assembles it and requires byte-identical; writes `docs\F7_RAM_MAP.md`.
    - **What I must do:** nothing.
    - **Exact instructions:** none.
    - **Required input:** none.
    - **Claude Code prompt:** the resume prompt.
    - **Expected result:** 128 named bytes (84 sure, 32 likely, 12 "?"), all checks PASS, byte-identical rebuild.
    - **Verification:** `python -I tools\f7_ram.py rom\pacman.a26` says "problems: none" and BYTE-IDENTICAL.
    - **Completion criteria:** all of the above; each of `$80-$FF` named exactly once.
    - **Potential problems:** the names say what a byte holds, not yet how the game uses it; scratch bytes (`$D2-$E3`) are shared by the picture and the logic and stay "?"; names marked likely may be refined by F9-F17 (then the table here is updated).
    - **Recovery:** new script and local files; undo by returning to `067528b` (with your OK).
    - **Documentation updates:** `docs\F7_RAM_MAP.md`, `DECISIONS.md` (T-F7-1), `CLAUDE.md`, `docs\GLOSSARY.md`, `PROJECT_STATUS.md`, `PLAN.md`.
    - **Git checkpoint:** `Stage F7: Naming all 128 RAM bytes`.
    - **Next stage:** F8 — Routine list and call map.
8. **[Done] F8 — Routine list and call map.** *(expanded and done 2026-10-26)*
    - **Objective:** list every subroutine of the program, with its name, size, callers, callees, the memory and chip registers it touches and how often it ran, and show the whole call structure.
    - **Why it matters:** the C++ version is built routine by routine; the call map says what depends on what, which routines return early (drop the caller's return address), and how deep the stack gets.
    - **Prerequisites:** F7 Done (`5d7896d`): RAM names; F6 code map and run counts; the F7 listing.
    - **What Claude Code will do:**
      1. `tools\f8_routines.py`: finds entries from the start-up and every JSR target, follows the code of each (branches, jumps, falling through), turns code reached by jump from two routines into shared tails, folds the pieces of the frame loop into `MAIN`, and records callers, callees, RAM and chip use, double returns (PLA PLA) and stack need.
      2. Names each routine from reading the code (confidence sure / likely / "?").
      3. Checks, then writes `reference\disasm\pacman_routines.asm` (local only; banner and name per routine) and requires byte-identical; writes `docs\F8_ROUTINES.md`.
    - **What I must do:** nothing.
    - **Exact instructions:** none.
    - **Required input:** none.
    - **Claude Code prompt:** the resume prompt.
    - **Expected result:** 40 routines (the main loop and 39 others), call tree, all checks PASS, byte-identical rebuild.
    - **Verification:** `python -I tools\f8_routines.py rom\pacman.a26` says "problems: none" and BYTE-IDENTICAL.
    - **Completion criteria:** every instruction that ran belongs to exactly one routine; JSR counts equal the run counts; no recursion; stack need fits the 10 bytes of F7.
    - **Potential problems:** routines that end by dropping the caller's return and continuing in the main loop (`PACMAN_KEEP_GOING`) break the usual call/return picture; routine names marked "?" are guesses from the shape of the code and are settled in F10-F13.
    - **Recovery:** new script and local files; undo by returning to `5d7896d` (with your OK).
    - **Documentation updates:** `docs\F8_ROUTINES.md`, `DECISIONS.md` (T-F8-1), `CLAUDE.md`, `docs\GLOSSARY.md`, `PROJECT_STATUS.md`, `PLAN.md`.
    - **Git checkpoint:** `Stage F8: Routine list and call map`.
    - **Next stage:** F9 — Input reading.
9. **[Done] F9 — Input reading.** *(expanded and done 2026-10-26)*
    - **Objective:** find every place where the program reads the joysticks, buttons and switches, say exactly what each read means and when it happens, and prove it with the recorded games and two extra experiments.
    - **Why it matters:** the rebuilt game takes its inputs from a small structure; which joystick belongs to which player, which switch is read when, and the Select repeat must match the original exactly.
    - **Prerequisites:** F8 Done (`8c31973`): routines, RAM names; the 70 baseline traces; the Stella-core tools.
    - **What Claude Code will do:**
      1. `tools\f9_input.py`: scan the code for every instruction that reads SWCHA, SWCHB, INPT0-5; check the effect on memory in all 70 traces (joystick, fire, Reset, Select, difficulty, TV type); run two experiments no baseline game contains (player 2 with the right joystick; Select held for 160 frames; the script runner got the right-joystick commands); measure the scanline of each read with `stella_pctrace`; write `docs\F9_INPUT.md`.
      2. The experiments showed three pieces of code the listing had as data (the not-taken side of branches no baseline game takes). Add three extra input files (`tools\f9_inputs\X-*.inputs`) that run them, include them in `f1_coverage.py` and `f6_coverage.py`, and re-run the whole chain F1-F8 (all byte-identical).
    - **What I must do:** nothing.
    - **Exact instructions:** none.
    - **Required input:** none.
    - **Claude Code prompt:** the resume prompt.
    - **Expected result:** the input map (five reads), all checks PASS, the corrected code map (C 1,810, c 1,735, D 509, untouched 42).
    - **Verification:** `python -I tools\f9_input.py rom\pacman.a26` says "problems: none" (10 PASS lines); `f1_coverage`, `f6_coverage` ("0 differences" 70 times) and `f2`-`f8` still say problems: none / BYTE-IDENTICAL.
    - **Completion criteria:** all of the above.
    - **Potential problems:** the recorded games contain only the left joystick, so player 2 never moved in them; any future recording of player 2 needs the right-joystick bits (the host has no keys for it yet).
    - **Recovery:** new files, two tool edits and one measuring-tool edit; undo by returning to `8c31973` (with your OK).
    - **Documentation updates:** `docs\F9_INPUT.md`, corrections in `docs\F4_DATA_TABLES.md` and `docs\F6_COVERAGE.md`, `DECISIONS.md` (T-F9-1), `CLAUDE.md`, `PROJECT_STATUS.md`, `PLAN.md`.
    - **Git checkpoint:** `Stage F9: Input reading`.
    - **Next stage:** F10 — Pac-Man's movement and turning.
10. **[Done] F10 — Pac-Man's movement and turning.** *(expanded and done 2026-10-26)*
    - **Objective:** state exactly how Pac-Man moves and turns (the grid, the wall table, the stick rules, the speed, the two openings) and prove the statement with a model that reproduces the recorded games frame by frame.
    - **Why it matters:** this is the core of the game feel; the C++ version of Pac-Man's movement is a direct translation of the model, and the lockstep test of F20 starts from it.
    - **Prerequisites:** F9 Done (`4eca153`): input; F7 RAM names; F8 routines; the 70 baseline traces.
    - **What Claude Code will do:**
      1. Read the routines `$F61E-$F7BA`, `MOVE_OBJECT`, `TRY_*`, `LOCATE_OBJECT`, `ANIMATE_OBJECT`, `WRAP_*` and write the rules as a model in `tools\f10_pacman.py`, using only the ROM's wall table, the joystick and Pac-Man's bytes of the frame before.
      2. Predict, for every frame in which Pac-Man's code runs normally, x, y, state bits, speed accumulator, JOY_DIRECTION and the idle counter, and compare with the recorded memory of all 70 games; count the frames not predicted by reason; count which roads of the code were taken.
      3. Write `docs\F10_PACMAN.md` (generated).
    - **What I must do:** nothing.
    - **Exact instructions:** none.
    - **Required input:** none.
    - **Claude Code prompt:** the resume prompt.
    - **Expected result:** 0 wrong frames out of the predicted ones; every road of the rules taken in the games.
    - **Verification:** `python -I tools\f10_pacman.py rom\pacman.a26` says `result: OK` (202,473 of 202,473 predicted frames right).
    - **Completion criteria:** all of the above.
    - **Potential problems:** wafer eating, scoring, ghost collisions, the power pill and the bonus item are not predicted (F11, F12, F15); player 2 never had a joystick in the recordings; the x-border road was never taken.
    - **Recovery:** one new script and generated documents; undo by returning to `4eca153` (with your OK).
    - **Documentation updates:** `docs\F10_PACMAN.md`, `DECISIONS.md` (T-F10-1), `CLAUDE.md`, `docs\GLOSSARY.md`, `PROJECT_STATUS.md`, `PLAN.md`.
    - **Git checkpoint:** `Stage F10: Pac-Man's movement and turning`.
    - **Next stage:** F11 — Maze and wafer storage.
11. **[Done] F11 — Maze and wafer storage.** *(expanded and done 2026-10-26)*
    - **Objective:** find how the maze and its 126 wafers and 4 power pills are stored in the ROM and in memory, and prove the rules for eating them and for starting a new map.
    - **Why it matters:** the maze is the second large block of game data (the walls and wafer places in the ROM, the 16 wafer bytes in memory); the rebuilt game must store and change them the same way for the lockstep test to hold.
    - **Prerequisites:** F10 Done (`7f65405`): the movement model; F7 RAM names; the 70 baseline traces.
    - **What Claude Code will do:**
      1. `tools\f11_maze.py`: read the 80-byte wall/wafer table (`$FE13`), work out which memory bit belongs to which wafer, check that the 128 places map one-to-one to cells, and predict - using the F10 movement model for Pac-Man's place - the 16 wafer bytes, the pill bits, the score of the player in turn and the new map after the last wafer, for every frame on the normal road of all 70 games.
      2. Write `docs\F11_MAZE.md` (generated): storage layout, eating rules, an ASCII map of the maze from the table, the wafer-place table.
    - **What I must do:** nothing.
    - **Exact instructions:** none.
    - **Required input:** none.
    - **Claude Code prompt:** the resume prompt.
    - **Expected result:** 0 wrong frames; 128 places one-to-one, 126 set at the start.
    - **Verification:** `python -I tools\f11_maze.py rom\pacman.a26` says `result: OK` (202,502 of 202,502 predicted frames right; 4,123 wafers, 102 power pills and 20 new maps among them).
    - **Completion criteria:** all of the above.
    - **Potential problems:** the scared-time bits of the pill byte, ghosts eaten, the bonus item and the frames around deaths belong to F12/F15/F16 and are skipped or not compared (counts in the document); how the kernel draws the dots is F18.
    - **Recovery:** one new script and a generated document; undo by returning to `7f65405` (with your OK).
    - **Documentation updates:** `docs\F11_MAZE.md`, `DECISIONS.md` (T-F11-1), `CLAUDE.md`, `PROJECT_STATUS.md`, `PLAN.md`.
    - **Git checkpoint:** `Stage F11: Maze and wafer storage`.
    - **Next stage:** F12 — Collisions.
12. **[Done] F12 — Collisions.** *(expanded and done 2026-10-26)*
    - **Objective:** find out how the game detects that Pac-Man touches a ghost or the bonus item (the chip's collision flags or its own position checks), and prove the game's reaction to those flags.
    - **Why it matters:** this decides when Pac-Man dies, when a ghost is eaten and how many points it gives; the rebuilt game must react in the same frames, and it must produce the same flags from its own picture.
    - **Prerequisites:** F11 Done (`52a7832`): maze; F10 movement; F7 RAM names; F8 routines; the 70 baseline traces.
    - **What Claude Code will do:**
      1. Add an optional `watch=<file>:<pc>,<pc>` argument to the measuring tool `stella_coverage` (`tools\stella_probe\coverage.cpp`): it logs the accumulator after chosen instructions, i.e. the flag value the game really read. Rebuild it; all 70 replays must still equal their golden traces.
      2. `tools\f12_collisions.py`: list every collision register in the listing; replay all 70 games with the log; check the reaction rules (bonus item, ghost eaten, death trigger, no reaction when the flag is clear, which ghost is tested and when) on every logged frame; tabulate flag against the positions of ghost and Pac-Man.
      3. Write `docs\F12_COLLISIONS.md` (generated).
    - **What I must do:** nothing.
    - **Exact instructions:** none.
    - **Required input:** none.
    - **Claude Code prompt:** the resume prompt.
    - **Expected result:** 0 wrong frames; the flags are the chip's own.
    - **Verification:** `python -I tools\f12_collisions.py rom\pacman.a26` says `result: OK` (70 replays equal to their golden traces; 665 ghost flags, 193 ghosts eaten, 303 deaths started, 18 bonus items, all reactions right; 0 reactions without a flag).
    - **Completion criteria:** all of the above.
    - **Potential problems:** the flag is true pixel overlap of the two pictures; it is not predicted from positions here (it needs the sprite shapes and the pixel columns: F18 / F23). The death sequence, scared timers and the ghosts' journey home are F13, F16, F17.
    - **Recovery:** one new script, a small option in the measuring tool and a generated document; undo by returning to `52a7832` (with your OK).
    - **Documentation updates:** `docs\F12_COLLISIONS.md`, `DECISIONS.md` (T-F12-1), `CLAUDE.md`, `docs\GLOSSARY.md`, `PROJECT_STATUS.md`, `PLAN.md`.
    - **Git checkpoint:** `Stage F12: Collisions`.
    - **Next stage:** F13 — Ghost movement and decisions.
13. **[Done] F13 — Ghost movement and decisions.** *(expanded and done 2026-10-26)*
    - **Objective:** state exactly how a ghost decides and moves (chase, scared, eyes, pen, openings, random wandering) and prove it with a model that reproduces the recorded games frame by frame.
    - **Why it matters:** the ghosts are the opposition; their routes, the random number they use and their timing must be identical for the lockstep test (F20) and for the feel of the original.
    - **Prerequisites:** F12 Done (`b909eab`); F10 movement; F11 maze; F7 RAM names; F8 routines; the 70 baseline traces.
    - **What Claude Code will do:**
      1. Read `GHOST_UPDATE`, `GHOST_DECIDE`, the `TRY_*` routines, `RNG_STEP`, `MOVE_OBJECT`, `ANIMATE_OBJECT`, `WRAP_*` and the top logic that calls them, and write them as a model in `tools\f13_ghosts.py`.
      2. Predict, for the ghost a frame handles, x, y, state, accumulator, timer and the random number `$CB`, from the memory of the frame before; compare with all 70 recorded games; count frames on other roads by reason; count the roads taken; check the frozen-game timers.
      3. Write `docs\F13_GHOSTS.md` (generated).
    - **What I must do:** nothing.
    - **Exact instructions:** none.
    - **Required input:** none.
    - **Claude Code prompt:** the resume prompt.
    - **Expected result:** 0 wrong frames out of the predicted ones.
    - **Verification:** `python -I tools\f13_ghosts.py rom\pacman.a26` says `result: OK` (348,146 of 348,146 predicted frames right; the random number `$CB` predicted in 86,851 of them; 82,024 frozen frames checked for the timers).
    - **Completion criteria:** all of the above.
    - **Potential problems:** the scared-ghost y-aim looks like an error in the original (a mirrored y is worked out and not stored) and is kept exactly; ghosts handled three times a frame; roads never taken in the games are marked in the code listing.
    - **Recovery:** one new script and a generated document; undo by returning to `b909eab` (with your OK).
    - **Documentation updates:** `docs\F13_GHOSTS.md`, `DECISIONS.md` (T-F13-1), `CLAUDE.md`, `docs\GLOSSARY.md`, `PROJECT_STATUS.md`, `PLAN.md`.
    - **Git checkpoint:** `Stage F13: Ghost movement and decisions`.
    - **Next stage:** F14 — Randomness: where it comes from.
14. **[Done] F14 — Randomness: where it comes from.** *(expanded and done 2026-10-26)*
    - **Objective:** find every source of random numbers in the game and prove that nothing outside the game (clock, timer, buttons) feeds them.
    - **Why it matters:** if the original were random in a way we cannot reproduce, the lockstep test could not work; if it is deterministic, one machine-made test replays everything.
    - **Prerequisites:** F13 Done (`3526edc`); F7 RAM names; F8 routines; the 70 baseline traces.
    - **What Claude Code will do:**
      1. `tools\f14_random.py`: list every instruction that writes or reads `$CB` and every read of the timer and the input chips; analyse the generator (cycle); check in all 70 games that each change of `$CB` is the start value or a number of generator steps.
      2. Write `docs\F14_RANDOM.md` (generated).
    - **What I must do:** nothing.
    - **Exact instructions:** none.
    - **Required input:** none.
    - **Claude Code prompt:** the resume prompt.
    - **Expected result:** one random byte, three writers, no outside source.
    - **Verification:** `python -I tools\f14_random.py rom\pacman.a26` says `result: OK` (89,039 changes by generator steps, 121 starts at $FF, 0 unexplained, over 438,212 frames).
    - **Completion criteria:** all of the above.
    - **Potential problems:** none found; the long-run statistics of the numbers are not needed to copy the game.
    - **Recovery:** one new script and a generated document; undo by returning to `3526edc` (with your OK).
    - **Documentation updates:** `docs\F14_RANDOM.md`, `DECISIONS.md` (T-F14-1), `CLAUDE.md`, `PROJECT_STATUS.md`, `PLAN.md`; the "Randomness" risk line in the plan is settled.
    - **Git checkpoint:** `Stage F14: Randomness`.
    - **Next stage:** F15 — Scoring, bonus item and lives.
15. **[Done] F15 — Scoring, bonus item and lives.** *(expanded and done 2026-10-26)*
    - **Objective:** state exactly how points, the bonus item and lives work, and prove it on every recorded frame.
    - **Why it matters:** score and lives are what a player sees; the rebuilt game must add the same amounts at the same moments, and E5 left open whether a score gives an extra life.
    - **Prerequisites:** F14 Done (`cfb5730`); F11 maze, F12 collisions (the flag logs in `reference\f12`, made by `tools\f12_collisions.py`), F7 RAM names.
    - **What Claude Code will do:**
      1. `tools\f15_scoring.py`: check every frame of the 70 games for (A) the score change = sum of the amounts added, (B) every change of the lives byte, (C) the bonus timer at the 256-frame tick, (D) the end of a death: one life less or game over.
      2. Write `docs\F15_SCORING.md` (generated).
    - **What I must do:** nothing.
    - **Exact instructions:** none.
    - **Required input:** none.
    - **Claude Code prompt:** the resume prompt.
    - **Expected result:** 0 wrong frames; no extra life by score.
    - **Verification:** `python -I tools\f15_scoring.py rom\pacman.a26` says `result: OK` (290,818 score frames right; 1,637 bonus-tick frames right and 434,279 other frames right; 222 deaths with a life less and 52 game overs; lives changed only by new map +1 and death -1).
    - **Completion criteria:** all of the above.
    - **Potential problems:** two-player scoring and the 999,999 wrap were never seen; the difficulty effect on the bonus is from the code only.
    - **Recovery:** one new script and a generated document; undo by returning to `cfb5730` (with your OK).
    - **Documentation updates:** `docs\F15_SCORING.md`, `DECISIONS.md` (T-F15-1), `CLAUDE.md`, `PROJECT_STATUS.md`, `PLAN.md`.
    - **Git checkpoint:** `Stage F15: Scoring, bonus item and lives`.
    - **Next stage:** F16 — Levels, speed-ups and timers.
16. **[Done] F16 — Levels, speed-ups and timers.** *(expanded and done 2026-10-26)*
    - **Objective:** find everything that depends on the level and every timer, and prove the rules.
    - **Why it matters:** speed and timing are the difficulty of the game; the rebuilt game must have the same ones on every level.
    - **Prerequisites:** F15 Done (`fcf4e66`); F10, F13 (movement and ghosts); the flag logs in `reference\f12`.
    - **What Claude Code will do:**
      1. `tools\f16_levels.py`: list every use of the level byte, the game number and the event timer; check the start level and the +1 per map; compute the ghost speed from the accumulator cycle for each level and compare with the measured steps; check the event timer (scared time, freeze, death delay) on every frame.
      2. Write `docs\F16_LEVELS.md` (generated).
    - **What I must do:** nothing.
    - **Exact instructions:** none.
    - **Required input:** none.
    - **Claude Code prompt:** the resume prompt.
    - **Expected result:** 0 wrong frames; a complete list of what the level changes.
    - **Verification:** `python -I tools\f16_levels.py rom\pacman.a26` says `result: OK` (46 game starts and 20 level steps right; 283,995 timer frames right).
    - **Completion criteria:** all of the above.
    - **Potential problems:** levels above 14 do not exist; the difficulty-A scare time is from the code only if no recording shows it.
    - **Recovery:** one new script and a generated document; undo by returning to `fcf4e66` (with your OK).
    - **Documentation updates:** `docs\F16_LEVELS.md`, `DECISIONS.md` (T-F16-1), `CLAUDE.md`, `PROJECT_STATUS.md`, `PLAN.md`; correction of the pause length in the F12 and F13 documents.
    - **Git checkpoint:** `Stage F16: Levels, speed-ups and timers`.
    - **Next stage:** F17 — Game states.
17. **[Done] F17 — Game states.** *(expanded and done 2026-10-26)*
    - **Objective:** name every state of the game (title, game start, play, frozen start, ghost-eaten pause, death, game over, the game-number screen) and prove every change between them.
    - **Why it matters:** the rebuilt game needs the same state machine; a missed change (for example Reset during a death) would show in the lockstep test far from its cause.
    - **Prerequisites:** F16 Done (`5f3ed2d`); F9 input, F12 flags, F15, F16; the flag logs in `reference\f12`.
    - **What Claude Code will do:**
      1. `tools\f17_states.py`: classify every frame of the 70 games into a state from the memory alone; check each change of state against a table of causes (buttons, timers, collisions, cleared map, lives); check the memory on entry into game start, the Select step, a next try, a next map and game over.
      2. Write `docs\F17_STATES.md` (generated).
    - **What I must do:** nothing.
    - **Exact instructions:** none.
    - **Required input:** none.
    - **Claude Code prompt:** the resume prompt.
    - **Expected result:** no unexplained change; every entry as listed.
    - **Verification:** `python -I tools\f17_states.py rom\pacman.a26` says `result: OK` (438,212 frames; 121 game starts, 143 Select steps, 241 next tries (19 with a change of player), 52 game overs, 20 next maps).
    - **Completion criteria:** all of the above.
    - **Potential problems:** player 2 never had a joystick in a recording; the delay of the fire button after a game over (E6) is not explained.
    - **Recovery:** one new script and a generated document; undo by returning to `5f3ed2d` (with your OK).
    - **Documentation updates:** `docs\F17_STATES.md`, `DECISIONS.md` (T-F17-1), `CLAUDE.md`, `PROJECT_STATUS.md`, `PLAN.md`.
    - **Git checkpoint:** `Stage F17: Game states`.
    - **Next stage:** F18 — The drawing kernel and flicker schedule.
18. **[Done] F18 — The drawing kernel and the flicker schedule.** *(expanded and done 2026-10-26)*
    - **Objective:** know exactly what the drawing code puts on every scanline of every frame, and which object (ghost, Pac-Man, pill, item, wafers, score, markers) it is.
    - **Why it matters:** the native renderer (F23) must reproduce the picture, the flicker and (as it turned out) the collision flags from the game state; it needs an exact reference to compare against.
    - **Prerequisites:** F17 Done (`cbbe493`); F12 flag logs in `reference\f12`; F8 routines; the built `stella_coverage.exe` and `build\host\pacman2600.exe`.
    - **What Claude Code will do:**
      1. Add options to the measuring tool `stella_coverage` (`tiaw=` every write to the TIA chip with scanline and clock, `ramdump=` the 128 bytes when an instruction is about to run, `frames=` and `tiastride=` to choose frames); rebuild; the 70 replays still equal their golden traces.
      2. `tools\f18_cpu.py`: a small interpreter of the original bytes (read only) that runs `$F07F-$F4E7` over 128 bytes and lists the TIA writes. `tools\f18_kernel.py`: checks that list against the emulator, then reads out the line map, the row template, the flicker, the sprite columns and the collision flags.
      3. Write `docs\F18_KERNEL.md` (generated).
    - **What I must do:** nothing.
    - **Exact instructions:** none.
    - **Required input:** none.
    - **Claude Code prompt:** the resume prompt.
    - **Expected result:** the interpreter equals the emulator write for write; flicker and flags follow from it.
    - **Verification:** `python -I tools\f18_kernel.py rom\pacman.a26` says `result: OK` (8,240 of 8,240 sampled frames equal write for write; the ghost drawn is (FRAME_COUNTER and 3) + 1 in 101,417 of 101,417 frames; the ghost flag predicted from the sprites in 101,417 of 101,417 frames; 141 sprite rows of real pictures exact). The option `--cached` reuses the saved results to rewrite the document (the full run takes about 15 minutes).
    - **Completion criteria:** all of the above.
    - **Potential problems:** the pixel-for-pixel picture and the colours are F23; the wall patterns are not listed here.
    - **Recovery:** two new scripts, options in the measuring tool and a generated document; undo by returning to `cbbe493` (with your OK).
    - **Documentation updates:** `docs\F18_KERNEL.md`, `DECISIONS.md` (T-F18-1), `CLAUDE.md`, `docs\GLOSSARY.md`, `PROJECT_STATUS.md`, `PLAN.md`.
    - **Git checkpoint:** `Stage F18: The drawing kernel and flicker schedule`.
    - **Next stage:** F19 — Sound routines and their tables.
19. **[Done] F19 — Sound routines and their tables.** *(expanded and done 2026-10-26)*
    - **Objective:** state exactly what the game writes to the sound chip in every frame and prove it.
    - **Why it matters:** the native sound generator (F24) and the lockstep test need the exact register writes, not only the sound of the emulator.
    - **Prerequisites:** F18 Done (`16b3260`); F11/F12/F15 events; the flag logs in `reference\f12`.
    - **What Claude Code will do:**
      1. Add the option `tiaregs=` (log only chosen chip registers) to `stella_coverage`; replay all 70 games logging every write to the six sound registers.
      2. `tools\f19_sound.py`: a model of `SOUND_START`, `SOUND_EFFECTS`, `SOUND_WRITE`, `SOUND_MUSIC` fed by the game events; compare the predicted writes (value and order) and the bytes `$E8`/`$E9` with the emulator in every frame.
      3. Write `docs\F19_SOUND.md` (generated).
    - **What I must do:** nothing.
    - **Exact instructions:** none.
    - **Required input:** none.
    - **Claude Code prompt:** the resume prompt.
    - **Expected result:** 0 wrong frames.
    - **Verification:** `python -I tools\f19_sound.py rom\pacman.a26` says `result: OK` (435,986 of 435,986 predicted frames exactly right, 123,763 of them with a sound; `--cached` rewrites the document from saved logs).
    - **Completion criteria:** all of the above.
    - **Potential problems:** Select/Reset frames and the frames where the title screen starts or ends are skipped; sound samples themselves are F24.
    - **Recovery:** one new script, a logging option and a generated document; undo by returning to `16b3260` (with your OK).
    - **Documentation updates:** `docs\F19_SOUND.md`, `DECISIONS.md` (T-F19-1), `CLAUDE.md`, `PROJECT_STATUS.md`, `PLAN.md`.
    - **Git checkpoint:** `Stage F19: Sound routines and their tables`.
    - **Next stage:** F20 — Lockstep harness.
20. **[Done] F20 — Lockstep harness.** *(expanded and done 2026-10-26)*
    - **Objective:** a test tool that runs the original program and (later) our native routines together on the same inputs, compares all 128 memory bytes after every frame, and on the first difference stops and reports the frame, the byte, both values and the routine responsible.
    - **Why it matters:** it is the safety net for F21-F25: each routine we rewrite is accepted only when all 70 recorded games stay identical, and a mistake points at its cause at once.
    - **Prerequisites:** F19 Done (`91f2e45`); the F7/F8 names; the 70 golden traces; the flag logs of F12.
    - **What Claude Code will do:**
      1. `tools\f20_gen_tables.py`: writes `src\lockstep\generated_tables.hpp` (opcode table, routine list, RAM names; no ROM bytes).
      2. `src\lockstep`: a small 6502 core (`machine.*`) with inputs, collision-flag oracle, frame boundary and hooks for native routines; `lockstep_main.cpp` (the command-line tool); `natives.*` with two test fixtures (RNG_STEP right and deliberately wrong); `tools\build_lockstep.cmd`.
      3. `tools\f20_lockstep.py`: makes the flag oracles from the F12 logs, runs all 70 games in four ways (original; correct native; broken native; no oracle) and writes `docs\F20_LOCKSTEP.md`.
    - **What I must do:** nothing.
    - **Exact instructions:** none.
    - **Required input:** none.
    - **Claude Code prompt:** the resume prompt.
    - **Expected result:** the original program in the harness reproduces all golden memory; a wrong native routine is found and named.
    - **Verification:** `python -I tools\f20_lockstep.py rom\pacman.a26` says `result: OK` (70 of 70 games, 439,680 frames identical, 26 s of work; correct native identical; broken native found and named in 70 of 70; missing oracle found).
    - **Completion criteria:** all of the above.
    - **Potential problems:** timing is not modelled (the cycle-exact picture reference is `tools\f18_cpu.py`); collision flags come from an oracle until the renderer exists (F23); picture and sound are not compared here.
    - **Recovery:** new files only (plus the build script); undo by returning to `91f2e45` (with your OK).
    - **Documentation updates:** `docs\F20_LOCKSTEP.md`, `DECISIONS.md` (T-F20-1), `CLAUDE.md`, `docs\GLOSSARY.md`, `PROJECT_STATUS.md`, `PLAN.md`.
    - **Git checkpoint:** `Stage F20: Lockstep harness`.
    - **Next stage:** F21 — Rewrite one routine per session (F21.1 first).
21. **[Done] F21 — Rewrite one routine per session** (F21.1, F21.2 …, one per item in F8's list, smallest and leaf routines first). Each rewrite must turn its part of the lockstep test green before the next starts. The ledger of done and to-do routines is `docs\F21_ROUTINES.md`. **F21.1 done 2026-10-26: CLEAR_SCORES. F21.2 done 2026-10-26: SWAP_PLAYERS. F21.3 done 2026-10-26: RNG_STEP. F21.4 done 2026-10-26: the WRAP routines. F21.5 done 2026-10-26: NEW_MAZE and PLACE_OBJECTS. F21.6 done 2026-10-26: NEW_GAME_SETUP. F21.7 done 2026-10-26: LOCATE_OBJECT. F21.8 done 2026-10-26: MOVE_OBJECT. F21.9 done 2026-10-26: SET_DIRECTION. F21.10 done 2026-10-26: ADD_SCORE. F21.11 done 2026-10-26: LIVES_MARKERS. F21.12 done 2026-10-26: the TRY routines. F21.13 done 2026-10-26: the TRY_PAIR routines. F21.14 done 2026-10-26: ANIMATE_OBJECT. F21.15 done 2026-10-26: GHOST_DECIDE. F21.16 done 2026-10-26: GHOST_UPDATE. F21.17 done 2026-10-26: SOUND_START. F21.18 done 2026-10-26: SOUND_EFFECTS. F21.19 done 2026-10-26: SOUND_MUSIC. F21.20 done 2026-10-26: PACMAN_KEEP_GOING. F21.21 done 2026-10-26: SCORE_DIGITS. F21.22 done 2026-10-26: the top logic. F21.23 done 2026-10-26: the bottom logic. F21.24 done 2026-10-26: the start of the next frame. F21.25 done 2026-10-26: the start-up. F21.26 done 2026-10-26: what the picture code leaves in memory. F21.27 done 2026-10-26: the whole game as native code. F21 complete.** *(expanded below)*
    - **Objective (F21.1):** set up the way a routine is rewritten and prove it with the first, smallest routine that changes memory: CLEAR_SCORES (`$FC7D`, 14 bytes: both scores and both lives to 0).
    - **Why it matters:** every later session follows the same protocol; the protocol must detect a wrong routine even when the recorded games do not show it (a wrong register).
    - **Prerequisites:** F20 Done (`bb3df0f`).
    - **What Claude Code will do:**
      1. `src\game\routines.hpp/.cpp`: `game::clearScores` working on the 128 bytes with the F7 names (`src\game\ram_map.hpp`, generated by `tools\f20_gen_tables.py`).
      2. `src\lockstep`: the adapter in `natives.cpp` (also reproduces A, X and the flags the original leaves), the new switch `--verify` (every call of a native routine is also run through the original code on a copy and memory, registers and flags are compared) and two fixtures that are wrong on purpose.
      3. `tools\f21_routines.py`: runs all 70 games with each rewritten routine and with all of them together, runs the fixtures, writes the ledger `docs\F21_ROUTINES.md`.
    - **What I must do:** nothing.
    - **Exact instructions:** none.
    - **Required input:** none.
    - **Claude Code prompt:** the resume prompt.
    - **Expected result:** 70 of 70 games identical with the native routine; the routine check clean; the fixtures found.
    - **Verification:** `python -I tools\f21_routines.py rom\pacman.a26` says `result: OK` (CLEAR_SCORES: 70 of 70 games identical, 959 calls compared with the original, 0 different; fixture with wrong memory found in 68 of 68 games that call the routine; fixture with a wrong register found by the routine check in 68 games although all frames stay identical).
    - **Completion criteria:** all of the above.
    - **Potential problems:** routines that read cartridge tables wait for F22; routines that only write chip registers wait for F23/F24 (the ledger says which).
    - **Recovery:** new files and small changes to the lockstep tool; undo by returning to `bb3df0f` (with your OK).
    - **Documentation updates:** `docs\F21_ROUTINES.md`, `DECISIONS.md` (T-F21-1), `CLAUDE.md`, `PROJECT_STATUS.md`, `PLAN.md`.
    - **Git checkpoint:** `Stage F21.1: Rewrite CLEAR_SCORES`.
    - **Next stage:** F21.2 — Rewrite SWAP_PLAYERS (done, below).
    - **F21.2 — Rewrite SWAP_PLAYERS** *(expanded and done 2026-10-26)*
      - **Objective:** rewrite SWAP_PLAYERS (`$FB9F`, 14 bytes: exchanges the 20-byte block of the player in turn, `$88-$9B`, with the other player's block, `$9C-$AF`) as C++ and prove it with the F21.1 protocol.
      - **Why it matters:** it is the whole of the two-player turn change; a mistake would show at the first change of turn.
      - **Prerequisites:** F21.1 Done (`696d22a`).
      - **What Claude Code will do:** `game::swapPlayers` in `src\game\routines.cpp`; the adapter in `src\lockstep\natives.cpp` (leaves A = old `$88`, Y = old `$9C`, X = $FF, negative set, zero clear, carry untouched, like the original); the routine added to the DONE list of `tools\f21_routines.py`.
      - **What I must do / Exact instructions / Required input:** nothing.
      - **Claude Code prompt:** the resume prompt.
      - **Expected result:** 70 of 70 games identical; every call equal to the original.
      - **Verification:** `python -I tools\f21_routines.py rom\pacman.a26` says `result: OK` (SWAP_PLAYERS: 70 of 70 games identical, 139 calls compared with the original, 0 different; CLEAR_SCORES still clean; both together identical).
      - **Completion criteria:** all of the above.
      - **Potential problems:** only the 19 changes of turn and the 120 two-player game starts of the recordings exercise it; the registers it leaves are covered by the call check.
      - **Recovery:** two small code additions; undo by returning to `696d22a` (with your OK).
      - **Documentation updates:** `docs\F21_ROUTINES.md`, `DECISIONS.md` (T-F21-2), `CLAUDE.md`, `PROJECT_STATUS.md`, `PLAN.md`.
      - **Git checkpoint:** `Stage F21.2: Rewrite SWAP_PLAYERS`.
      - **Next stage:** F21.3 — Rewrite RNG_STEP (done, below).
    - **F21.3 — Rewrite RNG_STEP** *(expanded and done 2026-10-26)*
      - **Objective:** rewrite RNG_STEP (`$FBE0`, 20 bytes: steps the shift register `$CB`, the game's only random number) as C++ with the F21.1 protocol.
      - **Why it matters:** it is the heart of every ghost decision; one wrong bit would change every game after a few frames.
      - **Prerequisites:** F21.2 Done (`fd1468b`); F14 (the generator is known).
      - **What Claude Code will do:** `game::rngStep` in `src\game\routines.cpp` (the bit-mask table at `$FE6A` is written as `0x80 >> n`; `tools\f21_routines.py` checks that the cartridge bytes equal it); the adapter `rngStepNative` (A = result, Y = new state and 7, carry = old bit 7, zero/negative from A); the routine added to the DONE list. The F20 test fixtures `rng` (now this adapter) and `rng-broken` stay.
      - **What I must do / Exact instructions / Required input:** nothing.
      - **Claude Code prompt:** the resume prompt.
      - **Expected result:** 70 of 70 games identical; every call equal to the original.
      - **Verification:** `python -I tools\f21_routines.py rom\pacman.a26` says `result: OK` (RNG_STEP: 70 of 70 games identical, 155,820 calls compared with the original, 0 different; mask table equals 0x80 >> n; all three rewritten routines together identical); `tools\f20_lockstep.py` still OK.
      - **Completion criteria:** all of the above.
      - **Potential problems:** none; the mask table is a plain pattern, not a copy of cartridge data.
      - **Recovery:** small additions; undo by returning to `fd1468b` (with your OK).
      - **Documentation updates:** `docs\F21_ROUTINES.md`, `DECISIONS.md` (T-F21-3), `CLAUDE.md`, `PROJECT_STATUS.md`, `PLAN.md`.
      - **Git checkpoint:** `Stage F21.3: Rewrite RNG_STEP`.
      - **Next stage:** F21.4 — Rewrite the WRAP routines (done, below).
    - **F21.4 — Rewrite the WRAP routines** *(expanded and done 2026-10-26)*
      - **Objective:** rewrite WRAP_TO_BOTTOM (`$FD26`), WRAP_TO_TOP (`$FD32`) and their shared end WRAP_SET_Y (`$FD3C`) as C++ (put Pac-Man or a ghost into the opening of the bottom or top wall).
      - **Why it matters:** they are the only place where the openings of the maze are entered, for Pac-Man (from the stick code) and ghosts (from the ghost decision, by a jump).
      - **Prerequisites:** F21.3 Done (`3552e18`).
      - **What Claude Code will do:** `game::wrapToBottom` / `game::wrapToTop` (direction bits $30 / $F0 and y = $51 / $0B; object in X), one adapter for both (A = new y, zero/negative clear, X, Y, carry untouched); the group added to the DONE list (one native name `wrap`, so the ledger shows three rows and the runs count once); `tools\f21_routines.py` no longer runs a native twice.
      - **What I must do / Exact instructions / Required input:** nothing.
      - **Claude Code prompt:** the resume prompt.
      - **Expected result:** 70 of 70 games identical; every call equal to the original, entered by `JSR` and by `JMP`.
      - **Verification:** `python -I tools\f21_routines.py rom\pacman.a26` says `result: OK` (the WRAP group: 70 of 70 games identical, 40 calls compared with the original, 0 different; all four rewrites together identical).
      - **Completion criteria:** all of the above.
      - **Potential problems:** only 40 calls in the recordings (15 and 12 from Pac-Man's code, 10 and 2 from the ghost decision, plus the extra games); both entry kinds occur.
      - **Recovery:** small additions; undo by returning to `3552e18` (with your OK).
      - **Documentation updates:** `docs\F21_ROUTINES.md`, `DECISIONS.md` (T-F21-4), `CLAUDE.md`, `PROJECT_STATUS.md`, `PLAN.md`.
      - **Git checkpoint:** `Stage F21.4: Rewrite the WRAP routines`.
      - **Next stage:** F21.5 — Rewrite NEW_MAZE and PLACE_OBJECTS (done, below).
    - **F21.5 — Rewrite NEW_MAZE and PLACE_OBJECTS** *(expanded and done 2026-10-26)*
      - **Objective:** rewrite PLACE_OBJECTS (`$FC21`, 36 bytes: freeze the game, timer `$3C`, Pac-Man and the ghosts to their start places) and NEW_MAZE (`$FC0C`, 21 bytes: fill the wafer map, pill flags `$3C`, then PLACE_OBJECTS) as C++.
      - **Why it matters:** they start every try, every map and every game; the maze content and the start positions must be exact.
      - **Prerequisites:** F21.4 Done (`8a3de12`); F11 (the two places that start empty).
      - **What Claude Code will do:** `game::placeObjects` and `game::newMaze` (which ends by calling placeObjects, like the original falls into it); one adapter for both (A = $80, X = $FF, Y = 0, negative set, zero clear, carry untouched); both added to the DONE list. NEW_MAZE is also reached by falling through from NEW_GAME_SETUP and PLACE_OBJECTS by falling through from NEW_MAZE: both ways are covered because the harness looks at every instruction address.
      - **What I must do / Exact instructions / Required input:** nothing.
      - **Claude Code prompt:** the resume prompt.
      - **Expected result:** 70 of 70 games identical; every call equal to the original.
      - **Verification:** `python -I tools\f21_routines.py rom\pacman.a26` says `result: OK` (NEW_MAZE: 70 of 70 games identical, 1,026 calls compared, 0 different; PLACE_OBJECTS: 1,319 calls compared, 0 different; all six rewrites together identical).
      - **Completion criteria:** all of the above.
      - **Potential problems:** none.
      - **Recovery:** small additions; undo by returning to `8a3de12` (with your OK).
      - **Documentation updates:** `docs\F21_ROUTINES.md`, `DECISIONS.md` (T-F21-5), `CLAUDE.md`, `PROJECT_STATUS.md`, `PLAN.md`.
      - **Git checkpoint:** `Stage F21.5: Rewrite NEW_MAZE and PLACE_OBJECTS`.
      - **Next stage:** F21.6 — Rewrite NEW_GAME_SETUP (done, below).
    - **F21.6 — Rewrite NEW_GAME_SETUP** *(expanded and done 2026-10-26)*
      - **Objective:** rewrite NEW_GAME_SETUP (`$FBF4`, 24 bytes: score colours `$0A`/`$09`, three lives, start level ((game number - 2) and 6) + 4, then NEW_MAZE) as C++.
      - **Why it matters:** it is the start of every game, for both players; the start level (F16) comes from here.
      - **Prerequisites:** F21.5 Done (`2cbeebb`).
      - **What Claude Code will do:** `game::newGameSetup` (ends by calling newMaze, like the original falls into it); the adapter shares the one of NEW_MAZE (A = $80, X = $FF, Y = 0, negative set, zero clear) and additionally clears carry and overflow, which the original leaves clear after its SEC/SBC/CLC/ADC; added to the DONE list.
      - **What I must do / Exact instructions / Required input:** nothing.
      - **Claude Code prompt:** the resume prompt.
      - **Expected result:** 70 of 70 games identical; every call equal to the original.
      - **Verification:** `python -I tools\f21_routines.py rom\pacman.a26` says `result: OK` (NEW_GAME_SETUP: 70 of 70 games identical, 1,006 calls compared, 0 different; all seven rewrites together identical). On the first try the call check found the carry flag wrong in 142 calls while every frame was still identical; fixed.
      - **Completion criteria:** all of the above.
      - **Potential problems:** none left.
      - **Recovery:** small additions; undo by returning to `2cbeebb` (with your OK).
      - **Documentation updates:** `docs\F21_ROUTINES.md`, `DECISIONS.md` (T-F21-6), `CLAUDE.md`, `PROJECT_STATUS.md`, `PLAN.md`.
      - **Git checkpoint:** `Stage F21.6: Rewrite NEW_GAME_SETUP`.
      - **Next stage:** F21.7 — Rewrite LOCATE_OBJECT (done, below).
    - **F21.7 — Rewrite LOCATE_OBJECT** *(expanded and done 2026-10-26)*
      - **Objective:** rewrite LOCATE_OBJECT (`$FBAD`, 51 bytes: row, offsets in the cell, cell index and position code of object X) as C++.
      - **Why it matters:** Pac-Man and every ghost use it every frame (F10, F13); it is the busiest routine of the logic.
      - **Prerequisites:** F21.6 Done (`a1794f6`).
      - **What Claude Code will do:** `game::locateObject` (the row loop copied with its exit rule, then offset, cell index, position code; returns the five values); the adapter reproduces A = position code, Y = row, carry = bit 6 of the cell index, zero/negative from the row, the overflow flag left by the last SBC, and the flags byte that the original PHP leaves in the stack area (that byte is part of the 128 compared bytes); added to the DONE list.
      - **What I must do / Exact instructions / Required input:** nothing.
      - **Claude Code prompt:** the resume prompt.
      - **Expected result:** 70 of 70 games identical; every call equal to the original.
      - **Verification:** `python -I tools\f21_routines.py rom\pacman.a26` says `result: OK` (LOCATE_OBJECT: 70 of 70 games identical, 1,329,282 calls compared with the original, 0 different; all eight rewrites together identical). On the first tries the checks found two things: the flags byte in the stack (an effect of PHP) and the overflow flag of the last SBC; both fixed in the adapter.
      - **Completion criteria:** all of the above.
      - **Potential problems:** y of 128 or more never occurs (the largest is $5B); the loop is copied as it is but that path was never run.
      - **Recovery:** small additions; undo by returning to `a1794f6` (with your OK).
      - **Documentation updates:** `docs\F21_ROUTINES.md`, `DECISIONS.md` (T-F21-7), `CLAUDE.md`, `PROJECT_STATUS.md`, `PLAN.md`.
      - **Git checkpoint:** `Stage F21.7: Rewrite LOCATE_OBJECT`.
      - **Next stage:** F21.8 — Rewrite MOVE_OBJECT (done, below).
    - **F21.8 — Rewrite MOVE_OBJECT** *(expanded and done 2026-10-26)*
      - **Objective:** rewrite MOVE_OBJECT (`$F9EE`, 56 bytes: the speed accumulator of object X times 17 and, on a carry, the one-pixel step with the border rules) as C++.
      - **Why it matters:** every movement of Pac-Man and the ghosts (F10, F13, F16) goes through it, about a million times in the recordings.
      - **Prerequisites:** F21.7 Done (`3456f4c`).
      - **What Claude Code will do:** `game::moveObject` (returns what happened for the adapter); the adapter reproduces the registers and flags of each path (no carry; horizontal step stopped or made; vertical step negative, stopped or made); added to the DONE list. The routine is also entered by jumps (from GHOST_UPDATE and by falling in from SET_DIRECTION): the harness catches every way in.
      - **What I must do / Exact instructions / Required input:** nothing.
      - **Claude Code prompt:** the resume prompt.
      - **Expected result:** 70 of 70 games identical; every call equal to the original.
      - **Verification:** `python -I tools\f21_routines.py rom\pacman.a26` says `result: OK` (MOVE_OBJECT: 70 of 70 games identical, 1,174,804 calls compared with the original, 0 different; all nine rewrites together identical). Right at the first run.
      - **Completion criteria:** all of the above.
      - **Potential problems:** none.
      - **Recovery:** small additions; undo by returning to `3456f4c` (with your OK).
      - **Documentation updates:** `docs\F21_ROUTINES.md`, `DECISIONS.md` (T-F21-8), `CLAUDE.md`, `PROJECT_STATUS.md`, `PLAN.md`.
      - **Git checkpoint:** `Stage F21.8: Rewrite MOVE_OBJECT`.
      - **Next stage:** F21.9 — Rewrite SET_DIRECTION (done, below).
    - **F21.9 — Rewrite SET_DIRECTION** *(expanded and done 2026-10-26)*
      - **Objective:** rewrite SET_DIRECTION (`$F9E5`, 9 bytes: store direction code Y in bits 7-6 of the state of object X and set the "step due" bit, then fall into MOVE_OBJECT) as C++.
      - **Why it matters:** every turn of Pac-Man and of a ghost is set here, and it moves the object at once.
      - **Prerequisites:** F21.8 Done (`2f8c763`).
      - **What Claude Code will do:** `game::setDirection` (state = (state and $3F) or (d << 6) or $10, then `moveObject`); the table `TBL_FE63` is the pattern (d << 6) or $10, checked equal to the cartridge bytes by `tools\f21_routines.py`; the adapter shares its register/flag code with MOVE_OBJECT (refactored into `finishMove`); added to the DONE list.
      - **What I must do / Exact instructions / Required input:** nothing.
      - **Claude Code prompt:** the resume prompt.
      - **Expected result:** 70 of 70 games identical; every call equal to the original.
      - **Verification:** `python -I tools\f21_routines.py rom\pacman.a26` says `result: OK` (SET_DIRECTION: 70 of 70 games identical, 73,589 calls compared with the original, 0 different; table equals (d << 6) or $10; all ten rewrites together identical).
      - **Completion criteria:** all of the above.
      - **Potential problems:** a direction code above 3 would read other cartridge bytes in the original; it never happens in the recordings and the adapter masks it.
      - **Recovery:** small additions; undo by returning to `2f8c763` (with your OK).
      - **Documentation updates:** `docs\F21_ROUTINES.md`, `DECISIONS.md` (T-F21-9), `CLAUDE.md`, `PROJECT_STATUS.md`, `PLAN.md`.
      - **Git checkpoint:** `Stage F21.9: Rewrite SET_DIRECTION`.
      - **Next stage:** F21.10 — Rewrite ADD_SCORE (done, below).
    - **F21.10 — Rewrite ADD_SCORE** *(expanded and done 2026-10-26)*
      - **Objective:** rewrite ADD_SCORE (`$FC45`, 35 bytes: decimal addition of the amount number Y to the score of the player in turn) as C++, and introduce the cartridge data view.
      - **Why it matters:** every point of the game goes through it; it is also the first rewrite that needs cartridge data (the amount tables at `$FF06`/`$FF0C`), which the source must not contain.
      - **Prerequisites:** F21.9 Done (`1d3f4d5`); F15 (the amounts).
      - **What Claude Code will do:** `game::Cartridge` (a view that gives a cartridge byte by its address, `cart.at(0xFF06 + y)`; no data in the source; the lockstep tool fills it from the ROM file, the loader of F22 will fill it from the player's ROM); `game::addScore` (decimal two-digit addition with the carry chain of the 6502, the player chosen by PLAYER_MODE bit 7, the caller's X kept in the scratch byte TEMP_E1 as the original does); the adapter reproduces A = new high byte, X restored (zero/negative from X), decimal flag clear, carry and overflow of the last decimal ADC; added to the DONE list.
      - **What I must do / Exact instructions / Required input:** nothing.
      - **Claude Code prompt:** the resume prompt.
      - **Expected result:** 70 of 70 games identical; every call equal to the original.
      - **Verification:** `python -I tools\f21_routines.py rom\pacman.a26` says `result: OK` (ADD_SCORE: 70 of 70 games identical, 4,607 calls compared with the original, 0 different; all eleven rewrites together identical). Right at the first run.
      - **Completion criteria:** all of the above.
      - **Potential problems:** an amount number above 6 would read other cartridge bytes; never happens in the recordings; non-decimal digits in a score byte (the title shows `$CB`) are handled by the same arithmetic as the chip.
      - **Recovery:** small additions; undo by returning to `1d3f4d5` (with your OK).
      - **Documentation updates:** `docs\F21_ROUTINES.md`, `DECISIONS.md` (T-F21-10), `CLAUDE.md`, `PROJECT_STATUS.md`, `PLAN.md`.
      - **Git checkpoint:** `Stage F21.10: Rewrite ADD_SCORE`.
      - **Next stage:** F21.11 — Rewrite LIVES_MARKERS (done, below).
    - **F21.11 — Rewrite LIVES_MARKERS** *(expanded and done 2026-10-26)*
      - **Objective:** rewrite LIVES_MARKERS (`$FC68`, 21 bytes: three playfield values for one player's row of life markers) as C++.
      - **Why it matters:** it is what shows the number of lives on the status line (F18: lines 217-222).
      - **Prerequisites:** F21.10 Done (`4c6d060`) for the cartridge view.
      - **What Claude Code will do:** `game::livesMarkers(ram, cart, lives, offset)`: TEMP_D2+offset = $40 (0 when no lives), TEMP_D3+offset = cartridge byte `$FF1B + lives`, TEMP_D4+offset = cartridge byte `$FEEC + lives`; the adapter leaves A = the last byte (zero/negative from it), X, Y and carry untouched; added to the DONE list.
      - **What I must do / Exact instructions / Required input:** nothing.
      - **Claude Code prompt:** the resume prompt.
      - **Expected result:** 70 of 70 games identical; every call equal to the original.
      - **Verification:** `python -I tools\f21_routines.py rom\pacman.a26` says `result: OK` (LIVES_MARKERS: 70 of 70 games identical, 879,220 calls compared with the original, 0 different; all twelve rewrites together identical). Right at the first run.
      - **Completion criteria:** all of the above.
      - **Potential problems:** none.
      - **Recovery:** small additions; undo by returning to `4c6d060` (with your OK).
      - **Documentation updates:** `docs\F21_ROUTINES.md`, `DECISIONS.md` (T-F21-11), `CLAUDE.md`, `PROJECT_STATUS.md`, `PLAN.md`.
      - **Git checkpoint:** `Stage F21.11: Rewrite LIVES_MARKERS`.
      - **Next stage:** F21.12 — Rewrite the TRY routines (done, below).
    - **F21.12 — Rewrite the TRY routines** *(expanded and done 2026-10-26)*
      - **Objective:** rewrite TRY_UP (`$FB36`), TRY_DOWN (`$FB5C`), TRY_LEFT (`$FB70`), TRY_RIGHT (`$FB87`) and their shared end TRY_COMMIT (`$FB44`) as C++: may object X turn into this direction from the cell it stands in, and if so set the direction.
      - **Why it matters:** every turn of Pac-Man and every ghost decision (F10, F13) is made here, and the routines have the odd property that, when the turn is taken, they drop their caller's return address, so the decision routine above them ends at once.
      - **Prerequisites:** F21.11 Done (`7d39c61`); F21.10 (the cartridge data view); F21.7 (LOCATE_OBJECT).
      - **What Claude Code will do:** `game::tryDirection(ram, cart, object, direction)` returns how it ended (at the edge, blocked by a wall, a ghost may not turn back, taken); the wall table at `$FE13` is read through the cartridge view; the table `TBL_FE67` is ((d xor 1) << 6), `TBL_FE63` is (d << 6) or $10 (both checked against the cartridge); one adapter for the four entry points reproduces the registers and flags of every path and, when the turn is taken, throws away the caller's return (two pops) before the final return; added to the DONE list.
      - **What I must do / Exact instructions / Required input:** nothing.
      - **Claude Code prompt:** the resume prompt.
      - **Expected result:** 70 of 70 games identical; every call equal to the original, including the stack behaviour.
      - **Verification:** `python -I tools\f21_routines.py rom\pacman.a26` says `result: OK` (the TRY group: 70 of 70 games identical, 326,604 calls compared with the original, 0 different; all seventeen routines together identical). The first run found one difference in 303 calls: the carry that goes into the first ROL of TRY_DOWN is the one left by its CMP #7, not the caller's; fixed in the adapter.
      - **Completion criteria:** all of the above.
      - **Potential problems:** none left.
      - **Recovery:** small additions; undo by returning to `7d39c61` (with your OK).
      - **Documentation updates:** `docs\F21_ROUTINES.md`, `DECISIONS.md` (T-F21-12), `CLAUDE.md`, `PROJECT_STATUS.md`, `PLAN.md`.
      - **Git checkpoint:** `Stage F21.12: Rewrite the TRY routines`.
      - **Next stage:** F21.13 — Rewrite the TRY_PAIR routines (done, below).
    - **F21.13 — Rewrite the TRY_PAIR routines** *(expanded and done 2026-10-26)*
      - **Objective:** rewrite TRY_PAIR_1 (`$FAFC`) with TRY_PAIR_UD (`$FB05`), and TRY_PAIR_2 (`$FB19`) with TRY_PAIR_LR (`$FB22`): after a random step, try the two directions of one axis in the order the random number picks.
      - **Why it matters:** this is how a ghost chooses a side turn at random (F13, F14); their stack behaviour is delicate (a taken turn ends the decision routine above them).
      - **Prerequisites:** F21.12 Done (`3c34886`); F21.3 (RNG_STEP).
      - **What Claude Code will do:** `game::tryPair` (order by the random result, calls `tryDirection` once or twice); the adapter for TRY_PAIR_1 and TRY_PAIR_2 does the random step's registers, then the registers of each TRY routine in order (shared code refactored from F21.12), then either the two pops of a taken turn or the end of TRY_PAIR_UD/LR (A = $FB, negative set, zero clear, return); it also writes the return addresses that the original's JSRs leave in the stack bytes (those bytes are part of the compared memory). UD and LR are only reachable from the PAIR routines and are covered inside; added to the DONE list.
      - **What I must do / Exact instructions / Required input:** nothing.
      - **Claude Code prompt:** the resume prompt.
      - **Expected result:** 70 of 70 games identical; every call equal to the original, stack bytes included.
      - **Verification:** `python -I tools\f21_routines.py rom\pacman.a26` says `result: OK` (the PAIR group: 70 of 70 games identical, 66,441 calls compared with the original, 0 different; all twenty-one routines together identical). The first run showed the stale return addresses in the stack bytes; reproduced in the adapter.
      - **Completion criteria:** all of the above.
      - **Potential problems:** the left-then-right order and the horizontal "neither taken" end are marked "never run" in the listing (F9); they are written from the code and are covered by the call check only if a recording reaches them.
      - **Recovery:** small additions; undo by returning to `3c34886` (with your OK).
      - **Documentation updates:** `docs\F21_ROUTINES.md`, `DECISIONS.md` (T-F21-13), `CLAUDE.md`, `PROJECT_STATUS.md`, `PLAN.md`.
      - **Git checkpoint:** `Stage F21.13: Rewrite the TRY_PAIR routines`.
      - **Next stage:** F21.14 — Rewrite ANIMATE_OBJECT (done, below).
    - **F21.14 — Rewrite ANIMATE_OBJECT** *(expanded and done 2026-10-26)*
      - **Objective:** rewrite ANIMATE_OBJECT (`$FC8B`, 155 bytes) as C++: the animation of an object whose state has bit 5 set (a ghost leaving the pen or crossing an opening, Pac-Man crossing an opening), including the sprite scratch bytes it sets.
      - **Why it matters:** it is the only code that moves an object along the opening path, and it also prepares the sprite for the picture (F18).
      - **Prerequisites:** F21.13 Done (`053952f`); F21.10 (the cartridge view); F10/F13 (what the animation does).
      - **What Claude Code will do:** `game::animateObject(ram, cart, object, sprite)` with the ten ways it can end (`AnimPath`); the three small cartridge tables (`$FF0D`, `$FF19`, `$FF25`) are read by address; the adapter reproduces A, X (the pointer index in the one path that changes X), the carry and overflow of the pointer addition and the carry of the last compare on every path; added to the DONE list.
      - **What I must do / Exact instructions / Required input:** nothing.
      - **Claude Code prompt:** the resume prompt.
      - **Expected result:** 70 of 70 games identical; every call equal to the original.
      - **Verification:** `python -I tools\f21_routines.py rom\pacman.a26` says `result: OK` (ANIMATE_OBJECT: 70 of 70 games identical, 2,791 calls compared with the original, 0 different; all twenty-two routines together identical). Right at the first run. Every byte of the routine is executed in the recordings (code map of F6/F9), so each of its ways to end is checked at least once.
      - **Completion criteria:** all of the above.
      - **Potential problems:** none.
      - **Recovery:** small additions; undo by returning to `053952f` (with your OK).
      - **Documentation updates:** `docs\F21_ROUTINES.md`, `DECISIONS.md` (T-F21-14), `CLAUDE.md`, `PROJECT_STATUS.md`, `PLAN.md`.
      - **Git checkpoint:** `Stage F21.14: Rewrite ANIMATE_OBJECT`.
      - **Next stage:** F21.15 — Rewrite GHOST_DECIDE (done, below).
    - **F21.15 — Rewrite GHOST_DECIDE** *(expanded and done 2026-10-26)*
      - **Objective:** rewrite GHOST_DECIDE (`$FA26`, 214 bytes) as C++: the decision of a ghost that has just been located (aim at Pac-Man, flee, go home as eyes, enter an opening, wander at random).
      - **Why it matters:** this is the ghosts' intelligence (F13) and the only routine that uses the random number; it is built entirely from the routines rewritten before it.
      - **Prerequisites:** F21.14 Done (`3cd1f3e`); F21.3, F21.4, F21.12, F21.13 (RNG, WRAP, TRY, TRY_PAIR); F13 (the rules, proven on 348,146 frames).
      - **What Claude Code will do:** `game::ghostDecide` (the rules of F13 in C++: early exits, title screen, eyes, opening, scared, timer, chase, the four random chains); it writes the scratch bytes TEMP_D2/D3 as the original does. Harness change: a native routine may now state a **contract** (`Machine::Contract`): which registers its callers really use, and that old return addresses below the stack pointer (free stack bytes) are not game state. GHOST_DECIDE's only caller (GHOST_UPDATE) reloads A and does not use Y, the carry or the overflow flag, so X, the stack pointer and the return address are compared, with all live memory; the free stack bytes are ignored in the frame comparison and in the call check for the routines that declare it. The strict comparison (everything, as before) stays the default for every other routine.
      - **What I must do / Exact instructions / Required input:** nothing.
      - **Claude Code prompt:** the resume prompt.
      - **Expected result:** 70 of 70 games identical; every call equal to the original in memory above the stack pointer and in X, SP and PC.
      - **Verification:** `python -I tools\f21_routines.py rom\pacman.a26` says `result: OK` (GHOST_DECIDE: 70 of 70 games identical, 1,036,677 calls compared with the original, 0 different; all twenty-three routines together identical; a fixture that changes one scratch byte is found in 70 of 70 games). Right at the first run; `tools\f20_lockstep.py` still OK.
      - **Completion criteria:** all of the above.
      - **Potential problems:** the path where a ghost falls out of the last random chain into TRY_PAIR_1's code is never run and is treated as "unknown" (the harness stops if it ever happens); the register contract rests on reading the one caller.
      - **Recovery:** harness and game additions; undo by returning to `3cd1f3e` (with your OK).
      - **Documentation updates:** `docs\F21_ROUTINES.md`, `DECISIONS.md` (T-F21-15), `CLAUDE.md`, `PROJECT_STATUS.md`, `PLAN.md`.
      - **Git checkpoint:** `Stage F21.15: Rewrite GHOST_DECIDE`.
      - **Next stage:** F21.16 — Rewrite GHOST_UPDATE (done, below).
    - **F21.16 — Rewrite GHOST_UPDATE** *(expanded and done 2026-10-26)*
      - **Objective:** rewrite GHOST_UPDATE (`$F9AA`, 58 bytes) as C++: everything for one ghost in one frame.
      - **Why it matters:** the top logic calls it three times in a row for the ghost of the frame (F13); it ties LOCATE_OBJECT, GHOST_DECIDE, SET_DIRECTION and MOVE_OBJECT together and handles the eyes at the pen.
      - **Prerequisites:** F21.15 Done (`12163c3`); F21.7, F21.8, F21.9 (LOCATE_OBJECT, MOVE_OBJECT, SET_DIRECTION).
      - **What Claude Code will do:** `game::ghostUpdate` (eyes on the pen row: at the pen door turn left and move, above the pen wait while scared or become a ghost again and turn right; an animating ghost does nothing; otherwise locate, decide, move unless the decision put it into an opening); the adapter declares the contract X, stack pointer, return address and live memory (its callers load A and Y afresh and do not use the flags); added to the DONE list.
      - **What I must do / Exact instructions / Required input:** nothing.
      - **Claude Code prompt:** the resume prompt.
      - **Expected result:** 70 of 70 games identical; every call equal to the original under the contract.
      - **Verification:** `python -I tools\f21_routines.py rom\pacman.a26` says `result: OK` (GHOST_UPDATE: 70 of 70 games identical, 1,058,724 calls compared with the original, 0 different; all twenty-four routines together identical). Right at the first run.
      - **Completion criteria:** all of the above.
      - **Potential problems:** none new.
      - **Recovery:** small additions; undo by returning to `12163c3` (with your OK).
      - **Documentation updates:** `docs\F21_ROUTINES.md`, `DECISIONS.md` (T-F21-16), `CLAUDE.md`, `PROJECT_STATUS.md`, `PLAN.md`.
      - **Git checkpoint:** `Stage F21.16: Rewrite GHOST_UPDATE`.
      - **Next stage:** F21.17 — Rewrite SOUND_START (done, below).
    - **F21.17 — Rewrite SOUND_START** *(expanded and done 2026-10-26)*
      - **Objective:** rewrite SOUND_START (`$FF43`, 18 bytes) as C++: start sound effect Y unless a higher-ranking one is playing.
      - **Why it matters:** every wafer, pill, ghost and bonus sound starts here (F19).
      - **Prerequisites:** F21.16 Done (`de1ea96`); F19 (the sound rules); F21.10 (the cartridge view).
      - **What Claude Code will do:** `game::soundStart(ram, cart, sound)` (the three cartridge tables `$FFE9`, `$FE6E`, `$FFED` by address); the adapter reproduces A (the blocking bits, or the new length) with zero/negative from it; added to the DONE list.
      - **What I must do / Exact instructions / Required input:** nothing.
      - **Claude Code prompt:** the resume prompt.
      - **Expected result:** 70 of 70 games identical; every call equal to the original.
      - **Verification:** `python -I tools\f21_routines.py rom\pacman.a26` says `result: OK` (SOUND_START: 70 of 70 games identical, 4,607 calls compared with the original, 0 different; all twenty-five routines together identical). Right at the first run.
      - **Completion criteria:** all of the above.
      - **Potential problems:** none.
      - **Recovery:** small additions; undo by returning to `de1ea96` (with your OK).
      - **Documentation updates:** `docs\F21_ROUTINES.md`, `DECISIONS.md` (T-F21-17), `CLAUDE.md`, `PROJECT_STATUS.md`, `PLAN.md`.
      - **Git checkpoint:** `Stage F21.17: Rewrite SOUND_START`.
      - **Next stage:** F21.18 — Rewrite SOUND_EFFECTS (done, below).
    - **F21.18 — Rewrite SOUND_EFFECTS (with SOUND_WRITE)** *(expanded and done 2026-10-26)*
      - **Objective:** rewrite SOUND_EFFECTS (`$FF55`, 54 bytes) with its shared writer SOUND_WRITE (`$FF8B`, 8 bytes) as C++: each frame the sound effect that SOUND_START chose is written to the sound chip, channel 0. First teach the call check to compare the writes to the chip, because the result of these routines is chip registers, not memory.
      - **Why it matters:** these two routines are what the player hears for wafers, pills, ghosts and the bonus item (F19); the memory comparison cannot see them.
      - **Prerequisites:** F21.17 Done (`7d233ce`); F19 (the sound rules, proven by the register-write logs).
      - **What Claude Code will do:** (1) harness: `Machine` records every write to a chip register (except WSYNC) while a call is checked, and the call check compares the original's list with the native's, in order; (2) `game::soundEffects(ram, chip)` and the interface `game::SoundChip` (`write(register, value)`; in the finished game the sound generator of F24 stands behind it); (3) the adapter (X = 0 is a precondition, as at the one call; contract: X, stack pointer, return address); (4) a fixture that changes one chip value and nothing in memory; added to the DONE list.
      - **What I must do / Exact instructions / Required input:** nothing.
      - **Claude Code prompt:** the resume prompt.
      - **Expected result:** 70 of 70 games identical; every call equal to the original in memory and in the chip writes.
      - **Verification:** `python -I tools\f21_routines.py rom\pacman.a26` says `result: OK` (SOUND_EFFECTS: 70 of 70 games identical, 439,610 calls compared with the original including the chip writes, 0 different; the fixture with one wrong chip value is found in 67 of the 67 games that start any effect; all twenty-six routines together identical). Right at the first run.
      - **Completion criteria:** all of the above.
      - **Potential problems:** the 3 games without any sound effect cannot show a difference; none other.
      - **Recovery:** harness and game additions; undo by returning to `7d233ce` (with your OK).
      - **Documentation updates:** `docs\F21_ROUTINES.md`, `DECISIONS.md` (T-F21-18), `CLAUDE.md`, `PROJECT_STATUS.md`, `PLAN.md`.
      - **Git checkpoint:** `Stage F21.18: Rewrite SOUND_EFFECTS`.
      - **Next stage:** F21.19 — Rewrite SOUND_MUSIC (done, below).
    - **F21.19 — Rewrite SOUND_MUSIC** *(expanded and done 2026-10-26)*
      - **Objective:** rewrite SOUND_MUSIC (`$FF94`, 85 bytes) as C++: the death sound, the start tune and the scared-ghost sweep, written to the sound chip.
      - **Why it matters:** the long sounds of the game (F19); the tune has a quirk (with the timer at 0 the note number comes from the flags) that must be kept.
      - **Prerequisites:** F21.18 Done (`11be83b`) for the chip interface and the chip-write check; F19.
      - **What Claude Code will do:** `game::soundMusic(ram, cart, chip)` (death: volume = timer, pitch = timer / 2, control 4; tune: note from the timer's bits 3-5, or from the flags when the timer is 0, two cartridge tables `$FFF1` / `$FFF5`; sweep on channel 1; TEMP_D3 = 4 and the tune bit cleared as in the original); the adapter (X = 1 is a precondition of the sweep; contract: stack pointer and return address, since the one caller uses no register afterwards); added to the DONE list.
      - **What I must do / Exact instructions / Required input:** nothing.
      - **Claude Code prompt:** the resume prompt.
      - **Expected result:** 70 of 70 games identical; every call equal to the original in memory and chip writes.
      - **Verification:** `python -I tools\f21_routines.py rom\pacman.a26` says `result: OK` (SOUND_MUSIC: 70 of 70 games identical, 439,610 calls compared with the original including the chip writes, 0 different; all twenty-seven routines together identical). Right at the first run.
      - **Completion criteria:** all of the above.
      - **Potential problems:** none.
      - **Recovery:** small additions; undo by returning to `11be83b` (with your OK).
      - **Documentation updates:** `docs\F21_ROUTINES.md`, `DECISIONS.md` (T-F21-19), `CLAUDE.md`, `PROJECT_STATUS.md`, `PLAN.md`.
      - **Git checkpoint:** `Stage F21.19: Rewrite SOUND_MUSIC`.
      - **Next stage:** F21.20 — Rewrite PACMAN_KEEP_GOING (done, below).
    - **F21.20 — Rewrite PACMAN_KEEP_GOING** *(expanded and done 2026-10-26)*
      - **Objective:** rewrite PACMAN_KEEP_GOING (`$F793`, 39 bytes) as C++: Pac-Man with the stick neutral; exactly on a cell he tries to go on in his own direction, and if a wall is in the way he stands still.
      - **Why it matters:** this is why Pac-Man stops at walls (F10); the routine has two different ways of ending, one of which skips the move.
      - **Prerequisites:** F21.19 Done (`30b7bb7`); F21.12 (the TRY routines).
      - **What Claude Code will do:** `game::pacmanKeepGoing` returns NotOnCell, Moved or Stands; the adapter returns normally for the first two (the main loop then calls MOVE_OBJECT) and, for Stands, pops the caller's return (stack pointer + 2) and continues at `$F7BA` without a move. Harness: a native routine may now name the address where it continues instead of returning (`Contract::stopAt`), and the call check runs the original until that address or its return. Contract: X, stack pointer, return/arrival address and live memory; added to the DONE list.
      - **What I must do / Exact instructions / Required input:** nothing.
      - **Claude Code prompt:** the resume prompt.
      - **Expected result:** 70 of 70 games identical; every call equal to the original.
      - **Verification:** `python -I tools\f21_routines.py rom\pacman.a26` says `result: OK` (PACMAN_KEEP_GOING: 70 of 70 games identical, 127,587 calls compared with the original, 0 different; all twenty-eight routines together identical). Right at the first run (after a missing line in my own edit).
      - **Completion criteria:** all of the above.
      - **Potential problems:** none.
      - **Recovery:** harness and game additions; undo by returning to `30b7bb7` (with your OK).
      - **Documentation updates:** `docs\F21_ROUTINES.md`, `DECISIONS.md` (T-F21-20), `CLAUDE.md`, `PROJECT_STATUS.md`, `PLAN.md`.
      - **Git checkpoint:** `Stage F21.20: Rewrite PACMAN_KEEP_GOING`.
      - **Next stage:** F21.21 — Rewrite SCORE_DIGITS (done, below).
    - **F21.21 — Rewrite SCORE_DIGITS (with the CLEAR_GRAPHICS it falls into)** *(expanded and done 2026-10-26)*
      - **Objective:** rewrite SCORE_DIGITS (`$F3EE`, 149 bytes) as C++: the decimal digits of one player's score become the picture pointers of the score line (leading zeros blank) and the seven rows of the line are written to the picture chip, followed by CLEAR_GRAPHICS.
      - **Why it matters:** it is the first picture routine rewritten; F18 showed the score is drawn on lines 207-213, five digits, with the six-picture trick.
      - **Prerequisites:** F21.20 Done (`faa35b8`); F21.18 (the chip-write comparison); F18 (the picture).
      - **What Claude Code will do:** `game::scoreDigits(ram, cart, chip, offset)` with the chip as the interface `game::TiaChip` (the sound interface of F21.18 is the same chip: `SoundChip` is now an alias); the digit shapes are read from the cartridge by address through the pointers in `$D5-$DE`; the adapter compares the picture-chip writes in order (not their cycle positions: the timing of the picture is proved by the executor of F18 and will be proved pixel by pixel by the renderer of F23); contract: stack pointer and return address; added to the DONE list.
      - **What I must do / Exact instructions / Required input:** nothing.
      - **Claude Code prompt:** the resume prompt.
      - **Expected result:** 70 of 70 games identical; every call equal to the original in memory and chip writes.
      - **Verification:** `python -I tools\f21_routines.py rom\pacman.a26` says `result: OK` (SCORE_DIGITS: 70 of 70 games identical, 460,580 calls compared with the original including all chip writes, 0 different; all twenty-nine routines together identical). Right at the first run.
      - **Completion criteria:** all of the above.
      - **Potential problems:** the cycle timing of the writes is not part of this check.
      - **Recovery:** small additions; undo by returning to `faa35b8` (with your OK).
      - **Documentation updates:** `docs\F21_ROUTINES.md`, `DECISIONS.md` (T-F21-21), `CLAUDE.md`, `PROJECT_STATUS.md`, `PLAN.md`.
      - **Git checkpoint:** `Stage F21.21: Rewrite SCORE_DIGITS`.
      - **Next stage:** F21.22 — Rewrite the top logic of the frame (done, below).
    - **F21.22 — Rewrite the top logic of the frame (`$F87E-$F9A7`)** *(expanded and done 2026-10-26)*
      - **Objective:** rewrite the first part of the frame's game logic as C++: a new game (fire on the title screen, or Reset), the step of the game number (Select, at once and every 32 frames while held), the difficulty switch of the player in turn, the sprite colours, the ghost of the frame (three updates unless the game is frozen or paused), the opening animations of that ghost and of Pac-Man, and the six colour values.
      - **Why it matters:** it is the part of the main loop that starts games and moves the ghosts; with it, every routine called from the top logic is now used by native code.
      - **Prerequisites:** F21.21 Done (`bf7de46`); all the routines it calls (CLEAR_SCORES, NEW_GAME_SETUP, SWAP_PLAYERS, GHOST_UPDATE, ANIMATE_OBJECT); F9 (the inputs), F17 (the states), F16/F13.
      - **What Claude Code will do:** `game::topLogic(ram, cart, input, chip)` with `game::ConsoleInput` (fire, Reset, Select, the two difficulty switches); the colour table of the cartridge (`$FF2B`, `$FF36`) is read by address; the adapter builds the input from the machine's console registers and goes on at `$F07F`; the region starts after the instruction that ends the frame (`$F87C`), so the frame comparison is unchanged; contract: stack pointer and arrival address; added to the DONE list and to a new "parts of the main loop" section of the ledger.
      - **What I must do / Exact instructions / Required input:** nothing.
      - **Claude Code prompt:** the resume prompt.
      - **Expected result:** 70 of 70 games identical; the native top logic equal to the original in every frame.
      - **Verification:** `python -I tools\f21_routines.py rom\pacman.a26` says `result: OK` (top logic: 70 of 70 games identical, 439,610 calls compared with the original including the chip writes, 0 different; all thirty routines together identical). Right at the first run.
      - **Completion criteria:** all of the above.
      - **Potential problems:** none.
      - **Recovery:** additions; undo by returning to `bf7de46` (with your OK).
      - **Documentation updates:** `docs\F21_ROUTINES.md`, `DECISIONS.md` (T-F21-22), `CLAUDE.md`, `PROJECT_STATUS.md`, `PLAN.md`.
      - **Git checkpoint:** `Stage F21.22: Rewrite the top logic of the frame`.
      - **Next stage:** F21.23 — Rewrite the bottom logic of the frame (done, below).
    - **F21.23 — Rewrite the bottom logic of the frame (`$F4E7-$F7CE`)** *(expanded and done 2026-10-26)*
      - **Objective:** rewrite the second and largest part of the frame's game logic as C++: nothing on the title screen; the pause after a ghost was eaten; the event timer (scare, freeze, death delay and the change of try or turn, game over); the freeze set-up of the ghost speeds; the collisions (bonus item, power pill, ghost); the joystick, Pac-Man's turns, wafer, the new map, his move and stand-still rules; and the sound at the end.
      - **Why it matters:** this is where the game's rules (F10-F12, F15-F17, F19) are applied; with it, all the logic of a frame is native code except the start of the next frame.
      - **Prerequisites:** F21.22 Done (`fa1b23e`); all the routines it uses (LOCATE_OBJECT, ADD_SCORE, SOUND_*, PLACE_OBJECTS, SWAP_PLAYERS, SET_DIRECTION, MOVE_OBJECT, WRAP_*, PACMAN_KEEP_GOING, NEW_MAZE).
      - **What Claude Code will do:** `game::bottomLogic(ram, cart, input, chip)` with `game::FrameInput` (the joystick port and the two collision flags; the flags come from the oracle until the renderer exists, F23); the tables TBL_FE13 (walls), TBL_FE6C (pill masks), TBL_FE6A, TBL_FE79 (wafer places) are read from the cartridge by address; the adapter reads the joystick port and the flags from the machine and goes on at `$F7D1`; contract: stack pointer and arrival address; added to the DONE list (parts of the main loop).
      - **What I must do / Exact instructions / Required input:** nothing.
      - **Claude Code prompt:** the resume prompt.
      - **Expected result:** 70 of 70 games identical; the native bottom logic equal to the original in every frame.
      - **Verification:** `python -I tools\f21_routines.py rom\pacman.a26` says `result: OK` (bottom logic: 70 of 70 games identical, 439,610 calls compared with the original including the chip writes, 0 different; all thirty-one pieces together identical). Right at the first run.
      - **Completion criteria:** all of the above.
      - **Potential problems:** none; the paths the recordings never take are written from the code (the same ones marked in the listing).
      - **Recovery:** additions; undo by returning to `fa1b23e` (with your OK).
      - **Documentation updates:** `docs\F21_ROUTINES.md`, `DECISIONS.md` (T-F21-23), `CLAUDE.md`, `PROJECT_STATUS.md`, `PLAN.md`.
      - **Git checkpoint:** `Stage F21.23: Rewrite the bottom logic of the frame`.
      - **Next stage:** F21.24 — Rewrite the start of the next frame (done, below).
    - **F21.24 — Rewrite the start of the next frame (`$F7D6-$F87A`)** *(expanded and done 2026-10-26)*
      - **Objective:** rewrite the code that starts every frame, after the overscan wait, as C++: the vertical blank and sync switched on and the collision flags cleared; the picture of Pac-Man and of the ghost sprite chosen; the frame counter, the 256-frame tick, the bonus item's timer and the title-screen shift; the picture set-up values; the colour mask; three colour indices.
      - **Why it matters:** the frame counter is the clock of the game (which ghost, which blink, when the bonus item comes and goes: F15, F18).
      - **Prerequisites:** F21.23 Done (`2f9f924`); F15, F18.
      - **What Claude Code will do:** `game::frameStart(ram, cart, input, chip)` (the sprite picture tables `$FDAB`, `$FDC2`, `$FDFA` by address; the colour / black-and-white switch is a new field of `game::ConsoleInput`); the adapter goes on at `$F87C` with X = 0 (the next instruction writes X to the vertical sync); contract: X, stack pointer and arrival address; added to the DONE list.
      - **What I must do / Exact instructions / Required input:** nothing.
      - **Claude Code prompt:** the resume prompt.
      - **Expected result:** 70 of 70 games identical; the native frame start equal to the original in every frame.
      - **Verification:** `python -I tools\f21_routines.py rom\pacman.a26` says `result: OK` (frame start: 70 of 70 games identical, 439,680 calls compared with the original including the chip writes, 0 different; all thirty-two pieces together identical). Right at the first run.
      - **Completion criteria:** all of the above.
      - **Potential problems:** none.
      - **Recovery:** additions; undo by returning to `2f9f924` (with your OK).
      - **Documentation updates:** `docs\F21_ROUTINES.md`, `DECISIONS.md` (T-F21-24), `CLAUDE.md`, `PROJECT_STATUS.md`, `PLAN.md`.
      - **Git checkpoint:** `Stage F21.24: Rewrite the start of the next frame`.
      - **Next stage:** F21.25 — Rewrite the start-up (done, below).
    - **F21.25 — Rewrite the start-up (`$F000-$F052`)** *(expanded and done 2026-10-26)*
      - **Objective:** rewrite the code that runs when the console is switched on as C++: the whole page `$00-$FF` cleared (the chip registers, written twice over their mirrors, and the 128 memory bytes), game number 6, the bonus item's timer, the title shift and the random number, NEW_GAME_SETUP, the blank pattern of the title-screen score, Pac-Man's first y, the speed counters, and the chip writes that position the bonus item's ball and missile.
      - **Why it matters:** it fixes every starting value of the game (F7, F14, F15, F17); the first frame of every recording starts here.
      - **Prerequisites:** F21.24 Done (`52476e3`); NEW_GAME_SETUP (F21.6).
      - **What Claude Code will do:** `game::startUp(ram, chip)`; the adapter sets the stack pointer to `$FF` and goes on at the frame start; contract: stack pointer and arrival address, free stack bytes ignored; added to the DONE list.
      - **What I must do / Exact instructions / Required input:** nothing.
      - **Claude Code prompt:** the resume prompt.
      - **Expected result:** 70 of 70 games identical from the very first frame.
      - **Verification:** `python -I tools\f21_routines.py rom\pacman.a26` says `result: OK` (start-up: 70 of 70 games identical, 70 calls compared with the original including the 128 + 6 chip writes, 0 different; all thirty-three pieces together identical). Right at the first run.
      - **Completion criteria:** all of the above.
      - **Potential problems:** none.
      - **Recovery:** additions; undo by returning to `52476e3` (with your OK).
      - **Documentation updates:** `docs\F21_ROUTINES.md`, `DECISIONS.md` (T-F21-25), `CLAUDE.md`, `PROJECT_STATUS.md`, `PLAN.md`.
      - **Git checkpoint:** `Stage F21.25: Rewrite the start-up`.
      - **Next stage:** F21.26 — The picture code: what it leaves in memory (done, below).
    - **F21.26 — The picture code: what it leaves in memory** *(expanded and done 2026-10-26)*
      - **Objective:** the drawing code (`$F07F-$F4E6`, with the two timer waits and the KERNEL_LINE routines) is the only original code still running in the tests; find out which memory bytes it writes, and show that nothing the game reads later depends on them, so that the renderer of F23 does not have to reproduce them.
      - **Why it matters:** the finished game has no 6502 and no drawing code of the original; the lockstep comparison must still be exact for everything that is game state.
      - **Prerequisites:** F21.25 Done (`152dc54`): all other pieces native.
      - **What Claude Code will do:** an experiment piece (`picture-skip`, not a rewrite, no call check) that replaces the drawing code by nothing; `lockstep.exe` gets `--ignore <hex list>` and, with `--all`, a list of the bytes that differ in some frame; `tools\f21_routines.py` runs all 70 games with every native piece and the empty picture, collects the bytes that differ, runs again with exactly those bytes ignored, and writes the list into `tools\f21_scratch_bytes.txt` and the ledger.
      - **What I must do / Exact instructions / Required input:** nothing.
      - **Claude Code prompt:** the resume prompt.
      - **Expected result:** a short list of scratch bytes; everything else identical in all 70 games.
      - **Verification:** `python -I tools\f21_routines.py rom\pacman.a26` says `result: OK` (without the drawing code, 11 bytes differ: `$87 CELL_INDEX`, `$C9 CELL_ROW`, `$D2-$D6 TEMP_D2..D6`, `$D8 SPRITE0_Y`, `$DA TEMP_DA`, `$E1 TEMP_E1`, `$E2 TEMP_E2`; with exactly those ignored, 70 of 70 games are identical in every frame).
      - **Completion criteria:** all of the above.
      - **Potential problems:** the experiment shows that no compared byte depended on the scratch bytes in 439,680 frames; it is evidence from the recordings, not a proof for every possible game (F25 repeats it with random-input games).
      - **Recovery:** harness and tool additions; undo by returning to `152dc54` (with your OK).
      - **Documentation updates:** `docs\F21_ROUTINES.md`, `DECISIONS.md` (T-F21-26), `CLAUDE.md`, `PROJECT_STATUS.md`, `PLAN.md`.
      - **Git checkpoint:** `Stage F21.26: What the picture code leaves in memory`.
      - **Next stage:** F21.27 — Put the pieces together (done, below).
    - **F21.27 — Put the pieces together: the whole game as native code** *(expanded and done 2026-10-26)*
      - **Objective:** one native frame function that runs a whole recorded game with no 6502 core and no original program code, and compare it with the golden traces.
      - **Why it matters:** it is the end of F21 and the first time the rebuilt game stands on its own.
      - **Prerequisites:** F21.26 Done (`0f8c65a`).
      - **What Claude Code will do:** `game::Game::runFrame` (`src\game\game.hpp/.cpp`): the first frame is the start-up and the frame start; every later frame is the top logic, the bottom logic (the picture is drawn between them by the renderer of F23, not yet) and the frame start; `lockstep_native.exe` (`src\lockstep\native_main.cpp`) runs a recording with the game only and compares the memory with the golden trace after every frame, leaving out the stack bytes `$F6-$FF` and the 11 scratch bytes of F21.26; `tools\f21_native.py` runs all 70 games, also runs them with nothing left out to show that only those bytes differ, and writes `docs\F21_NATIVE.md`.
      - **What I must do / Exact instructions / Required input:** nothing.
      - **Claude Code prompt:** the resume prompt.
      - **Expected result:** 70 of 70 games identical in every frame.
      - **Verification:** `python -I tools\f21_native.py rom\pacman.a26` says `result: OK` (70 of 70 games, 439,680 frames identical; with nothing left out but the stack bytes exactly the 11 scratch bytes differ; 11.2 seconds of work for all frames).
      - **Completion criteria:** all of the above; F21 is complete.
      - **Potential problems:** the collision flags still come from the oracle; the picture and the sound are not made.
      - **Recovery:** new files only; undo by returning to `0f8c65a` (with your OK).
      - **Documentation updates:** `docs\F21_NATIVE.md`, `DECISIONS.md` (T-F21-27), `CLAUDE.md`, `PROJECT_STATUS.md`, `PLAN.md`.
      - **Git checkpoint:** `Stage F21.27: The whole game as native code`.
      - **Next stage:** F22 — Data loader (reads the maze, shapes, colours and sound tables from the player's ROM at start-up, by address, after checking the fingerprint).
22. **[Done] F22 — Data loader.** Reads maze, shapes, colours and sound tables from the player's ROM at start-up, by address, after checking the fingerprint. Our source contains addresses and logic, not Atari's data. *(expanded and done 2026-10-26 below)*
    - **F22 — Data loader** *(expanded and done 2026-10-26)*
      - **Objective:** a loader that takes the player's own ROM, refuses anything but the exact cartridge, and gives the native game its cartridge bytes by address.
      - **Why it matters:** the finished game must contain no Atari data (Rule 7); this is the one door through which the data comes in, and it checks the key.
      - **Prerequisites:** F21 Done (`a8f04df`).
      - **What Claude Code will do:** `game::CartridgeData` (`src\game\cartridge_data.hpp/.cpp`): `load` / `loadFile` check size 4096 and SHA-1 `0940fea7...`, keep a private copy, give `view()` as the `game::Cartridge`; after a refusal nothing is held. Own SHA-1 (no dependency on the window program). `lockstep_native` now gets its data only through it. Tests `tests\game\test_cartridge_data.cpp`; `tools\f22_loader.py` runs them, the native game, a foreign-file refusal and a scan of `src` for copied cartridge bytes (with a positive control); writes `docs\F22_LOADER.md`.
      - **What I must do / Exact instructions / Required input:** nothing.
      - **Claude Code prompt:** the resume prompt.
      - **Expected result:** right ROM loads; every wrong file refused; native game still 70 of 70 identical; no Atari data in `src`.
      - **Verification:** `python -I tools\f22_loader.py rom\pacman.a26` says `result: OK` (16 loader tests ok; 70 games, 439,680 frames identical through the loader; foreign file refused with exit 2; scan control found, source scan finds none).
      - **Completion criteria:** all of the above.
      - **Potential problems:** the window program still has its own ROM finder (D3) and the emulator; joining them happens at F26.
      - **Recovery:** new files and two small edits; undo by returning to `a8f04df` (with your OK).
      - **Documentation updates:** `docs\F22_LOADER.md`, `docs\GLOSSARY.md`, `DECISIONS.md` (T-F22-1), `CLAUDE.md`, `PROJECT_STATUS.md`, `PLAN.md`.
      - **Git checkpoint:** `Stage F22: Data loader`.
      - **Next stage:** F23 — Native renderer.
23. **[Done] F23 — Native renderer.** Produces each 160-pixel-wide frame from the game state; compared pixel for pixel against the emulator on every frame of every golden trace, flicker included. Split into four sessions (Rule 12, T-F23-1): **F23.1 done 2026-10-26: the video-chip model. F23.2 done 2026-10-26: the drawing code as native code. F23.3 done 2026-10-26: whole games, every picture identical. F23.4 done 2026-10-26: the collision flags from the model replace the oracle. F23 complete.** *(expanded below)*
    - **F23.1 — The video-chip model** *(expanded and done 2026-10-26)*
      - **Objective:** a small model of the video chip that turns timed chip writes into the 160 x 228 picture, proved against the emulator's pictures.
      - **Why it matters:** the renderer has two halves: what the game writes (the drawing code) and how the chip turns writes into pixels. Proving the second half alone, with the emulator's own writes, means any later difference is the first half's fault.
      - **Prerequisites:** F22 Done (`3955176`); F18 (the picture is a function of the 128 bytes at `$F07F`).
      - **What Claude Code will do:** `stella_coverage` gets the option `pic=<file>:<frames>` (saves the emulator picture of chosen frames); `tools\f23_dump.py` saves the emulator's chip writes of 8,266 sampled frames (every 53rd of the 70 games, plus the frame before each) in `reference\f23`; `game::TiaVideo` (`src\game\tia_video.*`); `tests\game\test_tia_video.cpp` feeds the writes to the model and compares each finished picture's fingerprint with the golden trace; `tools\f23_tia.py` runs it with controls and timing-window scans and writes `docs\F23_VIDEO.md`.
      - **What I must do / Exact instructions / Required input:** nothing.
      - **Claude Code prompt:** the resume prompt.
      - **Expected result:** all sampled pictures equal; the controls fail.
      - **Verification:** `python -I tools\f23_tia.py rom\pacman.a26` says `result: OK` (8,266 of 8,266 equal; moving the player, missile or ball position constant by one makes 0, 958 or 7,299 equal).
      - **Completion criteria:** all of the above.
      - **Potential problems:** the exact moment of register writes is not pinned by the game's pictures (windows in the document); model code for modes the game never uses is untested.
      - **Recovery:** new files and a small addition to `tools\stella_probe\coverage.cpp`; undo by returning to `3955176` (with your OK).
      - **Documentation updates:** `docs\F23_VIDEO.md`, `DECISIONS.md` (T-F23-1), `CLAUDE.md`, `PROJECT_STATUS.md`, `PLAN.md`.
      - **Git checkpoint:** `Stage F23.1: The video-chip model`.
      - **Next stage:** F23.2 — The drawing code as native code (done, below).
    - **F23.2 — The drawing code as native code** *(expanded and done 2026-10-26)*
      - **Objective:** C++ that does what the original does between the top logic and the bottom logic (`$F07F-$F4E6`) and gives every write to the video chip with its scanline and colour clock, proved write for write against the emulator.
      - **Why it matters:** with F23.1 (writes to pixels) this makes the picture without the original program; and the 11 scratch bytes that F21.27 had to leave out are now made by our own code.
      - **Prerequisites:** F23.1 Done (`f06e174`).
      - **What Claude Code will do:** `game::drawPicture` (`src\game\kernel.*`) and `game::TimedChip` / `game::ChipWrite`: the sprite and missile positions (the coarse delay loop and fine move worked out as the original does), the maze rows of 20 lines, the bottom wall, the score digits (timed form of SCORE_DIGITS), the life markers; clocks counted from the original instructions' cycles; `game::Game` calls it between the logics (sink optional); `tools\f23_dump.py` also saves the 128 bytes at `$F07F` and `$F4E7` (`r_`, `e_` files); `tests\game\test_kernel.cpp`; `tools\f23_kernel.py` runs the comparison, a control, and the whole native game with nothing left out; writes `docs\F23_KERNEL.md`.
      - **What I must do / Exact instructions / Required input:** nothing.
      - **Claude Code prompt:** the resume prompt.
      - **Expected result:** the write lists and the memory equal the emulator's in every sampled frame; the whole game identical with nothing left out.
      - **Verification:** `python -I tools\f23_kernel.py rom\pacman.a26` says `result: OK` (8,266 of 8,266 write lists equal, 4,528,964 writes; 8,266 of 8,266 memories equal; control with clocks +3: 0 equal; 70 of 70 games, 439,680 frames identical with only the stack bytes left out).
      - **Completion criteria:** all of the above.
      - **Potential problems:** the start line of the drawing code is taken from the emulator in the test; only every 53rd frame is compared write by write.
      - **Recovery:** new files and small edits; undo by returning to `f06e174` (with your OK).
      - **Documentation updates:** `docs\F23_KERNEL.md`, `docs\GLOSSARY.md`, `DECISIONS.md` (T-F23-2), `CLAUDE.md`, `PROJECT_STATUS.md`, `PLAN.md`.
      - **Git checkpoint:** `Stage F23.2: The drawing code as native code`.
      - **Next stage:** F23.3 — Whole games (done, below).
    - **F23.3 — Whole games: drawing code and chip model against every golden picture** *(expanded and done 2026-10-26)*
      - **Objective:** the native game (logic + drawing code + video-chip model; no 6502, no emulator) plays every recorded game and makes a picture in every frame whose fingerprint equals the golden trace's.
      - **Why it matters:** this is the pixel-for-pixel proof of the renderer, flicker included, over 439,680 frames.
      - **Prerequisites:** F23.2 Done (`58fd59d`).
      - **What Claude Code will do:** `game::Renderer` (`src\game\renderer.*`): the chip as the logic parts see it (their few writes placed on fixed scanlines 5, 240, 259 through the new `TiaChip::position`) and the sink of the drawing code, feeding `TiaVideo`; the power-on places of missile 1 and the ball; `lockstep_native --pictures` (fingerprint of each frame's picture against the golden trace; `--save-picture`, `--trace-frame` for looking at differences); `tools\f23_pictures.py` (all 70 games + a control); `docs\F23_PICTURES.md`.
      - **What I must do / Exact instructions / Required input:** nothing.
      - **Claude Code prompt:** the resume prompt.
      - **Expected result:** memory and picture identical in every frame of all 70 games.
      - **Verification:** `python -I tools\f23_pictures.py rom\pacman.a26` says `result: OK` (70 of 70 games, 439,680 frames; control found).
      - **Completion criteria:** all of the above.
      - **Potential problems:** collision flags still from the oracle (F23.4); frame 0 not compared.
      - **Recovery:** new files and small edits; undo by returning to `58fd59d` (with your OK).
      - **Documentation updates:** `docs\F23_PICTURES.md`, `DECISIONS.md` (T-F23-3), `CLAUDE.md`, `PROJECT_STATUS.md`, `PLAN.md`.
      - **Git checkpoint:** `Stage F23.3: Whole games, every picture`.
      - **Next stage:** F23.4 — The collision flags from the model (done, below).
    - **F23.4 — The collision flags from the model** *(expanded and done 2026-10-26)*
      - **Objective:** the game gets its two collision flags (ghost touches Pac-Man; bonus item touches Pac-Man) from the picture the model draws, not from recorded answers.
      - **Why it matters:** it removes the last borrowed input; the native game then depends only on the player's inputs and the cartridge data.
      - **Prerequisites:** F23.3 Done (`f3815be`).
      - **What Claude Code will do:** `TiaVideo` keeps the latches (player 0 - player 1, missile 1 - player 1, ball - player 1; cleared by CXCLR) and can draw what has been written so far (`renderDrawn`, the rendering is now resumable); `TimedChip::collisions`; `Game` asks the picture between the drawing and the bottom logic; `tools\f23_collisions.py` runs all 70 games with no oracle, plus a control; `docs\F23_COLLISIONS.md`.
      - **What I must do / Exact instructions / Required input:** nothing.
      - **Claude Code prompt:** the resume prompt.
      - **Expected result:** memory and pictures identical in all games with no oracle.
      - **Verification:** `python -I tools\f23_collisions.py rom\pacman.a26` says `result: OK` (70 of 70 games, 439,680 frames; the control without flags fails in 52 games).
      - **Completion criteria:** all of the above; F23 is complete.
      - **Potential problems:** the bonus flag is set in only 18 recorded frames, the ghost flag in 667; other situations are covered by the random-input games of F25.
      - **Recovery:** edits and new files; undo by returning to `f3815be` (with your OK).
      - **Documentation updates:** `docs\F23_COLLISIONS.md`, `DECISIONS.md` (T-F23-4), `CLAUDE.md`, `PROJECT_STATUS.md`, `PLAN.md`.
      - **Git checkpoint:** `Stage F23.4: The collision flags from the model`.
      - **Next stage:** F24 — Native sound generator.
24. **[Done] F24 — Native sound generator.** A small model of the TIA's two sound channels; output compared sample by sample with the emulator's. Split in two sessions (Rule 12, T-F24-1): **F24.1 done 2026-10-26: the sound-chip model, exact on all 439,680 frames given the emulator's writes.** F24.2 done 2026-10-26: the timing of the sound writes in the native game; F24 complete. *(expanded below)*
    - **F24.1 — The sound-chip model** *(expanded and done 2026-10-26)*
      - **Objective:** a model of the two sound channels that turns timed writes to the sound registers into the emulator's sound, proved on every frame of every recorded game.
      - **Why it matters:** as for the picture (F23.1), proving the chip half alone with the emulator's own writes means any later difference is the game half's.
      - **Prerequisites:** F23 Done (`30e532a`); F19 (the sound routines).
      - **What Claude Code will do:** `tools\f24_dump.py` saves the emulator's writes to the six sound registers for every frame of the 70 games (`reference\f24`); `game::TiaAudio` (`src\game\tia_audio.*`); `tests\game\test_tia_audio.cpp`; `tools\f24_audio.py` runs it with controls and writes `docs\F24_AUDIO.md`.
      - **What I must do / Exact instructions / Required input:** nothing.
      - **Claude Code prompt:** the resume prompt.
      - **Expected result:** every frame's sound fingerprint equals the golden trace's.
      - **Verification:** `python -I tools\f24_audio.py rom\pacman.a26` says `result: OK` (439,680 of 439,680 frames equal; 124,570 not silent; moving the write time by one clock fails in 16,548 and 49,844 frames).
      - **Completion criteria:** all of the above.
      - **Potential problems:** the native game's own sound writes have no exact timing yet (F24.2).
      - **Recovery:** new files and small edits; undo by returning to `30e532a` (with your OK).
      - **Documentation updates:** `docs\F24_AUDIO.md`, `DECISIONS.md` (T-F24-1), `CLAUDE.md`, `PROJECT_STATUS.md`, `PLAN.md`.
      - **Git checkpoint:** `Stage F24.1: The sound-chip model`.
      - **Next stage:** F24.2 — The timing of the sound writes (done, below).
    - **F24.2 — The timing of the sound writes in the native game** *(expanded and done 2026-10-26)*
      - **Objective:** the native game places its sound writes at the exact moment the original does, so that its sound equals the emulator's sample for sample in every frame.
      - **Why it matters:** the sound chip makes two samples a line, and the bottom logic that writes the sound registers runs a varying number of lines; only exact cycle counts give the right samples.
      - **Prerequisites:** F24.1 Done (`404efc1`), F23 Done.
      - **What Claude Code will do:** `tools\f24_cycles.py` (the listing with the cycles of every instruction); the bottom logic rewritten along the listing with its cycles counted along the path taken (`struct Bottom` and the routines it calls: LOCATE_OBJECT, MOVE_OBJECT, SET_DIRECTION, the TRY routines, PACMAN_KEEP_GOING, ADD_SCORE, SOUND_START, SOUND_EFFECTS, SOUND_MUSIC, NEW_MAZE, PLACE_OBJECTS, SWAP_PLAYERS, WRAP); `drawPicture` returns where the beam ends; `TiaChip::timeBase` / `writeAtCycle`; `game::Renderer` turns the cycle count into scanline and colour clock and feeds the video and sound models; the Select step of the top logic is counted from the end of the sync; `lockstep_native --pictures --sound` compares memory, picture and sound of every frame; `tools\f24_sound.py` runs all 70 games and a control; `docs\F24_SOUND.md`.
      - **What I must do / Exact instructions / Required input:** nothing.
      - **Claude Code prompt:** the resume prompt.
      - **Expected result:** memory, picture and sound identical in every frame of all 70 games.
      - **Verification:** `python -I tools\f24_sound.py rom\pacman.a26` says `result: OK` (70 of 70 games, 439,680 frames identical in memory, picture and sound; control one cycle late: the sound differs in 67 games).
      - **Completion criteria:** all of the above; F24 is complete.
      - **Potential problems:** only the paths of the bottom logic that the 70 recordings run are proved; the random-input games of F25 will run more.
      - **Recovery:** edits and new files; undo by returning to `404efc1` (with your OK).
      - **Documentation updates:** `docs\F24_SOUND.md`, `DECISIONS.md` (T-F24-2), `CLAUDE.md`, `PROJECT_STATUS.md`, `PLAN.md`, `tools\f22_loader.py` (scan threshold).
      - **Git checkpoint:** `Stage F24.2: The timing of the sound writes`.
      - **Next stage:** F25 — Whole-game lockstep.
25. **[Done] F25 — Whole-game lockstep.** All golden traces identical in RAM, picture and sound. Anything not identical is either fixed or entered in `DISCREPANCIES.md` with cause and impact, for your review. *(expanded and done 2026-10-26 below; nothing was left over: both differences found were fixed or belong to the test input, see T-F25-1)*
    - **F25 — Whole-game lockstep** *(expanded and done 2026-10-26)*
      - **Objective:** the finished native game plays 120 new games with seeded random input and the 70 recorded games and equals the emulator in memory, picture and sound after every frame.
      - **Why it matters:** the recordings are only 70 games; random games run paths of the original that no recording runs, and any wrong cycle count, missed rule or timing detail shows up as a difference.
      - **Prerequisites:** F24 Done (`d8091bd`).
      - **What Claude Code will do:** `tools\f25_games.py` makes the input files `reference\f25\R-nnn.inputs` from fixed seeds in five kinds (play, two players, switches, title screen, chaotic) and the golden traces with the program that has the emulator inside; runs `lockstep_native --pictures --sound` over all of them and the 70 recordings; writes `docs\F25_LOCKSTEP.md`. Differences found are explained and fixed (the frame-start line, the input rule of the reference emulator).
      - **What I must do / Exact instructions / Required input:** nothing.
      - **Claude Code prompt:** the resume prompt.
      - **Expected result:** every frame of every game identical.
      - **Verification:** `python -I tools\f25_games.py rom\pacman.a26 --games 120` says `result: OK` (120 new games, 2,400,000 frames, and the 70 recorded games, 439,680 frames: memory, picture and sound identical; the new games had 1,245,463 frames of play, 423,179 with two players, 183,187 with the bonus item showing, 1,543 deaths, up to level 10).
      - **Completion criteria:** all of the above.
      - **Potential problems:** the second player never moves (no right-joystick input in the recordings); random play seldom reaches high levels.
      - **Recovery:** edits and new files; undo by returning to `d8091bd` (with your OK).
      - **Documentation updates:** `docs\F25_LOCKSTEP.md`, `DECISIONS.md` (T-F25-1), `CLAUDE.md`, `PROJECT_STATUS.md`, `PLAN.md`.
      - **Git checkpoint:** `Stage F25: Whole-game lockstep`.
      - **Next stage:** F26 — Gate F: emulator out.
26. **[Done] F26 — Gate F: emulator out.** The game build no longer contains Stella; it lives on only in the test tool. The gate rests on lockstep: every golden trace and every random-input run identical in memory, picture and sound; you may also play a full game, but nothing waits for it. Tag `0.5.0-native`.
    - **F26 — Gate F: emulator out** *(expanded and done 2026-10-26)*
      - **Objective:** the program you play (`pacman2600.exe`) runs on our own game code and contains no Stella; Stella stays only in `pacman2600_ref.exe`, the reference the tests compare with; Gate F is checked and tagged.
      - **Why it matters:** F21 to F25 proved the native game equal to the emulator in a test tool; this stage makes the real program use it, so the original 6502 program and Stella are no longer needed to play.
      - **Prerequisites:** F25 Done (`8167404`).
      - **What Claude Code will do:** `src\host\native_emulator.cpp` (class `Emulator` made of `src\game`), `src\host\ntsc_palette.hpp` (written by `tools\f26_palette.py` from Stella's palette table with Stella's own adjustment), `src\host\CMakeLists.txt` (targets `pacman2600` native and `pacman2600_ref` with Stella); tools that make golden traces use the reference build; checks: the 70 recorded games and 350,460 recorded key frames against the native program, side-by-side with Stella's script runner, pictures colour for colour, no Stella in the binary, window, fullscreen, sound and pad checks, the whole-game lockstep again; `docs\GATE_F.md`; release README and `CLAUDE.md` updated.
      - **What I must do / Exact instructions / Required input:** nothing (an optional look at the game in the window is offered, never required, D-008).
      - **Claude Code prompt:** the resume prompt.
      - **Expected result:** the native program equals the reference in every frame's picture and sound fingerprint and in the memory that is game state.
      - **Verification:** see `docs\GATE_F.md`.
      - **Completion criteria:** every item of `docs\GATE_F.md` PASS; one commit; tag `0.5.0-native` (the tag is local; nothing is pushed).
      - **Potential problems:** the picture colours are now ours (a table computed from Stella's colour values, T-F26-2); the stack and 10 scratch bytes of memory are not kept by the native game, so traces that include them differ by design (T-F26-1).
      - **Recovery:** edits and new files; undo by returning to `8167404` (with your OK).
      - **Documentation updates:** `docs\GATE_F.md`, `DECISIONS.md` (T-F26-1, T-F26-2), `CLAUDE.md`, `PROJECT_STATUS.md`, `PLAN.md`.
      - **Git checkpoint:** `Stage F26: Gate F: emulator out`.
      - **Next stage:** G1 — Choose the module layout from the real game.

#### When a routine is hard to understand

1. Split it into blocks of 5–15 instructions and explain each block separately.
2. Watch it live: breakpoints in Stella's debugger on the golden trace frames where it matters.
3. Experiment: change one RAM byte in the debugger and record what changes on screen.
4. Check public references (Stella docs, 2600 programming guides, community notes), always recording the source.
5. Still unclear after two sessions: write a literal translation marked `// LITERAL: not yet understood`, so the game stays correct, mark it "Reconstructed (literal)" in the coverage map, and come back later. Gate G requires zero literal blocks remaining, or each one approved by you as documented.

### Phase G — Readable source and maintainable architecture (7 stages)

Reshape the verified native code so a stranger can read and mod it, without changing a single frame of output. The lockstep suite runs after every change in this phase; any red result means the change is undone.

1. **[Done] G1 — Choose the module layout from the real game.** Claude Code proposes files based on what Phase F found (likely along the lines of game state, maze, player, ghosts, scoring, game flow, renderer, sound, ROM data, platform). It explains each in one sentence; recorded as T-G1-1 in `DECISIONS.md` (decided by Claude Code, not an owner approval: a technical choice with no effect on the game, D-008; the owner may still ask for changes).
    - **G1 — Choose the module layout from the real game** *(expanded and done 2026-10-26)*
      - **Objective:** decide which file each piece of the working native game will live in, from the code as it is.
      - **Why it matters:** `routines.cpp` (1,329 lines), `kernel.cpp` (646) and `main.cpp` (1,601) are too big to read; G2-G7 need one agreed target.
      - **Prerequisites:** F26 Done, Gate F passed (`eb3155f`), working tree clean.
      - **What Claude Code will do:** measure file sizes, read the structure of `routines.cpp`, `kernel.cpp`, `main.cpp`; write `docs\G1_MODULE_LAYOUT.md`; record T-G1-1. No source code changes.
      - **What I must do:** nothing (you may read the layout and ask for changes).
      - **Exact instructions:** none.
      - **Required input:** none.
      - **Claude Code prompt:** the resume prompt.
      - **Expected result:** a folder tree, a table giving every current function a new home, the order of the moves, and the new `src\host` split.
      - **Verification:** script: all 29 functions declared in `routines.hpp` and `kernel.hpp` appear in the table; quoted line counts equal `wc -l`; `git status` shows only documents changed.
      - **Completion criteria:** the above, one commit.
      - **Potential problems:** the cut lines inside the kernel and the `Bottom` struct are only known when the code is moved (G4); the document says so.
      - **Recovery:** documents only; undo by returning to `eb3155f` (with your OK).
      - **Documentation updates:** `docs\G1_MODULE_LAYOUT.md`, `DECISIONS.md` (T-G1-1), `PROJECT_STATUS.md`, `PLAN.md`, `docs\GLOSSARY.md`.
      - **Git checkpoint:** `Stage G1: Choose the module layout from the real game`.
      - **Next stage:** G2 — Coding and comment conventions.
2. **[Done] G2 — Coding and comment conventions.** `docs\CONVENTIONS.md`: plain names (`ghostLeavesHouse()`, not `sub_F3A2()`), no file over ~400 lines, every function starting with a one-line "what and why", every original-behaviour quirk tagged `// ORIGINAL:` with a link to its evidence (the F-stage document; T-G2-1: there is no `RE_NOTES.md`).
    - **G2 — Coding and comment conventions** *(expanded and done 2026-10-26)*
      - **Objective:** write the rules every later source file follows, and a tool that measures them.
      - **Why it matters:** G3-G7 move and rename code; without written rules each move would be judged by taste.
      - **Prerequisites:** G1 Done (`5eafc5d`), working tree clean.
      - **What Claude Code will do:** write `docs\CONVENTIONS.md` (names, size limits, comment rules, the `ORIGINAL:` tag with a table of nine known quirks, code style, checking) and `tools\g2_conventions_check.py`; record T-G2-1. No source change.
      - **What I must do:** nothing.
      - **Exact instructions:** none.
      - **Required input:** none.
      - **Claude Code prompt:** the resume prompt.
      - **Expected result:** a conventions document that matches how the code is written now, and a checker that reports today's findings.
      - **Verification:** the checker runs and reports (5 files over the limit, 7 long functions, 46 long lines, 0 tags); `--strict` exits 1 today; the tag's evidence pattern tested on three sample lines; every document cited in the quirk table checked by search to say what is claimed (three wrong citations found and corrected).
      - **Completion criteria:** the above, one commit, `git status` shows only documents and one tool.
      - **Potential problems:** the nine quirks are a start; G4 adds more as code is read.
      - **Recovery:** documents and one script; undo by returning to `5eafc5d` (with your OK).
      - **Documentation updates:** `docs\CONVENTIONS.md`, `DECISIONS.md` (T-G2-1), `PROJECT_STATUS.md`, `PLAN.md`, `docs\GLOSSARY.md`.
      - **Git checkpoint:** `Stage G2: Coding and comment conventions`.
      - **Next stage:** G3 — Separate the state from its memory layout.
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

**Later, at the stage that needs it:** approving the tool list (A5), your one-minute play check (C2), the default screen mode (Gate D, done), no play sessions (D-008), any literal-code approvals (Gate G), modern features (Phase I), the legal decision and project name (J1, J2).

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
