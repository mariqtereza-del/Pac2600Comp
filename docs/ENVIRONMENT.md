# Environment (Stage A2)

Checked on 2026-10-23 by Claude Code, in Windows PowerShell 5.1, as a normal user (no administrator rights).
Every check below was read-only. Nothing was installed, changed or deleted.

Note: this PC uses a Bulgarian number format, so PowerShell prints decimals with a comma (`86,7` = 86.7).

## 1. Hardware and Windows

| Item | Finding | Command that produced it |
| --- | --- | --- |
| Windows | **Windows 11 Pro, version 25H2, build 26200.9711, 64-bit** | `Get-ItemProperty 'HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion' \| Select-Object ProductName, DisplayVersion, CurrentBuild, UBR, EditionID` and `Get-CimInstance Win32_OperatingSystem \| Select-Object Caption, Version, OSArchitecture` |
| Windows (quirk) | The registry's `ProductName` still says "Windows 10 Pro". This is a known Microsoft leftover; `Win32_OperatingSystem` reports "Microsoft Windows 11 Pro" and build 26200 is Windows 11. | same as above |
| Drive F: | **86.7 GB free** of 931.5 GB (844.8 GB used), NTFS, label "Novoto SSD WEEEE" | `Get-PSDrive F` and `Get-Volume -DriveLetter F` |
| GPU | **NVIDIA GeForce RTX 5070**, driver 32.0.16.1692 | `Get-CimInstance Win32_VideoController \| Select-Object Name, DriverVersion, CurrentHorizontalResolution, CurrentVerticalResolution, CurrentRefreshRate` |
| Main display mode | **2560 × 1440 at 180 Hz** | same as GPU (fields `CurrentHorizontalResolution`, `CurrentVerticalResolution`, `CurrentRefreshRate`) |
| Number of displays | **1**, `\\.\DISPLAY1`, primary, 2560×1440 | `Add-Type -AssemblyName System.Windows.Forms; [System.Windows.Forms.Screen]::AllScreens` |
| PS4 controller | **Not connected at the time of the check** (no Sony device, vendor ID 054C, present). Not needed until stage D11. | `Get-PnpDevice -PresentOnly \| Where-Object { $_.FriendlyName -match 'Wireless Controller\|DualShock\|Sony\|PS4' -or $_.InstanceId -match 'VID_054C' }` |

These match what ROADMAP.md already expected (Windows 11 25H2, 2560×1440, 180 Hz).

## 2. Software tools

### Summary

| Tool | On the normal PATH? | Installed anywhere? | Version |
| --- | --- | --- | --- |
| Git (standalone, Git for Windows) | No at A2; **yes since B1** | No at A2; installed in B1 at `C:\Program Files\Git\cmd\git.exe` | 2.56.0.windows.3 (B1) |
| Git (inside MSYS2) | No | Yes, `C:\msys64\usr\bin\git.exe` | 2.55.0 |
| Git (private copy inside GitHub Desktop) | No | Yes, `%LOCALAPPDATA%\GitHubDesktop\app-3.6.7\resources\app\git\cmd\git.exe` | 2.53.0.windows.4 (an older 2.47.3 copy also sits in the old `app-3.5.3` folder) |
| CMake (standalone) | No | **No** | — |
| CMake (inside VS Build Tools 2026) | No (only inside the Developer environment) | No at A2; installed in B2 | 4.3.1-msvc1 (B2) |
| CMake (inside MSYS2) | No | Yes, `C:\msys64\ucrt64\bin\cmake.exe` | 4.4.3 |
| Microsoft C++ compiler (Visual Studio / Build Tools, `cl.exe`) | No (only inside the Developer environment) | No at A2; **installed in B2** (VS Build Tools 2026 18.11.1) | 19.51.36260, toolset MSVC 14.51.36231 (B2) |
| Ninja build tool (inside VS Build Tools 2026) | No (only inside the Developer environment) | No at A2; installed in B2 | 1.13.2 (B2) |
| GCC C++ compiler (inside MSYS2, UCRT64) | No | Yes, `C:\msys64\ucrt64\bin\g++.exe` | 16.2.0 (Rev4, MSYS2) |
| Ninja build tool (inside MSYS2) | No | Yes, `C:\msys64\ucrt64\bin\ninja.exe` | 1.13.2 |
| Python | **Yes** | Yes, `%LOCALAPPDATA%\Programs\Python\Python314\python.exe` | 3.14.0 (64-bit) |
| Stella emulator | No | **No** | — |
| SDL3 library (inside MSYS2) | — | **No** | — |
| SDL2 library (inside MSYS2) | — | Yes (headers at `C:\msys64\ucrt64\include\SDL2`) | not checked (not needed for A2) |
| winget (Windows package tool) | Yes | Yes | v1.29.380 |

