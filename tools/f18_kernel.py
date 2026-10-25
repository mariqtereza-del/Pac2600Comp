"""Stage F18: the drawing kernel and the flicker schedule.

  python -I tools\\f18_kernel.py <rom>      (after f1-f17; needs the 70 baseline traces, the flag logs reference\\f12 made by f12_collisions.py,
                                             the built stella_coverage.exe and build\\host\\pacman2600.exe)

The picture of a frame is drawn by the code from $F07F to the start of the bottom logic ($F4E7). Its only inputs are the 128 memory bytes at
$F07F and the ROM. `tools\\f18_cpu.py` runs that code (the original bytes) over those 128 bytes and lists every write to the TIA chip with the
scanline and colour clock. This tool checks that list against the real emulator and then reads the schedule out of it:

1. TIA writes: in 8,2xx sampled frames of all 70 games every write (scanline, clock, address, register, value) equals the emulator's.
2. Flicker: the ghost drawn in a frame is (FRAME_COUNTER & 3) + 1, in every frame of the check (all frames where the ghost flag is read).
3. Collisions: the ghost flag of F12 is predicted from the two sprites' bitmaps and pixel columns in every one of those frames.
4. Pixels: the sprite pixel columns equal those of the emulator's picture in a few pictures.
5. The line map of the picture (fixed scanline structure) and the row template.
Writes docs\\F18_KERNEL.md. The ROM is only read.
"""
import collections
import concurrent.futures
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / 'tools'))
import f18_cpu as cpu  # noqa: E402
import f6_coverage as f6  # noqa: E402
import f12_collisions as f12  # noqa: E402

BS = chr(92)
OUT = ROOT / 'reference' / 'f18'
EXE = ROOT / 'build' / 'stella_probe' / 'stella_coverage.exe'
HOST = ROOT / 'build' / 'host' / 'pacman2600.exe'
START, STOP = 0xF07F, 0xF4E7
NAMES = {0x01: 'VBLANK', 0x04: 'NUSIZ0', 0x05: 'NUSIZ1', 0x06: 'COLUP0', 0x07: 'COLUP1', 0x08: 'COLUPF', 0x09: 'COLUBK', 0x0A: 'CTRLPF',
         0x0B: 'REFP0', 0x0C: 'REFP1', 0x0D: 'PF0', 0x0E: 'PF1', 0x0F: 'PF2', 0x10: 'RESP0', 0x11: 'RESP1', 0x12: 'RESM0', 0x13: 'RESM1',
         0x14: 'RESBL', 0x1B: 'GRP0', 0x1C: 'GRP1', 0x1D: 'ENAM0', 0x1E: 'ENAM1', 0x1F: 'ENABL', 0x20: 'HMP0', 0x21: 'HMP1', 0x22: 'HMM0',
         0x23: 'HMM1', 0x24: 'HMBL', 0x25: 'VDELP0', 0x26: 'VDELP1', 0x2A: 'HMOVE', 0x2B: 'HMCLR'}
STRUCT = {k for k in NAMES if k not in (0x1B, 0x1C, 0x1D, 0x1E, 0x1F)}
STROBE = set(range(0x10, 0x16)) | {0x2A, 0x2B, 0x2C}
PIXEL_TESTS = [('BL-035', 1500), ('BL-035', 5000), ('BL-040', 900), ('BL-052', 20000), ('BL-056', 8000), ('BL-030', 500),
               ('BL-012', 700), ('BL-054', 15000)]


def signed(v):
    v >>= 4
    return v - 16 if v >= 8 else v


def run_stella(entry, pokes, extra):
    r = subprocess.run([str(EXE), str(ROM), str(ROOT / 'baseline' / (entry + '.inputs')), str(OUT / ('c_%s.csv' % entry)),
                        str(ROOT / 'reference' / 'baseline' / (entry + '.trace'))] + pokes + extra, capture_output=True, text=True)
    return r.returncode, r.stdout.strip().splitlines()[-1] if r.stdout.strip() else r.stderr.strip()


def read_dumps(path):
    d = {}
    for line in open(path):
        f, h = line.strip().split(',')
        d[int(f)] = bytes.fromhex(h)
    return d


