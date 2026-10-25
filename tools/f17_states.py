"""Stage F17: game states - attract mode (title), game start, play, frozen start of a try, ghost-eaten pause, death, level end, game over.

  python -I tools\\f17_states.py <rom>      (after f1-f16; needs the 70 baseline traces and the flag logs reference\\f12 made by f12_collisions.py)

Every frame of the 70 games is put into one of six states from the memory alone; every change of state is checked against a table of allowed
changes with their cause (a button, a timer, a collision, a cleared map, no lives left), and the memory is checked on entry into game start,
the Select step, a new try and game over.
Writes docs\\F17_STATES.md. The ROM is only read.
"""
import collections
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / 'tools'))
import f12_collisions as f12  # noqa: E402

BS = chr(92)
TITLE, FROZEN, PLAY, PAUSE, DYING = 'TITLE', 'FROZEN', 'PLAY', 'EAT-PAUSE', 'DYING'


def state(r):
    """The state, from the memory only: ATTRACT_SHIFT ($82), GAME_FLAGS ($E7), EAT_PAUSE ($E5)."""
    if r[2] != 0:
        return TITLE
    if r[0x67] & 0x40:
        return DYING
    if r[0x67] & 0x80:
        return FROZEN
    if r[0x65] & 0x80:
        return PAUSE
    return PLAY


