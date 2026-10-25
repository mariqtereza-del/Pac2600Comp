"""Stage E2: makes the baseline entries about starting up (BL-002 .. BL-025) and checks them.

  python -I tools\\e2_entries.py <rom>

For each entry it writes baseline\\<ID>.inputs (the input), runs the host with --trace twice through tools\\baseline.py (the two
traces must be byte-identical; the SHA-256 goes into the entry), then reads the trace and writes the plain-words observations.
Every number in the observations is computed from the trace; nothing is typed in by hand. Memory bytes are named by address;
their meanings are the working labels of Stages C6/C7 (docs\\CONTROLS.md, docs\\CAPTURES.md), not yet confirmed (Phase F).
The input rhythm is the one Stage C6 used for Game Select: held 10 frames, one press every 60 frames.
"""
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
BASE = ROOT / 'baseline'
OUT = ROOT / 'reference' / 'baseline'
SHA = '0940fea7f04cdb6d4b90c5ad1a7e344e68f6dbb1'
SELECT, RESET, FIRE = 32, 64, 16
# the C6 table (docs\CONTROLS.md) of $99 at game start, games 1-8: the only byte of that table that depends on the selection
# ($81 and $EC turned out to be an attract-mode timer, see the entries BL-021 to BL-025 and DECISIONS.md T-E2-1)
C6_99 = [0x08, 0x0A, 0x04, 0x06, 0x08, 0x0A, 0x04, 0x06]


def write_inputs(name, bits):
    out = ['PacMan2600-inputs 1', 'rom_sha1=' + SHA, 'frames=%d' % len(bits)]
    prev = None
    for f, b in enumerate(bits):
        if b != prev:
            out.append('%d %d' % (f, b))
            prev = b
    (BASE / (name + '.inputs')).write_text('\n'.join(out) + '\n')


def press(bits, start, mask, length=10):
    for f in range(start, min(start + length, len(bits))):
        bits[f] |= mask


def read_trace(name):
    lines = (OUT / (name + '.trace')).read_text(encoding='ascii').split('\n')
    return [l.split(',') for l in lines[3:] if l]


class T:
    def __init__(self, rows):
        self.rows = rows
        self.ram = [bytes.fromhex(r[5]) for r in rows]
        self.n = len(rows)

    def b(self, f, addr):
        return self.ram[f][addr - 0x80]

    def lives(self, f): return self.b(f, 0x98)
    def paused(self, f): return bool(self.b(f, 0xD2) & 0x40)
    def players(self, f): return 2 if self.b(f, 0xE6) & 0x40 else 1
    def game(self, f): return self.b(f, 0xCC)
    def pic(self, f): return self.rows[f][3]
    def aud(self, f): return self.rows[f][4]
    def inp(self, f): return int(self.rows[f][1])

    def first(self, start, test):
        for f in range(start, self.n):
            if test(f):
                return f
        return None


def secs(frames):
    return '%.1f seconds' % (frames / 60.0)


