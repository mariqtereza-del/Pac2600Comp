"""Stage F7: names for all 128 bytes of the console's memory ($80-$FF).

  python -I tools\\f7_ram.py <rom>      (after f2, f3, f4, f5 and f6 have been run)

1. Static facts: for every RAM byte, which executed instructions read it, write it, change it in place or use it as the start of
   an array (zero page indexed / pointer), from the code map of Stage F6.
2. Dynamic facts: the value range, the number of distinct values and the number of changes of every byte over the 70 baseline
   traces (439,680 frames).
3. The names, the confidence ("sure", "likely", "?") and the meaning of each byte are written in this file (RAM below), each
   from reading the code (docs\\F7_RAM_MAP.md says how) - never from a guess alone.
4. Checks that test claims the names make (frame counter, player swap block, decimal score digits, position ranges, stack area,
   bytes no instruction touches).
5. Writes reference\\disasm\\pacman_ram.asm (LOCAL ONLY): the listing of F5 with every zero-page RAM operand replaced by its name,
   assembles it and requires the result to be byte-identical to the ROM. Writes docs\\F7_RAM_MAP.md.
The ROM is only read.
"""
import collections
import hashlib
import pathlib
import re
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import f1_6502 as asm  # noqa: E402

ROOT = pathlib.Path(__file__).resolve().parent.parent
DIS = ROOT / 'reference' / 'disasm'
ORG = 0xF000

# confidence: S = sure (the code and the traces agree and the use is unambiguous), L = likely, Q = uncertain ("?")
# (address, name, confidence, meaning)
RAM = [
    (0x80, 'FRAME_COUNTER', 'S', 'Counts frames, +1 every frame ($F812). Bits 0-1 choose which ghost is handled in this frame ($F92C); bit 2 is tested before the ghost-collision check ($F5D3); bit 3 alternates the missile/ball positions in the kernel ($F088, $F2D2) and, with bit 4, picks the Pac-Man mouth picture; bits 4-5 pick the ghost picture ($F7E0-$F7ED). Select sets it to 1.'),
    (0x81, 'TICK256', 'L', 'Counts 256-frame ticks (+1 whenever FRAME_COUNTER wraps, $F816); cleared when a joystick direction is pressed ($F633). The wrap of this byte steps ATTRACT_SHIFT.'),
    (0x82, 'ATTRACT_SHIFT', 'S', '0 while a game is being played. Not 0 in the title/attract state: it is then XORed into every colour ($F996) so the picture changes colour; stepped (never to 0) when TICK256 wraps. Power-on value $FF; Select sets 1; fire or Reset sets 0.'),
    (0x83, 'SELECT_REPEAT', 'L', 'Counts frames while the Select switch is held ($F8FF); a new game number is taken when it is 0, and again every 32 frames ($F903).'),
    (0x84, 'GAME_SELECT', 'S', 'Game choice: +1 per Select press, 2 to $11 (16 choices: 8 games x 1 or 2 players), $12 wraps to 2 ($F8E7). Sets LEVEL_SPEED at the start of a game.'),
    (0x85, 'Y_OFFSET', 'S', 'Where the object being looked at stands in its maze row, vertically: 0 = exactly on a row, otherwise negative ($F6-$FD) = (y mod 10) - 10. Made by the position routine $FBAD.'),
    (0x86, 'X_OFFSET', 'S', 'The same horizontally: x AND 3 (0 = exactly on a cell column). Made by $FBAD.'),
    (0x87, 'CELL_INDEX', 'S', 'The maze cell the object being looked at is in: x/4, +40 on odd rows (0-79), index into the maze wall table TBL_FE13. The kernel reuses this byte as the row pointer of the picture.'),
]
for _i in range(16):
    RAM.append((0x88 + _i, 'MAZE_%02d' % _i, 'S', 'Wafer map of the current player: 16 bytes, one bit per wafer or power-pill place; all $FF at the start of a level ($FC0C), $8E=$7F and $8A=$FE (the bits that are not wafers). A bit is cleared when the wafer is eaten ($F69D). The kernel draws the dots from these bytes.' if _i == 0 else 'Wafer map, byte %d of 16 (see MAZE_00).' % _i))