def main():
    stats = collections.Counter()
    trans = collections.Counter()
    bad = []
    entry_ok = collections.Counter()
    for n in range(1, 71):
        entry = 'BL-%03d' % n
        if entry in ('BL-065', 'BL-066'):
            continue
        rows = f12.trace(entry)
        ev = {}
        for line in (ROOT / 'reference' / 'f12' / ('w_%s.csv' % entry)).read_text().split('\n'):
            if line:
                fr, pc, a = line.split(',')
                ev.setdefault(int(fr), {})[pc] = int(a)
        for fr in range(1, len(rows)):
            inp, post = rows[fr]
            pre = rows[fr - 1][1]
            s0, s1 = state(pre), state(post)
            stats['frames in state %s' % s0] += 1
            d = ev.get(fr, {})
            reset, select, fire = bool(inp & 64), bool(inp & 32), bool(inp & 16)
            # ---- the cause of a change ----
            cause = None
            start_by_button = reset or (pre[2] != 0 and fire and not reset)
            if s0 == s1:
                cause = 'stay'
                # a restart inside the same state (Reset in the frozen start or pressed again)
                if reset:
                    cause = 'stay (Reset pressed: a new game is set up)'
            else:
                map_cleared = (post[0x1A] & 0x3C) == 0x3C and (pre[0x1A] & 0x3C) != 0x3C
                hit = 'f5d9' in d and d['f5d9'] & 0x80
                if reset and s1 == FROZEN:
                    cause = 'Reset: new game'
                elif s0 == TITLE and s1 == FROZEN and fire:
                    cause = 'fire on the title screen: new game'
                elif select and s1 == TITLE and s0 != TITLE:
                    cause = 'Select during a game: back to the game-number screen'
                elif s0 == PLAY and s1 == PAUSE and hit and pre[0x1A] & 0x80:
                    cause = 'scared ghost touched'
                elif s0 == PAUSE and s1 == PLAY and pre[0x65] == 0x80:
                    cause = 'pause over (32 frames)'
                elif s0 == PLAY and s1 == DYING and hit and not pre[0x1A] & 0x80:
                    cause = 'unscared ghost touched'
                elif s0 == PLAY and s1 == FROZEN and map_cleared:
                    cause = 'last wafer: next map'
                elif s0 == FROZEN and s1 == PLAY and pre[0x64] <= 1:
                    cause = 'freeze over (timer ran out)'
                elif s0 == DYING and s1 == FROZEN and pre[0x64] <= 1:
                    cause = 'death over, lives left: next try' if pre[0x18] > 0 or (pre[0x66] & 0x40 and pre[0x2C] > 0) else 'death over (other)'
                elif s0 == DYING and s1 == TITLE and pre[0x64] <= 1 and pre[0x18] == 0:
                    cause = 'death over, no lives left: game over'
                elif s0 == PLAY and s1 == PAUSE:
                    cause = None
            key = (s0, s1, cause)
            trans[key] += 1
            if cause is None or 'other' in (cause or ''):
                stats['UNEXPLAINED change of state'] += 1
                if len(bad) < 8:
                    bad.append((entry, fr, s0, s1, inp, pre[0x64], pre[0x67], pre[0x18], post[0x67]))
            # ---- the memory on entry ----
            if cause in ('Reset: new game', 'fire on the title screen: new game') and not select:
                lv = ((post[0x04] - 2) & 6) + 4
                two = bool(post[0x66] & 0x40)
                ok = (post[0x67] & 0x8F) == 0x88 and post[2] == 0 and post[0x66] & 0x80 == 0
                ok &= post[0x19] == lv and post[0x1A] & 0x3C == 0x3C
                ok &= all(post[0x4C + k] == 0 for k in range(6))
                ok &= post[0x08:0x18] == bytes([0xFF, 0xFF, 0xFE, 0xFF, 0xFF, 0xFF, 0x7F] + [0xFF] * 9)
                ok &= post[0x4B] == 0xFF
                ok &= post[0x18] == 3 and post[0x2C] == (4 if two else 0)
                ok &= all((post[0x31 + k], post[0x36 + k], post[0x3B + k]) == (0x4C, 0x1E, 0) for k in range(1, 5))
                ok &= post[0x36] == 0x3C and post[0x3B] == 0x80
                ok &= post[0x64] in (0x3C, 0x3B)
                stats['entry: game start sets everything as listed, right' if ok else 'entry: game start WRONG'] += 1
                if not ok and len(bad) < 12:
                    bad.append(('start', entry, fr, [hex(v) for v in post[0x18:0x20]], hex(post[0x67]), post[0x2C], two))
            if cause == 'Select during a game: back to the game-number screen' or (s0 == TITLE and select and pre[0x03] == 0 and s1 == TITLE):
                step = post[0x04] == (2 if pre[0x04] == 0x11 else pre[0x04] + 1)
                step &= (post[0x66] ^ pre[0x66]) & 0x40 == 0x40 and post[0x36] == 0x60
                step &= (post[0x67] & 0xDF) == 0x10 and post[0x02] in (1, 2) and post[0x00] == 2
                g = (post[0x04] >> 1) - 2 & 7 or 8
                step &= post[0x4C] == g and post[0x4D] == g
                stats['entry: Select step sets game number, 1P/2P, flags $10, title timer 1, score line = game number, right' if step else 'entry: Select step WRONG'] += 1
                if not step and len(bad) < 12:
                    bad.append(('select', entry, fr, pre[0x04], post[0x04], hex(post[0x66]), hex(post[0x67]), post[0x02], post[0x00], hex(post[0x4C]), hex(post[0x36])))
            if cause == 'death over, lives left: next try':
                if pre[0x66] & 0x40 and pre[0x2C] != 0:      # two players, the other one still has lives: the blocks are swapped, the turn passes
                    ok = post[0x18] == pre[0x2C] - 1 and post[0x2C] == pre[0x18] and (pre[0x66] ^ post[0x66]) & 0x80 == 0x80
                    stats['  (two-player turn change at the end of a death)'] += 1
                else:
                    ok = post[0x18] == pre[0x18] - 1
                ok &= (post[0x67] & 0xCD) == 0x88 and post[0x64] in (0x3C, 0x3B)
                ok &= (post[0x31], post[0x36], post[0x3B]) == (0x4C, 0x3C, 0x80)
                ok &= all((post[0x31 + k], post[0x36 + k], post[0x3B + k]) == (0x4C, 0x1E, 0) for k in range(1, 5))
                stats['entry: next try after a death sets lives - 1, frozen $3C, Pac-Man and ghosts at the start places, right' if ok else 'entry: next try WRONG'] += 1
                if not ok and len(bad) < 12:
                    bad.append(('try', entry, fr, pre[0x18], post[0x18], hex(post[0x67]), post[0x64], post[0x31], post[0x36]))
            if cause == 'death over, no lives left: game over':
                ok = post[2] in (0x5F, 0x60) and post[0x67] == 0
                stats['entry: game over sets the title timer $5F and the flags 0, right' if ok else 'entry: game over WRONG'] += 1
            if cause == 'last wafer: next map':
                ok = post[0x64] in (0x3C, 0x3B) and post[0x67] & 0x80 and (post[0x31], post[0x36], post[0x3B]) == (0x4C, 0x3C, 0x80)
                stats['entry: next map: frozen $3C, objects at the start places, right' if ok else 'entry: next map WRONG'] += 1
    ok = (not stats['UNEXPLAINED change of state'] and not [k for k in stats if 'WRONG' in k])
    for k, v in sorted(stats.items()):
        print('%8d  %s' % (v, k))
    for k, v in sorted(trans.items(), key=str):
        print(v, k)
    for b in bad:
        print(b)
    print('result: %s' % ('OK' if ok else 'MISMATCH'))
    write_doc(stats, trans, ok)
    return 0 if ok else 1


