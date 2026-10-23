"""Stage C4 report: summarise stella_timing output (DECISIONS.md T-C4-1).

Usage: python -I tools/c4_report.py <timing_output_dir> <attract_frames> <reset_frames>
Reads only; writes summary.txt inside <timing_output_dir>.
"""
import collections
import csv
import pathlib
import re
import sys

out_dir = pathlib.Path(sys.argv[1])
attract, reset_len = int(sys.argv[2]), int(sys.argv[3])
info = dict(l.split('=', 1) for l in (out_dir / 'info.txt').read_text().splitlines() if '=' in l)
rows = list(csv.DictReader(open(out_dir / 'frames.csv', newline='')))
lines = []
say = lines.append

def phase(f):
    return 'attract' if f < attract else ('reset held' if f < attract + reset_len else 'play/after')

cps_stella = int(info['stella_cycles_per_second'])
cpf = collections.Counter(int(r['cycles']) for r in rows)
main_cpf = cpf.most_common(1)[0][0]
NTSC_CLOCK = 1193191.66666667  # Stella 7.0, CartARM.cxx line 59: "NTSC 6507 clock rate"
say(f"Stella settings: {info['stella_lines_per_frame']} lines/frame, {info['stella_cycles_per_frame']} cycles/frame, "
    f"{cps_stella} cycles/s, audio {info['stella_audio_sample_rate']} Hz")
say('measured cycles per frame: ' + ', '.join(f'{c} ({n} frames)' for c, n in sorted(cpf.items())))
say(f'cycles per line: {main_cpf} / 262 = {main_cpf / 262:g}')
say(f'frame rate as Stella plays it: {cps_stella} / {main_cpf} = {cps_stella / main_cpf:.6f} Hz')
say(f'frame rate on Stella\'s NTSC console clock: {NTSC_CLOCK:.2f} / {main_cpf} = {NTSC_CLOCK / main_cpf:.6f} Hz')
say(f'difference: {(cps_stella / main_cpf) / (NTSC_CLOCK / main_cpf) - 1:+.4%} '
    f'(= one frame every {1 / ((cps_stella - NTSC_CLOCK) / main_cpf):.1f} s)')

# Frame structure: the fixed events, and the variable wait starts.
fixed = collections.Counter()
top, bottom = collections.defaultdict(list), collections.defaultdict(list)
no_top, no_bottom = [], []
for r in rows[1:]:
    f, ev = int(r['frame']), r['events']
    fixed[re.sub(r'wait L\d+-L(\d+) \(\d+ reads\)', r'wait ...-L\1', ev)] += 1
    waits = re.findall(r'wait L(\d+)-L(\d+)', ev)
    t = [int(a) for a, b in waits if int(b) == 33]
    b_ = [int(a) for a, b in waits if int(b) == 258]
    (top[phase(f)].append(t[0]) if t else no_top.append(f))
    (bottom[phase(f)].append(b_[0]) if b_ else no_bottom.append(f))

say('\nframe event patterns with wait starts hidden (frames 1-23999):')
for p, n in fixed.most_common():
    say(f'  x{n}: {p}')
say(f'frames without a top wait ending at L33: {len(no_top)}; without a bottom wait ending at L258: {len(no_bottom)}')
for name, d, end in (('top (after VSYNC)', top, 33), ('bottom (after drawing)', bottom, 258)):
    say(f'\nwait start line, {name} area, wait ends L{end}:')
    for p, v in d.items():
        say(f'  {p:11s}: earliest L{min(v)}, latest L{max(v)}, most common L{collections.Counter(v).most_common(1)[0][0]}  ({len(v)} frames)')
    allv = [x for v in d.values() for x in v]
    worst = max(allv)
    say(f'  busiest frame waits from L{worst} -> spare {end - worst} lines ({(end - worst) * 76} cycles); '
        f'quietest from L{min(allv)} -> spare {end - min(allv)} lines')

(out_dir / 'summary.txt').write_text('\n'.join(lines) + '\n', encoding='utf-8')
print('\n'.join(lines))
