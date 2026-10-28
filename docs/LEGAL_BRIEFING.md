# Legal and licensing briefing (Stage J1)

**This is not legal advice.** It is a plain-words list of the facts found in this project, so that you can decide, ideally after asking a lawyer. Claude Code is not a lawyer and cannot give a legal opinion. Where a point is about the law and not about the project, it is marked **(ask a lawyer)**.

Nothing has been published. No repository exists on GitHub. The project is on your PC only.

## 1. What would be published, in one paragraph

A Windows program that plays Pac-Man for the Atari 2600 with the game rules written again in C++ (not the cartridge's code), with the picture and sound made by our own models of the console's chips. The player must own a legal copy of the cartridge file; the program checks its size and fingerprint and refuses to start without it. The cartridge file is never included.

## 2. The five questions

### 2.1 The ROM (the cartridge file)

- It is Atari's copyrighted program. It is **never** distributed: not in Git, not in the zip, not in any test file. This is settled (Rule 7), and the release scan, the `.gitignore` and Gate G check it.
- The program still needs a few tables of numbers that the cartridge holds (the maze, the sound steps, the shapes of the figures): **512 bytes in 34 tables**, written into our source as data (D-009, `docs\EMBEDDED_DATA.md`). The owner approved this on 2026-10-26. Whether a few hundred bytes of game data are protected, and whether the programs' behaviour is, is a question of law. **(ask a lawyer)**
- The fingerprint check is a check only; it does not copy or unlock anything.

### 2.2 The rewritten game code

- `src\game` was written by studying how Atari's program behaves, then writing the same behaviour in C++. Every routine was proved identical to the original frame by frame (2.84 million frames).
- A court might treat code written this way as a *derivative* of the original program, or as an independent expression of the same rules. That is not settled by the fact that it is proved identical; being identical is exactly what a rewrite is meant to be. **(ask a lawyer)**
- EU law (the software directive, which applies in Bulgaria) allows taking a program apart only in narrow cases, mainly to make programs work together. Whether this project fits is a question for a lawyer who knows the directive. **(ask a lawyer)**
- Our notes (`docs\`, `reference\`, the raw listings) name ROM addresses and contain disassembly. They are **not** to be published unless you decide so (the release already leaves `docs\` out, T-D15-1). Stage J3 will sort every file.

### 2.3 The name "Pac-Man"

- "Pac-Man" is a trademark of Bandai Namco. The program's file name (`pacman2600.exe`), the window title and the repository name should not use it. Stage J2 chooses a new name. A name that says what the program does without the mark (for example "plays the Atari 2600 cartridge") is usually safer than a name that sounds like the original. **(ask a lawyer if unsure)**

### 2.4 The code we included from others (this decides the licence of our own code)

| What | Where it is used | Licence | What it requires |
|---|---|---|---|
| **Stella** (the reference emulator) | the test program `pacman2600_ref.exe` and the test tools; **not** in `pacman2600.exe` (Gate G proves it) | GNU GPL, "version 2, **or any later version**" (read in `third_party\stella\Copyright.txt`; the package listings that said "2 only" were wrong for this release) | Anything that contains Stella must be offered with its complete source, under the GPL. |
| **The colour table** | in `pacman2600.exe` (`src\host\ntsc_palette.hpp`), made from Stella's palette (T-F26-2) | comes from Stella's source: GPL v2 or later | The program that contains it is a GPL work. |
| **CRT-Geom and the GDV NTSC filter** | in `pacman2600.exe` (ported to HLSL, I11) | the shaders are "GNU GPL version 2 or later" (cgwg and others; guest(r) and Dr. Venom) | Same: GPL v2 or later. |
| **CRT-Lottes** | in `pacman2600.exe` | public domain (Timothy Lottes) | nothing |
| **SDL3** | `SDL3.dll` | zlib licence | keep the notice; no other duty |
| **Microsoft runtime DLLs** | in the zip, next to the program | Microsoft allows shipping them with a program | keep them as they are |

**What follows from this (a technical reading, not legal advice):** the native program has no emulator in it, but it does contain GPL-licensed material (the colour table, two filters). So **`pacman2600.exe` as it is today is a GPL v2-or-later program**, and anyone who receives it must be able to get its complete source.

- **Our own code can be licensed GPL v2 or later.** This fits every part above, and it is what the shader authors and Stella both allow. This is the simplest choice and the one the README already says.
- **Our own code under another licence** (for example MIT, or "all rights reserved") would not be allowed for a program that contains those GPL parts. To use another licence, the GPL parts must first be taken out (a new colour table not made from Stella's, and without CRT-Geom and GDV), which is possible but is work and loses the filters.
- Because of this, the option "publish the host, keep the game logic private" does **not** work for a program in one `.exe`: if the program contains GPL parts and you hand out the `.exe`, you must give its whole source, game logic included. Keeping the game logic private would mean taking the GPL parts out of the program that is distributed.

### 2.5 Other points

- **Unsigned program:** Windows SmartScreen will say "unknown publisher". That is a nuisance, not a legal issue (J9 explains it in the README).
- **Your name and e-mail** are in Git as the author of every commit. A public repository shows them. Say if you want that changed (it cannot be undone for old commits without rewriting history, which needs your approval).
- **History:** before anything goes public, all past commits are scanned for a ROM (J11).
- **Warranty:** the GPL says the program comes without warranty; the README will say so.

## 3. The options

The plan listed three. With the findings above, here they are in plain words:

| | What is published | Consequences |
|---|---|---|
| **(a) Full source and the program** | everything the program needs, under **GPL v2 or later**, in a repository and in a release zip | Consistent with every licence found. Highest legal exposure on 2.2 (the rewritten game code is public). |
| **(b) Host, tools and docs public; game logic private** | only the parts that are not the rewritten game | Not workable for one `.exe` while GPL parts are inside (see 2.4). It would need the GPL parts removed from the distributed program, or no program distributed at all (source only, without `src\game`, which then cannot be built by others). |
| **(c) Publish nothing** | nothing | No legal exposure. The project stays yours on your PC and on private backups. |

A middle way that the plan already contains (J6): **put the project in a private repository on GitHub first**, with only you able to see it. That is not publishing; nobody else can read it. It gives you an off-site backup and lets Stages J3 to J10 be done and checked. The decision to go public is then made once, at the very end (J11), after you have asked a lawyer. Note that a private repository still means the files are stored on GitHub's servers (sending content to an outside service); you approve each push.

## 4. What Claude Code recommends

1. **Choose now only what is needed to continue:** that our own code will be licensed **GPL v2 or later** if it is ever published, because that is the only choice that fits what is already inside the program.
2. **Continue Phase J with a private repository** (J2 to J10), and leave the final choice between (a) and (c) to J11, after you have talked to a lawyer. Nothing is public until you switch it yourself.
3. Keep `docs\`, `reference\` and the raw listings out of anything shared (they stay local).

If you prefer (c), say so and Phase J stops here: the project is complete and safe on your PC.
