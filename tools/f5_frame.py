"""Stage F5: the frame loop of the program, found by running it.

  python -I tools\\f5_frame.py <rom>

1. Runs stella_pctrace.exe (one line per executed instruction with its scanline) on chosen frames of several baseline recordings:
   attract mode, play, a death, the game over, a new game, two players.
2. Checks that in every traced frame the same six landmarks occur in the same order:
     FRAME_START  $F7D6  the game waits for the end of the line and switches the vertical sync and blank on
     VSYNC_END    $F87C  the vertical sync is switched off (after the timer for the vertical blank was set at $F875)
     VBLANK_WAIT  $F0C5  the loop that waits for the timer (INTIM) to run out
     KERNEL_START $F0CA  the picture starts (VBLANK off); the "kernel" draws the screen line by line
     OVERSCAN_TIMER $F389  the second timer is set, while the picture is still being drawn
     OVERSCAN_WAIT $F7D1  the loop that waits for the second timer, then the next frame starts
3. Measures, over all traced frames, which scanlines each part covers.
4. Writes reference\\disasm\\pacman_frame.asm (LOCAL ONLY): the table listing of F4 with these landmarks as labels and a banner
   before each part; assembles it and requires the result to be byte-identical to the ROM.
"""
import collections
import hashlib
import pathlib
import re
import subprocess
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import f1_6502 as asm  # noqa: E402

ROOT = pathlib.Path(__file__).resolve().parent.parent
EXE = ROOT / 'build' / 'stella_probe' / 'stella_pctrace.exe'
OUT = ROOT / 'reference' / 'disasm'
F5 = ROOT / 'reference' / 'f5'
ORG = 0xF000
# (recording, first frame, last frame, what it shows)
RUNS = [('BL-002', 1000, 1004, 'attract mode'), ('BL-049', 2500, 2504, 'play'), ('BL-049', 5780, 5790, 'a death'), ('BL-048', 1795, 1800, 'game over'),
        ('BL-048', 2400, 2404, 'a new game starts'), ('BL-048', 2560, 2564, 'play of the new game'), ('BL-067', 700, 704, 'two players'), ('BL-058', 450, 454, 'Game Reset in play')]
LANDMARKS = [(0x7D6, 'FRAME_START'), (0x87C, 'VSYNC_END'), (0x0C5, 'VBLANK_WAIT'), (0x0CA, 'KERNEL_START'), (0x389, 'OVERSCAN_TIMER'), (0x7D1, 'OVERSCAN_WAIT')]
ORDER = [n for a, n in LANDMARKS]
BANNER = {
    'RESET': ['; ===== START-UP: the processor starts here (the RESET vector at $FFFC points to $F000) ===========================',
              '; Clears the whole page $00-$FF (the chip registers and the 128 bytes of memory), sets the first values, positions the',
              '; objects, then jumps into the frame loop at FRAME_START.'],
    'FRAME_START': ['; ===== PART 1: VERTICAL SYNC AND PREPARATION (3 lines: scanlines 259-261) ======================================',
                    '; The frame begins here: wait for the end of the line, switch the vertical blank and the vertical sync on, clear the',
                    '; collision latches, work out values for the frame, set the timer for the vertical blank ($F875).'],
    'VSYNC_END': ['; ===== PART 2: TOP LOGIC (scanlines 0 to about 20) =============================================================',
                  '; The vertical sync is switched off; the game logic that needs time runs here until the wait begins.'],
    'VBLANK_WAIT': ['; ===== PART 3: WAIT FOR THE VERTICAL-BLANK TIMER (until scanline 33) ===========================================',
                    '; A tight loop reading the timer; it ends when the timer runs out.'],
    'KERNEL_START': ['; ===== PART 4: THE PICTURE, drawn line by line (scanlines 34 to about 198) =====================================',
                     '; "Kernel": the maze rows, the ghosts, Pac-Man and the status bar are drawn here, timed to the cycle.'],
    'OVERSCAN_TIMER': ['; ===== PART 5: BOTTOM LOGIC (from about scanline 195 or later, to about 232) =====================================',
                       '; The second timer is set (while the last picture lines are still drawn); the rest of the game logic runs here.'],
    'OVERSCAN_WAIT': ['; ===== PART 6: WAIT FOR THE SECOND TIMER (until scanline 258), then the next frame starts at FRAME_START ===========',
                      '; A tight loop reading the timer.'],
}


def trace(rec, a, b):
    out = F5 / ('pc_%s_%d_%d.csv' % (rec, a, b))
    r = subprocess.run([str(EXE), str(ROM), str(ROOT / 'baseline' / (rec + '.inputs')), str(out), str(a), str(b), str(ROOT / 'reference' / 'baseline' / (rec + '.trace'))],
                       capture_output=True, text=True)
    return out, r.returncode, r.stdout.strip().splitlines()[-1]


