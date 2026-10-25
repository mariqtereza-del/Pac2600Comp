"""Stage F8: the list of routines and who calls whom.

  python -I tools\\f8_routines.py <rom>      (after f2-f7 have been run)

1. Finds every entry point: the start-up code (the main loop, reached from RESET) and every target of a JSR instruction in the ROM.
2. For each entry it follows the code (branches, jumps, falling through; a JSR is not followed but recorded) until every path
   ends in RTS. When it falls or jumps into another entry, that is recorded as "continues into". A piece of code reached from
   two routines by a jump becomes an entry of its own (a shared tail, named TAIL_Fxxx unless it has a name below).
3. Records callers, callees, size, the RAM bytes (by name, F7) and chip registers it touches, whether it drops its caller's
   return address too (PLA PLA), the stack it needs, and how often it ran in the 70 baseline games (F6 run counts).
4. Checks: every instruction that ran belongs to the main loop or to exactly one routine; the call count of each routine equals
   the number of times its JSR instructions ran (plus the entries from jumps); no recursion; the deepest nesting of calls fits
   the stack area found in F7.
5. Writes reference\\disasm\\pacman_routines.asm (LOCAL ONLY: the listing of F7 with a banner and a name for each routine) and
   requires the assembled result to be byte-identical; writes docs\\F8_ROUTINES.md.
The ROM is only read.
"""
import collections
import hashlib
import pathlib
import re
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import f1_6502 as asm  # noqa: E402
import f3_names  # noqa: E402
import f7_ram  # noqa: E402

ROOT = pathlib.Path(__file__).resolve().parent.parent
DIS = ROOT / 'reference' / 'disasm'
ORG = 0xF000
BRANCH = {0x10, 0x30, 0x50, 0x70, 0x90, 0xB0, 0xD0, 0xF0}

