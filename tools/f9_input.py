"""Stage F9: how the program reads the console's inputs (joystick, buttons, switches).

  python -I tools\\f9_input.py <rom>      (after f1-f8 have been run)

1. Static: every instruction of the ROM that reads a joystick, button or switch register (SWCHA, SWACNT, SWCHB, SWBCNT, INPT0-5).
2. Dynamic, from the 70 baseline traces: what the memory bytes do on frames where an input is pressed (joystick -> JOY_DIRECTION,
   fire/Reset -> game start, Select -> game number, difficulty -> GAME_FLAGS bit 5, TV type -> COLOUR_MASK).
3. Two experiments no baseline game contains, run in the Stella core with stella_script.exe: the second player's turn with the
   right joystick (and the left one pressed), and the Select switch held for 160 frames.
4. Where in the frame each read happens (scanline), measured with stella_pctrace.exe.
5. Where three code pieces that no baseline game reaches were found (see docs\\F9_INPUT.md, "A correction") and proof that they run.
Writes docs\\F9_INPUT.md. The ROM is only read.
"""
import collections
import pathlib
import re
import subprocess
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import f1_6502 as asm  # noqa: E402
import f3_names  # noqa: E402

ROOT = pathlib.Path(__file__).resolve().parent.parent
BIN = ROOT / 'build' / 'stella_probe'
OUT = ROOT / 'reference' / 'f9'
INPUT_REGS = {'SWCHA', 'SWACNT', 'SWCHB', 'SWBCNT', 'INPT0', 'INPT1', 'INPT2', 'INPT3', 'INPT4', 'INPT5'}
READ_POINTS = {  # offset -> what it reads
    0x61E: ('SWCHA', 'joystick directions'), 0x857: ('SWCHB', 'TV type switch (bit 3)'), 0x886: ('INPT4', 'fire button of the left joystick (bit 7, 0 = pressed)'),
    0x88A: ('SWCHB', 'Reset (bit 0) and Select (bit 1)'), 0x91C: ('SWCHB', 'difficulty switch (bit 6 left, bit 7 right)')}


def load(name):
    rows = []
    for l in (ROOT / 'reference' / 'baseline' / (name + '.trace')).read_text().split('\n'):
        p = l.strip().split(',')
        if len(p) >= 6 and p[0].isdigit():
            rows.append((int(p[1]), int(p[2], 16), bytes.fromhex(p[5])))
    return rows


def static_reads(rom):
    cov = []
    for l in (ROOT / 'reference' / 'f6' / 'coverage_all.txt').read_text().split('\n'):
        if re.match(r'F[0-9A-F]{3}  ', l):
            cov.extend(l[6:].strip())
    found = []
    for off in range(4096):
        if cov[off] != 'C':
            continue
        mn, mode, cy, by, _ = asm.TABLE[rom[off]]
        if mode not in ('ZERO_PAGE', 'ZERO_PAGE_X', 'ZERO_PAGE_Y', 'ABSOLUTE', 'ABSOLUTE_X', 'ABSOLUTE_Y', 'INDIRECT_X', 'INDIRECT_Y'):
            continue
        ad = rom[off + 1] | (rom[off + 2] << 8 if mode.startswith('ABSOLUTE') else 0)
        if ad >= 0x1000 or (0x80 <= ad < 0x280):
            continue
        n = f3_names.name_for(ad, mn.upper())
        if n and n[0] in INPUT_REGS:
            found.append((off, mn.upper(), n[0]))
    return found


