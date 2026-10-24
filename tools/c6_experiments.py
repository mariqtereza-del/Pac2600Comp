"""Stage C6 experiments: generate input scripts, run stella_script, and compare runs (DECISIONS.md T-C6-1).

Usage: python -I tools/c6_experiments.py <experiment>
  directions   game 1, Game Reset, then one direction held for the whole game (one run per direction)
  turns        left or right until Pac-Man stops, then up or down
  variations   games 1-8 (one player) and game 1 two players, same Reset and joystick route
  switches     game 1 with the same route, comparing left/right difficulty A vs B, fire held, B&W
  midgame      Select and Reset pressed during play
Reads only the ROM; writes scripts and results under reference/c6/<experiment>/ (local only).
"""
import csv
import pathlib
import random
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
ROM = ROOT / 'rom' / 'pacman.a26'
EXE = ROOT / 'build' / 'stella_probe' / 'stella_script.exe'
OUT = ROOT / 'reference' / 'c6'
RESET_AT = 1800       # same moment as C3-C5
FRAMES = 6000


def select_presses(n, start=300):
    """n presses of Game Select from attract mode, 10 frames held, 50 released (as in the select experiment)."""
    return [f'{start + 60 * k} {start + 60 * k + 9} SELECT' for k in range(n)]


def route(seed, start=RESET_AT + 10, end=FRAMES):
    """Pseudo-random joystick route: one direction held 30-119 frames at a time (same idea as T-C5-1)."""
    rng, f, lines = random.Random(seed), start, []
    while f < end:
        n = rng.randrange(30, 120)
        lines.append(f'{f} {min(f + n - 1, end - 1)} {rng.choice(["UP", "DOWN", "LEFT", "RIGHT"])}')
        f += n
    return lines


def run(name, lines, frames=FRAMES, dumps=()):
    d = OUT / name
    d.mkdir(parents=True, exist_ok=True)
    script = d / 'script.txt'
    script.write_text('\n'.join(list(lines) + [f'dump {a} {b} {s}' for a, b, s in dumps]) + '\n', encoding='ascii')
    res = subprocess.run([str(EXE), str(ROM), str(d), str(frames), str(script)], capture_output=True, text=True)
    if res.returncode != 0:
        sys.exit(f'{name}: stella_script failed ({res.returncode}): {res.stderr}')
    return list(csv.DictReader(open(d / 'frames.csv', newline='')))


def first_difference(a, b, key='hash'):
    return next((i for i, (x, y) in enumerate(zip(a, b)) if x[key] != y[key]), None)


def ram(rows, frame, addr):
    return int(rows[frame]['ram'][2 * (addr - 0x80):2 * (addr - 0x80) + 2], 16)


exp = sys.argv[1]
base_dumps = [(RESET_AT + 60, RESET_AT + 600, 60)]
if exp == 'directions':
    runs = {d: run(f'directions/{d.lower()}', [f'{RESET_AT} {RESET_AT + 9} RESET', f'{RESET_AT + 10} {FRAMES - 1} {d}'],
                   dumps=base_dumps) for d in ('UP', 'DOWN', 'LEFT', 'RIGHT')}
    runs['NONE'] = run('directions/none', [f'{RESET_AT} {RESET_AT + 9} RESET'], dumps=base_dumps)
    print('first frame differing from the no-input run:', {k: first_difference(v, runs['NONE']) for k, v in runs.items()})
    # RAM bytes whose value differs between the four direction runs 120 frames after the first difference
    f0 = min(first_difference(v, runs['NONE']) or FRAMES for k, v in runs.items() if k != 'NONE')
    for addr in range(0x80, 0x100):
        series = {k: [ram(v, f, addr) for f in range(f0 - 2, f0 + 120, 8)] for k, v in runs.items()}
        if len({tuple(s) for s in series.values()}) > 2:
            print(f'${addr:02X}', {k: s[:10] for k, s in series.items()})
elif exp == 'turns':
    # Left or right until Pac-Man stops (as seen in 'directions'), then up or down.
    runs = {}
    for first in ('LEFT', 'RIGHT'):
        for second in ('UP', 'DOWN'):
            runs[f'{first}+{second}'] = run(f'turns/{first.lower()}_{second.lower()}',
                                            [f'{RESET_AT} {RESET_AT + 9} RESET', f'{RESET_AT + 10} 1999 {first}',
                                             f'2000 {FRAMES - 1} {second}'], dumps=[(1990, 2150, 40)])
    for k, v in runs.items():
        print(k, 'B1 (x?) at 1990..2150:', [ram(v, f, 0xB1) for f in range(1990, 2151, 20)],
              ' ED (joystick):', [ram(v, f, 0xED) for f in (1990, 2010)])
    ref = runs['LEFT+UP']
    for addr in range(0x80, 0x100):
        s = {k: [ram(v, f, addr) for f in range(1990, 2151, 20)] for k, v in runs.items()}
        if s['LEFT+UP'] != s['LEFT+DOWN'] and len(set(s['LEFT+UP'])) > 2:
            print(f'${addr:02X} differs between LEFT+UP and LEFT+DOWN:', s)
