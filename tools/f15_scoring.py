"""Stage F15: scoring, the bonus item and lives - rules written from the code and checked against the recorded games.

  python -I tools\\f15_scoring.py <rom>      (after f1-f14; needs the 70 baseline traces and the flag logs reference\\f12 made by f12_collisions.py)

A. Score: every frame, the change of the score of the player in turn equals the sum of the amounts the code adds (wafer +1, power pill +5,
   ghost +20/40/80/160 by the pill's counter, bonus item +100); the other score never changes in play.
B. Lives: every change of the lives byte is explained (new map +1 up to 9, death resolution -1, game start, change of turn); there is no
   extra life for a score.
C. Bonus item: the timer at FRAME_START ($F812-$F833) is predicted every 256 frames.
Writes docs\\F15_SCORING.md. The ROM is only read.
"""
import collections
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / 'tools'))
import f12_collisions as f12  # noqa: E402

BS = chr(92)
ADD = {0: 20, 1: 40, 2: 80, 3: 160}


def flags(entry):
    ev = collections.defaultdict(dict)
    p = ROOT / 'reference' / 'f12' / ('w_%s.csv' % entry)
    for line in p.read_text().split('\n'):
        if line:
            fr, pc, a = line.split(',')
            ev[int(fr)][pc] = int(a)
    return ev


