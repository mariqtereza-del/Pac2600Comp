r"""Stage F25: new games with seeded random input, and the whole-game lockstep against the emulator.

  python -I tools\f25_games.py <rom> [--make] [--games N] [--frames N]

1. --make (or when the files are missing) writes the input files reference\f25\R-nnn.inputs from fixed seeds (the generator is below; the same seed always gives the
   same game) and lets the host program (pacman2600.exe --headless --replay --trace, the emulator inside) make the golden trace reference\f25\R-nnn.trace of each.
2. Runs build\lockstep\lockstep_native.exe --pictures --sound over every one of these games and over the 70 recorded games: all 128 memory bytes (but the stack),
   the fingerprint of every picture and of every sound must equal the golden trace's.
3. Writes docs\F25_LOCKSTEP.md. The ROM is only read.

The kinds of game (the input bits are those of docs\BASELINE.md: up 1, down 2, left 4, right 8, fire 16, select 32, reset 64, left difficulty A 128, right
difficulty A 256, black and white 512; only the left joystick exists in the recordings):
  play      Game Reset at frame 240, then holds of one direction for 4-40 frames (sometimes none), now and then fire.
  two       Select pressed (one/two players) before the start, then as play: the second player's turns are played with a neutral right joystick.
  switches  as play, with the difficulty and colour switches flipped at random times.
  title     Select pressed and held for different lengths on the title screen (the game number steps every 32 frames), fire and Reset at random times.
  chaotic   every few frames a random combination of all inputs, including opposite directions together.
"""
import concurrent.futures
import pathlib
import re
import subprocess
import sys
import time

ROOT = pathlib.Path(__file__).resolve().parent.parent
OUT = ROOT / 'reference' / 'f25'
HOST = ROOT / 'build' / 'host' / 'pacman2600_ref.exe'
NATIVE = ROOT / 'build' / 'lockstep' / 'lockstep_native.exe'
BS = chr(92)
UP, DOWN, LEFT, RIGHT, FIRE, SELECT, RESET, LDIFF, RDIFF, BW = 1, 2, 4, 8, 16, 32, 64, 128, 256, 512
KINDS = ['play', 'two', 'switches', 'title', 'chaotic']


class Lcg:
    def __init__(self, seed):
        self.s = (seed * 2862933555777941757 + 3037000493) % (1 << 64)

    def next(self, n):
        self.s = (self.s * 6364136223846793005 + 1442695040888963407) % (1 << 64)
        return (self.s >> 33) % n


def press(bits, frame, mask, length=4):
    for f in range(frame, min(len(bits), frame + length)):
        bits[f] |= mask


def play(bits, rng, start, fire_every=0):
    f = start
    n = len(bits)
    while f < n:
        hold = 4 + rng.next(37)
        d = [0, UP, DOWN, LEFT, RIGHT][rng.next(5)] if rng.next(100) < 85 else 0
        for g in range(f, min(n, f + hold)):
            bits[g] |= d
        f += hold
        if fire_every and rng.next(fire_every) == 0:
            press(bits, f, FIRE, 3)


def make_bits(kind, seed, frames):
    rng = Lcg(seed)
    bits = [0] * frames
    if kind == 'play':
        press(bits, 240, RESET)
        play(bits, rng, 370, fire_every=40)
        if rng.next(2):
            press(bits, 3000 + rng.next(3000), RESET)       # a new game in the middle
    elif kind == 'two':
        press(bits, 100 + rng.next(60), SELECT, 3 + rng.next(3))
        if rng.next(2):
            press(bits, 200, SELECT, 3)
        press(bits, 300, FIRE if rng.next(2) else RESET)
        play(bits, rng, 420, fire_every=60)
    elif kind == 'switches':
        press(bits, 240, RESET)
        play(bits, rng, 370)
        for mask in (LDIFF, RDIFF, BW):
            f = 300
            on = False
            while f < frames:
                f += 100 + rng.next(2500)
                on = not on
                for g in range(f, min(frames, f + 50 + rng.next(2000))):
                    bits[g] |= mask if on else 0
    elif kind == 'title':
        f = 30
        while f < frames:
            ln = [3, 3, 8, 40, 70, 150][rng.next(6)]
            press(bits, f, SELECT, ln)
            f += ln + 20 + rng.next(300)
            if rng.next(6) == 0:
                press(bits, f, FIRE if rng.next(2) else RESET, 3 + rng.next(5))
                play(bits, rng, f + 130)
                break
    else:   # chaotic
        f = 60
        while f < frames:
            hold = 1 + rng.next(30)
            mask = 0
            for b in (UP, DOWN, LEFT, RIGHT):
                if rng.next(3) == 0:
                    mask |= b
            if rng.next(12) == 0:
                mask |= FIRE
            if rng.next(60) == 0:
                mask |= [SELECT, RESET, LDIFF, RDIFF, BW][rng.next(5)]
            for g in range(f, min(frames, f + hold)):
                bits[g] |= mask
            f += hold
            if rng.next(500) == 0:
                press(bits, f, RESET, 3)
    return bits


