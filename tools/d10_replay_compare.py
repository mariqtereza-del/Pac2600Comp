"""Stage D10: do the keys do what the recorded Phase C inputs did?

For every Phase C experiment that has a script (reference\\c6\\*\\*\\script.txt) and for the random joystick play
reference\\audio\\play1 (its per-frame input is turned into a script), this runs
    pacman2600.exe --keys-replay <script> <csv> <rom>
which presses the keys in the real keyboard code, and compares the picture fingerprint of every frame with the one the
Phase C tool recorded. Prints one line per experiment and a total; exit code 0 only if every compared frame is equal.

Usage: python -I tools\\d10_replay_compare.py <rom> [<scratch folder>]   (run from the project folder)
Experiments that use Colour/B&W (Stage D12) are reported as skipped, not as passed.
"""
import csv
import os
import subprocess
import sys

EXE = os.path.join('build', 'host', 'pacman2600.exe')


def read_hashes(path, column):
    with open(path, newline='') as f:
        rows = list(csv.reader(f))
    head = rows[0]
    idx = head.index(column)
    return [r[idx] for r in rows[1:]]


def replay(script, out_csv, rom, settings, frames):
    subprocess.run([EXE, '--keys-replay', script, out_csv, rom, '--frames', str(frames), '--settings', settings],
                   check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    return read_hashes(out_csv, 'hash')


def main():
    rom = sys.argv[1]
    scratch = sys.argv[2] if len(sys.argv) > 2 else os.path.join('build', 'host', 'test_tmp')
    os.makedirs(scratch, exist_ok=True)
    settings = os.path.join(scratch, 'd10_replay.ini')
    total = 0
    differing = 0
    skipped = []
    results = []

    # Phase C6 experiments
    base = os.path.join('reference', 'c6')
    for group in sorted(os.listdir(base)):
        gdir = os.path.join(base, group)
        if not os.path.isdir(gdir) or group in ('scripts', 'switches_pics'):
            continue
        for exp in sorted(os.listdir(gdir)):
            edir = os.path.join(gdir, exp)
            script = os.path.join(edir, 'script.txt')
            frames_csv = os.path.join(edir, 'frames.csv')
            if not (os.path.isfile(script) and os.path.isfile(frames_csv)):
                continue
            text = open(script).read()
            if ' COLOR' in text or ' BW' in text:
                skipped.append(group + '/' + exp)
                continue
            want = read_hashes(frames_csv, 'hash')
            got = replay(script, os.path.join(scratch, 'd10_replay.csv'), rom, settings, len(want))
            diff = sum(1 for a, b in zip(want, got) if a != b) + abs(len(want) - len(got))
            total += len(want)
            differing += diff
            results.append((group + '/' + exp, len(want), diff))

    # Random joystick play, 6000 frames of seed 1 (Phase C5): the per-frame input is in frames.csv
    play = os.path.join('reference', 'audio', 'play1')
    if os.path.isfile(os.path.join(play, 'frames.csv')):
        inputs = read_hashes(os.path.join(play, 'frames.csv'), 'input')
        want = read_hashes(os.path.join(play, 'frames.csv'), 'hash')
        n = 6000
        names = {'U': 'UP', 'D': 'DOWN', 'L': 'LEFT', 'R': 'RIGHT'}
        script = os.path.join(scratch, 'd10_play1_script.txt')
        with open(script, 'w') as f:
            f.write('1800 1809 RESET\n')  # Game Reset, as the Phase C tool pressed it
            for i in range(n):
                if inputs[i] in names:
                    f.write('%d %d %s\n' % (i, i, names[inputs[i]]))
        got = replay(script, os.path.join(scratch, 'd10_replay.csv'), rom, settings, n)
        diff = sum(1 for a, b in zip(want[:n], got) if a != b)
        total += n
        differing += diff
        results.append(('audio/play1 (random joystick play)', n, diff))

    for name, n, diff in results:
        print('%-40s %6d frames  %d differences' % (name, n, diff))
    for name in skipped:
        print('%-40s skipped (uses Colour/B&W, Stage D12)' % name)
    print('TOTAL %d frames compared, %d differences' % (total, differing))
    return 0 if differing == 0 and total > 0 else 1


if __name__ == '__main__':
    sys.exit(main())