RAM += [
    (0x98, 'LIVES', 'S', 'Lives left of the current player (3 at the start; shown as life markers). A copy for the other player is LIVES_P2.'),
    (0x99, 'LEVEL_SPEED', 'L', 'Level/difficulty step of the current player: 4, 6, 8 or $0A at the start by game number, +1 on every cleared map up to $0E ($F6C2). Chooses the ghost move timers (TBL_FE6F) and the speed-ups.'),
    (0x9A, 'PILL_FLAGS', 'S', 'Bit 7 = power-pill time (ghosts scared); bit 6 = scare running out (blink); bits 2-5 = which of the four power pills are still there ($3C at the start); bits 0-1 = ghosts eaten in this power-pill time (0-3).'),
    (0x9B, 'SCORE_COLOUR', 'L', 'Colour number of the current player\'s score ($0A for player 1); copied to COLOUR_SCORE_P1 every frame ($F87E).'),
]
for _i in range(16):
    RAM.append((0x9C + _i, 'MAZE_P2_%02d' % _i, 'S', 'The other player\'s wafer map (16 bytes). The whole block $88-$9B is exchanged with $9C-$AF when the turn passes to the other player ($FB9F). Zero while only one player plays.' if _i == 0 else 'The other player\'s wafer map, byte %d of 16.' % _i))