Important: none of the MSYS2 tools are on the normal PATH, so typing `git`, `cmake` or `g++` in an ordinary Terminal will say "not recognized". They work when called by their full path (proved below).

### Evidence

**PATH check** — command:

```powershell
foreach ($c in 'git','cmake','cl','python','python3','py','stella') { $r = Get-Command $c -All -ErrorAction SilentlyContinue; if ($r) { $r | ForEach-Object { "{0} -> {1}" -f $c, $_.Source } } else { "$c -> not on PATH" } }
```

Output:

```text
git -> not on PATH
cmake -> not on PATH
cl -> not on PATH
python -> C:\Users\User\AppData\Local\Programs\Python\Python314\python.exe
python -> C:\Users\User\AppData\Local\Microsoft\WindowsApps\python.exe
python3 -> C:\Users\User\AppData\Local\Microsoft\WindowsApps\python3.exe
py -> C:\Windows\py.exe
stella -> not on PATH
```

(The `WindowsApps\python.exe` entries are Microsoft Store shortcuts; the real Python is the first line.)

**Visual Studio / Build Tools** — command:

```powershell
$vw = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"; Test-Path $vw
```

Output: `vswhere exists: False`. `vswhere.exe` is installed by every Visual Studio and Build Tools edition since 2017, so its absence plus the missing folders below means no Microsoft C++ compiler is installed. The many "Microsoft Visual C++ Redistributable" entries in the programs list are runtime libraries for running other programs, not a compiler.

**Standard install folders** — command: `Get-Item` on each path below.

```text
absent  C:\Program Files\Git
absent  C:\Program Files (x86)\Git
absent  C:\Users\User\AppData\Local\Programs\Git
absent  C:\Program Files\CMake
absent  C:\Program Files (x86)\CMake
absent  C:\Program Files\Microsoft Visual Studio
absent  C:\Program Files (x86)\Microsoft Visual Studio
FOUND   C:\Users\User\AppData\Local\Programs\Python
absent  C:\Program Files\Stella
absent  C:\Program Files (x86)\Stella
absent  F:\Ports\Tools
absent  F:\Ports\Tools\Stella
absent  C:\Users\User\AppData\Roaming\Stella
absent  C:\MinGW, C:\mingw64, C:\Program Files\LLVM, C:\BuildTools, chocolatey, scoop
FOUND   C:\msys64
```

**Installed-programs list (registry)** — command:

```powershell
Get-ItemProperty 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\*','HKLM:\SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall\*','HKCU:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\*' | Where-Object { $_.DisplayName -match 'Git|CMake|Visual Studio|Build Tools|Visual C\+\+|Python|Stella|SDL' }
```

Relevant lines: `GitHub Desktop 3.6.7`, `Python 3.14.0 (64-bit) 3.14.150.0`, `Python Launcher 3.14.150.0`, `Microsoft Visual Studio Code (User) 1.143.1` (a text editor, not a compiler), Visual C++ Redistributables 2005–2022 (runtimes only). No Git for Windows, CMake, Visual Studio, Build Tools or Stella entry.

**Python version** — commands: `& 'C:\Users\User\AppData\Local\Programs\Python\Python314\python.exe' --version` and `py --list`

```text
Python 3.14.0
 -V:3.14 *        Python 3.14 (64-bit)
```

**GitHub Desktop's private Git** — command:

```powershell
Get-ChildItem "$env:LOCALAPPDATA\GitHubDesktop" -Recurse -Filter git.exe | Where-Object { $_.FullName -match '\\cmd\\git.exe$' } | ForEach-Object { $_.FullName; & $_.FullName --version }
```

```text
C:\Users\User\AppData\Local\GitHubDesktop\app-3.5.3\resources\app\git\cmd\git.exe
git version 2.47.3.windows.1
C:\Users\User\AppData\Local\GitHubDesktop\app-3.6.7\resources\app\git\cmd\git.exe
git version 2.53.0.windows.4
```

**MSYS2 contents** — command: `Test-Path` on `gcc.exe, g++.exe, clang++.exe, cmake.exe, git.exe, make.exe, ninja.exe, python.exe` in `C:\msys64\usr\bin`, `mingw64\bin`, `ucrt64\bin`, `clang64\bin`:

```text
FOUND C:\msys64\usr\bin\git.exe
FOUND C:\msys64\ucrt64\bin\gcc.exe
FOUND C:\msys64\ucrt64\bin\g++.exe
FOUND C:\msys64\ucrt64\bin\cmake.exe
FOUND C:\msys64\ucrt64\bin\ninja.exe
```

**MSYS2 versions and SDL** — command (PATH change applied only inside that one command):

```powershell
$env:PATH = "C:\msys64\ucrt64\bin;C:\msys64\usr\bin;$env:PATH"; gcc --version; g++ --version; cmake --version; ninja --version; git --version; Test-Path C:\msys64\ucrt64\lib\cmake\SDL3; Test-Path C:\msys64\ucrt64\include\SDL3; Test-Path C:\msys64\ucrt64\include\SDL2
```

