# Third-party notices and credits

Pac2600Comp's own code is licensed under the **GNU General Public License, version 2 or later** (see [LICENSE](LICENSE)). This page lists everything in the project that comes from others, who made it, and under which terms. It is a factual list, not legal advice. The rights notice about the game itself is in [README.md](README.md).

## 1. What is in the program and what is not

| Component | Version | In `Pac2600Comp.exe`? | Licence |
| --- | --- | --- | --- |
| Stella (the reference emulator) | 7.0 | **No.** Only the colour values below. Stella itself is in the reference program `pacman2600_ref.exe` (a test tool, never released), in the test tools, and as unchanged source in this repository and in the source zip of a release. | GNU GPL version 2 or any later version |
| The colour table | made from Stella 7.0's | **Yes** (`src/host/ntsc_palette.hpp`: 128 colours) | GNU GPL version 2 or any later version (it comes from Stella's source) |
| SDL3 | 3.4.18 | **Yes** (`SDL3.dll`, built from the unchanged source) | zlib licence |
| CRT-Lottes, CRT-Geom and GDV NTSC 2600 filters | libretro slang-shaders, 2026-10-27 | **Yes**, as ports to HLSL (`crt_lottes.cpp`, `crt_geom.cpp`, `gdv_generated.cpp`, `gdv_luts.cpp`); the original shader files are in `third_party/shaders` and are only read as text | per file, see section 4 |
| Microsoft Visual C++ runtime | from Visual Studio Build Tools 2026 | **In the release zip** (`msvcp140.dll`, `vcruntime140.dll`, `vcruntime140_1.dll`) | Microsoft's terms for redistributable files, shipped next to the program as Microsoft allows |

## 2. Stella

Stella is "An Atari 2600 VCS Emulator", © by its authors:

> Copyright (c) 1995-2024 by Bradford W. Mott, Stephen Anthony and the

> This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation; either version 2 of the License, or any later version.

(from `third_party/stella/Copyright.txt`; the licence text is `third_party/stella/License.txt`, and a copy is in `LICENSE`). The files in `third_party/stella` are byte-identical to Stella's 7.0 release (`docs/DEPENDENCIES.md`). Stella's source tree carries libraries of its own, each with its licence file in place: `src/lib/httplib/LICENSE`, `src/lib/json/LICENSE.MIT`, `src/tools/evdev-joystick/License.txt`, and `debian/copyright`. None of them is part of `Pac2600Comp.exe`.

## 3. SDL3

SDL3 3.4.18 is in `third_party/SDL3`, unchanged, under the zlib licence (`third_party/SDL3/LICENSE.txt`):

```
Copyright (C) 1997-2026 Sam Lantinga <slouken@libsdl.org>

This software is provided 'as-is', without any express or implied
warranty.  In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
```

SDL's source tree also holds notices for parts of its own (`src/hidapi/LICENSE*.txt`, `src/video/yuv2rgb/LICENSE`, and in its `test` folder), each in its place; they stay with the source.

## 4. The CRT filter shaders

