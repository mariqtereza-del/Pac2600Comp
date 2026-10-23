# Glossary

Every project term in one line of plain English. New terms are added in the stage that first uses them.

## The game and the console

| Term | Meaning |
| --- | --- |
| Atari 2600 | Atari's 1977 home games console. This Pac-Man (1982) was written for it. |
| ROM | The game's program and pictures as stored on the cartridge chip; here a 4096-byte file. |
| `.a26` | The usual file extension for an Atari 2600 ROM file. |
| Cartridge / CX2646 | The plug-in game module; CX2646 is Atari's catalogue number for Pac-Man. |
| 6502 (6507) | The processor inside the 2600; it runs the ROM's instructions one by one. |
| Bank switching | A trick bigger cartridges use to fit more than 4 KB; this ROM doesn't need it. |
| Vectors | Fixed addresses at the end of the ROM that tell the processor where to start (here $F000). |
| `$F000` | A memory address written in hexadecimal (base 16), the usual notation for 6502 addresses. |
| TIA | The 2600's video and sound chip. |
| Beam racing | The 2600 has no picture memory, so the program must feed the TV picture line by line while the beam draws it. |
| Sprite (GRP0/GRP1) | A small movable picture, such as Pac-Man or a ghost; GRP0/GRP1 are the TIA settings that hold their shapes. |
| Flicker | Ghosts drawn on alternate frames because the 2600 can't show them all at once; part of the original look. |

## Fingerprints and safety

| Term | Meaning |
| --- | --- |
| Fingerprint / hash | A short code calculated from a file; any change to the file changes it. Used to prove the ROM is untouched. |
| SHA-1, SHA-256, MD5, CRC32 | Four common kinds of fingerprint; the project's main one is SHA-1. |
| No-Intro | A catalogue of verified original game files, used to confirm our ROM is a clean original. |
| Read-only flag | A Windows setting that makes a file refuse changes; set on the original ROM in B4. |
| Working copy | A copy of the ROM (`rom\pacman.a26`) used for analysis so the original is never touched. |
| Checkpoint zip | A full copy of the project in a zip file outside the project folder, made at the end of each phase. |
| Restore test | Unpacking a backup into a fresh folder and proving it is complete and still builds; a backup only counts once this works. |
| `git fsck` | Git's own health check of its history files; "no errors" means the history is undamaged. |
| `.partial` file | A file still being written; only renamed to its real name once it has been checked. |

## Tools