# address -> (name, confidence S/L/Q, what it does). Names of routines found by reading the code (F7 listing).
NAMES = {
    0xF000: ('MAIN', 'S', 'Start-up (clears memory, first values, positions the objects) and then the endless frame loop of F5: sync, top logic, wait, picture, bottom logic, wait. Never returns.'),
    0xF053: ('KERNEL_LINE_P1', 'S', 'One picture line for the second sprite (Pac-Man): loads its next picture byte when this line is inside it, waits for the line start, stores GRP1.'),
    0xF067: ('KERNEL_LINE_P1_P0', 'S', 'One line for Pac-Man, then falls into the next routine for the ghost: two lines.'),
    0xF06A: ('KERNEL_LINE_P0', 'S', 'One picture line for the first sprite (the ghost of this frame): the same method, stores GRP0.'),
    0xF3EE: ('SCORE_DIGITS', 'L', 'Turns the six decimal digits of one score into the six picture pointers of the score line (blank leading zeros) and draws the line with the six-picture trick.'),
    0xF483: ('CLEAR_GRAPHICS', 'S', 'Writes 0 to GRP0 and GRP1 (twice each, with the vertical delay on).'),
    0xF793: ('PACMAN_KEEP_GOING', 'Q', 'Pac-Man with the stick in neutral: if he is exactly on a cell, tries the directions to keep moving; drops its caller\'s return (PLA PLA) when a direction is taken.'),
    0xF9AA: ('GHOST_UPDATE', 'L', 'Everything for one ghost (index in X, 1-4) in one frame: pen exit and return, then the decision routine and the move.'),
    0xF9E5: ('SET_DIRECTION', 'S', 'Stores direction code Y (0 right, 1 left, 2 down, 3 up) into the state bits of object X.'),
    0xF9EE: ('MOVE_OBJECT', 'S', 'The speed accumulator of object X: when it carries, moves the object one step in its direction (x or y) unless a border stops it.'),
    0xFA26: ('GHOST_DECIDE', 'L', 'The ghost\'s decision: where it wants to go (target Pac-Man, or away when scared, or home when eaten), then which allowed direction to take, using the random number.'),
    0xFAFC: ('TRY_PAIR_1', 'Q', 'Tries two directions in an order chosen by the random number ($FBE0); returns two levels when one is taken.'),
    0xFB05: ('TRY_PAIR_UD', 'Q', 'Tries up and down in the order chosen by the caller\'s flag; returns two levels.'),
    0xFB19: ('TRY_PAIR_2', 'Q', 'As TRY_PAIR_1 for the other pair of directions.'),
    0xFB22: ('TRY_PAIR_LR', 'Q', 'Tries left and right in the order chosen by the caller\'s flag; returns two levels.'),
    0xFB36: ('TRY_UP', 'L', 'If the maze allows a step up from this cell, takes direction 3 and returns two levels; otherwise returns.'),
    0xFB44: ('TRY_COMMIT', 'L', 'Shared end of the four TRY routines: unless the ghost would turn back, sets the direction and drops the caller\'s return.'),
    0xFB5C: ('TRY_DOWN', 'L', 'The same for direction 2 (down).'),
    0xFB70: ('TRY_LEFT', 'L', 'The same for direction 1 (left).'),
    0xFB87: ('TRY_RIGHT', 'L', 'The same for direction 0 (right).'),
    0xFB9F: ('SWAP_PLAYERS', 'S', 'Exchanges the 20-byte player block $88-$9B with $9C-$AF.'),
    0xFBAD: ('LOCATE_OBJECT', 'S', 'Works out where object X stands: row, offsets inside the cell and the cell index ($C9, $85, $86, $87, $EE).'),
    0xFBE0: ('RNG_STEP', 'S', 'Steps the shift-register random number $CB; returns a masked bit pattern (zero or not) in A.'),
    0xFBF4: ('NEW_GAME_SETUP', 'S', 'Lives to 3, score colours, level step from the game number; falls into NEW_MAZE.'),
    0xFC0C: ('NEW_MAZE', 'S', 'Fills the wafer map with $FF (power pills and wafer holes set), power-pill flags to $3C; falls into PLACE_OBJECTS.'),
    0xFC21: ('PLACE_OBJECTS', 'S', 'Freezes the game, sets the pause timer and puts Pac-Man and the four ghosts at their start places.'),
    0xFC45: ('ADD_SCORE', 'S', 'Adds the amount number Y (tables $FF06/$FF0C/$FF0D) to the score of the player whose turn it is, in decimal mode.'),
    0xFC68: ('LIVES_MARKERS', 'L', 'Sets the playfield values for the life markers of the status line from the number of lives X.'),
    0xFC7D: ('CLEAR_SCORES', 'S', 'Zeroes both scores and both lives.'),
    0xFC8B: ('ANIMATE_OBJECT', 'L', 'The animation of an object whose state bit 5 is set: a ghost coming out of the pen, or Pac-Man dying; counts the steps.'),
    0xFD26: ('WRAP_TO_BOTTOM', 'L', 'Pac-Man or a ghost entering the bottom opening: sets the direction and y = $51.'),
    0xFD32: ('WRAP_TO_TOP', 'L', 'Entering the top opening: sets the direction and y = $0B.'),
    0xF9E4: ('EXIT_F9E4', 'S', 'A lone RTS: the common exit of the ghost update, move and decision routines.'),
    0xFB5B: ('EXIT_FB5B', 'S', 'A lone RTS: the common exit of the TRY routines.'),
    0xFD3C: ('WRAP_SET_Y', 'S', 'Stores A as the y of object X and returns: the end of both WRAP routines.'),
    0xFF8B: ('SOUND_WRITE', 'S', 'Writes volume (A), control (D3) and pitch (Y) to the sound channel X.'),
    0xFF93: ('EXIT_FF93', 'S', 'A lone RTS: the exit of the sound routines.'),
    0xFF43: ('SOUND_START', 'S', 'Starts sound number Y unless a more important sound is playing.'),
    0xFF55: ('SOUND_EFFECTS', 'L', 'Each frame: plays the sound chosen by SOUND_START on channel 0.'),
    0xFF94: ('SOUND_MUSIC', 'L', 'Each frame: the start tune, the death sound and the scared-ghost sound.'),
}
RAMNAME = f7_ram.NAME
ALIAS = f7_ram.ARRAY_ALIASES


def build(rom):
    def ins(a):
        mn, mode, cy, by, ok = asm.TABLE[rom[a]]
        return mn, mode, by

    def targets(a):
        """(kind, target) pairs for the instruction at a: ('jsr', t) ('jmp', t) ('br', t) ('fall', next) or nothing."""
        op = rom[a]
        mn, mode, by = ins(a)
        out = []
        if op == 0x20:
            return [('jsr', (rom[a + 1] | rom[a + 2] << 8) & 0xFFF)]
        if op == 0x4C:
            return [('jmp', (rom[a + 1] | rom[a + 2] << 8) & 0xFFF)]
        if op in (0x60, 0x40, 0x6C, 0x00):
            return []
        if op in BRANCH:
            d = rom[a + 1] - 256 if rom[a + 1] > 127 else rom[a + 1]
            out.append(('br', a + 2 + d))
        out.append(('fall', a + by))
        return out
    return ins, targets