def main():
    global ROM
    ROM = pathlib.Path(sys.argv[1]).resolve()
    rom = ROM.read_bytes()
    F5.mkdir(parents=True, exist_ok=True)
    problems, summary = [], []
    lines_at = collections.defaultdict(list)  # landmark -> list of scanlines where it first happens
    wait_end = collections.defaultdict(list)
    frames_checked = 0
    for rec, a, b, what in RUNS:
        out, rc, line = trace(rec, a, b)
        if rc != 0:
            problems.append('%s %d-%d: the traced run differs from the golden trace (%s)' % (rec, a, b, line))
            continue
        byframe = collections.defaultdict(list)
        for l in out.read_text().split('\n'):
            if l:
                f, ln, clk, pc = map(int, l.split(','))
                byframe[f].append((ln, clk, pc))
        for f in range(a + 1, b):  # the first and last frame of a range may be cut: they start or end in the previous/next frame
            seq, i = [], 0
            rows = byframe[f]
            last = None
            for ln, clk, pc in rows:
                for lm, name in LANDMARKS:
                    if pc == lm and last != name:
                        seq.append((name, ln))
                        last = name
                if pc in (0x0C5, 0x0C8, 0x7D1, 0x7D4):
                    pass
            names = [n for n, ln in seq]
            # waits are loops: collapse a landmark that repeats directly
            collapsed = [n for k, n in enumerate(names) if k == 0 or n != names[k - 1]]
            # Stella cuts a frame where the scanline counter wraps (line 262 = line 0): the frame cut falls just after VSYNC_END's setup,
            # so one traced frame runs VBLANK_WAIT ... OVERSCAN_WAIT, FRAME_START, VSYNC_END
            expect = ORDER[2:] + ORDER[:2]
            if collapsed != expect:
                problems.append('%s frame %d (%s): landmarks %s, expected %s' % (rec, f, what, collapsed, expect))
                continue
            frames_checked += 1
            first = {}
            for n, ln in seq:
                first.setdefault(n, ln % 262)
            for n in ORDER:
                lines_at[n].append(first[n])
            # the end of each wait: the line of the last instruction of the wait loop
            for lm, name in ((0x0C5, 'VBLANK_WAIT'), (0x7D1, 'OVERSCAN_WAIT')):
                ws = [ln % 262 for ln, clk, pc in rows if pc in (lm, lm + 3)]
                if ws:
                    wait_end[name].append((min(ws), max(ws)))
        summary.append('%s frames %d-%d (%s): %s' % (rec, a, b, what, line))
    # statistics
    stat = []
    for n in ORDER:
        v = lines_at[n]
        if v:
            stat.append('%-15s first reached on scanline %d to %d (most often %d), %d frames' % (n, min(v), max(v), collections.Counter(v).most_common(1)[0][0], len(v)))
    for n in ('VBLANK_WAIT', 'OVERSCAN_WAIT'):
        v = wait_end[n]
        if v:
            stat.append('%-15s the wait loop runs from scanline %d..%d to %d..%d' % (n, min(a for a, b in v), max(a for a, b in v), min(b for a, b in v), max(b for a, b in v)))

    # the listing
    src = (OUT / 'pacman_tables.asm').read_text(encoding='utf-8').split('\n')
    rename = {'L_F0C5': 'VBLANK_WAIT', 'L_F7D1': 'OVERSCAN_WAIT', 'L_F7D6': 'FRAME_START'}
    new_labels = {0x000: 'RESET', 0x87C: 'VSYNC_END', 0x0CA: 'KERNEL_START', 0x389: 'OVERSCAN_TIMER', 0x873: 'VBLANK_TIMER_SETUP'}
    out = []
    for line in src:
        m = re.search(r'; \$F([0-9A-F]{3})  ', line)
        if m and not line.lstrip().startswith(';'):
            off = int(m.group(1), 16)
            if off in new_labels:
                if off == 0x000:
                    out += BANNER['RESET']
                if off == 0x389:
                    out += BANNER['OVERSCAN_TIMER']
                if off == 0x87C:
                    out += BANNER['VSYNC_END']
                if off == 0x0CA:
                    out += BANNER['KERNEL_START']
                out.append('%s:' % new_labels[off])
        stripped = line
        for old, new in rename.items():
            stripped = re.sub(r'\b%s\b' % old, new, stripped)
        # banners before renamed labels
        mm = re.match(r'^(VBLANK_WAIT|OVERSCAN_WAIT|FRAME_START):$', stripped)
        if mm:
            out += BANNER[mm.group(1)]
        out.append(stripped)
    text = '\n'.join(out) + '\n'
    (OUT / 'pacman_frame.asm').write_text(text, encoding='utf-8', newline='')
    back, labels = asm.assemble(text, ORG)
    same = back == rom
    (OUT / 'pacman_frame_rebuilt.bin').write_bytes(back)
    result = (['traced runs:'] + ['  ' + s for s in summary] + ['frames checked for the six landmarks in the right order: %d' % frames_checked]
              + ['problems: %s' % (problems or 'none')] + stat
              + ['ROM SHA-1:     %s' % hashlib.sha1(rom).hexdigest(), 'rebuilt SHA-1: %s' % hashlib.sha1(back).hexdigest(),
                 'result: %s' % ('BYTE-IDENTICAL' if same else 'DIFFERENT')])
    (OUT / 'frame_stats.txt').write_text('\n'.join(result) + '\n', encoding='utf-8')
    print('\n'.join(result))
    return 0 if same and not problems else 1


if __name__ == '__main__':
    sys.exit(main())