```text
gcc.exe (Rev4, Built by MSYS2 project) 16.2.0
g++.exe (Rev4, Built by MSYS2 project) 16.2.0
cmake version 4.4.3
1.13.2
git version 2.55.0
False   (SDL3 CMake files)
False   (SDL3 headers)
True    (SDL2 headers)
```

**Portable copies elsewhere** — command: recursive search (depth 3) of Downloads, Desktop, `F:\Ports`, `C:\Tools`, `F:\Tools`, and all of `F:\Ports\Pac-Man - Arcade`, for `Stella*.exe, cmake.exe, cl.exe, git.exe` (and in the arcade folder also `gcc/g++/clang/mame/dasm/ninja`). Result: **nothing found**. The arcade folder contains only four `.zip` files (`PacArcadeComp-2.0.0-win64.zip`, `PacArcadeComp-github.zip`, `pacman-port_N55.zip`, `pacman.zip`).

**winget** — command: `winget --version` → `v1.29.380`.

### MSYS2 package versions (added in Stage A5, 2026-10-23)

Command (offline, read-only; lists installed packages, downloads nothing):

```powershell
C:\msys64\usr\bin\pacman.exe -Q mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja git mingw-w64-ucrt-x86_64-SDL2 mingw-w64-ucrt-x86_64-sdl3 mingw-w64-ucrt-x86_64-gdb mingw-w64-ucrt-x86_64-make make pacman msys2-runtime
```

```text
mingw-w64-ucrt-x86_64-gcc 16.2.0-4
mingw-w64-ucrt-x86_64-cmake 4.4.3-3
mingw-w64-ucrt-x86_64-ninja 1.13.2-1
git 2.55.0-2
mingw-w64-ucrt-x86_64-SDL2 2.32.10-1
pacman 6.1.0-25
msys2-runtime 3.6.10-5
not installed: mingw-w64-ucrt-x86_64-sdl3, mingw-w64-ucrt-x86_64-gdb, mingw-w64-ucrt-x86_64-make, make
```

181 packages are installed in total. The last full update, from `C:\msys64\var\log\pacman.log`, was `[2026-09-27T11:24:44+0000] starting full system upgrade`.

Per D-001, this project does **not** use MSYS2. These versions are recorded only as the alternative that was considered.

### Git for Windows installed (added in Stage B1, 2026-10-23)

You installed Git for Windows 2.56.0(3), 64-bit, with every default. Checked in a fresh PowerShell 5.1 session started after the install:

```
PS> git --version
git version 2.56.0.windows.3
PS> where.exe git
C:\Program Files\Git\cmd\git.exe
PS> (Get-Command git).Source
C:\Program Files\Git\cmd\git.exe
PS> git config --global user.name
mariqtereza-del
PS> git config --global user.email
the owner's e-mail address
```

- The installer added only `C:\Program Files\Git\cmd` to the machine PATH. MSYS2's Git 2.55.0 is still not on PATH, so it cannot be picked up by mistake.
- The user PATH also holds `%LOCALAPPDATA%\GitHubDesktop\bin`, from GitHub Desktop. It comes after the machine PATH, so `where.exe git` lists only the Git for Windows copy.
- The commit name and email were already in `C:\Users\User\.gitconfig`, set on 2025-10-26 before this project existed. You chose to keep them (DECISIONS.md, D-001 note). That file was not changed.
- Inside Claude Code's Bash tool, which is Git Bash, `git` resolves to `C:\Program Files\Git\ucrt64\bin\git.exe`. That copy belongs to the same Git for Windows install and prints the same version.

### Visual Studio Build Tools 2026 installed (added in Stage B2, 2026-10-23)

You installed Visual Studio Build Tools 2026 with the "Desktop development with C++" workload. Checked in a fresh session after the install.

Product and workload, using Microsoft's `vswhere.exe` (`C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe`):

```
PS> vswhere -all -products * -requires Microsoft.VisualStudio.Workload.VCTools -format text
displayName: Visual Studio Build Tools 2026
installationVersion: 18.11.12315.225
catalog_productDisplayVersion: 18.11.1
installationPath: C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools
isComplete: 1
isRebootRequired: 0
PS> vswhere -products * -requires Microsoft.VisualStudio.Component.VC.CMake.Project -property displayName
Visual Studio Build Tools 2026
PS> vswhere -all -products * -property displayName
Visual Studio Build Tools 2026
```

