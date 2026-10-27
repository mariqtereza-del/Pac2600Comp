# Dependencies (Stage B7, 2026-10-23)

Every outside library the project uses, pinned to one exact version, with where it came from and how to check it. Adding or upgrading a library needs the owner's approval (Rule 5) and an update here.

| Library | Version | Folder | Licence | Used for |
| --- | --- | --- | --- | --- |
| SDL3 | **3.4.18** (tag `release-3.4.18`, 2026-10-02) | `third_party\SDL3\` | zlib (`LICENSE.txt`) | Window, sound, controllers (Phase D host) |
| Stella | **7.0** (tag `7.0`, commit `d55b1aec0d06`, 2024-10-04), D-002 | `third_party\stella\` | GNU GPL version 2 (`License.txt`) | Its libretro core: the 2600 emulator embedded in the Phase 1 program, and the same version as the reference app (C1) |
| libretro slang-shaders (files only) | master of 2026-10-27, 26 files | `third_party\shaders\libretro\crt\` | per file: `crt-guest-advanced` family GNU GPL version 2 or later (guest(r), Dr. Venom); `crt-geom` GNU GPL version 2 or later (cgwg and others); `crt-lottes` public domain (Timothy Lottes); the NTSC passes say "based on Themaister's NTSC shader" and the lookup-table PNGs carry no licence text in the files (not stated: not invented) | CRT filters (Stage I11, D-017): the source of the shaders that are ported to HLSL; read as text, never run |

## Where they came from and how they were checked

### SDL3 3.4.18

- **Download:** `https://github.com/libsdl-org/SDL/releases/download/release-3.4.18/SDL3-3.4.18.tar.gz`, from SDL's official GitHub release page.
- **Size:** 15,651,004 bytes.
- **SHA-256:** `9c75cf16330322c217dedd2e0609f1124f1b54b8633e763467b4684d0f4334a3`. This is identical to the value GitHub publishes for the release asset.
- **Unpacked:** 2181 files. Two were left out: SDL's `CLAUDE.md` and `AGENTS.md`, notes for AI coding tools working on SDL itself (T-B7-1). Neither is needed to build.

### Stella 7.0

- **Download:** `https://github.com/stella-emu/stella/releases/download/7.0/stella-7.0-src.tar.xz`, from Stella's official GitHub release page.
- **Size:** 13,685,208 bytes.
- **SHA-256:** `d3fdd7a4eca4831f41b9f4ca39589502d01c612aee010395c27a088029adc0bc`. Stella publishes no checksum, so this is our own record.
- **Cross-check instead:** each of the 1057 unpacked files was compared with Git's file fingerprint in Stella's official history at tag 7.0 (commit `d55b1aec0d067a4c901a6dcdf81cb8f579685659`). Result: 1057 identical, 0 different, 0 extra, 0 missing.
- **Left out:** `test/roms/`, 459 files (49.4 MB). These are Stella's test ROMs, i.e. other games' ROM images, which this project must never store or publish (Rules 1 and 7, T-B7-1). After unpacking, `third_party\` contained 0 files with ROM-like extensions.
- **Versions:** the Windows app is published as "7.0c". Tags 7.0a/b/c differ from 7.0 only in `src/os/windows/Create_Builds.bat` and `src/os/windows/stella.iss` (installer scripts), so the emulation code is identical.

The downloaded archives are kept locally in `third_party\_downloads\`, which is not in Git, so the unpacking can be repeated and checked.

## How they are built

- **Build file:** `third_party\CMakeLists.txt`. It builds both libraries on its own (`tools\build_third_party.cmd`, output in `build\third_party\`), and Phase D will include it from the main project.
- **SDL3:** built by its own CMake files as `SDL3.dll` (shared). SDL's tests and examples are switched off, and so is its install step.
- **Stella libretro core (`stella_libretro.dll`):**
  - **Sources:** the 148 source files are read directly from Stella's `src/os/libretro/Makefile.common`, so the list can't drift.
  - **Settings:** copied from Stella's own Makefile for Microsoft's compiler (`windows_msvc2017_desktop`): `/std:c++20`, `/O2`, `NDEBUG`, `/MD`, `__LIB_RETRO__`, `SOUND_SUPPORT`, `HAVE_STDINT_H`, `__WIN32__` and `WINAPI_FAMILY=WINAPI_FAMILY_DESKTOP_APP`.
  - **C++ level:** Stella 7.0 asks for C++20 (not C++23), and `/std:c++20` works on this compiler.
- **`stella_core` (added in C3, T-C3-1):** the same Stella sources and settings as a static library, without `libretro.cxx`, for project tools such as `tools\stella_probe`.
- **One build-time fix (T-B7-4):** the vendored files are not edited.
  - Stella's `libretro.cxx` turns `snprintf` into the old `_snprintf` for every Microsoft compiler. On today's compiler that breaks the JSON library Stella bundles.
  - The build compiles a corrected copy (in `build\third_party\stella_patched\`) that limits this to compilers older than 2015, the same condition SDL's bundled zlib uses.
  - If a future Stella version changes that line, the build stops with a message rather than guessing.

### B7 build result (2026-10-23)

- Clean build in 62 s: 570 steps, 0 errors, 0 compiler warnings.
- `stella_libretro.dll`: 2,971,136 bytes, x64. It exports the 25 standard libretro functions (`retro_api_version` … `retro_unserialize`).
- `SDL3.dll`: 2,747,392 bytes, x64, file version 3.4.18.0.

## In Git

The unpacked sources are committed (T-B7-2), so the project builds without internet and the exact code is part of its history.

- Line-ending conversion is switched off for `third_party/SDL3/**` and `third_party/stella/**` in `.gitattributes`, so the files stay byte-identical to the releases.
- Both projects ship their own `.gitignore` files, which hid 8 genuine source files: 7 SDL shader files (`src/render/gpu/shaders/*.hlsl`) and Stella's `.vscode/settings.json`. Those 8 were added explicitly, so the committed copy is complete.

## Licence notes (details settled in Phase J, J1/J5)

- **SDL3 (zlib):** may be used in any program. The licence asks only that its notice isn't removed from the source and that changed versions are marked.
- **Stella (GPL v2):** any program we release that contains Stella's code (the Phase 1 emulator build, the test tool) must also make its complete source available under the GPL. The native game build planned for later phases will not contain Stella.
- **"v2 only" or "v2 or later"?** Package listings disagree. `License.txt` is the plain GPL v2 text, and individual files may say more. J1 reads the file headers to settle it.

### libretro slang-shaders, 26 files (Stage I11, D-017)

- **Approved by the owner on 2026-10-27** ("1" to the question whether the shader sources may be downloaded). Source: `https://raw.githubusercontent.com/libretro/slang-shaders/master/crt/` (the libretro project's collection on GitHub), the three presets (`crt-guest-advanced-ntsc.slangp`, `crt-lottes.slangp`, `crt-geom.slangp`) and every shader, include and lookup table they name.
- **What they are:** text files in the slang shader language (Vulkan GLSL with `#pragma parameter` lines) and four small PNG lookup tables. They are data, not instructions: they are only read as source text; they are not compiled or run as they are. The program runs HLSL versions written from them (D-017, T-I11-1).
- **Record:** `third_party\shaders\MANIFEST.txt` lists every file with its SHA-256 as downloaded. No installer, no executable and no library was downloaded or installed; the shader compiler used is `d3dcompiler` (part of Windows) and `dxc.exe` of the Windows SDK, already on the computer.
- **Licences:** see the table; the project's own licence is GNU GPL, which takes in GPL 2 or later and public domain files. Where a file states no licence it is recorded as not stated.
