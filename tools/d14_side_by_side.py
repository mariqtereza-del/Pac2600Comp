"""Stage D14: the same input, one run in our program and one in Stella's core; are the pictures the same?

For each input (a recording made by the program: <name>.inputs, or a Phase C script: <name>.txt):
  1. The input is turned into a Stella input script and into a .inputs recording (whichever is missing).
  2. The reference runs it: build\\stella_probe\\stella_script.exe (the Stella 7.0 core, Phase C tool), which writes the
     picture fingerprint of every frame and, at the chosen moments, the whole picture.
  3. Our program runs the recording with no window (--headless --replay), and optionally in the real window (--window).
  4. Every frame's fingerprint is compared, and at 10 moments the whole picture (160 x H palette numbers) byte by byte.

Usage (from the project folder):
  python -I tools\\d14_side_by_side.py <rom> <scratch folder> [--window] [--moments a,b,c,...] [--frames N] <input> [<input> ...]
Prints one block per input and a total; exit code 0 only if there are no differences anywhere.
"""
import csv
import os
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
REF = ROOT / 'build' / 'stella_probe' / 'stella_script.exe'
OURS = ROOT / 'build' / 'host' / 'pacman2600.exe'
SHA1 = '0940fea7f04cdb6d4b90c5ad1a7e344e68f6dbb1'

HELD = [('UP', 1), ('DOWN', 2), ('LEFT', 4), ('RIGHT', 8), ('FIRE', 16), ('SELECT', 32), ('RESET', 64)]
# a switch's two positions: (name when the bit is set, name when it is clear, bit)
SWITCHES = [('LDIFF_A', 'LDIFF_B', 128), ('RDIFF_A', 'RDIFF_B', 256), ('BW', 'COLOR', 512)]


def read_inputs(path):
    lines = [l.rstrip('\r') for l in open(path, encoding='utf-8') if l.strip() and not l.startswith('#')]
    frames = int(lines[2].split('=')[1])
    changes = [tuple(int(v) for v in l.split()) for l in lines[3:]]
    bits = []
    for f in range(frames):
        while len(changes) > 1 and changes[1][0] <= f:
            changes.pop(0)
        bits.append(changes[0][1] if changes else 0)
    return bits


def read_script(path):
    """A Phase C script: holds of UP... and switches that stay where they were set (default B, B, Colour)."""
    holds, last = [], 0
    for line in open(path, encoding='utf-8'):
        line = line.split('#')[0].split()
        if len(line) == 3 and line[0].isdigit():
            holds.append((int(line[0]), int(line[1]), line[2]))
            last = max(last, int(line[1]))
    bits, state = [], 0
    names = dict((n, b) for n, b in HELD)
    for f in range(last + 1):
        cur = 0
        for a, b, name in holds:
            if a <= f <= b:
                if name in names:
                    cur |= names[name]
                for on, off, bit in SWITCHES:
                    if name == on:
                        state |= bit
                    if name == off:
                        state &= ~bit
        bits.append(cur | state)
    return bits


def write_inputs(path, bits):
    out = ['PacMan2600-inputs 1', 'rom_sha1=' + SHA1, 'frames=%d' % len(bits)]
    prev = None
    for f, b in enumerate(bits):
        if b != prev:
            out.append('%d %d' % (f, b))
            prev = b
    pathlib.Path(path).write_text('\n'.join(out) + '\n')


def bits_to_script(bits, moments):
    out = []
    names = [(n, b, True) for n, b in HELD] + [(on, b, True) for on, off, b in SWITCHES] + [(off, b, False) for on, off, b in SWITCHES]
    for name, bit, wanted in names:
        start = None
        for f in range(len(bits) + 1):
            hit = f < len(bits) and bool(bits[f] & bit) == wanted
            if hit and start is None:
                start = f
            if not hit and start is not None:
                out.append('%d %d %s' % (start, f - 1, name))
                start = None
    for m in moments:
        out.append('dump %d %d 1' % (m, m))
    return '\n'.join(out) + '\n'


def hashes(path):
    return [r['hash'] for r in csv.DictReader(open(path, newline=''))]