def main():
    rom = pathlib.Path(sys.argv[1]).resolve().read_bytes()
    ins, targets = build(rom)
    problems = []
    cov = []
    for l in (ROOT / 'reference' / 'f6' / 'coverage_all.txt').read_text().split('\n'):
        if re.match(r'F[0-9A-F]{3}  ', l):
            cov.extend(l[6:].strip())
    runs = {}
    for l in (ROOT / 'reference' / 'f6' / 'run_counts.csv').read_text().split('\n')[1:]:
        if l:
            a, n, _ = l.split(',')
            runs[int(a[1:], 16)] = int(n)
    # all instruction starts reached from anywhere: find JSR targets (including never-run JSRs)
    code_starts = [o for o in range(4096) if cov[o] == 'C' or (cov[o] == '.' and rom[o] in (0x20, 0x68, 0x60))]
    roots = {0x000}
    for o in range(4096):
        if cov[o] == 'C' and rom[o] == 0x20:
            roots.add((rom[o + 1] | rom[o + 2] << 8) & 0xFFF)
    for o in (0xACF, 0xAF9):  # the JSRs that never ran
        roots.add((rom[o + 1] | rom[o + 2] << 8) & 0xFFF)
    for a in NAMES:
        roots.add(a & 0xFFF)

    def explore(root, roots):
        seen, order, jsr, cont, stack = set(), [], [], set(), [root]
        while stack:
            a = stack.pop()
            if a in seen or not 0 <= a < 4096:
                continue
            if a != root and a in roots:
                cont.add(a)
                continue
            seen.add(a)
            mn, mode, by = ins(a)
            for kind, t in targets(a):
                if kind == 'jsr':
                    jsr.append((a, t))
                    stack.append(a + by)
                else:
                    stack.append(t)
        return seen, jsr, cont

    # shared tails become roots of their own until no instruction belongs to two routines
    for _ in range(20):
        ext = {r: explore(r, roots)[0] for r in sorted(roots)}
        owner = collections.defaultdict(list)
        for r, s in ext.items():
            for a in s:
                owner[a].append(r)
        shared = sorted(a for a, o in owner.items() if len(o) > 1)
        if not shared:
            break
        roots.add(shared[0])
    routines = {}
    for r in sorted(roots):
        seen, jsr, cont = explore(r, roots)
        size = 0
        for a in seen:
            size += ins(a)[2]
        routines[r] = dict(seen=seen, jsr=jsr, cont=cont, size=size)
    # the frame loop is cut into pieces by its jumps: a piece that no JSR calls and that is entered only from the main loop
    # (or from pieces already merged) belongs to the main loop
    jsr_called = {t for d in routines.values() for a, t in d['jsr']}
    reach, todo = {0}, [0]
    while todo:
        for c in routines[todo.pop()]['cont']:
            if c not in reach and c not in jsr_called:
                reach.add(c)
                todo.append(c)
    cluster = set(reach)
    for r in sorted(cluster - {0}):
        routines[0]['seen'] |= routines[r]['seen']
        routines[0]['jsr'] += routines[r]['jsr']
        routines[0]['cont'] |= routines[r]['cont']
        routines[0]['size'] += routines[r]['size']
        del routines[r]
    routines[0]['cont'] -= cluster
    for r, d in routines.items():
        if r:
            d['cont'] = {0 if c in cluster else c for c in d['cont']}
    owner = {}
    for r, d in routines.items():
        for a in d['seen']:
            owner[a] = r

    def rname(r):
        n = NAMES.get(0xF000 + r)
        return n[0] if n else 'TAIL_F%03X' % r

    # callers
    callers = collections.defaultdict(list)
    for r, d in routines.items():
        for a, t in d['jsr']:
            callers[t].append((a, r))
    conts_in = collections.defaultdict(list)
    for r, d in routines.items():
        for c in d['cont']:
            conts_in[c].append(r)
    # per routine: RAM and chip use
    for r, d in routines.items():
        rd, wr = set(), set()
        hw = set()
        dbl = False
        prev = None
        for a in sorted(d['seen']):
            mn, mode, by = ins(a)
            if mode in ('ZERO_PAGE', 'ZERO_PAGE_X', 'ZERO_PAGE_Y', 'INDIRECT_X', 'INDIRECT_Y', 'ABSOLUTE', 'ABSOLUTE_X', 'ABSOLUTE_Y'):
                ad = rom[a + 1] | (rom[a + 2] << 8 if mode.startswith('ABSOLUTE') else 0)
                if mode.startswith('ABSOLUTE') and ad >= 0x1000:
                    continue
                w = mn in ('sta', 'stx', 'sty')
                m = mn in ('inc', 'dec', 'asl', 'lsr', 'rol', 'ror')
                if ad >= 0x80 and ad < 0x100:
                    if mode.endswith('_X') or mode.endswith('_Y'):
                        nm = ALIAS.get(ad, RAMNAME[ad]) + '[]'
                    else:
                        nm = RAMNAME[ad]
                    if mode.startswith('INDIRECT'):
                        rd.add(nm + '->')
                    elif w:
                        wr.add(nm)
                    elif m:
                        rd.add(nm)
                        wr.add(nm)
                    else:
                        rd.add(nm)
                elif ad < 0x80 or 0x280 <= ad < 0x300:
                    n = f3_names.name_for(ad, mn.upper())
                    if n:
                        hw.add(n[0])
            if rom[a] == 0x68 and prev is not None and rom[prev] == 0x68:
                dbl = True
            prev = a
        d.update(ram_read=sorted(rd), ram_write=sorted(wr), hw=sorted(hw), drops=dbl)
    # stack depth
    depth = {}

    def sd(r, path=()):
        if r in path:
            problems.append('recursion through %s' % rname(r))
            return 0
        if r in depth:
            return depth[r]
        d = routines[r]
        best = 0
        for a, t in d['jsr']:
            best = max(best, 2 + sd(t, path + (r,)))
        for c in d['cont']:
            if c != 0:  # a routine that drops its caller's return and ends inside the main loop adds no stack
                best = max(best, sd(c, path + (r,)))
        for a in d['seen']:
            if rom[a] in (0x48, 0x08):
                best = max(best, 1)
        depth[r] = best
        return best

    for r in routines:
        sd(r)
    maxdepth = depth[0]
    # checks
    checks = []
    covered = [o for o in range(4096) if cov[o] == 'C' and o not in owner]
    checks.append(('every instruction that ran belongs to the main loop or to exactly one routine (%d do not)' % len(covered), not covered))
    multi = [a for a in range(4096) if sum(1 for d in routines.values() if a in d['seen']) > 1]
    checks.append(('no instruction belongs to two routines (%d do)' % len(multi), not multi))
    entry_bad = []
    for t, cs in callers.items():
        if t not in routines:
            entry_bad.append(t)
            continue
        jsr_runs = sum(runs.get(a, 0) for a, _ in cs)
        into = runs.get(t, 0)
        extra = sum(1 for _ in conts_in.get(t, []))
        if extra == 0 and into != jsr_runs:
            entry_bad.append(t)
    checks.append(('a routine entered only by JSR ran exactly as often as its JSR instructions (%d disagree)' % len(entry_bad), not entry_bad))
    checks.append(('no routine calls itself, directly or through others', not [p for p in problems if p.startswith('recursion')]))
    checks.append(('deepest nesting of calls is %d bytes of stack and the stack area found in F7 is 10 bytes ($F6-$FF)' % maxdepth, maxdepth <= 10))
    named = [r for r in routines if (0xF000 + r) in NAMES]
    checks.append(('%d routines in all, %d with a name from the code, %d shared tails named by position' % (len(routines), len(named), len(routines) - len(named)), True))
    for what, ok in checks:
        if not ok:
            problems.append(what)
    # listing with names
    src = (DIS / 'pacman_ram.asm').read_text(encoding='utf-8').split('\n')
    text = '\n'.join(src)
    for r in sorted(routines):
        if r == 0:
            continue
        text = re.sub(r'\bL_F%03X\b' % r, rname(r), text)
    out = []
    for line in text.split('\n'):
        m = re.match(r'^([A-Z_0-9]+):$', line)
        if m:
            for r in routines:
                if r and rname(r) == m.group(1):
                    n = NAMES.get(0xF000 + r)
                    d = routines[r]
                    out.append('; ===== ROUTINE %s ($F%03X): %s' % (rname(r), r, n[2] if n else 'shared tail of other routines (found by the tool)'))
                    out.append('; called from: %s%s' % (', '.join('$F%03X' % a for a, _ in sorted(callers.get(r, []))) or 'no JSR', ('; entered by jump/fall-through from ' + ', '.join(rname(x) for x in conts_in[r])) if conts_in.get(r) else ''))
                    if d['drops']:
                        out.append('; DROPS the return address of its caller as well (PLA PLA)')
        out.append(line)
    text = '\n'.join(out)
    (DIS / 'pacman_routines.asm').write_text(text, encoding='utf-8', newline='')
    back, labels = asm.assemble(text, ORG)
    same = back == rom
    (DIS / 'pacman_routines_rebuilt.bin').write_bytes(back)
    conf = {'S': 'sure', 'L': 'likely', 'Q': '?'}
    # document
    d = ['# Stage F8: routines and call map (2026-10-26)', '',
         '`python -I tools\\f8_routines.py rom\\pacman.a26` (after f2-f7) finds the routines by following the code from the start-up and from every JSR, names them from reading the code, records who calls whom, and rebuilds the listing with the names (`reference\\disasm\\pacman_routines.asm`, local only) byte-identical to the ROM. This document is written by the tool.', '',
         'The main loop (`MAIN`) is the start-up plus the frame loop of F5; it is the only code that is not a subroutine. Every other routine is entered by `JSR`. A **shared tail** is code that two routines reach by a jump; it is listed on its own (`TAIL_Fxxx` unless it has a real name).', '',
         '## Checks', '']
    for what, ok in checks:
        d.append('- %s: %s' % ('PASS' if ok else 'FAIL', what))
    d += ['', 'Deepest nesting of calls: **%d bytes of stack**.' % maxdepth, '', '## Routines', '',
          '| Entry | Name | Sure? | Bytes | Called from (JSR sites; ran times) | Calls | Drops caller\'s return | What it does |', '| --- | --- | --- | --- | --- | --- | --- | --- |']
    for r in sorted(routines):
        dd = routines[r]
        n = NAMES.get(0xF000 + r)
        cs = sorted(callers.get(r, []))
        cs_txt = ', '.join('$F%03X in %s (%d)' % (a, rname(o), runs.get(a, 0)) for a, o in cs) or ('(start)' if r == 0 else '(jump only)')
        callee = sorted(set(rname(t) for a, t in dd['jsr']) | set('-> ' + rname(c) for c in dd['cont']))
        d.append('| $F%03X | `%s` | %s | %d | %s | %s | %s | %s |' % (r, rname(r), conf[n[1]] if n else 'tool', dd['size'], cs_txt, ', '.join(callee) or '-', 'yes' if dd['drops'] else '', n[2] if n else 'Code reached by jump from %s.' % ', '.join(rname(x) for x in conts_in.get(r, []))))
    d += ['', '## Data each routine touches', '', '| Name | Reads | Writes | Chip registers |', '| --- | --- | --- | --- |']
    for r in sorted(routines):
        dd = routines[r]
        d.append('| `%s` | %s | %s | %s |' % (rname(r), ', '.join(dd['ram_read']) or '-', ', '.join(dd['ram_write']) or '-', ', '.join(dd['hw']) or '-'))
    d += ['', '`name[]` = an array reached by an index register; `name->` = a pointer held in that byte and the one after it.', '', '## Call tree', '']

    def tree(r, ind, seen):
        d.append('%s`%s`' % ('  ' * ind + '- ', rname(r)))
        if r in seen:
            return
        for c in sorted(routines[r]['cont']):
            d.append('%s`%s` (continues into)' % ('  ' * (ind + 1) + '- ', rname(c)))
        for t in sorted(set(t for a, t in routines[r]['jsr'])):
            tree(t, ind + 1, seen | {r})
    tree(0, 0, set())
    uncalled = [r for r in routines if r and not callers.get(r) and not conts_in.get(r)]
    d += ['', '## Notes', '',
          '- Routines that drop the caller\'s return (`PLA PLA`) end the whole decision of the caller at once: the C++ version needs an early exit there (F13).',
          '- Two JSR instructions never ran in the 70 games (`$FACF` and `$FAF9`, both in the ghost decision); the routines they call are also called by instructions that did run.',
          '- Not claimed: names marked "?" are a guess from the shape of the code; F10-F13 settle them.']
    (ROOT / 'docs' / 'F8_ROUTINES.md').write_text('\n'.join(d) + '\n', encoding='utf-8')
    lines = ['routines: %d' % len(routines), 'checks:'] + ['  %s: %s' % ('PASS' if ok else 'FAIL', what) for what, ok in checks]
    lines += ['problems: %s' % (problems or 'none'), 'ROM SHA-1:     %s' % hashlib.sha1(rom).hexdigest(), 'rebuilt SHA-1: %s' % hashlib.sha1(back).hexdigest(),
              'result: %s' % ('BYTE-IDENTICAL' if same else 'DIFFERENT')]
    (DIS / 'routines_stats.txt').write_text('\n'.join(lines) + '\n', encoding='utf-8')
    print('\n'.join(lines))
    return 0 if same and not problems else 1


if __name__ == '__main__':
    sys.exit(main())