def picture(ram, line0=14):
    m = cpu.Machine(ROM_BYTES, ram)
    m.run(START, STOP, line0)
    return m.writes


def sprite_state(writes, refl):
    """Sprite columns from the writes: x0, x1 (pixel of the first graphics bit) and the lines where both graphics registers are not 0."""
    w = sorted(writes, key=lambda t: (t[0], t[1]))
    resp, hm = {}, {}
    for ln, ck, pc, reg, val in w:
        if reg in (0x10, 0x11) and reg - 0x10 not in resp:
            resp[reg - 0x10] = ck
        if reg in (0x20, 0x21) and reg - 0x20 not in hm:
            hm[reg - 0x20] = val
        if reg == 0x2A:
            break
    x = [(resp[i] - 68 + 5 - signed(hm[i])) % 160 for i in (0, 1)]
    return w, x


def collide(w, x, refl):
    cur = [0, 0]
    by = collections.defaultdict(list)
    for ln, ck, pc, reg, val in w:
        if reg in (0x1B, 0x1C):
            by[ln].append((reg - 0x1B, val))
    for ln in range(34, 199):
        for r, v in by.get(ln, []):
            cur[r] = v
        a, b = cur
        if a and b:
            s0 = {(x[0] + i) % 160 for i in range(8) if a >> (7 - i) & 1}
            s1 = {(x[1] + i) % 160 for i in range(8) if ((b >> i) & 1 if refl else (b >> (7 - i)) & 1)}
            if s0 & s1:
                return True
    return False