elif exp == 'variations':
    rt = route(seed=7)
    runs = {}
    for game in range(1, 9):
        presses = 2 * (game - 1)                      # power-on = game 1, one player (C6 select experiment)
        runs[f'game{game}'] = run(f'variations/game{game}', select_presses(presses) + [f'{RESET_AT} {RESET_AT + 9} RESET'] + rt,
                                  dumps=base_dumps)
    runs['game1_2p'] = run('variations/game1_2p', select_presses(1) + [f'{RESET_AT} {RESET_AT + 9} RESET'] + rt, dumps=base_dumps)
    ref = runs['game1']
    for k, v in runs.items():
        print(f'{k:9s} first picture difference vs game1: {first_difference(v, ref)}; '
              f'game start status bar same as game1: {v[RESET_AT + 60]["status_hash"] == ref[RESET_AT + 60]["status_hash"]}')
        diff = [f'${a:02X}={ram(v, RESET_AT + 30, a):02x}/{ram(ref, RESET_AT + 30, a):02x}' for a in range(0x80, 0x100)
                if ram(v, RESET_AT + 30, a) != ram(ref, RESET_AT + 30, a)]
        print(f'          RAM differing from game1 at frame {RESET_AT + 30}: {" ".join(diff[:24])}')
elif exp == 'variations2':
    # Clean per-variation tests: (V1) LEFT held from the start, (V2) no input; pictures at game start.
    games = [(g, 2 * (g - 1)) for g in range(1, 9)] + [('1_2p', 1)]
    for g, presses in games:
        pre = select_presses(presses) + [f'{RESET_AT} {RESET_AT + 9} RESET']
        left = run(f'variations2/game{g}_left', pre + [f'{RESET_AT + 10} {FRAMES - 1} LEFT'], dumps=[(1860, 1860, 1)])
        idle = run(f'variations2/game{g}_idle', pre, dumps=[(1860, 1860, 1), (2400, 5400, 600)])
        xs = [(f, ram(left, f, 0xB1)) for f in range(RESET_AT + 10, FRAMES)]
        start = next(f for f, x in xs if x != xs[0][1])
        stop = next((f for f, x in xs if f > start and x == 64), None)
        moves = [f for f in range(start, (stop or start) + 1) if ram(left, f, 0xB1) != ram(left, f - 1, 0xB1)]
        st = [f for f in range(RESET_AT + 120, FRAMES) if idle[f]['status_hash'] != idle[f - 1]['status_hash']]
        print(f'game {str(g):4s}: LEFT run: x {xs[0][1]} -> 64 from frame {start} to {stop} ({(stop - start) if stop else "-"} frames, '
              f'{len(moves)} position changes); idle run status-bar changes: {st[:12]}')
elif exp == 'switches':
    rt = route(seed=7)
    common = [f'{RESET_AT} {RESET_AT + 9} RESET'] + rt
    runs = {'default (B/B, colour)': run('switches/default', common, dumps=base_dumps),
            'left A': run('switches/left_a', ['100 100 LDIFF_A'] + common, dumps=base_dumps),
            'right A': run('switches/right_a', ['100 100 RDIFF_A'] + common, dumps=base_dumps),
            'both A': run('switches/both_a', ['100 100 LDIFF_A', '100 100 RDIFF_A'] + common, dumps=base_dumps),
            'fire held': run('switches/fire', common + [f'{RESET_AT + 10} {FRAMES - 1} FIRE'], dumps=base_dumps),
            'B&W': run('switches/bw', ['100 100 BW'] + common, dumps=base_dumps)}
    ref = runs['default (B/B, colour)']
    for k, v in runs.items():
        sw = sorted({(r['left_diff'], r['right_diff'], r['tv']) for r in v[200:]})
        print(f'{k:22s} switches after frame 200: {sw}; first picture difference vs default: {first_difference(v, ref)}; '
              f'first RAM difference: {first_difference(v, ref, "ram")}')
elif exp == 'more':
    # Right difficulty in a two-player game; fire pressed in attract mode (no Game Reset).
    rt = route(seed=7)
    two = select_presses(1) + [f'{RESET_AT} {RESET_AT + 9} RESET'] + rt
    p2 = run('more/two_players_default', two, frames=12000)
    p2r = run('more/two_players_right_a', ['100 100 RDIFF_A'] + two, frames=12000)
    p2l = run('more/two_players_left_a', ['100 100 LDIFF_A'] + two, frames=12000)
    print('2 players, right A vs default: first picture difference', first_difference(p2r, p2),
          '; first RAM difference', first_difference(p2r, p2, 'ram'))
    print('2 players, left A vs default:  first picture difference', first_difference(p2l, p2),
          '; first RAM difference', first_difference(p2l, p2, 'ram'))
    att = run('more/fire_in_attract', ['1000 1009 FIRE'], frames=3000, dumps=[(990, 1290, 60)])
    idle = run('more/idle_reference', [], frames=3000, dumps=[(990, 1290, 60)])
    d = first_difference(att, idle)
    print('fire pressed at frame 1000 in attract mode: first picture difference vs no input:', d,
          '; colours then:', att[d]['colors'] if d is not None else '-')
elif exp == 'midgame':
    rt = route(seed=7)
    common = [f'{RESET_AT} {RESET_AT + 9} RESET'] + rt
    ref = run('midgame/reference', common, dumps=base_dumps)
    sel = run('midgame/select', common + ['2500 2509 SELECT'], dumps=[(2490, 2700, 30)])
    rst = run('midgame/reset', common + ['2500 2509 RESET'], dumps=[(2490, 2700, 30)])
    for k, v in (('select at 2500', sel), ('reset at 2500', rst)):
        print(f'{k}: first picture difference vs reference: {first_difference(v, ref)}; '
              f'status bar at 2520 same: {v[2520]["status_hash"] == ref[2520]["status_hash"]}')
else:
    sys.exit(__doc__)