def run(args, timeout):
    r = subprocess.run([str(a) for a in args], capture_output=True, text=True, timeout=timeout)
    return r.returncode


def main():
    args = sys.argv[1:]
    use_window = '--window' in args
    args = [a for a in args if a != '--window']
    moments = None
    if '--moments' in args:
        i = args.index('--moments')
        moments = [int(v) for v in args[i + 1].split(',')]
        del args[i:i + 2]
    pad = 0
    if '--frames' in args:  # run on to this many frames with no keys pressed (the switches stay where they were)
        i = args.index('--frames')
        pad = int(args[i + 1])
        del args[i:i + 2]
    rom, scratch, inputs = args[0], pathlib.Path(args[1]), args[2:]
    scratch.mkdir(parents=True, exist_ok=True)
    total_frames = total_diff = total_pics = total_pic_diff = 0
    for item in inputs:
        item = pathlib.Path(item)
        name = item.stem
        bits = read_inputs(item) if item.suffix == '.inputs' else read_script(item)
        while len(bits) < pad:
            bits.append(bits[-1] & ~127 if bits else 0)
        n = len(bits)
        n = len(bits)
        picked = moments or [i * (n - 1) // 9 for i in range(10)]
        picked = [m for m in picked if m < n]
        d = scratch / name
        d.mkdir(exist_ok=True)
        rec = d / 'input.inputs'
        write_inputs(rec, bits)
        (d / 'input_script.txt').write_text(bits_to_script(bits, picked))
        ref = d / 'ref'
        ref.mkdir(exist_ok=True)
        if run([REF, rom, ref, n, d / 'input_script.txt'], 900) != 0:
            print(name, 'the reference run failed'); return 1
        ours = d / 'ours.csv'
        ini = d / 'scratch.ini'
        if run([OURS, '--headless', d / 'h.png', rom, '--replay', rec, '--fingerprints', ours, '--settings', ini], 900) != 0:
            print(name, 'our headless run failed'); return 1
        a, b = hashes(ref / 'frames.csv'), hashes(ours)
        diff = sum(1 for x, y in zip(a, b) if x != y) + abs(len(a) - len(b))
        pics = pic_diff = 0
        lines = []
        for m in picked:
            frames = m + 1 if m + 1 != 600 else 601  # 600 is the program's "not given" value for --frames
            raw = d / ('ours_%06d.raw' % m)
            if run([OURS, '--headless', d / 'p.png', rom, '--replay', rec, '--frames', frames, '--save-frame', m, '--raw', raw, '--settings', ini], 900) != 0:
                print(name, 'our picture run failed at', m); return 1
            refs = [p for p in ref.iterdir() if p.name.startswith('frame_%06d_' % m)]
            same = len(refs) == 1 and refs[0].read_bytes() == raw.read_bytes()
            pics += 1
            pic_diff += 0 if same else 1
            lines.append('  frame %6d: picture %s' % (m, 'identical' if same else 'DIFFERENT'))
        win = ''
        if use_window:
            wcsv = d / 'window.csv'
            if run([OURS, '--replay', rec, '--fingerprints', wcsv, '--timed', n / 60.0 + 2, d / 'w.txt', rom, '--mute', '--settings', d / 'w.ini'], 900 + n) != 0:
                print(name, 'our window run failed'); return 1
            w = hashes(wcsv)
            wd = sum(1 for x, y in zip(a, w) if x != y) + max(0, len(a) - len(w))
            win = '  window replay: %d frames compared, %d differences' % (min(len(a), len(w)), wd)
            diff += wd
        print('%s: %d frames, reference vs our headless replay: %d differences' % (name, n, diff if not use_window else diff - wd))
        if win:
            print(win)
        print('\n'.join(lines))
        total_frames += n
        total_diff += diff
        total_pics += pics
        total_pic_diff += pic_diff
    print('TOTAL %d frames, %d fingerprint differences; %d pictures compared, %d different' % (total_frames, total_diff, total_pics, total_pic_diff))
    return 0 if total_diff == 0 and total_pic_diff == 0 and total_frames > 0 else 1


if __name__ == '__main__':
    sys.exit(main())
