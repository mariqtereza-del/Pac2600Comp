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

## Technical choices made by Claude Code

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