The three filters are ports to HLSL of shaders from the **libretro slang-shaders collection** (<https://github.com/libretro/slang-shaders>, `crt/`), downloaded on 2026-10-27 and kept unchanged in `third_party/shaders` (`MANIFEST.txt` has the SHA-256 of each file; this page's table is made by `tools/j5_notices_check.py` from the files themselves and the tool checks those hashes).

- **GDV NTSC 2600** is made from the *crt-guest-advanced* family by **guest(r)**, with good ideas and suggestions from **Dr. Venom**, under the GNU GPL version 2 or later, as each file's header says.
- **CRT-Geom** ("CRT-interlaced") is © 2010-2012 **cgwg, Themaister and DOLLS**, under the GNU GPL version 2 or later. Its header adds: "cgwg gave their consent to have the original version of this shader distributed under the GPL in this message: http://board.byuu.org/viewtopic.php?p=26075#p26075 'Feel free to distribute my shaders under the GPL. After all, the barrel distortion code was taken from the Curvature shader, which is under the GPL.'"
- **CRT-Lottes** is "PUBLIC DOMAIN CRT STYLED SCAN-LINE SHADER by Timothy Lottes", as its header says.

What each of the 26 files says about itself:

<!-- shader-table:begin (made by tools/j5_notices_check.py --write) -->
| File | Credit in the file | Licence the file states |
| --- | --- | --- |
| `crt-geom.slangp` | (a preset: a list of passes) | not stated |
| `crt-guest-advanced-ntsc.slangp` | (a preset: a list of passes) | not stated |
| `crt-lottes.slangp` | (a preset: a list of passes) | not stated |
| `shaders/crt-geom.slang` | Copyright (C) 2010-2012 cgwg, Themaister and DOLLS | GNU GPL version 2 or later |
| `shaders/crt-lottes.slang` | by Timothy Lottes | public domain |
| `shaders/guest/advanced/afterglow0.slang` | Copyright (C) 2020 - 2025 guest(r) | GNU GPL version 2 or later |
| `shaders/guest/advanced/avg-lum-ntsc.slang` | Copyright (C) 2018-2025 guest(r) | GNU GPL version 2 or later |
| `shaders/guest/advanced/bloom_vertical.slang` | Copyright (C) 2020 - 2023 guest(r) | GNU GPL version 2 or later |
| `shaders/guest/advanced/crt-guest-advanced-ntsc-pass1.slang` | Copyright (C) 2018-2025 guest(r) | GNU GPL version 2 or later |
| `shaders/guest/advanced/crt-guest-advanced-ntsc-pass2.slang` | Copyright (C) 2018-2025 guest(r) | GNU GPL version 2 or later |
| `shaders/guest/advanced/custom-fast-sharpen.slang` | Copyright (C) 2005 - 2025 guest(r) | GNU GPL version 2 or later |
| `shaders/guest/advanced/deconvergence-ntsc.slang` | Copyright (C) 2018-2025 guest(r) | GNU GPL version 2 or later |
| `shaders/guest/advanced/gaussian_vertical.slang` | Copyright (C) 2020 - 2025 guest(r) | GNU GPL version 2 or later |
| `shaders/guest/advanced/interlace-ntsc.slang` | Copyright (C) 2020 - 2025 guest(r) | GNU GPL version 2 or later |
| `shaders/guest/advanced/linearize-ntsc-new.slang` | Copyright (C) 2020 - 2025 guest(r) | GNU GPL version 2 or later |
| `shaders/guest/advanced/pre-shaders-afterglow.slang` | Copyright (C) 2019-2025 guest(r) and Dr. Venom | GNU GPL version 2 or later |
| `shaders/guest/advanced/stock.slang` | (no author line) | not stated |
| `shaders/guest/advanced/lut/inv-trinitron-lut.png` | (a picture: no text) | not stated |
| `shaders/guest/advanced/lut/nec-lut.png` | (a picture: no text) | not stated |
| `shaders/guest/advanced/lut/ntsc-lut.png` | (a picture: no text) | not stated |
| `shaders/guest/advanced/lut/trinitron-lut.png` | (a picture: no text) | not stated |
| `shaders/guest/advanced/ntsc/ntsc-pass1.slang` | "based on Themaister's NTSC shader" | not stated |
| `shaders/guest/advanced/ntsc/ntsc-pass2.slang` | "based on Themaister's NTSC shader" | not stated |
| `shaders/guest/advanced/ntsc/ntsc-pass3.slang` | "based on Themaister's NTSC shader" | not stated |
| `shaders/guest/hd/bloom_horizontal.slang` | Copyright (C) 2020 - 2025 guest(r) | GNU GPL version 2 or later |
| `shaders/guest/hd/gaussian_horizontal.slang` | Copyright (C) 2020-2025 guest(r) | GNU GPL version 2 or later |
<!-- shader-table:end -->

**"Not stated"** means exactly that: the file has no licence text, and none was guessed. This applies to the three NTSC passes (`ntsc-pass1.slang` to `ntsc-pass3.slang`: "based on Themaister's NTSC shader", with no licence line), the pass-through `stock.slang`, the four lookup-table pictures and the preset lists. The GDV NTSC 2600 filter uses the NTSC passes and the lookup tables; whoever relies on them should check their upstream source, the libretro collection.

## 5. Other credits

- **Pac-Man** was created by **Toru Iwatani** at **Namco** (the arcade game, 1980). The **Atari 2600 version** was programmed by **Tod Frye** and published by **Atari**. PAC-MAN is a trademark of Bandai Namco Entertainment Inc. This program is not made by or connected to them; it needs the player's own legal copy of the game and does not contain it.
- **Stella** is by Bradford W. Mott, Stephen Anthony and the Stella Team, whose emulator was the reference every proof of this project compares against.
- **SDL** is by Sam Lantinga and the SDL contributors.
- The **shaders** are by guest(r), Dr. Venom, cgwg, Themaister, DOLLS and Timothy Lottes, collected by the libretro project.
- The **achievements** are the RetroAchievements community's set for this game (<https://retroachievements.org/game/13094>). Pac2600Comp does not connect to RetroAchievements and is not connected to it.
- **Pac2600Comp** is by the Pac2600Comp authors.