def start_facts(t, press_frame):
    """What happens after the press at press_frame that starts a game (Reset or Fire)."""
    facts = []
    s = t.first(press_frame, lambda f: t.lives(f) == 3)
    if s is None:
        return ['The game did not start within the entry.'], None
    facts.append('Frame %d: the lives counter (byte $98) reads 3: the game starts on frame %d, the %s the press began.'
                 % (s, s, 'same frame' if s == press_frame else '%d frames after' % (s - press_frame)))
    tune = t.first(s, lambda f: not t.paused(f))
    if tune is None:
        return facts + ['The pause flag (byte $D2 bit 6) stays set to the end of the entry.'], None
    facts.append('Frames %d-%d: the pause flag (byte $D2, bit 6) is set while the start tune plays (%d frames, %s). Frame %d: it clears and the game runs.'
                 % (s, tune - 1, tune - s, secs(tune - s), tune))
    facts.append('At frame %d (the first running frame) the two-player bit (byte $E6 bit 6) is %s: %d player%s.'
                 % (tune, 'set' if t.players(tune) == 2 else 'clear', t.players(tune), 's' if t.players(tune) == 2 else ''))
    p60 = press_frame + 60
    facts.append('Frame %d (60 frames after the press): $81 = %02x, $EC = %02x (they add up to 0c), $99 = %02x, $C2 = %02x. Byte $CC reads %02x.'
                 % (p60, t.b(p60, 0x81), t.b(p60, 0xEC), t.b(p60, 0x99), t.b(p60, 0xC2), t.b(p60, 0xCC)))
    death = t.first(tune, lambda f: t.paused(f))
    if death is None:
        facts.append('No joystick is touched; no ghost has caught Pac-Man by the end of the entry (frame %d).' % (t.n - 1))
        return facts, tune
    facts.append('No joystick is touched. Frame %d: the pause flag is set again: Pac-Man is caught and the death begins (%d frames after play began, %s).'
                 % (death, death - tune, secs(death - tune)))
    before = t.lives(tune)
    drop = t.first(death, lambda f: t.lives(f) != before)
    if drop is not None:
        facts.append('Frame %d: the lives counter changes from %d to %d, %d frames after the death began.' % (drop, before, t.lives(drop), drop - death))
    else:
        facts.append('The lives counter does not change before the end of the entry.')
    return facts, tune


def write_entry(name, title, shows, frames, facts, how):
    path = BASE / (name + '.entry.txt')
    text = path.read_text(encoding='utf-8')
    head = text.split('observations:')[0]
    body = ['observations:',
            '  Frames are counted from power-on (frame 0). Memory bytes are named by their address; the meanings are working labels',
            '  (Stages C6 and C7, docs\\CONTROLS.md and docs\\CAPTURES.md), not yet confirmed (Phase F). Every number below was read from the trace.']
    for fact in facts:
        body.append('  - ' + fact)
    body.append('  - How to check it against the real game: ' + how)
    path.write_text(head + '\n'.join(body) + '\n', encoding='utf-8', newline='')


def make(name, title, shows, bits):
    write_inputs(name, bits)
    (BASE / (name + '.entry.txt')).write_text(
        'id: %s\ntitle: %s\nshows: %s\ninputs: %s.inputs\nframes: %d\nrom_sha1: %s\ntrace_sha256: (made by tools\\baseline.py make)\ntrace_bytes: (made by tools\\baseline.py make)\nobservations:\n'
        % (name, title, shows, name, len(bits), SHA), encoding='utf-8', newline='')
    r = subprocess.run([sys.executable, '-I', str(ROOT / 'tools' / 'baseline.py'), 'make', str(ROM), name], capture_output=True, text=True)
    print(r.stdout.strip())
    if r.returncode != 0:
        raise SystemExit(name + ': baseline.py make failed: ' + r.stdout + r.stderr)
    return T(read_trace(name))