RAM += [
    (0xAC, 'LIVES_P2', 'S', 'The other player\'s lives (the block $88-$9B swapped with $9C-$AF at a change of turn).'),
    (0xAD, 'LEVEL_SPEED_P2', 'L', 'The other player\'s LEVEL_SPEED.'),
    (0xAE, 'PILL_FLAGS_P2', 'S', 'The other player\'s PILL_FLAGS.'),
    (0xAF, 'SCORE_COLOUR_P2', 'L', 'The other player\'s score colour number ($09 at the start); copied to COLOUR_SCORE_P2.'),
    (0xB0, 'SPRITE_X', 'S', 'Horizontal place of the figure on the first sprite in this frame: set to the x of the ghost being drawn ($F95F); the byte after it ($B1) is Pac-Man\'s, so the two are positioned by one loop ($F09F).'),
    (0xB1, 'PACMAN_X', 'S', 'Pac-Man\'s x (4 to $94). Also the start of the x array for objects: Pac-Man, then the four ghosts.'),
    (0xB2, 'GHOST_X0', 'S', 'Ghost 0 x.'), (0xB3, 'GHOST_X1', 'S', 'Ghost 1 x.'), (0xB4, 'GHOST_X2', 'S', 'Ghost 2 x.'), (0xB5, 'GHOST_X3', 'S', 'Ghost 3 x.'),
    (0xB6, 'PACMAN_Y', 'S', 'Pac-Man\'s y (0 to about $5F). Start of the y array for objects.'),
    (0xB7, 'GHOST_Y0', 'S', 'Ghost 0 y.'), (0xB8, 'GHOST_Y1', 'S', 'Ghost 1 y.'), (0xB9, 'GHOST_Y2', 'S', 'Ghost 2 y.'), (0xBA, 'GHOST_Y3', 'S', 'Ghost 3 y.'),
    (0xBB, 'PACMAN_STATE', 'L', 'Pac-Man\'s state bits: bit 0 = facing left (picture mirrored, $F7BA); bits 7-6 = direction code and bit 4 = a step is waiting ($F9E5/$FB54 use the codes $10, $50, $90, $D0); bit 5 = dying animation ($F96A); bit 3 never set for Pac-Man; the exact meaning of the direction code and bit 7 when set at the start is not yet certain.'),
    (0xBC, 'GHOST_STATE0', 'L', 'Ghost 0 state bits (same layout as PACMAN_STATE): bit 3 = eaten and going home ($08), bit 5 = in the pen/leaving it (animation), bit 4 = step waiting, bits 7-6 = direction code.'),
    (0xBD, 'GHOST_STATE1', 'L', 'Ghost 1 state bits.'), (0xBE, 'GHOST_STATE2', 'L', 'Ghost 2 state bits.'), (0xBF, 'GHOST_STATE3', 'L', 'Ghost 3 state bits.'),
    (0xC0, 'SPEED_ACC0', 'L', 'Pac-Man\'s speed accumulator: the byte times 17 is added to itself every frame and a carry means "move one step" ($F9EE). Reset at a new level.'),
    (0xC1, 'SPEED_ACC1', 'L', 'Ghost 0 speed accumulator (same method; start value $0F).'), (0xC2, 'SPEED_ACC2', 'L', 'Ghost 1 speed accumulator.'),
    (0xC3, 'SPEED_ACC3', 'L', 'Ghost 2 speed accumulator.'), (0xC4, 'SPEED_ACC4', 'L', 'Ghost 3 speed accumulator.'),
    (0xC5, 'GHOST_TIMER0', 'L', 'Ghost 0 countdown to the next change of target ($FA82); reloaded from TBL_FE6F by LEVEL_SPEED.'),
    (0xC6, 'GHOST_TIMER1', 'L', 'Ghost 1 countdown.'), (0xC7, 'GHOST_TIMER2', 'L', 'Ghost 2 countdown.'), (0xC8, 'GHOST_TIMER3', 'L', 'Ghost 3 countdown.'),
    (0xC9, 'CELL_ROW', 'L', 'Maze row of the object being looked at (y / 10) with flag bits mixed in by $FBD7-$FBDB; the kernel counts maze rows in the same byte while drawing.'),
    (0xCA, 'ROW_PARITY', 'Q', 'Used by the kernel as the even/odd marker of the maze row being drawn (1 or 0); written only there ($F0D4, $F2C6). Not used by the logic.'),
    (0xCB, 'RNG_STATE', 'S', 'The game\'s pseudo-random number: a shift register stepped by $FBE0 (shift, exclusive-or of two bits, rotate in); set to $FF at power-on and at the start of every game ($F892). Used when a ghost chooses a direction. Stage F14 follows it.'),
    (0xCC, 'SCORE_P1_LO', 'S', 'Score of player 1: the two lowest decimal digits (tens, ones; each nibble a digit, decimal mode). Shows $CB (blank pattern) before the first game; holds the game number while Select is pressed.'),
    (0xCD, 'SCORE_P2_LO', 'S', 'Score of player 2: the two lowest digits.'),
    (0xCE, 'SCORE_P1_MID', 'S', 'Player 1: hundreds and thousands (value $ED = blank pattern before the first game).'),
    (0xCF, 'SCORE_P2_MID', 'S', 'Player 2: hundreds and thousands.'),
    (0xD0, 'SCORE_P1_HI', 'S', 'Player 1: ten-thousands and hundred-thousands (value $0F = blank pattern).'),
    (0xD1, 'SCORE_P2_HI', 'S', 'Player 2: ten-thousands and hundred-thousands.'),
    (0xD2, 'TEMP_D2', 'Q', 'Scratch. Holds the first playfield value of a maze row in the kernel; the target y when a ghost chooses a direction; Pac-Man\'s wanted direction in the move code; the score-line pointers.'),
    (0xD3, 'TEMP_D3', 'Q', 'Scratch (second playfield value in the kernel; target x of a ghost).'),
    (0xD4, 'TEMP_D4', 'Q', 'Scratch (kernel playfield values).'), (0xD5, 'TEMP_D5', 'Q', 'Scratch (kernel playfield values; score digit pointers).'),
    (0xD6, 'TEMP_D6', 'Q', 'Scratch (kernel playfield values).'), (0xD7, 'TEMP_D7', 'Q', 'Scratch (kernel playfield values; score digit pointers).'),
    (0xD8, 'SPRITE0_Y', 'L', 'First line of the first sprite (ghost) in the picture: the ghost\'s y + 11 ($F966). In the score line it holds a pointer byte.'),
    (0xD9, 'SPRITE1_Y', 'S', 'First line of Pac-Man in the picture: PACMAN_Y + 11 ($F84A).'),
    (0xDA, 'TEMP_DA', 'Q', 'Only used as a pointer byte of the score line ($FE after it).'),
    (0xDB, 'SPRITE0_LINES', 'L', 'Line counter of the ghost picture: 7 down to -1 as its lines are drawn.'),
    (0xDC, 'SPRITE1_LINES', 'L', 'Line counter of the Pac-Man picture: 6 down to -1.'),
    (0xDD, 'SPRITE0_PTR_LO', 'S', 'Low byte of the pointer to the ghost picture (one of the four pictures of TBL_FDAB; high byte = SPRITE0_PTR_HI).'),
    (0xDE, 'SPRITE0_PTR_HI', 'S', 'High byte of that pointer: always $FD ($F841).'),
    (0xDF, 'SPRITE1_PTR_LO', 'S', 'Low byte of the pointer to the Pac-Man picture (the mouth phase from TBL_FDC2, or the dying pictures from TBL_FDFA).'),
    (0xE0, 'SPRITE1_PTR_HI', 'S', 'High byte of that pointer: always $FD.'),
    (0xE1, 'TEMP_E1', 'Q', 'Scratch (kernel: the missile/ball enable value; score line: digit counter).'),
    (0xE2, 'TEMP_E2', 'Q', 'Scratch (kernel; score line).'), (0xE3, 'TEMP_E3', 'Q', 'Scratch (kernel: the third playfield value).'),
    (0xE4, 'EVENT_TIMER', 'L', 'Countdown of the current pause/effect: the power-pill time, the pause after a death ($3F), the start-of-level pause ($3C). Counts down every second frame ($F500); its value also shapes the death sound.'),
    (0xE5, 'EAT_PAUSE', 'L', 'Pause after eating a ghost: set to $9F, counts down while negative ($F4F2); while bit 7 is set the ghosts do not move ($F94C).'),
    (0xE6, 'PLAYER_MODE', 'S', 'Bit 6 = two-player game; bit 7 = it is player two\'s turn.'),
    (0xE7, 'GAME_FLAGS', 'L', 'Bit 7 = the game is frozen (start of a level, ready to go: ghosts at home); bit 6 = death sequence; bit 5 = the difficulty switch of the current player is set to A; bit 4 = set by Select, tested at game start (meaning not certain); bit 3 = start tune playing; bit 1 = bonus item showing.'),
    (0xE8, 'SOUND_ACTIVE', 'L', 'The sound now playing, as a bit (8, 4, 2, 1 from TBL_FE6E/FE6F); 0 = none. A new sound starts only if its mask in TBL_FFE9 allows it ($FF43).'),
    (0xE9, 'SOUND_TIMER', 'L', 'Frames left of that sound; loaded from TBL_FFED.'),
    (0xEA, 'UNUSED_EA', 'L', 'No instruction reads or writes it; always 0.'),
    (0xEB, 'UNUSED_EB', 'L', 'No instruction reads or writes it; always 0.'),
    (0xEC, 'BONUS_TIMER', 'L', 'Counts 256-frame ticks to the next appearance/disappearance of the bonus item ($F81A): when it reaches 0, GAME_FLAGS bit 1 is flipped and the timer is reloaded with $12 (item gone) or 3 (item showing). Power-on value $0C; the attract screen shows the same timing.'),
    (0xED, 'JOY_DIRECTION', 'S', 'The joystick of the current player: bit 0 up, 1 down, 2 left, 3 right (0 = none) - from SWCHA, upper or lower half by player ($F61E-$F62D).'),
    (0xEE, 'POS_CODE', 'Q', 'Cell index x 4 plus row flags from the position routine ($FBDD); read once ($F6F4, compared with $F9). Meaning not certain.'),
    (0xEF, 'COLOUR_MASK', 'S', '$FF for colour, $0F for black-and-white (from the TV TYPE switch, $F85E); bit 3 flipped in attract mode.'),
    (0xF0, 'COLOUR_BG', 'S', 'Background colour: starts every frame as colour number 7 ($F869) and is turned into a real colour (table lookup, XOR ATTRACT_SHIFT, AND COLOUR_MASK) in the top logic ($F992).'),
    (0xF1, 'COLOUR_MAZE', 'S', 'Maze colour: colour number 8, converted the same way.'),
    (0xF2, 'COLOUR_SPRITE0', 'S', 'Colour of the first sprite (the ghost drawn in this frame): the ghost number 0-3, or 4/5 when scared; converted to a colour.'),
    (0xF3, 'COLOUR_SPRITE1', 'S', 'Colour of Pac-Man: number 6, converted.'),
    (0xF4, 'COLOUR_SCORE_P2', 'S', 'Colour of player 2\'s score line (from SCORE_COLOUR_P2).'),
    (0xF5, 'COLOUR_SCORE_P1', 'S', 'Colour of player 1\'s score line (from SCORE_COLOUR).'),
]
for _i, _a in enumerate(range(0xF6, 0x100)):
    RAM.append((_a, 'STACK_%02X' % _a, 'S', 'The processor\'s stack (return addresses of subroutine calls), $FF downwards; the deepest chain of calls needs 10 bytes ($F6-$FF).' if _a == 0xF6 else 'Stack.'))