def main():
    global ROM, ROM_BYTES
    ROM = pathlib.Path(sys.argv[1]).resolve() if len(sys.argv) > 1 and not sys.argv[1].startswith('--') else ROOT / 'rom' / 'pacman.a26'
    ROM_BYTES = ROM.read_bytes()
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / 'pic').mkdir(exist_ok=True)
    items = [it for it in f6.entries() if it[0] not in ('BL-065', 'BL-066')]
    result = {}
    problems = []

    import pickle
    cache = OUT / 'results.pkl'
    if '--cached' in sys.argv and cache.exists():
        t1, t2, t4, per_frame_writes, layout, sample_ram, problems = pickle.load(open(cache, 'rb'))
    else:
        # ---- 2 + 3: frame lists: every frame in which the ghost flag is read ----
        frames = {}
        flags = {}
        for e, pokes in items:
            ev = {}
            for line in open(ROOT / 'reference' / 'f12' / ('w_%s.csv' % e)):
                fr, pc, a = line.strip().split(',')
                ev.setdefault(int(fr), {})[pc] = int(a)
            frames[e] = sorted(fr for fr, d in ev.items() if 'f5d9' in d and fr >= 1)
            flags[e] = {fr: bool(d['f5d9'] & 0x80) for fr, d in ev.items() if 'f5d9' in d}

        def job(item):
            e, pokes = item
            fl = OUT / ('frames_%s.txt' % e)
            fl.write_text('\n'.join(map(str, frames[e])))
            a = run_stella(e, pokes, ['frames=%s' % fl, 'ramdump=%s:07F' % (OUT / ('rc_%s.csv' % e))])
            b = run_stella(e, pokes, ['tiaw=%s:0:999999' % (OUT / ('t_%s.csv' % e)), 'tiastride=53', 'ramdump=%s:07F' % (OUT / ('r_%s.csv' % e))])
            return e, a, b

        with concurrent.futures.ThreadPoolExecutor(8) as ex:
            runs = list(ex.map(job, items))
        bad_runs = [r for r in runs if r[1][0] or r[2][0] or '0 differences' not in r[1][1] or '0 differences' not in r[2][1]]
        print('stella runs: %d, with problems: %d' % (len(runs) * 2, len(bad_runs)))
        if bad_runs:
            problems.append('emulator runs with problems: %s' % bad_runs[:3])

        # ---- 1. TIA writes of sampled frames ----
        t1 = collections.Counter()
        first_bad = []
        layout = collections.defaultdict(collections.Counter)
        per_frame_writes = []
        sample_ram = None
        for e, _ in items:
            dumps = read_dumps(OUT / ('r_%s.csv' % e))
            log = {}
            for line in open(OUT / ('t_%s.csv' % e)):
                f, ln, ck, pc, reg, val = line.strip().split(',')
                log.setdefault(int(f), []).append((int(ln), int(ck), int(pc, 16), int(reg, 16), int(val, 16)))
            norm = lambda x: (x[0], x[1], x[2], x[3], 0 if x[3] in STROBE else x[4])
            for f in sorted(dumps):
                ent = [norm(x) for x in log.get(f, []) if 0x053 <= x[2] < 0x4E6]
                start = [x for x in log.get(f, []) if x[2] == 0x083]
                if not start:
                    t1['no start write'] += 1
                    continue
                w = picture(dumps[f], start[0][0] - 1)
                mine = [norm((a, b, c & 0xFFF, d, e2)) for a, b, c, d, e2 in w]
                t1['frames'] += 1
                per_frame_writes.append(len(mine))
                if ent == mine:
                    t1['equal'] += 1
                else:
                    t1['DIFFERENT'] += 1
                    if len(first_bad) < 3:
                        i = next((i for i, (p, q) in enumerate(zip(ent, mine)) if p != q), 0)
                        first_bad.append((e, f, len(ent), len(mine), ent[i:i + 2], mine[i:i + 2]))
                by = collections.defaultdict(set)
                for ln, ck, pc, reg, val in w:
                    if reg in STRUCT and ln >= 34:
                        by[ln].add(reg)
                for ln in range(34, 224):
                    layout[ln][tuple(sorted(by.get(ln, ())))] += 1
                if sample_ram is None and e == 'BL-050' and f > 4000:
                    sample_ram = dumps[f]
        print('1. TIA writes:', dict(t1), first_bad)
        if t1['DIFFERENT'] or t1['equal'] < 8000:
            problems.append('TIA writes differ')

        # ---- 2 + 3: flicker rule and collision flag ----
        t2 = collections.Counter()
        miss = []
        for e, _ in items:
            dumps = read_dumps(OUT / ('rc_%s.csv' % e))
            rows = f12.trace(e)
            for f in frames[e]:
                ram = dumps[f]
                X = ram[0] & 3
                t2['frames'] += 1
                if ram[0x30] == ram[0x32 + X]:
                    t2['ghost drawn = (FRAME_COUNTER & 3) + 1: right'] += 1
                else:
                    t2['ghost rule WRONG'] += 1
                refl = bool(rows[f - 1][1][0x3B] & 1)
                w, x = sprite_state(picture(ram), refl)
                got = collide(w, x, refl)
                want = flags[e][f]
                if got == want:
                    t2['flag predicted right'] += 1
                    t2['  of which set'] += int(want)
                else:
                    t2['flag WRONG'] += 1
                    if len(miss) < 5:
                        miss.append((e, f, got, want, x))
        print('2/3.', dict(t2), miss)
        if t2['ghost rule WRONG'] or t2['flag WRONG']:
            problems.append('flicker or flag wrong')

        # ---- 4. pixels of the emulator's own pictures ----
        t4 = collections.Counter()
        for e, fr in PIXEL_TESTS:
            inp = ROOT / 'baseline' / (e + '.inputs')
            nfr = int([l for l in open(inp) if l.startswith('frames=')][0].split('=')[1])
            fr = min(fr, nfr - 2)
            fl = OUT / 'one.txt'
            fl.write_text(str(fr))
            subprocess.run([str(EXE), str(ROM), str(inp), str(OUT / 'x.csv'), str(ROOT / 'reference' / 'baseline' / (e + '.trace')), 'frames=%s' % fl,
                            'ramdump=%s:07F' % (OUT / 'one_ram.csv')], capture_output=True)
            f_, h = open(OUT / 'one_ram.csv').readline().strip().split(',')
            subprocess.run([str(HOST), '--headless', str(OUT / 'pic' / 't.png'), str(ROM), '--replay', str(inp), '--frames', str(fr + 1), '--save-frame', str(fr),
                            '--raw', str(OUT / 'pic' / 't.raw'), '--settings', str(OUT / 'pic' / 's.ini')], capture_output=True)
            raw = (OUT / 'pic' / 't.raw').read_bytes()
            rows_px = [raw[i * 160:(i + 1) * 160] for i in range(len(raw) // 160)]
            refl = bool(f12.trace(e)[fr - 1][1][0x3B] & 1)
            w, x = sprite_state(picture(bytes.fromhex(h)), refl)
            cur = [0, 0]
            by = collections.defaultdict(list)
            for ln, ck, pc, reg, val in w:
                if reg in (0x1B, 0x1C):
                    by[ln].append((reg - 0x1B, val))
            for ln in range(34, 199):
                for r, v in by.get(ln, []):
                    cur[r] = v
                row = rows_px[ln - 23]
                for idx, (g, xx, rf) in enumerate(((cur[0], x[0], False), (cur[1], x[1], refl))):
                    if not g or cur[1 - idx]:
                        continue
                    cols = [(xx + i) % 160 for i in range(8) if ((g >> i) & 1 if rf else (g >> (7 - i)) & 1)]
                    vals = {row[c] for c in cols}
                    if len(vals) == 1:
                        c = vals.pop()
                        n = sum(1 for v in row if v == c)
                        t4['sprite rows with exactly the predicted pixels' if n == len(cols) else 'rows where the colour also appears elsewhere (not decisive)'] += 1
                    else:
                        t4['sprite rows WRONG'] += 1
        print('4.', dict(t4))
        if t4['sprite rows WRONG']:
            problems.append('pixel rows wrong')

        pickle.dump((t1, t2, t4, per_frame_writes, layout, sample_ram, problems), open(cache, 'wb'))
    # ---- 5. the line map, power-up positions of the bonus item ----
    fixed = [ln for ln in layout if len(layout[ln]) == 1]
    varying = {ln: layout[ln] for ln in layout if len(layout[ln]) > 1}
    m = cpu.Machine(ROM_BYTES, bytes(128))
    m.run(0xF000, 0xF050, 0)
    item_pos = {}
    for ln, ck, pc, reg, val in m.writes:
        if reg in (0x13, 0x14, 0x23, 0x24) and reg not in item_pos:
            item_pos[reg] = (ck, val)
    # two pill positions: RESM0 clock for the two phases of FRAME_COUNTER bit 3
    pill = {}
    for ph in (0, 8):
        ram = bytearray(sample_ram)
        ram[0] = ph
        w2 = picture(bytes(ram))
        for ln, ck, pc, reg, val in w2:
            if reg == 0x12 and ln < 34:
                pill[ph] = ck
            if reg == 0x22 and ln < 34:
                pill[(ph, 'hm')] = val
    ok = not problems
    print('problems:', problems)
    print('result: %s' % ('OK' if ok else 'MISMATCH'))
    write_doc(t1, t2, t4, per_frame_writes, layout, fixed, varying, sample_ram, item_pos, pill, ok)
    return 0 if ok else 1


def effects(ram):
    """Which scanlines and registers change when one part of the memory changes (the executor run twice)."""
    base = set(picture(ram))
    tests = [('wafer bytes `$88-$97` all 0', lambda r: [r.__setitem__(i, 0) for i in range(0x08, 0x18)]),
             ('`PILL_FLAGS` = 0 (no power pill)', lambda r: r.__setitem__(0x1A, 0x00)),
             ('`LIVES` = 5', lambda r: r.__setitem__(0x18, 5)),
             ('score 123456 (player 1)', lambda r: [r.__setitem__(0x4C, 0x56), r.__setitem__(0x4E, 0x34), r.__setitem__(0x50, 0x12)]),
             ('bonus item on (`GAME_FLAGS` bit 1)', lambda r: r.__setitem__(0x67, r[0x67] | 2))]
    out = []
    for name, mut in tests:
        r = bytearray(ram)
        r[0x18] = 0 if 'LIVES' in name else r[0x18]
        base_r = set(picture(bytes(r))) if 'LIVES' in name else base
        mut(r)
        other = set(picture(bytes(r)))
        diff = base_r ^ other
        lines = sorted({x[0] for x in diff})
        regs = sorted({NAMES.get(x[3], '$%02X' % x[3]) for x in diff})
        out.append((name, lines, regs))
    return out


def vertical_stats():
    """First scanline with graphics of each sprite against its y byte (SPRITE0_Y / SPRITE1_Y = object y + 11), play frames of three games."""
    c = [collections.Counter(), collections.Counter()]
    n = 0
    for e in ('BL-050', 'BL-035', 'BL-054'):
        for line in open(OUT / ('r_%s.csv' % e)):
            f, h = line.strip().split(',')
            ram = bytes.fromhex(h)
            if ram[2] != 0:
                continue
            first = [None, None]
            for ln, ck, pc, reg, val in picture(ram):
                if reg in (0x1B, 0x1C) and val and first[reg - 0x1B] is None:
                    first[reg - 0x1B] = ln
            for r, y in ((0, ram[0x58]), (1, ram[0x59])):
                if first[r] is not None:
                    c[r][first[r] - 2 * y] += 1
            n += 1
    return n, c


def ranges(lines):
    out, i = [], 0
    while i < len(lines):
        j = i
        while j + 1 < len(lines) and lines[j + 1] == lines[j] + 1:
            j += 1
        out.append(str(lines[i]) if i == j else '%d-%d' % (lines[i], lines[j]))
        i = j + 1
    return ', '.join(out)


def row_sig(by, base):
    return [tuple(n for n in by.get(base + k, ()) if n not in ('GRP0', 'GRP1')) for k in range(20)]


def write_doc(t1, t2, t4, per_frame_writes, layout, fixed, varying, sample_ram, item_pos, pill, ok):
    vs = vertical_stats()
    w = picture(sample_ram)
    by = collections.defaultdict(list)
    for ln, ck, pc, reg, val in w:
        by[ln].append(NAMES.get(reg, '$%02X' % reg))
    r1 = row_sig(by, 35 + 20)
    diff = {}
    for r in range(8):
        s = row_sig(by, 35 + 20 * r)
        diff[r] = [(k, s[k]) for k in range(20) if s[k] != r1[k]]
    d = ['# Stage F18: the drawing kernel and the flicker schedule (2026-10-26)', '',
         '`python -I tools%sf18_kernel.py rom%spacman.a26` shows what the picture of a frame is made of and proves it against the real emulator. This document is written by that tool.' % (BS, BS), '',
         '## Result: %s' % ('every check passed' if ok else 'MISMATCH'), '',
         '| Count | Check |', '| --- | --- |',
         '| %d of %d | sampled frames (every 53rd frame of all 70 games): all of the roughly %d writes per frame to the TIA chip of the picture (scanline, colour clock, instruction address, register, value) equal the emulator\'s, one by one |' % (t1['equal'], t1['frames'], sum(per_frame_writes) // max(1, len(per_frame_writes))),
         '| %d | frames in which the ghost flag is read (F12): the ghost drawn as player 0 is exactly number `FRAME_COUNTER & 3` + 1 (right: %d, wrong: %d) |' % (t2['frames'], t2['ghost drawn = (FRAME_COUNTER & 3) + 1: right'], t2['ghost rule WRONG']),
         '| %d of %d | the ghost flag (set %d times) is predicted from the two sprites\' bitmaps and pixel columns alone (wrong: %d) |' % (t2['flag predicted right'], t2['frames'], t2['  of which set'], t2['flag WRONG']),
         '| %d | rows of sprite pixels in 8 pictures of the emulator that equal the predicted columns exactly (wrong: %d; %d rows are not decisive because the same colour is also used elsewhere in the row) |' % (t4['sprite rows with exactly the predicted pixels'], t4['sprite rows WRONG'], t4['rows where the colour also appears elsewhere (not decisive)']), '',
         '## What this proves', '',
         'The picture is a pure function of the **128 memory bytes at `$F07F`** (the moment the top logic hands over to the drawing code) and the ROM. `tools%sf18_cpu.py` is a small interpreter that runs the original bytes from `$F07F` to `$F4E7` over those 128 bytes; its list of TIA writes is the emulator\'s, write for write. So nothing else (no timer, no switch, no joystick, no earlier TIA state except the sprite mirror bit, below) influences what is drawn. The native renderer of F23 has to produce the same pixels from the same bytes; this list is its reference.' % BS, '',
         '## The picture, line by line (fixed for every frame)', '',
         'The code is cycle-counted: 76 processor cycles (228 colour clocks) per scanline, one `STA WSYNC` per line. **The set of registers written on each scanline from 34 to 213 is the same in every one of the %d sampled frames** (%d of %d lines; the others are the status line at the bottom, where the row of life markers is missing when no marker is shown).' % (t1['frames'], len([ln for ln in layout if len(layout[ln]) == 1]), len(layout)), '',
         '| Scanlines | What is drawn |', '| --- | --- |',
         '| 0-33 | vertical sync and blank; the top logic (until about line 14), positioning of the sprites (lines 15-19: `RESP`/`HMP`/`HMOVE`), then blank until the timer ends |',
         '| 34 | `KERNEL_START`: video on, colours (background, maze), `CTRLPF` $31 (reflected playfield, ball 8 wide) |',
         '| 35-194 | **8 maze rows of 20 scanlines**: row r starts at line 35 + 20 r (see the row template) |',
         '| 195-198 | bottom wall of the maze (`PF` = $FF $FF $7F), sprites still drawn |',
         '| 199-201 | switch to the score line: sprites repositioned for the score (`RESP0`, `RESP1`, `HMP0`, `HMOVE`), `NUSIZ` = 3 (three copies), vertical delay on both players |',
         '| 202-213 | the **score digits** of the player in turn, 7 lines of 6 digits using the six-picture trick of `SCORE_DIGITS` (players 0 and 1 interleaved, three copies each, with vertical delay) |',
         '| 214-223 | line 214 switches the sprites back to normal; 216 sets the colours of the marker line; 217-222 are the **life markers** (`CTRLPF` $02, `PF0/PF1` patterns, none when `LIVES` is 0); 223 ends the picture (`CTRLPF` 0, `COLUPF` 0) |', '',
         '### The row template (20 scanlines)', '',
         'Registers written on each line of a maze row, from the executor (graphics registers `GRP0`/`GRP1` are written on every line, alternately, and are left out of the table):', '',
         '| Line in the row | Registers written (row 1) |', '| --- | --- |']
    for k in range(20):
        d.append('| %d | %s |' % (k, ', '.join(r1[k]) if r1[k] else '(only the graphics register)'))
    d += ['', 'Differences to row 1: ' + '; '.join('row %d: %s' % (r, ', '.join('line %d %s' % (k, '+'.join(s) if s else 'nothing') for k, s in diff[r])) for r in range(8) if diff[r]) + '.', '',
          'What changes which lines (the executor run twice with one part of the memory changed; this is the evidence for the roles below):', '',
          '| Change in memory | Scanlines that change | Registers |', '| --- | --- | --- |']
    for name, lines, regs in effects(sample_ram):
        d.append('| %s | %s | %s |' % (name, ranges(lines), ', '.join(regs)))
    d += ['', 'So:', '',
          '- **The wafers are playfield dots**: only lines 11 and 12 of each maze row (46-47, 66-67, ... 186-187) depend on the wafer bytes, and only through `PF0/PF1/PF2` (written twice per line, left half and right half, with `CTRLPF` $30 so the right half is not a mirror image; line 13 restores $31). The walls are playfield too (lines 0 and 4 of a row and the sides), chosen from the tables in the ROM; none of the five tests above changed them.',
          '- **Player 0 is the ghost of this frame**, **player 1 is Pac-Man**; every picture row of a sprite is two scanlines high (player 0 changes on odd lines, player 1 on even lines); graphics come from the ROM through the pointers in `$DD-$E0`.',
          '- **Missile 0 draws the power pills**: one line (18 of a row) in the rows that start at lines 35 and 155 (rows 0 and 6), 2 = on.',
          '- **Missile 1 and the ball together draw the bonus item**: lines 6 and 9 of the row that starts at line 115 (row 4), only while `GAME_FLAGS` bit 1 is set (kernel row counter 3, `$F316-$F320`).',
          '- **The score** is lines 207-213 (7 lines of `GRP0/GRP1`); **the life markers** are lines 217-222 (`PF0/PF1`), one marker per life, none when `LIVES` is 0.', '',
          '## The flicker schedule', '',
          '- **Ghosts:** only **one** ghost is drawn per frame, as player 0: ghost number `(FRAME_COUNTER & 3) + 1` (checked in %d frames). Each ghost is therefore on the screen 1 frame in 4 (and its position is updated in that same frame, F13). The picture shows four ghosts only through the persistence of the television; the screenshots of a single frame show one.' % t2['frames'],
          '- **Pac-Man** is drawn in every frame, as player 1. Its mouth direction uses the sprite mirror (`REFP1`, written in the bottom logic from `PACMAN_STATE` bit 0 and cleared by the score-line set-up): it is mirrored exactly when bit 0 of the state was set at the end of the previous frame (needed for 100% of the collision predictions).',
          '- **Power pills** blink: missile 0 sits at one of two places depending on `FRAME_COUNTER` bit 3 (position code at `$F088-$F094`: its `RESM0` strobe falls at colour clock %s for bit 3 = 0 and at %s for bit 3 = 1), and the pill row/side chosen by the code at `$F2D2-$F2E5` follows the same bit, so each existing pill is visible for 8 frames and hidden for 8.' % (pill.get(0), pill.get(8)),
          '- **Bonus item:** missile 1 and the ball are positioned once at power-on (`RESBL`/`RESM1` at `$F040/$F042`, strobe clocks %s and %s, fine move `HMBL` $80 and `HMM1` $F0) and enabled only in the row that starts at line 115 while `GAME_FLAGS` bit 1 is set.' % (item_pos.get(0x14, ('?',))[0], item_pos.get(0x13, ('?',))[0]), '',
          '## Where a sprite is on the screen', '',
          'Horizontal: the position routine (`$F09C-$F0BF`) cuts the x byte into a coarse part (a delay loop of `3 + (x + 1) / 15` passes) and a fine part (the `HMP` nibble); `RESPn` is strobed at colour clock `c` (the clock at the end of the `STA RESPn,X` instruction) and `HMOVE` then shifts the sprite left by the signed `HMP` nibble. **The first graphics bit appears at pixel column `c - 68 + 5 - HMP` (mod 160)**; player 0 and player 1 use the same offset (checked by the collision flags: any other relative shift gives wrong flags).',
          'Vertical: with `SPRITE0_Y`/`SPRITE1_Y` = object y + 11 (the bytes `$D8/$D9`), the first scanline with graphics is **2 x `SPRITE0_Y` + 17 for the ghost (player 0) and 2 x `SPRITE1_Y` + 18 for Pac-Man (player 1)**, i.e. 2 y + 39 and 2 y + 40 from the game y; this holds in %d of %d play frames of three games for player 0 and %d of %d for player 1 (the rest start 2 or more scanlines later: the first graphics byte is empty, or the sprite is in an opening animation). Each graphics byte takes 2 scanlines; `SPRITE0_LINES`/`SPRITE1_LINES` start at 7 and 6 and count down to -1, so a ghost has 8 bytes and Pac-Man 7.' % (vs[1][0].get(17, 0), vs[0], vs[1][1].get(18, 0), vs[0]), '',
          '## Collisions follow from the picture', '',
          'With the two sprites\' bitmaps and columns as above, the chip\'s flag "player 0 touched player 1" (the ghost flag of F12) is predicted for every frame in which the game reads it: %d of %d (%d set). The native renderer of F23 therefore does not need a separate collision routine: it only has to report overlapping pixels of the two sprites in the maze lines 34-198 (the score digits never overlap, as the flags show).' % (t2['flag predicted right'], t2['frames'], t2['  of which set']), '',
          '## Not claimed', '',
          '- Colours and the full pixel-for-pixel picture (playfield bit patterns, wafers, walls, score digits, markers) are not rebuilt here; the executor gives their TIA writes exactly and F23 turns them into pixels and compares with the emulator picture by picture.',
          '- The pixel columns of missile 0, missile 1 and the ball are given as strobe clocks, not as columns.',
          '- The table of which tile/wall patterns each maze byte selects is read from the code and the executor, not listed in full here.']
    (ROOT / 'docs' / 'F18_KERNEL.md').write_text('\n'.join(d) + '\n', encoding='utf-8')


if __name__ == '__main__':
    sys.exit(main())
