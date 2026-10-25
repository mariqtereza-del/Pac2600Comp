r"""Stage F23.1: makes the reference data for the video-chip model.

  python -I tools\f23_dump.py <rom> [stride]      (needs build\stella_probe\stella_coverage.exe and the 70 baseline traces)

For every baseline game, for every `stride`-th frame (default 53) f and also the frame before it (f-1, whose last lines leave a few chip settings
for the next picture), the real emulator's complete list of writes to the video chip is saved in reference\f23\t_BL-nnn.csv
(frame, scanline, colour clock after the instruction, program address, register, value), and in reference\f23\p_BL-nnn.bin the pictures of
the sampled frames f themselves (int32 frame number, then 228 x 160 colour bytes) for the first 6 sampled frames of each game (for looking at
differences). The ROM is only read. Local only (reference\ is ignored by Git).
"""
import concurrent.futures
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / 'tools'))
import f6_coverage as f6  # noqa: E402

EXE = ROOT / 'build' / 'stella_probe' / 'stella_coverage.exe'
OUT = ROOT / 'reference' / 'f23'


def frame_count(entry):
    for line in open(ROOT / 'baseline' / (entry + '.inputs')):
        if line.startswith('frames='):
            return int(line.strip()[7:])
    raise SystemExit('no frames= line in ' + entry)


def dump(item, rom, stride):
    entry, pokes = item[0], item[1]
    n = frame_count(entry)
    sample = list(range(stride, n, stride))
    fl = OUT / ('frames_%s.txt' % entry)
    fl.write_text('\n'.join(str(f) for f in sorted(set(sample) | {f - 1 for f in sample})) + '\n')
    t = OUT / ('t_%s.csv' % entry)
    p = OUT / ('p_%s.bin' % entry)
    if p.exists():
        p.unlink()
    args = [str(EXE), str(rom), str(ROOT / 'baseline' / (entry + '.inputs')), str(OUT / ('c_%s.csv' % entry)),
            str(ROOT / 'reference' / 'baseline' / (entry + '.trace'))] + list(pokes) + [
        'frames=%s' % fl, 'tiaw=%s:0:999999' % t, 'pic=%s:%s' % (p, ','.join(str(f) for f in sample[:6]))]
    r = subprocess.run(args, capture_output=True, text=True)
    # Stage F23.2: the 128 memory bytes when the drawing code starts ($F07F) and when it ends ($F4E7)
    for tag, pc in (('r', '07F'), ('e', '4E7')):
        a2 = [str(EXE), str(rom), str(ROOT / 'baseline' / (entry + '.inputs')), str(OUT / ('c_%s.csv' % entry)),
              str(ROOT / 'reference' / 'baseline' / (entry + '.trace'))] + list(pokes) + ['frames=%s' % fl, 'ramdump=%s:%s' % (OUT / ('%s_%s.csv' % (tag, entry)), pc)]
        r2 = subprocess.run(a2, capture_output=True, text=True)
        if r2.returncode != 0:
            return entry, r2.returncode, (r2.stdout.strip().splitlines() or [r2.stderr.strip()])[-1], len(sample)
    (OUT / ('c_%s.csv' % entry)).unlink()
    return entry, r.returncode, (r.stdout.strip().splitlines() or [r.stderr.strip()])[-1], len(sample)


def main():
    rom = pathlib.Path(sys.argv[1]).resolve()
    stride = int(sys.argv[2]) if len(sys.argv) > 2 else 53
    OUT.mkdir(parents=True, exist_ok=True)
    items = f6.entries()
    with concurrent.futures.ThreadPoolExecutor(8) as ex:
        res = list(ex.map(lambda it: dump(it, rom, stride), items))
    bad = [r for r in res if r[1] != 0]
    print('dumped %d games, %d sampled frames; failures: %s' % (len(res), sum(r[3] for r in res), bad or 'none'))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