| Term | Meaning |
| --- | --- |
| Emulator | A program that imitates the 2600 so its original ROM runs on a PC. |
| Stella | The best-known, very accurate Atari 2600 emulator; the project's "ruler" for what is correct. |
| Reference emulator / app | The standalone Stella 7.0 program in `F:\Ports\Tools\Stella-7.0c`, used to see and measure the original game. |
| Portable app | A program that runs from its own folder without being installed into Windows. |
| `-baseinappdir` | A Stella start-up option that keeps all its settings inside its own folder (used by `tools\run_stella.cmd`). |
| Digital signature | A maker's electronic seal on a program file; our Stella 7.0c build has none, so we compared its files with the official zip instead. |
| SmartScreen | Windows' warning when starting a program it doesn't recognise. |
| Game Properties | Stella's window (Tab → Game Properties…) showing what it knows and detects about the loaded game. |
| Auto-detect | Stella working out a setting (cartridge type, TV format) from the ROM itself instead of from its database. |
| NTSC / PAL | The US and European TV standards; NTSC draws about 60 pictures per second, PAL about 50. This ROM is NTSC. |
| Phosphor effect | Stella's optional blending of each picture with the previous one, like an old TV's afterglow; it softens flicker. |
| libretro core | A version of an emulator packaged so other programs can embed it; we use Stella's own. |
| SDL3 | A library that opens windows and handles sound and game controllers on the PC. |
| Library | Ready-made code another program uses, instead of writing it from scratch. |
| Compiler | A program that turns source code (text) into a program you can run (`.exe`). |
| MSVC / `cl` | Microsoft's C++ compiler; `cl` is its command. |
| Visual Studio Build Tools | Microsoft's package that installs the compiler and build tools without the full editor. |
| Developer environment | A temporary setup started by `VsDevCmd.bat` in which `cl`, `cmake` and `ninja` can be used. |
| CMake | A tool that reads a project description (`CMakeLists.txt`) and prepares the build. |
| Ninja | A fast tool that runs the actual build steps CMake prepared. |
| C++ / C++23 | The programming language of the port; C++23 is the 2023 edition of it. |
| `/std:c++23preview` | The compiler setting that selects C++23 on this compiler (see DECISIONS T-B3-2). |
| PATH | The list of folders Windows searches when you type a command name. |
| Python | A scripting language used for small analysis helpers (standard library only). |
| PowerShell / cmd | The two Windows command-line shells used to run commands and scripts. |
| MSYS2 | A separate set of Unix-style tools already on the PC; deliberately not used by this project. |
| dasm | An assembler for 6502 code, planned for rebuilding the ROM from source (Phase F). |
| Static / shared library (`.lib` / `.dll`) | Ready-made code either built into the program (static) or kept as a separate file the program loads (shared, a DLL). |
| DLL | "Dynamic-link library": a Windows file of shared code that programs load when they start. |
| Export (`retro_*`) | A function a DLL offers to other programs; Stella's core offers 25 standard libretro ones. |
| Vendoring | Keeping an outside library's source inside our project at a fixed version, instead of downloading it at build time. |
| Source archive (`.tar.gz`, `.tar.xz`) | A compressed bundle of a project's source files, like a zip. |
| Tag | A permanent name for one exact point in a project's history, e.g. Stella's `7.0`. |
| Patch | A small, documented change to someone else's code; ours is applied to a copy at build time (T-B7-4). |
| GPL / zlib licence | Stella's licence (share the source of anything released with it) / SDL's licence (use freely, keep the notice). |
| Assembler | A tool that turns readable 6502 instructions back into a ROM file. |

## Git

| Term | Meaning |
| --- | --- |
| Git | A tool that keeps a full history of the project's files. |
| Repository | The project folder plus Git's hidden `.git` history folder. |
| Commit | One saved, named snapshot of the project you can always return to. |
| Branch / `main` | A line of history; this project uses one, called `main`. |
| `.gitignore` | The list of files Git must never save; it keeps every ROM out. |
| `.gitattributes` | Rules telling Git how to treat files, e.g. line endings and binary files. |
| Line endings (LF / CRLF) | The invisible end-of-line marks, which differ between Unix and Windows. |
| Staged | Files picked for the next commit but not yet committed. |
| Push / GitHub | Uploading the history to GitHub, a website hosting repositories; only in Phase J and only with approval. |

## Reverse engineering and testing

| Term | Meaning |
| --- | --- |
| Disassembly | Turning ROM bytes back into readable 6502 instructions. |
| Reverse engineering | Working out how a program works from the program itself, without its original source. |
| Source reconstruction | Writing readable source code that rebuilds the exact same ROM. |
| Port | The game rebuilt to run natively on another machine (here Windows), behaving like the original. |
| Trace / golden trace | A recording of what the game does frame by frame; a "golden" one is the accepted reference. |
| Lockstep test | Running the emulator and the port side by side with the same inputs and checking they agree. |
| Baseline | The recorded set of the original game's behaviours that the port must match. |
| Frame | One full TV picture; the 2600 draws 60 per second (NTSC). |

## Project words

| Term | Meaning |
| --- | --- |
| Phase / stage | The plan is split into phases A–J, each made of small stages done one per session. |
| Gate | The checklist at the end of a phase that must pass before the next phase starts. |
| D-001, D-002 … | Decisions the owner approved, in `DECISIONS.md`. |
| T-B3-2 etc. | Small technical choices Claude Code made inside a stage, also in `DECISIONS.md`. |
| Resume prompt | The standard message pasted at the start of each session. |
| `CLAUDE.md` | The rules file Claude Code reads automatically every session. |
| Preservation / modernization | Keeping the game exactly as it was / adding new features; kept separate (Rules 2–3). |
