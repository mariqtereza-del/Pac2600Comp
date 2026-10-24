"""Stage C7: find the reference moments in a stella_script replay and pick one picture frame for each
(DECISIONS.md T-C7-1).

Usage: python -I tools/c7_captures.py <replay_dir> [<dump_lines_file>]
Reads <replay_dir>/frames.csv (a stella_script run whose script has 'region 98 108 76 83', the area under
the ghost pen), prints every moment found and the chosen frames, and optionally writes the matching
'dump' lines for stella_script. Reads only the replay; writes only the optional file.
Working RAM labels (C6/C7, to be confirmed in Phase F):
  $88-$97 wafer map (bits), $98 lives counting the one in play, $B1/$B6 Pac-Man x/y,
  $D2 bit 6 = game paused (death, ghost eaten, level end), $D2 bits 0-3 = direction, $F2 bit 1 = power pill.
"""
import csv
import pathlib
import sys

RESET_AT = 1800
rows = list(csv.DictReader(open(pathlib.Path(sys.argv[1]) / 'frames.csv', newline='')))


def ram(f, a):
    return int(rows[f]['ram'][2 * (a - 0x80):2 * (a - 0x80) + 2], 16)


def wafers(f):
    return sum(bin(ram(f, a)).count('1') for a in range(0x88, 0x98))


def paused(f):
    return bool(ram(f, 0xD2) & 0x40)


def pill(f):
    return bool(ram(f, 0xF2) & 0x02)


def runs(frames, gap=1):
    out, start, prev = [], None, None
    for f in frames:
        if start is None:
            start = prev = f
        elif f - prev > gap:
            out.append((start, prev))
            start = prev = f
        else:
            prev = f
    if start is not None:
        out.append((start, prev))
    return out


play = range(RESET_AT + 10, len(rows))
level_ends = [f for f in play if wafers(f) > wafers(f - 1)]
pause_starts = [f for f in play if paused(f) and not paused(f - 1)]
ghosts = [f for f in pause_starts if pill(f) and f not in level_ends]
deaths = [f for f in pause_starts if not pill(f) and f not in level_ends]
lives_drops = [f for f in play if ram(f, 0x98) < ram(f - 1, 0x98)]
game_over = next((f for f in play if ram(f, 0x98) == 0), None)
pills = runs([f for f in play if pill(f)])
movement = next(f for f in play if (ram(f, 0xB1), ram(f, 0xB6)) != (ram(RESET_AT + 10, 0xB1), ram(RESET_AT + 10, 0xB6)))
bonus = [r for r in runs([f for f in play if set(rows[f]['region_colors'].split(':')) - {'84', 'f6'}], gap=4)
         if r[1] - r[0] >= 60 and (game_over is None or r[0] < game_over)]

print('first movement after Game Reset:', movement)
print('power pill periods ($F2 bit 1):', pills)
print('ghosts eaten (pause during a pill):', ghosts)
print('deaths (other pauses):', deaths, ' lives drop at:', lives_drops)
print('level ends (wafer map refilled):', level_ends)
print('game over (lives 0):', game_over)
print('bonus item shown under the ghost pen:', [(a, b, rows[a]['region_colors']) for a, b in bonus])
print('wafers / lives at the end of the replay:', wafers(len(rows) - 1), ram(len(rows) - 1, 0x98))


def pick_pill_frame(a, b):
    """A frame in the middle part of the period where the blue-ghost colour 7a is drawn."""
    mid = (a + b) // 2
    return next(f for f in range(mid, b) if '7a' in rows[f]['colors'].split(':'))


choice = {'01_title': RESET_AT - 300, '02_game_start': movement - 30}
if pills:
    choice['03_power_pill'] = pick_pill_frame(*pills[0])
if ghosts:
    choice['04_ghost_eaten'] = ghosts[min(1, len(ghosts) - 1)] + 7  # eyes of the eaten ghost visible
if deaths:
    choice['05_death'] = deaths[0] + 60
if bonus:
    choice['06_bonus_item'] = (bonus[0][0] + bonus[0][1]) // 2
if level_ends:
    choice['07_level_end'] = level_ends[0] - 1
if game_over is not None:
    choice['08_game_over'] = game_over + 300
print('chosen frames:', choice)
if len(sys.argv) > 2:
    pathlib.Path(sys.argv[2]).write_text(''.join(f'dump {f} {f} 1  # {name}\n' for name, f in choice.items()))
