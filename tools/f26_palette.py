r"""Stage F26: writes src\host\ntsc_palette.hpp from Stella's standard NTSC palette (the 128 colours; the table in PaletteHandler.cxx has a 0 after each).

  python -I tools\f26_palette.py
"""
import math
import pathlib
import struct

ROOT = pathlib.Path(__file__).resolve().parent.parent
t = (ROOT / 'third_party' / 'stella' / 'src' / 'common' / 'PaletteHandler.cxx').read_text(encoding='utf-8')
a = t.index('const PaletteArray PaletteHandler::ourNTSCPalette = {')
b = t.index('};', a)
body = t[a + len('const PaletteArray PaletteHandler::ourNTSCPalette = {'):b]
tokens = [x.strip() for x in body.replace('\n', ' ').split(',') if x.strip()]
assert len(tokens) == 256, len(tokens)
assert all(tokens[i] == '0' for i in range(1, 256, 2))
raw = [int(tokens[i], 16) for i in range(0, 256, 2)]


def f32(x):
    return struct.unpack('<f', struct.pack('<f', x))[0]


# The adjustment Stella applies to every palette before the picture is shown (PaletteHandler::adjustedPalette with its default settings: hue 0,
# saturation 0 -> factor 1, contrast 0, brightness 0, gamma 0): the hue/saturation matrix (an identity up to rounding) and a gamma curve of 1.1333
# in float arithmetic like the original code. Without it the colours are darker/lighter than the reference shows (found in Stage F26).
gamma = f32(1.1333)
adjust = [f32(f32(f32(f32(i) * f32(1.0 / 255)) ** gamma) * f32(256.0) + f32(0.5)) for i in range(256)]
S, H = 1.0, 0.0
su = f32(S * math.cos(-H * math.pi))
sw = f32(S * math.sin(-H * math.pi))


def adjusted(pixel):
    r, g, b = (pixel >> 16) & 255, (pixel >> 8) & 255, pixel & 255
    nr = f32(f32(f32(f32(.299) + f32(f32(.701) * su)) + f32(f32(.168) * sw)) * r) + f32(f32(f32(f32(.587) - f32(f32(.587) * su)) + f32(f32(.330) * sw)) * g) + f32(f32(f32(f32(.114) - f32(f32(.114) * su)) - f32(f32(.497) * sw)) * b)
    ng = f32(f32(f32(f32(.299) - f32(f32(.299) * su)) - f32(f32(.328) * sw)) * r) + f32(f32(f32(f32(.587) + f32(f32(.413) * su)) + f32(f32(.035) * sw)) * g) + f32(f32(f32(f32(.114) - f32(f32(.114) * su)) + f32(f32(.292) * sw)) * b)
    nb = f32(f32(f32(f32(.299) - f32(f32(.300) * su)) + f32(f32(1.25) * sw)) * r) + f32(f32(f32(f32(.587) - f32(f32(.588) * su)) - f32(f32(1.05) * sw)) * g) + f32(f32(f32(f32(.114) + f32(f32(.886) * su)) - f32(f32(.203) * sw)) * b)
    out = 0
    for v in (nr, ng, nb):
        v = int(min(max(f32(v), 0.0), 255.0))
        v = min(max(int(adjust[v]), 0), 255)
        out = (out << 8) + v
    return out


pal = [adjusted(v) for v in raw]
lines = ['  ' + ', '.join('0x%06x' % v for v in pal[i:i + 8]) + ',' for i in range(0, 128, 8)]
(ROOT / 'src' / 'host' / 'ntsc_palette.hpp').write_text('''// The 128 colours of the television picture (Stage F26): the colour a TIA colour number (the high seven bits of the byte the game writes to a colour
// register) is shown as. The values are those of the "standard" NTSC palette of Stella 7.0 (third_party/stella/src/common/PaletteHandler.cxx, GPL,
// ourNTSCPalette: the even entries of its 256-entry table) after the adjustment Stella applies to it (gamma 1.1333), the colours the whole project was measured with; they are colours, not Atari data.
// Written by tools/f26_palette.py. 0xRRGGBB.
#pragma once
#include <array>
#include <cstdint>

namespace ntsc {
inline constexpr std::array<std::uint32_t, 128> kPalette = {
''' + '\n'.join(lines) + '''
};
}  // namespace ntsc
''', encoding='utf-8', newline='\n')
print('written', len(pal), 'colours; $84 ->', '%06x' % pal[0x84 >> 1])
