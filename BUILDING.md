# Building and testing

How to build the PC game and its tests from the repository alone. Nothing here needs the original game file, except the last section ("Playing"). Everything below was followed word for word in a fresh copy in Stage H5 (`DECISIONS.md` T-H5-1).

## What you need

- Windows 10 or 11, 64 bit.
- Visual Studio Build Tools with the **"Desktop development with C++"** workload. It brings the MSVC compiler, CMake and Ninja. Only one version has been tried: MSVC 14.51 from Build Tools 2026 (18.11.1); older ones may not know the C++23 switch the project uses. Nothing else is installed or downloaded: SDL3 and Stella are already in `third_party\`.
- Python on the PATH (standard library only; no packages). Only 3.14.0 has been tried. Used by the test scripts.
- Git (to get the files; the tests also ask Git which files are tracked).

CMake and Ninja are only visible inside the Visual Studio "Developer" command window. You do not have to open it yourself: the build scripts below find the Build Tools with `vswhere` and start it for you.

## Build (about 5 to 10 minutes the first time)

Open a Command Prompt or PowerShell in the repository folder (the one that holds this file) and run:

1. `tools\build_lockstep.cmd` — builds the test programs of the game itself (no third-party code). Must end with `BUILD OK`. Output: `build\lockstep\`.
2. `tools\build_host.cmd` — builds SDL3 and Stella and then the game program. Must end with `BUILD OK`. Output: `build\host\pacman2600.exe` (the game, no emulator inside), `build\host\SDL3.dll`, `build\host\pacman2600_ref.exe` (the same program with Stella inside; only used as the reference when making proofs) and the host test programs.

Both scripts write only inside `build\`. A failed build leaves its messages on the screen; fix nothing by hand in `third_party\`.

## Test (about 15 seconds, no game file needed)

`powershell -NoProfile -ExecutionPolicy Bypass -File tools\test_public.ps1`

It must end with `result: OK`. The rule checks ask Git which files belong to the project, so the folder must be a Git repository (a `git clone` is). If you got the files as a plain zip with no `.git` folder, run `git init` and `git add -A` in it first; without a file list the check fails on purpose ("no list of tracked files") rather than pass on nothing. This is the **public test set**: the game's tables, 83 rule checks of the game modules, eleven whole recorded games replayed from the files in `baseline\` and `tests\game\data\` and compared picture by picture and sound by sound, and the rule checks (no ROM in Git, conventions, architecture). `TESTING.md` lists every test and what it needs. The tests that need the original game file (the lockstep proofs and the full set) are explained there too.

## Release zip (needs everything committed)

1. Commit your work (the script refuses a tree with uncommitted changes; `-AllowDirty` makes a test zip).
2. `powershell -NoProfile -ExecutionPolicy Bypass -File tools\make_release.ps1`

It writes `out\Pac2600Comp-<version>-windows.zip` and a report beside it. Before the zip gets its final name it is scanned (also inside the source zip it contains) for anything that could be the game file or an archive of it; a hit leaves a `.partial` file and stops. `make_release.ps1 -TestScan` shows that the scan refuses what it should.
`tools\d16_gate_check.ps1 -Zip <zip> -Work <new folder> -Rom <game file>` unzips the release into a new folder and runs the program through 31 checks.

## Playing

The game file is **not** part of this repository or of any release (Rule 7). To play you need your own legal copy of "Pac-Man (1981) (Atari).a26": exactly 4,096 bytes, SHA-1 `0940fea7f04cdb6d4b90c5ad1a7e344e68f6dbb1`. The program checks both before it starts and refuses any other file. Start `build\host\pacman2600.exe`; the first time it asks for the file. Keys: F1 shows them.

## What is not in the repository

`rom\`, `reference\`, `build\`, `out\`, `recordings\` and every `.a26`, `.bin`, `.rom` and archive file are ignored by Git (`.gitignore`). `reference\` holds the golden traces the proofs compare against and can only be recreated with the original game file and `tools\build_stella_probe.cmd`.

## If a build fails

- `Build Tools not found by vswhere`: the C++ workload is not installed.
- `cmake ... not found`: you started the command by hand outside the Developer window; use the scripts.
- A warning or error about `/std:c++23preview`: your compiler is probably older than the 14.51 one used here (untested).