ARRAY_ALIASES = {  # base address -> alias used when the byte is the start of an indexed access
    0x88: 'MAZE', 0x9C: 'MAZE_P2', 0xB0: 'SPRITE_X_ARR', 0xB1: 'OBJ_X', 0xB6: 'OBJ_Y', 0xBB: 'OBJ_STATE', 0xBC: 'GHOST_STATE',
    0xC0: 'SPEED_ACC', 0xC4: 'OBJ_TIMER', 0xCC: 'SCORE_LO', 0xCE: 'SCORE_MID', 0xD0: 'SCORE_HI', 0xD5: 'DISPLAY_BUF', 0xDD: 'SPRITE_PTR',
    0xE9: 'SOUND_TIMER', 0xF0: 'COLOUR_TABLE'}
NAME = {a: n for a, n, c, t in RAM}
LINE = re.compile(r'^(\s+)([A-Z]{3})\s+(\(?)\$([89A-F][0-9A-F])((?:,[XY])?\)?(?:,Y)?)(\s*;.*)?$')
WRITERS = {'sta', 'stx', 'sty'}
RMW = {'inc', 'dec', 'asl', 'lsr', 'rol', 'ror'}


def static_users(rom):
    cov = []
    for l in (ROOT / 'reference' / 'f6' / 'coverage_all.txt').read_text().split('\n'):
        if re.match(r'F[0-9A-F]{3}  ', l):
            cov.extend(l[6:].strip())
    users = collections.defaultdict(lambda: collections.defaultdict(int))
    for off in range(4096):
        if cov[off] != 'C':
            continue
        mn, mode, cy, by, _ = asm.TABLE[rom[off]]
        if mode in ('ZERO_PAGE', 'ZERO_PAGE_X', 'ZERO_PAGE_Y', 'INDIRECT_X', 'INDIRECT_Y'):
            ad = rom[off + 1]
        elif mode in ('ABSOLUTE', 'ABSOLUTE_X', 'ABSOLUTE_Y'):
            ad = rom[off + 1] | rom[off + 2] << 8
            if ad > 0xFF:
                continue
        else:
            continue
        if ad < 0x80:
            continue
        kind = 'write' if mn in WRITERS else ('change' if mn in RMW else 'read')
        if mode.startswith('INDIRECT'):
            kind = 'pointer'
        elif mode.endswith('_X') or mode.endswith('_Y'):
            kind += '(indexed)'
        users[ad][kind] += 1
    return users