def main():
    stats = collections.Counter()
    bad = collections.defaultdict(list)
    maxscore = 0
    lives_changes = collections.Counter()
    bonus_seen = collections.Counter()
    for n in range(1, 71):
        entry = 'BL-%03d' % n
        if entry in ('BL-065', 'BL-066'):
            continue
        rows = f12.trace(entry)
        ev = flags(entry)
        for fr in range(1, len(rows)):
            inp, post = rows[fr]
            pre = rows[fr - 1][1]
            stats['frames'] += 1
            p2 = bool(pre[0x66] & 0x80)
            two = bool(pre[0x66] & 0x40)
            for who in (0, 1):
                maxscore = max(maxscore, f12.score(post, bool(who)))
            # ---- D. end of a death: one life less, or game over when none is left ($F538-$F550) ----
            if (pre[0x67] & 0x40) and not (post[0x67] & 0x40) and not (inp & 0x60) and pre[2] == 0 and not two:
                if pre[0x18] > 0:
                    ok_d = post[0x18] == pre[0x18] - 1 and post[2] == 0 and post[0x67] & 0x08
                    stats['D: death with lives left: one life less, new try, right' if ok_d else 'D: death with lives left WRONG'] += 1
                else:
                    ok_d = post[2] == 0x5F + (1 if post[0] == 0 else 0) and post[0x67] == 0
                    stats['D: death with no lives left: game over (attract timer $5F, flags 0), right' if ok_d else 'D: game over WRONG'] += 1
            # ---- C. bonus timer at FRAME_START (end of the frame) ----
            d = ev.get(fr, {})
            bonus_hit = 'f575' in d and d['f575'] & 0x40
            skip_start = (inp & 0x60) or pre[2] != 0 or post[2] != pre[2]
            if not bonus_hit and not (inp & 0x60) and (pre[2] == 0) == (post[2] == 0) and post[0x66] & 0x40 == pre[0x66] & 0x40 and post[0x67] & 0x10 == pre[0x67] & 0x10:
                # predicted values after FRAME_START: starting from the memory after the bottom logic; for an unmoved timer that is the row before
                fc_wrap = (post[0] == 0)
                tick = pre[1]
                tm = pre[0x6C]
                e7 = pre[0x67]
                att = pre[2]
                if fc_wrap:
                    stats['C: frames with the 256-frame tick'] += 1
                    tick = (tick + 1) & 0xFF
                    if tick == 0:
                        att = (att + 1) & 0xFF if True else att
                        if att == 0:
                            att = 1
                        bonus_seen['tick counter wrapped (attract)'] += 1
                    else:
                        tm = (tm - 1) & 0xFF
                        if tm == 0:
                            e7 ^= 2
                            tm = 0x12
                            if e7 & 2:
                                tm = 3
                                if e7 & 0x20:
                                    tm = 2
                        if att:
                            att = (att + 1) & 0xFF
                            if att == 0:
                                att = 1
                    got = (post[0x6C], post[0x67] & 2, post[2])
                    want = (tm, e7 & 2, att)
                    if got == want:
                        stats['C: tick frames right'] += 1
                        if (pre[0x67] ^ e7) & 2:
                            bonus_seen['item shown' if e7 & 2 else 'item hidden'] += 1
                    else:
                        stats['C: tick frames WRONG'] += 1
                        if len(bad['C']) < 6:
                            bad['C'].append((entry, fr, 'pre tm %02X e7 %02X tick %02X att %02X' % (pre[0x6C], pre[0x67], pre[1], pre[2]), 'got', got, 'want', want))
                else:
                    if (post[0x6C], post[0x67] & 2) == (pre[0x6C], pre[0x67] & 2):
                        stats['C: other frames: timer and item flag unchanged, right'] += 1
                    else:
                        stats['C: other frames: timer or item flag changed WRONG'] += 1
                        if len(bad['C2']) < 6:
                            bad['C2'].append((entry, fr, pre[0x6C], post[0x6C], pre[0x67], post[0x67]))
            # ---- A. score, B. lives: only frames of normal play ----
            if inp & 0x60 or pre[2] != 0 or post[2] != 0:
                stats['skipped (A,B): Select/Reset or title screen'] += 1
                continue
            if (pre[0x66] ^ post[0x66]) & 0x80:
                stats['skipped (A,B): change of turn'] += 1
                continue
            if pre[0x67] & 0x10 or post[0x67] & 0x10:
                stats['skipped (A,B): game start'] += 1
                continue
            # score of the player in turn
            delta = f12.score(post, p2) - f12.score(pre, p2)
            other = f12.score(post, not p2) - f12.score(pre, not p2)
            wafers = f12.wafers_gone(pre, post)
            pills = sum(1 for b in (0x04, 0x08, 0x10, 0x20) if pre[0x1A] & b and not post[0x1A] & b)
            ghost = 0
            if 'f5d9' in d and d['f5d9'] & 0x80 and pre[0x1A] & 0x80:
                X = pre[0] & 3
                if not pre[0x3C + X] & 8 and post[0x3C + X] & 8:
                    ghost = ADD[pre[0x1A] & 3]
            bonus = 100 if bonus_hit else 0
            newmap_now = (post[0x1A] & 0x3C) == 0x3C and (pre[0x1A] & 0x3C) != 0x3C and post[0x08:0x18] == bytes([0xFF, 0xFF, 0xFE, 0xFF, 0xFF, 0xFF, 0x7F] + [0xFF] * 9)
            if newmap_now:
                wafers = 1              # the last wafer is eaten in the same frame, then the maze is refilled
                stats['A:   frames with a new map'] += 1
            want = wafers + 5 * pills + ghost + bonus
            stats['A: frames checked'] += 1
            if post[0x67] & 0x80 and not pre[0x67] & 0x80:
                pass
            if delta == want and other == 0:
                stats['A: score change right'] += 1
                if want:
                    stats['A:   frames with points'] += 1
                stats['A:   wafers +1: %d' % 0] += 0
                stats['A:   total wafers'] += wafers
                stats['A:   total power pills'] += pills
                stats['A:   total ghosts'] += 1 if ghost else 0
                stats['A:   total bonus items'] += 1 if bonus else 0
            else:
                stats['A: score change WRONG'] += 1
                if len(bad['A']) < 8:
                    bad['A'].append((entry, fr, 'delta', delta, 'want', want, 'wafers', wafers, 'pills', pills, 'ghost', ghost, 'bonus', bonus, 'other', other))
            # lives of the player in turn and of the other one
            for name, a in (('in turn', 0x18), ('waiting', 0x2C)):
                dl = post[a] - pre[a]
                if dl:
                    newmap = (post[0x1A] & 0x3C) == 0x3C and (pre[0x1A] & 0x3C) != 0x3C and all(post[0x08 + k] == 0xFF or k in (2, 6) for k in range(16))
                    death = (pre[0x67] & 0x40) and not (post[0x67] & 0x40)
                    kind = ('new map' if newmap else 'death resolved' if death else 'other')
                    lives_changes[(name, dl, kind)] += 1
                    if kind == 'other' and len(bad['B']) < 8:
                        bad['B'].append((entry, fr, name, pre[a], post[a], hex(pre[0x67]), hex(post[0x67])))
    lives_ok = all(k[2] != 'other' for k in lives_changes)
    for k, v in sorted(stats.items()):
        print('%8d  %s' % (v, k))
    for k, v in sorted(lives_changes.items()):
        print('lives', k, v)
    for k, v in sorted(bonus_seen.items()):
        print('bonus', k, v)
    for k, v in bad.items():
        for x in v:
            print(k, x)
    print('highest score seen:', maxscore)
    ok = (not stats['A: score change WRONG'] and not stats['C: tick frames WRONG'] and not stats['C: other frames: timer or item flag changed WRONG']
          and lives_ok and not stats['D: death with lives left WRONG'] and not stats['D: game over WRONG'] and stats['A: frames checked'] > 100000)
    print('result: %s' % ('OK' if ok else 'MISMATCH'))
    return stats, lives_changes, bonus_seen, maxscore, ok