The tools themselves are checked inside a Developer environment started by a small script, `VsDevCmd.bat -arch=x64 -host_arch=x64 -no_logo` (in `...\BuildTools\Common7\Tools\`):

```
VCToolsVersion=14.51.36231
VSCMD_VER=18.0
> cl
Microsoft (R) C/C++ Optimizing Compiler Version 19.51.36260 for x64
> where cl
C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Tools\MSVC\14.51.36231\bin\Hostx64\x64\cl.exe
> cmake --version
cmake version 4.3.1-msvc1
> where cmake
C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe
> ninja --version
1.13.2
> where ninja
C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe
```

- The install went to `C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools`, the installer's default for Build Tools. The B2 plan had only checked `C:\Program Files\Microsoft Visual Studio`. Both are on C:.
- The compiler is MSVC toolset **14.51** (folder `14.51.36231`, compiler build `19.51.36260`). D-001 asked for 14.51 or newer. The folder number and the compiler's own build number differ slightly, and that is normal for Microsoft's compiler.
- `cl`, `cmake` and `ninja` are **not** on the normal PATH (checked: `Get-Command` finds none of them in an ordinary PowerShell). This is how Microsoft sets them up. Nothing else on the PC changes, and MSYS2's CMake/Ninja are not on PATH either, so the wrong copy can't be picked up.
- `VsDevCmd.bat` prints one harmless line, `'vswhere.exe' is not recognized as an internal or external command`. One of Microsoft's helper scripts looks for `vswhere` on the normal PATH. The environment is still set up correctly, as the `VCToolsVersion` value and the `where` lines show. Scripts in later stages can hide that line.
- Visual Studio's CMake is 4.3.1 (Microsoft's build). MSYS2 has a newer 4.4.3, but D-001 chose the copy that comes with the Build Tools, and 4.3.1 is recent enough for SDL3 and Stella.

### The compiler chain works (added in Stage B3, 2026-10-23)

A 10-line `main.cpp` and a `CMakeLists.txt` asking for C++23 were built in `scratch_b3\` by `build_b3.cmd`. The script asks `vswhere` for the Build Tools path, calls `VsDevCmd.bat -arch=x64 -host_arch=x64`, then runs `cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=cl`, `cmake --build` and the program. The final run, from a clean build folder:

```
-- The CXX compiler identification is MSVC 19.51.36260.0
-- Check for working CXX compiler: C:/Program Files (x86)/Microsoft Visual Studio/18/BuildTools/VC/Tools/MSVC/14.51.36231/bin/Hostx64/x64/cl.exe - skipped
-- Build files have been written to: F:/Ports/Pac-Man - Atari 2600/scratch_b3/build
[3/4] Building CXX object CMakeFiles\hello.dir\main.cpp.obj
[4/4] Linking CXX executable hello.exe
hello
_MSC_FULL_VER: 195136260
_MSVC_LANG (C++ level): 202302
pointer size: 8 bytes (64-bit)
hello.exe exit code: 0
build.ninja: FLAGS = /DWIN32 /D_WINDOWS /EHsc /O2 /Ob2 /DNDEBUG /std:c++23preview -MD
```

- The project folder's path contains spaces and a hyphen, and the build handled it without trouble.
- **C++23 switch finding:** see `DECISIONS.md` T-B3-2.
  - CMake 4.3.1 first chose `/std:c++latest` (level 202400).
  - `/std:c++23` is rejected by this compiler, which falls back to C++14 with only a warning.
  - The project pins `/std:c++23preview` (level 202302). The test of each switch was built directly with `cl` (`/std:c++20` → 202002, `/std:c++23` → warning D9002 and 201402, `/std:c++23preview` → 202302, `/std:c++latest` → 202400).
- **Running programs from `cmd`:** Claude Code's shell has `NoDefaultCurrentDirectoryInExePath=1`, so `cmd` won't run a program in the current folder by its bare name. Scripts must call `.\name.exe` or a full path. (The first try of the switch test failed this way. It wasn't a compiler fault.)

## 3. Not checked

| Item | Reason |
| --- | --- |
| Checks needing administrator rights | None were needed; nothing was skipped for that reason. |
| Contents of the `.a26` ROM files | Forbidden in this stage (ROM work starts at A3). Only the folder listing was read. |
| Inside the four PacArcadeComp `.zip` files | Not needed for A2; unpacking would create files. |
| Whether the MSYS2 compiler actually builds a program | Building is stage B3's job; A2 only checks presence and version. |
| SDL2 exact version | SDL3 is the planned library; SDL2 is not part of the plan. |
| Display scaling (DPI) setting | Not asked for; the mode (2560×1440, 180 Hz) is what matters until D6/D7. |

## 4. Other things noticed (read-only, folder listings only)

- The project folder contains `ROADMAP.md` and `Pac-Man (1981) (Atari).a26` (listing shows 4096 bytes, dated 26.8.1997). Not opened.
- `F:\Ports` itself also contains a second file with the same name, `Pac-Man (1981) (Atari).a26`. Not opened. Stage A3 should only use the copy in the project folder.