def traces():
    rows = {}
    for f in sorted((ROOT / 'reference' / 'baseline').glob('BL-???.trace')):
        r = []
        for l in f.read_text().split('\n'):
            p = l.split(',')
            if len(p) >= 6 and p[0].isdigit():
                r.append(bytes.fromhex(p[5]))
        rows[f.stem] = r
    return rows


def main():
    rom = pathlib.Path(sys.argv[1]).resolve().read_bytes()
    problems, notes = [], []
    # 1. the table is complete
    addrs = [a for a, *_ in RAM]
    if addrs != list(range(0x80, 0x100)):
        problems.append('the name table does not hold each of $80-$FF exactly once')
    if len(set(NAME.values())) != len(NAME):
        problems.append('a name is used twice')
    users = static_users(rom)
    tr = traces()
    stat = [dict(values=collections.Counter(), changes=0) for _ in range(128)]
    nframes = 0
    for name, rows in tr.items():
        prev = None
        for r in rows:
            nframes += 1
            for i in range(128):
                stat[i]['values'][r[i]] += 1
                if prev is not None and prev[i] != r[i]:
                    stat[i]['changes'] += 1
            prev = r
    # 2. checks
    checks = []
    # frame counter: +1 every frame except the moments the game sets it to 1
    jumps = collections.Counter()
    for name, rows in tr.items():
        for a, b in zip(rows, rows[1:]):
            d = (b[0] - a[0]) & 255
            if d != 1:
                jumps[(d, b[0])] += 1
    total_jumps = sum(jumps.values())
    ok = all(b == 2 for (d, b) in jumps)  # Select stores 1 ($F8E5) and the same frame adds 1 ($F812)
    checks.append(('FRAME_COUNTER goes up by 1 every frame, except %d times where Select sets it to 1 and the frame adds 1 (so it reads 2)' % total_jumps, ok, sorted(jumps.items())[:4]))
    # player block swap
    swaps = bad = 0
    for name, rows in tr.items():
        for a, b in zip(rows, rows[1:]):
            if (a[0xE6 - 0x80] ^ b[0xE6 - 0x80]) & 0x80 and a[0xE6 - 0x80] & 0x40:
                swaps += 1
                # the lives byte of the player who comes in is lowered by one in the same frame ($F53E)
                same = a[0x08:0x18] == b[0x1C:0x2C] and a[0x19:0x1C] == b[0x2D:0x30] and a[0x1C:0x2C] == b[0x08:0x18] and a[0x2D:0x30] == b[0x19:0x1C]
                if not (same and a[0x2C] - 1 == b[0x18] and a[0x18] == b[0x2C]):
                    bad += 1
    checks.append(('block $88-$9B and $9C-$AF exchange when the turn changes (%d turn changes, %d not an exchange; the incoming lives are also lowered by 1)' % (swaps, bad), swaps > 0 and bad == 0, None))
    # BCD digits of scores in play
    badbcd = 0
    played = 0
    for name, rows in tr.items():
        for r in rows:
            if r[0x82 - 0x80] == 0:
                played += 1
                for a in (0xCD, 0xCF, 0xD1, 0xCE, 0xD0):
                    v = r[a - 0x80]
                    if (v >> 4) > 9 or (v & 15) > 9:
                        badbcd += 1
    checks.append(('score bytes hold decimal digits in every frame of play (%d frames, %d wrong; P1 low byte excluded: it holds the game number during Select)' % (played, badbcd), badbcd == 0, None))
    # positions
    xs = [stat[i]['values'] for i in range(0x31, 0x36)]
    xmin = min(min(v) for v in xs)
    xmax = max(max(v) for v in xs)
    checks.append(('Pac-Man and ghost x always between $%02X and $%02X' % (xmin, xmax), 4 <= xmin and xmax <= 0x94, None))
    ymax = max(max(stat[i]['values']) for i in range(0x37, 0x3B))
    checks.append(('ghost y never above $%02X' % ymax, ymax <= 0x5F, None))
    # stack area written by no instruction directly
    direct = [a for a in range(0xF6, 0x100) if users.get(a)]
    checks.append(('no instruction names a stack byte $F6-$FF directly (%d do)' % len(direct), not direct, direct))
    # bytes no instruction names
    unnamed_use = [a for a in range(0x80, 0x100) if not users.get(a)]
    unused_ok = all(a in unnamed_use for a in (0xEA, 0xEB)) and all(max(stat[a - 0x80]['values']) == 0 for a in (0xEA, 0xEB))
    checks.append(('UNUSED_EA/EB: no instruction names them and they are 0 in every frame', unused_ok, None))
    # lives 0..3 and never above 9
    lmax = max(stat[0x18]['values'])
    checks.append(('LIVES never above %d' % lmax, lmax <= 9, None))
    # the position bytes of ghosts really are written for the ghost routine
    # 3. the named listing
    src = (DIS / 'pacman_frame.asm').read_text(encoding='utf-8').split('\n')
    out, used = [], collections.Counter()
    for line in src:
        m = LINE.match(line)
        if m:
            ind, mn, par, hx, tail, com = m.groups()
            addr = int(hx, 16)
            idx = (tail in (',X', ',Y', ',X)')) and not par
            if idx and addr in ARRAY_ALIASES:
                nm = ARRAY_ALIASES[addr]
            else:
                nm = NAME[addr]
            used[nm] += 1
            line = '%s%s %s%s%s%s' % (ind, mn, par, nm, tail, com or '')
        out.append(line)
    # absolute-width accesses stay numbers: add the name as a comment
    out2 = []
    for line in out:
        m = re.match(r'^(\s+[A-Z]{3}\s+\$00([89A-F][0-9A-F])(?:,[XY])?)(\s*;.*)$', line)
        if m:
            line = '%s%s [%s]' % (m.group(1), m.group(3), NAME[int(m.group(2), 16)])
        out2.append(line)
    header = ['; Pac-Man (Atari 2600), listing with the names of the 128 bytes of RAM (Stage F7). LOCAL ONLY.',
              '; Every zero-page operand $80-$FF is a name; "[NAME]" in a comment marks a four-digit address that must stay a number.',
              '; Names marked "?" in docs\\F7_RAM_MAP.md are uncertain. An array alias (MAZE, OBJ_X ...) is the first byte of an indexed access.']
    for a, n, c, t in RAM:
        header.append('%-16s = $%02X%s' % (n, a, '   ; ?' if c == 'Q' else ''))
    for a, n in sorted(ARRAY_ALIASES.items()):
        header.append('%-16s = $%02X   ; alias: indexed access starting at %s' % (n, a, NAME[a]))
    text = '\n'.join(header + [''] + out2)
    # put the equates before the first hardware equate: simply first in the file
    (DIS / 'pacman_ram.asm').write_text(text, encoding='utf-8', newline='')
    back, labels = asm.assemble(text, ORG)
    same = back == rom
    (DIS / 'pacman_ram_rebuilt.bin').write_bytes(back)
    # 4. report
    sure = sum(1 for a, n, c, t in RAM if c == 'S')
    likely = sum(1 for a, n, c, t in RAM if c == 'L')
    unsure = sum(1 for a, n, c, t in RAM if c == 'Q')
    lines = ['bytes named: %d (sure %d, likely %d, uncertain "?" %d)' % (len(RAM), sure, likely, unsure),
             'frames in the 70 traces: %d' % nframes, 'operands replaced by names: %d' % sum(used.values()), 'checks:']
    for what, ok_, extra in checks:
        lines.append('  %s: %s' % ('PASS' if ok_ else 'FAIL', what))
        if not ok_:
            problems.append(what)
    lines += ['problems: %s' % (problems or 'none'),
              'ROM SHA-1:     %s' % hashlib.sha1(rom).hexdigest(), 'rebuilt SHA-1: %s' % hashlib.sha1(back).hexdigest(),
              'result: %s' % ('BYTE-IDENTICAL' if same else 'DIFFERENT')]
    (DIS / 'ram_stats.txt').write_text('\n'.join(lines) + '\n', encoding='utf-8')
    print('\n'.join(lines))
    # 5. the document
    conf = {'S': 'sure', 'L': 'likely', 'Q': '?'}
    d = ['# Stage F7: the 128 bytes of RAM (2026-10-26)', '',
         '`python -I tools\\f7_ram.py rom\\pacman.a26` (after `f2`..`f6`) names every byte of the console\'s memory, `$80-$FF`, replaces the zero-page operands of the listing by these names (`reference\\disasm\\pacman_ram.asm`, local only) and requires the rebuilt ROM to be byte-identical. This document is written by that tool.', '',
         '**How the names were found:** each byte was followed through every instruction that touches it (the code map of F6; the frame loop of F5) and watched over all 70 baseline traces (%d frames). A name is **sure** when the code and the traces agree and the use is unambiguous, **likely** when the code shows the use but a detail is open, **?** when the byte is shared scratch space or its use was not worked out. The meaning of a name is what the byte *holds*, not yet how the game uses it (F9 onward).' % nframes, '',
         'Counts: %d sure, %d likely, %d uncertain.' % (sure, likely, unsure), '',
         '## Checks that test the names', '']
    for what, ok_, extra in checks:
        d.append('- %s: %s' % ('PASS' if ok_ else 'FAIL', what))
    d += ['', '## The map', '', '| Address | Name | Sure? | Seen values (distinct / changes) | Code (read / write / change / indexed) | Meaning |', '| --- | --- | --- | --- | --- | --- |']
    for a, n, c, t in RAM:
        u = users.get(a, {})
        us = ', '.join('%s %d' % (k, v) for k, v in sorted(u.items())) or 'no direct use'
        v = stat[a - 0x80]
        d.append('| $%02X | `%s` | %s | %d / %d | %s | %s |' % (a, n, conf[c], len(v['values']), v['changes'], us, t))
    d += ['', '## Dual use (important for the rebuild)', '',
          '- `$87`, `$C9`, `$CA`, `$D2-$D7`, `$E1-$E3` are used by the kernel (picture) and by the logic for different things; they never hold both at once because the logic runs outside the picture. The rebuild must keep them as separate variables or save them.',
          '- `$D5-$DE` also hold pointers to the score digits while the score line is drawn.',
          '- `$F0-$F5` hold colour *numbers* at the start of a frame and real colours after `$F992`.', '',
          '## Not claimed', '',
          'Bits marked "likely" or "?" are listed with what is certain. The ghost direction code and states, the speed accumulators and the flags in `GAME_FLAGS` are settled in F10-F17. The random number (`RNG_STATE`) is a real shift register, which corrects the Stage E4 note "no randomness found": it is seeded the same way every game, so replays stay identical, but it is the game\'s own random source (F14).']
    (ROOT / 'docs' / 'F7_RAM_MAP.md').write_text('\n'.join(d) + '\n', encoding='utf-8')
    return 0 if same and not problems else 1


if __name__ == '__main__':
    sys.exit(main())