def main():
    rom = pathlib.Path(sys.argv[1]).resolve()
    romb = rom.read_bytes()
    OUT.mkdir(parents=True, exist_ok=True)
    problems, lines = [], []
    results = []  # (text, ok)

    # 1. static
    reads = static_reads(romb)
    expect = {0x61E, 0x857, 0x886, 0x88A, 0x91C}
    got = {o for o, _, _ in reads}
    results.append(('exactly five instructions read inputs: %s' % ', '.join('$F%03X %s %s' % r for r in reads), got == expect))

    # 2. traces
    c = collections.Counter()
    for f in sorted((ROOT / 'reference' / 'baseline').glob('BL-???.trace')):
        rows = load(f.stem)
        for i in range(1, len(rows)):
            inp, sw, r = rows[i]
            pinp, psw, pr = rows[i - 1]
            ed, p2 = r[0x6D], bool(r[0x66] & 0x80)
            if ed != pr[0x6D]:
                c['ed changes'] += 1
                if not p2 and ed == (inp & 15):
                    c['ed equals the joystick bits'] += 1
                elif p2 and ed == 0:
                    c['ed set in player 2 turn (left stick ignored: 0)'] += 1
                else:
                    c['ed other'] += 1
            if pr[2] != 0 and r[2] == 0:
                c['start by fire'] += 1 if inp & 16 else 0
                c['start by reset'] += 1 if inp & 64 else 0
                if not inp & (16 | 64):
                    c['start unexplained'] += 1
            if r[4] != pr[4]:
                c['game number steps'] += 1
                if not inp & 32:
                    c['game number step without Select'] += 1
            if r[2] == 0:
                p2t = p2
                want = bool(inp & (256 if p2t else 128))
                c['difficulty bit ok' if (((r[0x67] >> 5) & 1) == 1) == want else 'difficulty bit differs'] += 1 if (not p2t) else 0
                c['mask ok'] += 1 if r[0x6F] == (0x0F if inp & 512 else 0xFF) else 0
                c['mask differs'] += 1 if r[0x6F] != (0x0F if inp & 512 else 0xFF) else 0
            else:
                c['mask ok (title)'] += 1 if r[0x6F] == ((0x0F if inp & 512 else 0xFF) ^ 8) else 0
                c['mask differs (title)'] += 1 if r[0x6F] != ((0x0F if inp & 512 else 0xFF) ^ 8) else 0
    results.append(('JOY_DIRECTION: in %d changes during player 1 turns it equals the left joystick bits (%d times), in player 2 turns it is 0 although the left stick is pressed (%d times), %d other'
                    % (c['ed changes'], c['ed equals the joystick bits'], c['ed set in player 2 turn (left stick ignored: 0)'], c['ed other']), c['ed other'] <= 1 and c['ed equals the joystick bits'] > 8000))
    results.append(('game starts: %d by fire, %d by Reset, %d unexplained' % (c['start by fire'], c['start by reset'], c['start unexplained']), c['start unexplained'] == 0 and c['start by fire'] > 0 and c['start by reset'] > 0))
    results.append(('the game number steps %d times, %d of them without Select pressed' % (c['game number steps'], c['game number step without Select']), c['game number step without Select'] == 0))
    results.append(('GAME_FLAGS bit 5 equals the left difficulty switch (A = 1) in %d frames of play of player 1, differs in %d' % (c['difficulty bit ok'], c['difficulty bit differs']), c['difficulty bit differs'] == 0 and c['difficulty bit ok'] > 0))
    results.append(('COLOUR_MASK is $FF (colour) or $0F (black and white) in %d frames of play and with bit 3 flipped in %d frames of the title screen; %d differ'
                    % (c['mask ok'], c['mask ok (title)'], c['mask differs'] + c['mask differs (title)']), c['mask differs'] + c['mask differs (title)'] == 0))

    # 3. experiments
    p2s = OUT / 'p2.txt'
    p2s.write_text('# BL-005 inputs, then the second player\'s turn is tested\n120 129 SELECT\n300 309 RESET\n1000 1049 RLEFT\n1050 1099 UP\n1100 1149 RRIGHT\n1150 1199 RDOWN\n1000 1500 RDIFF_A\n')
    (OUT / 'p2').mkdir(exist_ok=True)
    subprocess.run([str(BIN / 'stella_script.exe'), str(rom), str(OUT / 'p2'), '1400', str(p2s)], capture_output=True)
    rows = [bytes.fromhex(l.split(',')[8]) for l in (OUT / 'p2' / 'frames.csv').read_text().split('\n')[1:] if len(l.split(',')) >= 9]
    seg = lambda a, b: collections.Counter(r[0x6D] for r in rows[a:b])
    right_left = seg(1010, 1050)[4] > 20
    left_up = seg(1060, 1100)[1] == 0
    right_right = seg(1110, 1150)[8] > 20
    right_down = seg(1160, 1200)[2] > 20
    p2flag = all(r[0x67] & 0x20 for r in rows[1010:1200])
    results.append(('player 2 turn (frames 1000-1199): JOY_DIRECTION follows the RIGHT joystick (left %d, right %d, down %d frames) and ignores the LEFT joystick pushed up (up bits seen: %d); GAME_FLAGS bit 5 set while the RIGHT difficulty is A in all 190 frames: %s'
                    % (seg(1010, 1050)[4], seg(1110, 1150)[8], seg(1160, 1200)[2], seg(1060, 1100)[1], p2flag), right_left and left_up and right_right and right_down and p2flag))
    sels = OUT / 'sel.txt'
    sels.write_text('100 259 SELECT\n')
    (OUT / 'sel').mkdir(exist_ok=True)
    subprocess.run([str(BIN / 'stella_script.exe'), str(rom), str(OUT / 'sel'), '300', str(sels)], capture_output=True)
    sr = [bytes.fromhex(l.split(',')[8]) for l in (OUT / 'sel' / 'frames.csv').read_text().split('\n')[1:] if len(l.split(',')) >= 9]
    steps = [i for i in range(1, len(sr)) if sr[i][4] != sr[i - 1][4]]
    gaps = sorted(set(b - a for a, b in zip(steps, steps[1:])))
    results.append(('Select held from frame 100 to 259: the game number steps at frames %s (first on the press, then every %s frames)' % (steps, gaps[0] if gaps else '?'), steps[:1] == [100] and gaps == [31]))

    # 4. timing
    def trace_lines(inputs, trace, a, b):
        out = OUT / 'pc.csv'
        cmd = [str(BIN / 'stella_pctrace.exe'), str(rom), str(inputs), str(out), str(a), str(b)] + ([str(trace)] if trace else [])
        subprocess.run(cmd, capture_output=True)
        seen = collections.defaultdict(set)
        for l in out.read_text().split('\n'):
            p = l.split(',')
            if len(p) == 4 and p[3].isdigit() and int(p[3]) in READ_POINTS:
                seen[int(p[3])].add(int(p[1]))
        return seen
    timing = collections.defaultdict(set)
    for inputs, trace, a, b in (('baseline/BL-005.inputs', 'reference/baseline/BL-005.trace', 50, 54), ('baseline/BL-005.inputs', 'reference/baseline/BL-005.trace', 119, 121),
                                ('baseline/BL-005.inputs', 'reference/baseline/BL-005.trace', 299, 301), ('baseline/BL-005.inputs', 'reference/baseline/BL-005.trace', 1000, 1004),
                                ('baseline/BL-046.inputs', 'reference/baseline/BL-046.trace', 2000, 2006)):
        for k, v in trace_lines(ROOT / inputs, ROOT / trace, a, b).items():
            timing[k] |= v
    results.append(('all five read points were seen in the traced frames: %s' % ', '.join('$F%03X at scanline %s' % (k, '/'.join(str(x) for x in sorted(v))) for k, v in sorted(timing.items())), set(timing) == expect))

    # 5. code that no baseline game reaches
    proofs = []
    for name, off, label in (('X-select-hold', 0x907, 'BEQ $F8C8 at $F907 (the game number steps again while Select is held)'), ('X-left-A-idle', 0x832, 'DEY at $F832 (difficulty A, bonus item showing)'),
                             ('X-power-A', 0x5C2, 'LDY #$50 at $F5C2 (difficulty A, power pill eaten)')):
        first = None
        for l in (ROOT / 'reference' / 'f1' / ('cov_%s.csv' % name)).read_text().split('\n'):
            p = l.split(',')
            if p[0] == 'x' and int(p[1]) == off:
                first = int(p[3])
        proofs.append((label, name, first))
    results.append(('three instructions that no baseline game runs were run by the extra games: %s' % '; '.join('%s in %s, first at frame %s' % p for p in proofs), all(p[2] is not None for p in proofs)))
    for t, ok in results:
        if not ok:
            problems.append(t)

    # document
    d = ['# Stage F9: input reading (2026-10-26)', '',
         '`python -I tools\\f9_input.py rom\\pacman.a26` (after `f1`..`f8`) finds every place where the program reads the console\'s inputs, checks what it does with them against the 70 baseline games, runs two extra experiments, and measures the scanline of each read. This document is written by that tool.', '',
         '## Checks', '']
    d += ['- %s: %s' % ('PASS' if ok else 'FAIL', t) for t, ok in results]
    d += ['', '## What the program reads (all of it)', '',
          '| Where | Reads | What it is | When | Used for |', '| --- | --- | --- | --- | --- |',
          '| `$F61E` | `SWCHA` | the two joysticks: upper half = left joystick, lower half = right joystick; 0 = pressed | bottom logic of every frame in which the game is running (not on the title screen, not frozen, and not in the frames where a ghost kills Pac-Man) | `JOY_DIRECTION` (`$ED`): inverted, **left joystick in player 1\'s turn, right joystick in player 2\'s turn** ($E6 bit 7); bit 0 up, 1 down, 2 left, 3 right. Any direction also zeroes `TICK256`. |',
          '| `$F857` | `SWCHB` bit 3 | TV type: 1 = colour, 0 = black and white | start of every frame | `COLOUR_MASK`: `$FF` or `$0F`, bit 3 flipped on the title screen |',
          '| `$F886` | `INPT4` (via `$3C`) | fire button of the **left** joystick; bit 7 = 0 when pressed | top logic, only when `ATTRACT_SHIFT` is not 0 (title screen / after game over) | starts a game |',
          '| `$F88A` | `SWCHB` bits 0, 1 | Reset (bit 0) and Select (bit 1); 0 = pressed | top logic, every frame | Reset starts a game (also during play); Select steps the game number: at once on the press and again every 31 frames while it is held, giving 16 choices ($84, 2 to $11; the choice also toggles the two-player bit of `PLAYER_MODE`) |',
          '| `$F91C` | `SWCHB` bit 6 / bit 7 | difficulty: **left** switch (bit 6) in player 1\'s turn, **right** switch (bit 7) in player 2\'s turn; 1 = A | top logic, every frame | `GAME_FLAGS` bit 5 (the difficulty of the player whose turn it is): A shortens the bonus item cycle and the scared time of the ghosts |',
          '', 'Nothing else is read: the second fire button, both paddles and `SWACNT`/`SWBCNT` are never touched. In play the fire button has no effect (C6). In a two-player game the right joystick\'s fire button and the left joystick are never used for player 2.', '',
          '## For the rebuild', '',
          'The input of one frame is: joystick bits (left and right, 4 bits each), left fire, Select, Reset, left difficulty, right difficulty, TV type. Every read can use the state at the start of the frame (the host sets the inputs once per frame); the reads fall on scanline 261 (TV type), scanlines 0-16 (fire, Reset, Select, difficulty) and 226-238 (joysticks), as measured, so no input needs to change inside a frame. The recorded games (`.inputs` files) contain only the left joystick: **in player 2\'s turn they therefore never move Pac-Man**. A faithful port must read the right joystick there; the host program (D10) maps the keyboard and pad to the left joystick only and needs a second set of keys for player 2 (a decision for the owner at Phase H, noted in `DECISIONS.md` T-F9-1).', '',
          '## A correction to F2-F6 (code that was listed as data)', '',
          'Three places of the listing were data in F2 to F8 and are code: `$F5C2-$F5C3` (`LDY #$50`), `$F832` (`DEY`) and `$F907-$F908` (`BEQ $F8C8`). Each is the not-taken side of a branch that no baseline game takes: the difficulty switch at A with a power pill or with the bonus item showing, and the Select switch held for 31 frames. The reading of the input led to them; three extra games (`tools\\f9_inputs\\X-*.inputs`, no golden trace, only their instructions count) now run them, and `f1_coverage.py` and `f6_coverage.py` include them. The map is now 1,810 instruction starts, 1,735 operand bytes, 509 data bytes and 42 bytes never touched; the listing rebuilds byte-identical at every stage.', '',
          'The lesson, kept as a rule for Phase F: **a byte that no recording runs is not proof that it is data.** The 42 untouched bytes (12 bytes of never-run code at `$FACF-$FAD1`, `$FAF9-$FAFB`, `$FB2A-$FB2C`, `$FB33-$FB35`, and 30 bytes in table holes and the end of the ROM) stay as listed in `docs/F6_COVERAGE.md`; later stages must read the code around any table hole before trusting it.']
    (ROOT / 'docs' / 'F9_INPUT.md').write_text('\n'.join(d) + '\n', encoding='utf-8')
    for t, ok in results:
        print('%s: %s' % ('PASS' if ok else 'FAIL', t))
    print('problems: %s' % (problems or 'none'))
    return 0 if not problems else 1


if __name__ == '__main__':
    sys.exit(main())
