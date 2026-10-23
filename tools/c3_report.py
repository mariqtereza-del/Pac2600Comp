"""Stage C3 report: summarise stella_probe output and make viewable pictures (DECISIONS.md T-C3-1).

Usage: python -I tools/c3_report.py <probe_output_dir> <stella_src_dir> <attract_frames> <reset_frames>
Reads only; writes summary.txt and png/ inside <probe_output_dir>.
"""
import collections
import csv
import pathlib
import re
import struct
import sys
import zlib

out_dir = pathlib.Path(sys.argv[1])
stella_src = pathlib.Path(sys.argv[2])
attract, reset_len = int(sys.argv[3]), int(sys.argv[4])

# Stella's "standard" NTSC palette, read from its own source (PaletteHandler.cxx, ourNTSCPalette).
text = (stella_src / 'common' / 'PaletteHandler.cxx').read_text(encoding='utf-8')
block = re.search(r'PaletteHandler::ourNTSCPalette = \{(.*?)\};', text, re.S).group(1)
palette = [int(v, 0) for v in re.findall(r'0x[0-9a-fA-F]+|\b0\b', block)]
assert len(palette) == 256, len(palette)

rows = list(csv.DictReader(open(out_dir / 'frames.csv', newline='')))
lines = []
def say(s=''):
    lines.append(s)

def phase(f):
    if f < attract:
        return 'attract'
    if f < attract + reset_len:
        return 'reset held'
    return 'play'

say(f'frames: {len(rows)}  (attract 0-{attract - 1}, Game Reset held {attract}-{attract + reset_len - 1}, then no input)')
say(f'frames without a new picture: {sum(r["ready"] != "1" for r in rows)}')

say('\nscanlines per frame (all frames):')
for value, n in sorted(collections.Counter(int(r['scanlines']) for r in rows).items()):
    say(f'  {value:4d} lines: {n} frames')
say('runs of equal line count (start frame, length, lines, phase):')
start = 0
for i in range(1, len(rows) + 1):
    if i == len(rows) or rows[i]['scanlines'] != rows[start]['scanlines']:
        say(f'  frame {start:6d}  x{i - start:6d}  {rows[start]["scanlines"]:>4} lines  ({phase(start)})')
        start = i

for key in ('start_line', 'height'):
    say(f'\n{key} values: ' + ', '.join(f'{v} ({n} frames)' for v, n in sorted(collections.Counter(int(r[key]) for r in rows).items())))

drawn = [r for r in rows if int(r['first_row']) >= 0]
say(f'\nframes with any non-black pixel: {len(drawn)}')
for p in ('attract', 'play'):
    sel = [r for r in drawn if phase(int(r['frame'])) == p]
    if sel:
        say(f'  {p}: rows {min(int(r["first_row"]) for r in sel)}-{max(int(r["last_row"]) for r in sel)}'
            f' (of the visible window), columns {min(int(r["first_col"]) for r in sel)}-{max(int(r["last_col"]) for r in sel)}')
say('  most common drawn box (first_row, last_row, first_col, last_col): ' + ', '.join(
    f'{k} x{n}' for k, n in collections.Counter((r['first_row'], r['last_row'], r['first_col'], r['last_col']) for r in drawn).most_common(5)))

say('\ncolours used (TIA value, Stella NTSC RGB, frames it appears in):')
used = collections.Counter(c for r in rows for c in r['colors'].split(':'))
for c, n in sorted(used.items(), key=lambda kv: int(kv[0], 16)):
    say(f'  ${c}  #{palette[int(c, 16)]:06x}  {n}')

say(f'\ndistinct pictures (by fingerprint): {len({r["hash"] for r in rows})}')
(out_dir / 'summary.txt').write_text('\n'.join(lines) + '\n', encoding='utf-8')
print('\n'.join(lines))

# Raw pictures -> PNG, each TIA pixel drawn 4 wide x 2 high (for viewing only).
def png(path, w, h, rgb_rows):
    raw = b''.join(b'\x00' + r for r in rgb_rows)
    chunk = lambda t, d: struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d) & 0xffffffff)
    path.write_bytes(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0))
                     + chunk(b'IDAT', zlib.compress(raw, 9)) + chunk(b'IEND', b''))

png_dir = out_dir / 'png'
png_dir.mkdir(exist_ok=True)
count = 0
for raw_path in sorted(out_dir.glob('frame_*.raw')):
    w, h = map(int, re.search(r'_(\d+)x(\d+)\.raw$', raw_path.name).groups())
    data = raw_path.read_bytes()
    out_rows = []
    for y in range(h):
        line = b''.join(palette[c].to_bytes(3, 'big') * 4 for c in data[y * w:(y + 1) * w])
        out_rows += [line, line]
    png(png_dir / (raw_path.stem + '.png'), w * 4, h * 2, out_rows)
    count += 1
print(f'\nPNG pictures written: {count} in {png_dir}')