def write_inputs(path, bits, sha1):
    lines = ['PacMan2600-inputs 1', 'rom_sha1=' + sha1, 'frames=%d' % len(bits)]
    last = None
    for f, b in enumerate(bits):
        if b != last:
            lines.append('%d %d' % (f, b))
            last = b
    path.write_text('\n'.join(lines) + '\n', encoding='ascii')


def make_trace(rom, name):
    scratch = OUT / (name + '.scratch.ini')
    r = subprocess.run([str(HOST), '--headless', str(OUT / (name + '.png')), str(rom), '--replay', str(OUT / (name + '.inputs')), '--trace', str(OUT / (name + '.trace')),
                        '--settings', str(scratch)], capture_output=True, text=True)
    return name, r.returncode


def run_native(rom, inputs, trace, pokes=()):
    cmd = [str(NATIVE), str(rom), str(inputs), str(trace), '--pictures', '--sound']
    for p in pokes:
        cmd += ['--poke', p]
    t = time.time()
    r = subprocess.run(cmd, capture_output=True, text=True)
    return r.returncode, r.stdout.strip().split('\n'), time.time() - t


def main():
    rom = pathlib.Path(sys.argv[1]).resolve()
    args = sys.argv[2:]
    count = int(args[args.index('--games') + 1]) if '--games' in args else 40
    frames = int(args[args.index('--frames') + 1]) if '--frames' in args else 20000
    OUT.mkdir(parents=True, exist_ok=True)
    sys.path.insert(0, str(ROOT / 'tools'))
    import f6_coverage as f6  # noqa: E402
    sha1 = '0940fea7f04cdb6d4b90c5ad1a7e344e68f6dbb1'
    names = []
    for i in range(count):
        name = 'R-%03d' % (i + 1)
        kind = KINDS[i % len(KINDS)]
        names.append((name, kind))
        if '--make' in args or not (OUT / (name + '.trace')).exists():
            write_inputs(OUT / (name + '.inputs'), make_bits(kind, 1000 + i, frames), sha1)
    todo = [n for n, k in names if '--make' in args or not (OUT / (n + '.trace')).exists()]
    if todo:
        with concurrent.futures.ThreadPoolExecutor(8) as ex:
            res = list(ex.map(lambda n: make_trace(rom, n), todo))
        bad = [r for r in res if r[1] != 0]
        print('golden traces made for %d games; failures: %s' % (len(res), bad or 'none'))
        if bad:
            return 1

    def one(item):
        name, kind = item
        return name, kind, run_native(rom, OUT / (name + '.inputs'), OUT / (name + '.trace'))

    with concurrent.futures.ThreadPoolExecutor(8) as ex:
        runs = list(ex.map(one, names))
        base = list(ex.map(lambda it: (it[0], run_native(rom, ROOT / 'baseline' / (it[0] + '.inputs'), ROOT / 'reference' / 'baseline' / (it[0] + '.trace'), it[1])), f6.entries()))
    problems = []
    total = 0
    secs = 0.0
    per_kind = {}
    for name, kind, (rc, lines, t) in runs:
        m = re.search(r'IDENTICAL \((\d+) frames\)', lines[-1])
        secs += t
        k = per_kind.setdefault(kind, [0, 0, 0])
        k[0] += 1
        if rc != 0 or not m:
            problems.append('%s (%s): %s' % (name, kind, ' | '.join(lines[:4])))
        else:
            k[1] += 1
            k[2] += int(m.group(1))
            total += int(m.group(1))
    btotal = 0
    for name, (rc, lines, t) in base:
        m = re.search(r'IDENTICAL \((\d+) frames\)', lines[-1])
        secs += t
        if rc != 0 or not m:
            problems.append('%s: %s' % (name, ' | '.join(lines[:4])))
        else:
            btotal += int(m.group(1))
    print('new random-input games: %d of %d identical, %d frames' % (sum(v[1] for v in per_kind.values()), len(runs), total))
    for kind in KINDS:
        if kind in per_kind:
            print('  %-9s %d of %d games, %d frames' % ((kind,) + tuple(per_kind[kind])))
    print('recorded games: %d of %d identical, %d frames; %.1f s of work in all' % (len(base) - sum(1 for n, (rc, l, t) in base if rc != 0), len(base), btotal, secs))
    print('problems:', problems[:6], '...' if len(problems) > 6 else '')
    # what the new games really reached (from the golden traces): frames in play, with two players, with the bonus item, deaths, the highest level
    st = {'frames': 0, 'play': 0, 'two': 0, 'bonus': 0, 'deaths': 0, 'level': 0, 'scared': 0}
    for name, kind in names:
        last_lives = None
        for row in (OUT / (name + '.trace')).read_text().splitlines()[3:]:
            c = row.split(',')
            if len(c) < 6:
                continue
            r = bytes.fromhex(c[5])
            st['frames'] += 1
            if r[0x02] == 0:
                st['play'] += 1
                st['level'] = max(st['level'], r[0x19])
                st['two'] += (r[0x66] & 0x40) != 0
                st['bonus'] += (r[0x67] & 0x02) != 0
                st['scared'] += (r[0x1A] & 0x80) != 0
                if last_lives is not None and r[0x18] < last_lives:
                    st['deaths'] += 1
                last_lives = r[0x18]
            else:
                last_lives = None
    print('what the new games reached:', st)
    ok = not problems
    print('result: %s' % ('OK' if ok else 'MISMATCH'))
    d = ['# Stage F25: whole-game lockstep (2026-10-26)', '',
         'The finished native game (`game::Game` with the drawing code, the video-chip model, the collision flags from the picture and the sound-chip model: no 6502, no emulator, no original code, no recorded answers) plays the same inputs as the emulator and is compared after **every frame** in three ways: all 128 memory bytes (but the stack bytes `$F6-$FF`), the fingerprint of the 228 x 160 picture and the fingerprint of the sound handed over. `python -I tools%sf25_games.py rom%spacman.a26` makes %d new games with seeded random input (the golden traces are made by the program with the emulator inside, `pacman2600 --headless --replay --trace`) and runs them together with the 70 recorded games.' % (BS, BS, len(names)), '',
         '## Result: %s' % ('identical' if ok else 'MISMATCH'), '',
         '- **%d new games, %d frames, and the 70 recorded games, %d frames: memory, picture and sound identical in every frame.**' % (len(runs), total, btotal) if ok else '- MISMATCH: %s' % problems[:3], '']
    d += ['| Kind of game | Games | Frames |', '| --- | --- | --- |'] + ['| %s | %d | %d |' % (k, per_kind[k][1], per_kind[k][2]) for k in KINDS if k in per_kind]
    d += ['', 'Kinds: `play` (Game Reset at frame 240, then random holds of the joystick, now and then fire, sometimes a new game later), `two` (Select before the start: two players, the second player\'s turns with a neutral right joystick), `switches` (difficulty and colour switches flipped at random times), `title` (Select pressed and held for different lengths so that the game number steps, fire and Reset at random times), `chaotic` (every few frames a random combination of all inputs including opposite directions together). The same seeds always give the same games.', '',
          'What the new games reached (counted in the golden traces): %d frames of play (not on the title screen), %d of them with two players, %d with the bonus item showing, %d frames with the ghosts scared, %d deaths, highest level number %d.' % (st['play'], st['two'], st['bonus'], st['scared'], st['deaths'], st['level']), '',
          '## Not claimed', '',
          '- The right joystick and the right fire button are not in the recordings of this project, so the second player of a two-player game never moves.',
          '- Random play seldom reaches the later levels or the bonus item; those are covered by the long recorded games (the bot of E7).',
          '- Time: %.1f s of work for all runs.' % secs]
    (ROOT / 'docs' / 'F25_LOCKSTEP.md').write_text('\n'.join(d) + '\n', encoding='utf-8')
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
