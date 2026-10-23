"""Stage C5 report: find the distinct sounds in stella_sound output (DECISIONS.md T-C5-1).

Usage:
  python -I tools/c5_report.py <sound_output_dir> <stella_src_dir> <attract_frames> <reset_frames>
Reads frames.csv, aud_changes.csv, audio.wav and any frame_*.raw in <sound_output_dir>; writes there:
  summary.txt, sounds.csv, dump_frames.txt (frames whose picture is wanted), clips/*.wav, png/*.png.
The TIA uses only the low bits of each sound register (AUDC 4, AUDF 5, AUDV 4); effective values are used.
"""
import collections
import csv
import pathlib
import re
import struct
import sys
import wave
import zlib

out_dir = pathlib.Path(sys.argv[1])
stella_src = pathlib.Path(sys.argv[2])
attract, reset_len = int(sys.argv[3]), int(sys.argv[4])
GAP = 3      # a channel silent for at most this many frames is still the same sound
MARGIN = 6   # frames of audio added before and after each clip

rows = list(csv.DictReader(open(out_dir / 'frames.csv', newline='')))
eff = lambda r, ch: (int(r[f'audc{ch}']) & 0x0F, int(r[f'audf{ch}']) & 0x1F, int(r[f'audv{ch}']) & 0x0F)

def phase(f):
    return 'attract' if f < attract else ('reset held' if f < attract + reset_len else 'after reset')

# Changes within a frame: does any register change more than once per frame?
per_frame = collections.Counter()
for c in csv.DictReader(open(out_dir / 'aud_changes.csv', newline='')):
    per_frame[(int(c['frame']), c['register'])] += 1
multi = sum(1 for n in per_frame.values() if n > 1)

# Sounds: per channel, runs of frames with effective volume > 0 (gaps up to GAP frames joined).
sounds = []
for ch in (0, 1):
    start = last = None
    for i, r in enumerate(rows):
        if eff(r, ch)[2] > 0:
            if start is None:
                start = i
            elif i - last > GAP + 1:
                sounds.append((ch, start, last))
                start = i
            last = i
    if start is not None:
        sounds.append((ch, start, last))
sounds.sort(key=lambda s: s[1])

def signature(ch, a, b):
    seq, prev = [], None
    for r in rows[a:b + 1]:
        state = eff(r, ch)
        if seq and state == prev:
            seq[-1][1] += 1
        else:
            seq.append([state, 1])
        prev = state
    return tuple((s, n) for s, n in seq)

groups = collections.OrderedDict()
for ch, a, b in sounds:
    groups.setdefault((ch, signature(ch, a, b)), []).append((a, b))

lines = []
say = lines.append
say(f'frames: {len(rows)}; sound register changes: {sum(per_frame.values())}; '
    f'(frame, register) pairs changed more than once in a frame: {multi}')
say(f'frames with any sound (effective volume > 0): {sum(1 for r in rows if eff(r, 0)[2] or eff(r, 1)[2])}')
say(f'sound segments: {len(sounds)}; distinct sounds (channel + identical register sequence): {len(groups)}')

samples = [int(r['audio_offset']) for r in rows]
with wave.open(str(out_dir / 'audio.wav'), 'rb') as w:
    rate, width, nch = w.getframerate(), w.getsampwidth(), w.getnchannels()
    pcm = w.readframes(w.getnframes())
clips = out_dir / 'clips'
clips.mkdir(exist_ok=True)
table, dump = [], set()
for n, ((ch, sig), occ) in enumerate(groups.items(), 1):
    a, b = occ[0]
    frames_long = b - a + 1
    states = ' '.join(f'C{c}F{f}V{v}x{k}' for (c, f, v), k in sig[:12]) + (' ...' if len(sig) > 12 else '')
    starts = [x for x, _ in occ]
    table.append({'sound': n, 'channel': ch, 'occurrences': len(occ), 'frames_long': frames_long,
                  'seconds': f'{frames_long / 60:.2f}', 'first_frame': a,
                  'phases': '/'.join(sorted({phase(x) for x in starts})),
                  'starts': ' '.join(map(str, starts[:20])) + (' ...' if len(starts) > 20 else ''),
                  'states': states})
    dump.update({max(a - 1, 0), a, a + frames_long // 2})
    s0 = samples[max(a - MARGIN, 0)]
    s1 = samples[min(b + MARGIN, len(rows) - 1)]
    with wave.open(str(clips / f'sound_{n:02d}_ch{ch}_frame{a:06d}.wav'), 'wb') as c:
        c.setnchannels(nch); c.setsampwidth(width); c.setframerate(rate)
        c.writeframes(pcm[s0 * nch * width:s1 * nch * width])

with open(out_dir / 'sounds.csv', 'w', newline='') as f:
    w = csv.DictWriter(f, fieldnames=list(table[0]) if table else ['sound'])
    w.writeheader(); w.writerows(table)
say('\nsound  ch  times  frames  seconds  first   phases                starts / register sequence (C=AUDC F=AUDF V=AUDV x frames)')
for t in table:
    say(f"{t['sound']:5d}  {t['channel']:2d}  {t['occurrences']:5d}  {t['frames_long']:6d}  {t['seconds']:>7}  "
        f"{t['first_frame']:6d}  {t['phases']:20s}  {t['starts']}")
    say(f"{'':45s}{t['states']}")
(out_dir / 'dump_frames.txt').write_text('\n'.join(map(str, sorted(dump))) + '\n')

# Pictures (if the tool was re-run with dump_frames.txt): raw -> PNG with Stella's NTSC palette.
text = (stella_src / 'common' / 'PaletteHandler.cxx').read_text(encoding='utf-8')
block = re.search(r'PaletteHandler::ourNTSCPalette = \{(.*?)\};', text, re.S).group(1)
palette = [int(v, 0) for v in re.findall(r'0x[0-9a-fA-F]+|\b0\b', block)]
def png(path, w, h, rgb_rows):
    raw = b''.join(b'\x00' + r for r in rgb_rows)
    chunk = lambda t, d: struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d) & 0xffffffff)
    path.write_bytes(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0))
                     + chunk(b'IDAT', zlib.compress(raw, 9)) + chunk(b'IEND', b''))
png_dir = out_dir / 'png'
png_dir.mkdir(exist_ok=True)
made = 0
for raw_path in sorted(out_dir.glob('frame_*.raw')):
    w, h = map(int, re.search(r'_(\d+)x(\d+)\.raw$', raw_path.name).groups())
    data = raw_path.read_bytes()
    out_rows = []
    for y in range(h):
        line = b''.join(palette[c].to_bytes(3, 'big') * 4 for c in data[y * w:(y + 1) * w])
        out_rows += [line, line]
    png(png_dir / (raw_path.stem + '.png'), w * 4, h * 2, out_rows)
    made += 1
say(f'\nclips written: {len(table)} in {clips}; pictures: {made} in {png_dir}; dump list: {len(dump)} frames')
(out_dir / 'summary.txt').write_text('\n'.join(lines) + '\n', encoding='utf-8')
print('\n'.join(lines))
