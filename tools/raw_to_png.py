"""Convert stella_probe/stella_script raw pictures (frame_NNNNNN_160xH.raw) to PNG with Stella's NTSC palette.

Usage: python -I tools/raw_to_png.py <folder_with_raw_files> <stella_src_dir>
Writes <folder>/png/<name>.png, each TIA pixel drawn 4 wide x 2 high (for viewing only). Reads nothing else.
"""
import pathlib
import re
import struct
import sys
import zlib

folder, stella_src = pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2])
text = (stella_src / 'common' / 'PaletteHandler.cxx').read_text(encoding='utf-8')
block = re.search(r'PaletteHandler::ourNTSCPalette = \{(.*?)\};', text, re.S).group(1)
palette = [int(v, 0) for v in re.findall(r'0x[0-9a-fA-F]+|\b0\b', block)]
assert len(palette) == 256, len(palette)


def png(path, w, h, rgb_rows):
    raw = b''.join(b'\x00' + r for r in rgb_rows)
    chunk = lambda t, d: struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d) & 0xffffffff)
    path.write_bytes(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0))
                     + chunk(b'IDAT', zlib.compress(raw, 9)) + chunk(b'IEND', b''))


out = folder / 'png'
out.mkdir(exist_ok=True)
count = 0
for raw_path in sorted(folder.glob('frame_*.raw')):
    w, h = map(int, re.search(r'_(\d+)x(\d+)\.raw$', raw_path.name).groups())
    data = raw_path.read_bytes()
    rows = []
    for y in range(h):
        line = b''.join(palette[c].to_bytes(3, 'big') * 4 for c in data[y * w:(y + 1) * w])
        rows += [line, line]
    png(out / (raw_path.stem + '.png'), w * 4, h * 2, rows)
    count += 1
print(f'PNG pictures written: {count} in {out}')