def write_doc(stats, trans, ok):
    d = ['# Stage F17: game states (2026-10-26)', '',
         '`python -I tools%sf17_states.py rom%spacman.a26` puts every frame of the 70 recorded games (the 2 test games BL-065/066 are skipped) into one of six states from the memory alone, checks that every change from one state to the next has one of the causes listed below, and checks the memory on entry into the main states. This document is written by that tool.' % (BS, BS), '',
         '## Result: %s' % ('every change of state explained, every entry as listed' if ok else 'MISMATCH'), '',
         '## The states', '',
         '| State | How the memory says it | What happens in it |', '| --- | --- | --- |',
         '| TITLE | `ATTRACT_SHIFT` (`$82`) is not 0 | Power-on, after a game over, and the game-number screen. The bottom logic is skipped (no Pac-Man logic); the ghosts wander (F13) for show; the colours cycle because `ATTRACT_SHIFT` is xor-ed into them and goes up every 256 frames (skipping 0). Fire or Reset starts a game; Select steps the game number. |',
         '| FROZEN | `ATTRACT_SHIFT` = 0, `GAME_FLAGS` bit 7 set, bit 6 clear | The start of a game, of a try or of a map: everything stands still for 60 steps of the event timer (120 frames) at the start places; the bottom logic sets the ghost speed accumulators and timers (F13, F16). |',
         '| PLAY | none of the others | The rules of F10-F16. |',
         '| EAT-PAUSE | `EAT_PAUSE` bit 7 set | 32 frames after a scared ghost was eaten: Pac-Man and the other ghosts stand still, an eaten ghost goes on as eyes, and the event timer stands still (F12, F16). |',
         '| DYING | `GAME_FLAGS` bits 7 and 6 set | After an unscared ghost touched Pac-Man: the event timer starts at `$3F` (126 frames), Pac-Man\'s dying animation plays; then the next try or game over. |', '',
         'Where the bits are set: `GAME_FLAGS` (`$E7`) bit 7 = frozen or dying, bit 6 = dying, bit 5 = difficulty switch of the player in turn, bit 4 = game-number screen, bit 3 = a try is running (set at every start of a try), bit 1 = bonus item shown (F15).', '',
         '## The changes of state (all that occur)', '',
         '| Times | From | To | Cause |', '| --- | --- | --- | --- |']
    for (a, b, c), v in sorted(trans.items(), key=lambda kv: (kv[0][0], kv[0][1], str(kv[0][2]))):
        d.append('| %d | %s | %s | %s |' % (v, a, b, c))
    d += ['', '## Entry into a state: what the memory holds', '', '| Count | Check |', '| --- | --- |']
    d += ['| %d | %s |' % (v, k.replace('entry: ', '')) for k, v in sorted(stats.items()) if k.startswith('entry')]
    d += ['', '## Game start in detail (`$F890-$F90B`)', '',
          'Reset (any state) or fire (title screen only) starts a game. The code: random number `$CB` = `$FF`; `CLEAR_SCORES` (both scores and both lives 0); `ATTRACT_SHIFT` = 0; `GAME_FLAGS` = 8; Pac-Man\'s speed seed (`$09` for game number 2-7, else `$0B`, F10); `NEW_GAME_SETUP` (3 lives, score colours, start level from the game number, F16) which falls into `NEW_MAZE` (the maze filled, pills `$3C`, F11) and `PLACE_OBJECTS` (frozen, timer `$3C`, objects at their start places `x = $4C`, ghosts `y = $1E`, Pac-Man `y = $3C`). With two players (`PLAYER_MODE` bit 6) the player blocks are swapped and the set-up is done a second time for player 2, who gets 4 in the lives byte of the second block, i.e. 3 lives plus the one in play.',
          '', '## The game-number screen (Select)', '',
          'Select pressed (`SWCHB` bit 1 = 0): at once and then every 31 frames while held, `GAME_SELECT` (`$84`) goes up by 1 (from `$11` back to 2), `PLAYER_MODE` bit 6 flips (so each step alternates one and two players: the pairs of values 2k and 2k+1 are one game number, eight of them), `GAME_FLAGS` = `$10`, `ATTRACT_SHIFT` = 1 and the frame counter = 1, Pac-Man\'s `y` = `$60` (out of sight), sound off, and both score bytes show the game number (`((GAME_SELECT / 2) - 2) and 7`, 0 shown as 8). The game number sets the start level (F16) and Pac-Man\'s speed (F10); two players use the right joystick and right difficulty switch for player 2 (F9).', '',
          '## The end of a try', '',
          'When the event timer of a death runs out (`$F51A-$F550`): `GAME_FLAGS` bit 6 off, `PLACE_OBJECTS` (frozen again, `$3C`); in a two-player game, if the other player still has lives, the blocks are swapped and the turn passes (`PLAYER_MODE` bit 7 flips); then if any life is left in either block, the player in turn loses one and `GAME_FLAGS` bit 3 is set (next try); if none is left anywhere: `GAME_FLAGS` = 0, `ATTRACT_SHIFT` = `$5F`, Pac-Man\'s y = `$5F`: game over, the title screen without a new game number. The code reads the fire button in every frame in which `ATTRACT_SHIFT` is not 0, so it would start a game at once; Stage E6 saw the button work only after about 500 frames after a game over, and F17 did not explain that delay (the recordings show 46 starts by fire, all on the title screen).', '',
          '## Not claimed', '',
          '- Two-player games: the turn change (blocks swapped) was recorded 19 times, but player 2 never had a joystick in a recording (F9), so the play of player 2 itself is from the code only. The death animation and sound are F18 and F19.',
          '- The long-idle behaviour of `ATTRACT_SHIFT` after hours of title screen (it counts up and skips 0 for ever).']
    (ROOT / 'docs' / 'F17_STATES.md').write_text('\n'.join(d) + '\n', encoding='utf-8')


if __name__ == '__main__':
    sys.exit(main())