def write_doc(stats, lives, bonus, maxscore, ok):
    d = ['# Stage F15: scoring, the bonus item and lives (2026-10-26)', '',
         '`python -I tools%sf15_scoring.py rom%spacman.a26` states the rules and checks them on every frame of the 70 recorded games (the 2 test games BL-065/066 are skipped). This document is written by that tool.' % (BS, BS), '',
         '## Result: %s' % ('all rules hold' if ok else 'MISMATCH'), '',
         '| Count | What |', '| --- | --- |']
    d += ['| %d | %s |' % (v, k) for k, v in sorted(stats.items()) if not k.endswith(': 0')]
    d += ['', 'Changes of the lives byte seen: ' + '; '.join('%s %+d: %s (%d)' % (k[0], k[1], k[2], v) for k, v in sorted(lives.items())) + '. Highest score seen: %d.' % maxscore, '',
          '## Score', '',
          '- Each player has a 6-digit decimal score in three bytes (low, middle, high; `$CC/$CE/$D0` for player 1, `$CD/$CF/$D1` for player 2), added in decimal mode by `ADD_SCORE` (`$FC45`) to the player whose turn it is (`PLAYER_MODE` bit 7). Only four places call it, and they are all in the bottom logic:',
          '', '| Amount number Y | Points | Called from | Event |', '| --- | --- | --- | --- |',
          '| 4 | +1 | `$F6A2` | a wafer eaten (F11) |', '| 5 | +5 | `$F5C8` | a power pill eaten (F11) |',
          '| 0-3 | +20, +40, +80, +160 | `$F5F7` | a scared ghost eaten; Y is the pill counter (F12) |', '| 6 | +100 | `$F57D` | the bonus item eaten (F12) |', '',
          '- The change of the score of the player in turn equals the sum of those amounts in every frame (the last wafer of a map counts +1 in the frame that refills the map); the other player score never changes. The amounts are in the tables `$FF06` (tens and ones) and `$FF0C` (hundreds).',
          '- There are only six digits: a carry out of the top byte is lost, so the score would wrap from 999,999 to 0 (analysis of the code; not seen, the highest score in the recordings is %d).' % maxscore,
          '- **There is no extra life for reaching a score**: nothing in `ADD_SCORE` or its callers looks at the score, and the lives byte never changes except as listed below (checked on all frames).', '',
          '## Lives', '',
          '- A game starts with 3 (`NEW_GAME_SETUP` `$FBF4`); the markers on the status line show the lives (`LIVES_MARKERS`). Counting the one in play, that is four tries.',
          '- +1 when a map is cleared (up to 9, F11); nothing else adds lives.',
          '- At the end of a death (when the event timer, which started at `$3F`, runs out; F17 follows the sequence): if lives are left, one less and the next try starts frozen; if none are left, game over: `GAME_FLAGS` = 0 and `ATTRACT_SHIFT` = `$5F` (the title screen follows; plus one if the frame counter wrapped in the same frame).',
          '- Two-player games swap the 20-byte player block (maze, lives, level, pills, colour) at the change of turn (F7); the recordings never played player 2 (F9), so only the code, not a recording, shows that part.', '',
          '## The bonus item', '',
          '- The item (drawn with missile 1 and the ball, F12) is shown while `GAME_FLAGS` bit 1 is set. `BONUS_TIMER` (`$EC`) starts at `$0C` at power-on and counts down by one each time the frame counter wraps (every 256 frames), except in the tick where the idle counter `TICK256` also wraps (then the attract timer goes up instead).',
          '- When the timer reaches 0 the item flag flips and the timer is reloaded: **item turns on -> 3 ticks (768 frames), or 2 ticks when `GAME_FLAGS` bit 5 is set; item turns off -> `$12` = 18 ticks (4,608 frames)**. Eating it (+100) turns it off at once and sets the timer to `$12`.',
          '- The frames of the 256-frame tick were predicted exactly (timer, item flag, attract timer): %d of %d; the item turned on %d times and off by the timer %d times in the recordings; in every other frame the timer and the flag do not change.' % (stats['C: tick frames right'], stats['C: frames with the 256-frame tick'], bonus['item shown'], bonus['item hidden']),
          '- The value is always +100, whatever the level. Bit 5 of `GAME_FLAGS` is set from the difficulty switch of the player in turn (`$F90D-$F926`); the shorter showing for it comes from the code only: no recording uses that switch position while the item is on the way.', '',
          '## Not claimed', '',
          '- Two-player scoring and lives (no recording), the 999,999 wrap (not reached), the difficulty-switch effect on the bonus (code only). The death sequence timing is F17; level changes are F16.']
    (ROOT / 'docs' / 'F15_SCORING.md').write_text('\n'.join(d) + '\n', encoding='utf-8')


if __name__ == '__main__':
    r = main()
    write_doc(*r)
    sys.exit(0 if r[4] else 1)