def main():
    global ROM
    ROM = pathlib.Path(sys.argv[1]).resolve()
    checks = []

    # BL-002: power-on and the attract mode
    bits = [0] * 3000
    t = make('BL-002', 'Power-on and the attract mode, nothing touched', 'What the original does from power-on for 3,000 frames (50 seconds) with no input at all: the title screen and its slow changes.', bits)
    pill = [f for f in range(1, t.n) if (t.b(f, 0xF2) ^ t.b(f - 1, 0xF2)) & 2]
    silent = max(set(t.aud(f) for f in range(1, t.n)), key=lambda a: sum(1 for f in range(1, t.n) if t.aud(f) == a))
    sounding = [f for f in range(1, t.n) if t.aud(f) != silent]
    gaps = sorted(set(b - a for a, b in zip(pill, pill[1:])))
    pill_late = [b - a for a, b in zip(pill[2:], pill[3:])]
    facts = ['The console switch register reads 3f on every frame (nothing pressed); the lives counter (byte $98) stays %d.' % t.lives(0) if all(t.rows[f][2] == '3f' for f in range(t.n)) and len(set(t.lives(f) for f in range(t.n))) == 1 else 'The switch register or the lives counter changes: see the trace.',
             'Byte $F2 bit 1 (which means "power pill in effect" during play, and means nothing here) toggles on frames %s: the first two gaps are %s frames, then %s.' % (', '.join(map(str, pill)), ' and '.join(str(b - a) for a, b in zip(pill, pill[1:3])), 'always %d frames apart' % pill_late[0] if len(set(pill_late)) == 1 else 'gaps of %s frames' % ', '.join(map(str, pill_late))),
             '%d different pictures appear in the 3,000 frames (the title screen changes colours slowly and the ghost moves).' % len(set(t.pic(f) for f in range(t.n))),
             'Sound: frame 0 has no sound data at all (the first, partial frame); from frame 1 to the end %s.' % ('every frame has the same sound fingerprint: the attract mode is silent' if not sounding else '%d frames carry sound, the first on frame %d and the last on frame %d' % (len(sounding), sounding[0], sounding[-1]))]
    write_entry('BL-002', '', '', 3000, facts, 'switch the console on and watch the title screen for 50 seconds without touching anything.')
    checks.append(('BL-002', len(pill) >= 5 and len(set(pill_late)) == 1))

    # BL-003: Game Select cycles the 16 selections
    bits = [0] * 1400
    presses = [120 + 60 * k for k in range(17)]
    for p in presses:
        press(bits, p, SELECT)
    t = make('BL-003', 'Game Select steps through the 16 selections', 'Game Select pressed 17 times from power-on (held 10 frames, every 60 frames): game 1 to 8, each for one and two players, then back to the start.', bits)
    rows = []
    ok = True
    for k, p in enumerate(presses):
        f = p + 40
        want_game, want_players = ((k + 1) % 16) // 2 + 1, ((k + 1) % 16) % 2 + 1
        ok = ok and t.game(f) == want_game and t.players(f) == want_players
        rows.append('press %2d (begins frame %4d): game %d, %d player%s' % (k + 1, p, t.game(f), t.players(f), 's' if t.players(f) == 2 else ''))
    facts = ['Before the first press (frame 119) byte $CC reads %02x and holds no game number yet; the console is at its power-on selection, game 1 for one player (Stage C6).%s'
             % (t.b(119, 0xCC), ' The console switch register reads 3d (Select low) while a press is held.' if t.rows[presses[0] + 3][2] == '3d' else ''),
             'The selection 40 frames after each press began (byte $CC = game number 1-8; byte $E6 bit 6 = two players):']
    facts += ['    ' + r for r in rows]
    facts.append('The 16th press returns to game 1, one player; the 17th press moves on to game 1, two players: the cycle has 16 selections.' if ok else 'The cycle did NOT follow game 1 to 8, one then two players: see the trace.')
    write_entry('BL-003', '', '', 1400, facts, 'switch on and press Game Select again and again; the status bar shows the game number, two bars mean two players, and after 16 presses it is back at game 1.')
    checks.append(('BL-003', ok))

    # BL-004 .. BL-019: each selection, then Game Reset
    agree = True
    for s in range(16):
        name = 'BL-%03d' % (4 + s)
        game, players = s // 2 + 1, s % 2 + 1
        bits = [0] * 1400
        for k in range(s):
            press(bits, 120 + 60 * k, SELECT)
        reset = 120 + 60 * s + 120
        press(bits, reset, RESET)
        t = make(name, 'Game %d, %d player%s: Select, then Reset starts it' % (game, players, 's' if players == 2 else ''),
                 'Game Select pressed %d time%s from power-on to choose game %d for %d player%s, then Game Reset; nobody plays.' % (s, '' if s == 1 else 's', game, players, 's' if players == 2 else ''), bits)
        facts, tune = start_facts(t, reset)
        if s == 0:
            sel = 'No Select press: the console is at its power-on selection, game 1 for one player (byte $CC reads %d here and does not hold a game number until Select is pressed).' % t.game(reset - 1)
            sel_ok = True
        else:
            sel = 'After the %d Select press%s (frame %d) the selection is game %d, %d player%s (byte $CC = game number, byte $E6 bit 6 = two players).' % (s, '' if s == 1 else 'es', reset - 1, t.game(reset - 1), t.players(reset - 1), 's' if t.players(reset - 1) == 2 else '')
            sel_ok = t.game(reset - 1) == game and t.players(reset - 1) == players
        facts.insert(0, sel + ('' if sel_ok else ' NOT the wanted selection.'))
        if players == 1 and tune is not None:
            same = t.b(reset + 60, 0x99) == C6_99[game - 1] and t.b(reset + 60, 0xC2) == C6_99[game - 1]
            facts.append('$99 and $C2 (60 frames after the press) %s the value measured in Stage C6 for game %d (%02x).' % ('equal' if same else 'DIFFER FROM', game, C6_99[game - 1]))
            agree = agree and same
        write_entry(name, '', '', 1400, facts, 'switch on, press Game Select %d time%s (the status bar shows the game number), press Game Reset and do not touch the joystick.' % (s, '' if s == 1 else 's'))
        checks.append((name, sel_ok and tune is not None))
        print('   %s: %s' % (name, facts[1][:110]))

    # BL-020: Fire starts a game from the attract mode
    bits = [0] * 1400
    press(bits, 120, FIRE)
    t = make('BL-020', 'The fire button starts a game from the attract mode', 'The joystick fire button pressed once in the attract mode (no Game Reset): it starts a game, as Reset does.', bits)
    facts, tune = start_facts(t, 120)
    facts.insert(0, 'The console switch register reads 3f on every frame: the console\'s Reset and Select are not touched; only the fire button is.' if all(t.rows[f][2] == '3f' for f in range(t.n)) else 'The switch register changes: see the trace.')
    write_entry('BL-020', '', '', 1400, facts, 'switch on and press the joystick fire button on the title screen; a game starts. Do not touch anything else.')
    checks.append(('BL-020', tune is not None))

    # BL-021 .. BL-025: Reset at different moments of the attract mode, no Select (the attract timer in $81 and $EC)
    timer = []
    for k, r in enumerate((600, 1000, 1400, 1800, 2400)):
        name = 'BL-%03d' % (21 + k)
        bits = [0] * (r + 900)
        press(bits, r, RESET)
        t = make(name, 'Game Reset pressed at frame %d of the attract mode' % r, 'Game Reset pressed once at frame %d after power-on (game 1, one player, no Select); nobody plays. $81 and $EC read the attract timer at the moment of the press.' % r, bits)
        facts, tune = start_facts(t, r)
        v81, vec = t.b(r + 60, 0x81), t.b(r + 60, 0xEC)
        timer.append((r, v81, vec))
        facts.insert(0, 'Reset is pressed at frame %d. At frame %d: $81 = %02x, which is %d = (%d / 256 rounded down), and $EC = %02x = 0c - $81.' % (r, r + 60, v81, (r + 60) // 256, r + 60, vec))
        write_entry(name, '', '', r + 900, facts, 'switch on, wait about %d seconds on the title screen, press Game Reset and do not touch the joystick.' % (r // 60))
        checks.append((name, v81 == (r + 60) // 256 and v81 + vec == 12 and tune is not None))
    print('timer rows (reset frame, $81, $EC):', timer)

    bad = [n for n, ok in checks if not ok]
    print('entries made: %d; checks failed: %s; 1-player starts equal the Stage C6 table: %s' % (len(checks), bad or 'none', agree))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
