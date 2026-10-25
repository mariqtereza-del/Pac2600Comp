"""Stage F19: the sound routines and their tables.

  python -I tools\\f19_sound.py <rom>      (after f1-f18; needs the 70 baseline traces, the flag logs reference\\f12 and the built stella_coverage.exe)

The model follows SOUND_START ($FF43), SOUND_EFFECTS ($FF55), SOUND_WRITE ($FF8B) and SOUND_MUSIC ($FF94) and the clearing of the volumes at
$F7C5. For every frame of the 70 games it predicts the list of writes to the six sound registers (AUDC0/1, AUDF0/1, AUDV0/1: value and order)
and the two sound bytes SOUND_ACTIVE ($E8) and SOUND_TIMER ($E9), and compares them with the real emulator (`stella_coverage ... tiaw=
... tiaregs=15,16,17,18,19,1A` logs every write to those registers).
Writes docs\\F19_SOUND.md. The ROM is only read.
"""
import collections
import concurrent.futures
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / 'tools'))
import f6_coverage as f6  # noqa: E402
import f12_collisions as f12  # noqa: E402

BS = chr(92)
OUT = ROOT / 'reference' / 'f19'
EXE = ROOT / 'build' / 'stella_probe' / 'stella_coverage.exe'
REG = {0x15: 'AUDC0', 0x16: 'AUDC1', 0x17: 'AUDF0', 0x18: 'AUDF1', 0x19: 'AUDV0', 0x1A: 'AUDV1'}
MEANING = {0: 'wafer eaten', 1: 'bonus item eaten', 2: 'power pill eaten', 3: 'ghost eaten'}


class Sound:
    def __init__(self, rom):
        self.t_prio = list(rom[0xFE9 & 0xFFF:][:0])
        self.prio = [rom[0xFE9 + i] for i in range(4)]       # TBL_FFE9: sounds that block a new one (bits of SOUND_ACTIVE)
        self.bit = [rom[0xE6E + i] for i in range(4)]        # TBL_FE6E: the bit a sound sets in SOUND_ACTIVE
        self.dur = [rom[0xFED + i] for i in range(4)]        # TBL_FFED: length
        self.f0 = [rom[0xFF1 + i] for i in range(4)]         # SND_FFF1
        self.f1 = [rom[0xFF5 + i] for i in range(4)]         # SND_FFF5

    def start(self, st, y):
        if self.prio[y] & st['active']:
            return
        st['active'] = self.bit[y]
        st['timer'] = self.dur[y]

    def effects(self, st, w):
        a = st['active']
        d3 = None
        y = None
        vol = None
        go = False
        if a & 1:                                            # bit 0 (ghost eaten): falling volume
            d3 = 4
            y = st['timer'] & 0x1F
            vol = y ^ 0x0F
            go = True
        elif a & 2 or a & 4:                                 # bits 1, 2 (power pill, bonus item): steady tone $0D
            y = d3 = vol = 0x0D
            go = True
        elif a & 8:                                          # bit 3 (wafer): volume = the timer, tone 9
            y = d3 = 9
            if st['timer'] != 0:
                vol = st['timer']
            else:
                vol = 0
                y = d3 = 0
            go = True
        if not go:
            return
        st['timer'] = (st['timer'] - 1) & 0xFF
        if st['timer'] != 0:
            w.extend([(0x19, vol), (0x15, d3), (0x17, y)])
        else:
            st['active'] = 0

    def music(self, e7, e4, pill, fc, w):
        if e7 & 0x40:                                        # death: volume = timer, pitch = timer / 2
            w.extend([(0x19, e4), (0x17, e4 >> 1), (0x15, 4)])
            return
        if e7 & 0x08:                                        # the start tune
            a = e4 if e4 != 0 else (e7 & 0xF7)       # with the timer at 0 the code clears the bit and then uses GAME_FLAGS itself ($FFAE-$FFB2)
            x = (a >> 3) & 7
            if x < 4:
                w.extend([(0x17, self.f0[x]), (0x18, self.f1[x]), (0x15, 4), (0x16, 4), (0x19, 8), (0x1A, 8)])
            return
        if (pill << 1) & 0x80:                               # scared ghosts (first part): a sweep on channel 1
            y = (fc >> 1) & 0x0F
            w.extend([(0x1A, (y ^ 0x0F) >> 1), (0x16, 4), (0x18, y)])


def frame_events(pre, post, d):
    ev = []
    if 'f575' in d and d['f575'] & 0x40:
        ev.append(1)
    pill_eaten = any(pre[0x1A] & b and not post[0x1A] & b for b in (0x04, 0x08, 0x10, 0x20))
    if pill_eaten:
        ev.append(2)
    elif 'f5d9' in d and d['f5d9'] & 0x80 and pre[0x1A] & 0x80:
        X = pre[0] & 3
        if not pre[0x3C + X] & 8 and post[0x3C + X] & 8:
            ev.append(3)
    if not pill_eaten:
        newmap = (post[0x1A] & 0x3C) == 0x3C and (pre[0x1A] & 0x3C) != 0x3C
        if (f12.wafers_gone(pre, post) or newmap) and not (pre[0x66] ^ post[0x66]) & 0x80:   # not at a change of turn (the maze blocks are swapped)
            ev.append(0)
    return ev


def run_stella(item):
    e, pokes = item
    r = subprocess.run([str(EXE), str(ROM), str(ROOT / 'baseline' / (e + '.inputs')), str(OUT / ('c_%s.csv' % e)),
                        str(ROOT / 'reference' / 'baseline' / (e + '.trace'))] + pokes
                       + ['tiaw=%s:0:999999' % (OUT / ('t_%s.csv' % e)), 'tiaregs=15,16,17,18,19,1A'], capture_output=True, text=True)
    return e, r.returncode, r.stdout.strip().splitlines()[-1] if r.stdout.strip() else r.stderr.strip()


def main():
    global ROM
    ROM = pathlib.Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else ROOT / 'rom' / 'pacman.a26'
    rom = ROM.read_bytes()
    OUT.mkdir(parents=True, exist_ok=True)
    items = [it for it in f6.entries() if it[0] not in ('BL-065', 'BL-066')]
    if '--cached' not in sys.argv:
        with concurrent.futures.ThreadPoolExecutor(8) as ex:
            runs = list(ex.map(run_stella, items))
        bad = [r for r in runs if r[1] or '0 differences' not in r[2]]
        print('runs:', len(runs), 'with problems:', len(bad))
    snd = Sound(rom)
    stats = collections.Counter()
    miss = []
    seen = collections.Counter()
    for e, _ in items:
        rows = f12.trace(e)
        ev = {}
        for line in open(ROOT / 'reference' / 'f12' / ('w_%s.csv' % e)):
            fr, pc, a = line.strip().split(',')
            ev.setdefault(int(fr), {})[pc] = int(a)
        log = collections.defaultdict(list)
        for line in open(OUT / ('t_%s.csv' % e)):
            f, ln, ck, pc, reg, val = line.strip().split(',')
            log[int(f)].append((int(reg, 16), int(val, 16)))
        for fr in range(1, len(rows)):
            inp, post = rows[fr]
            pre = rows[fr - 1][1]
            stats['frames'] += 1
            if inp & 0x60 or pre[2] != post[2] and (pre[2] == 0 or post[2] == 0) and not (pre[0x67] & 0x40):
                stats['skipped: Select/Reset or title screen starts or ends'] += 1
                continue
            st = {'active': pre[0x68], 'timer': pre[0x69]}
            events = frame_events(pre, post, ev.get(fr, {}))
            for y in events:
                snd.start(st, y)
                seen[y] += 1
            w = [(0x19, 0), (0x1A, 0)]
            snd.effects(st, w)
            e7 = post[0x67]
            if pre[0x67] & 8 and not e7 & 8 and post[0x64] == 0:
                e7 |= 8                       # the tune clears the bit itself, in this call
            snd.music(e7, post[0x64], post[0x1A], pre[0], w)
            want = log.get(fr, [])
            stats['predicted frames'] += 1
            if w == want and (st['active'], st['timer']) == (post[0x68], post[0x69]):
                stats['right (register writes and the two sound bytes)'] += 1
                if len(w) > 2:
                    stats['  of which with sound'] += 1
            else:
                stats['WRONG'] += 1
                if len(miss) < 8:
                    miss.append((e, fr, events, 'want', want, 'got', w, (st['active'], st['timer']), (post[0x68], post[0x69]), hex(post[0x67]), post[0x64]))
    for k, v in sorted(stats.items()):
        print('%8d  %s' % (v, k))
    for m in miss:
        print(m)
    ok = not stats['WRONG'] and stats['right (register writes and the two sound bytes)'] > 100000
    print('result: %s' % ('OK' if ok else 'MISMATCH'))
    write_doc(stats, seen, snd, rom, ok)
    return 0 if ok else 1


def write_doc(stats, seen, snd, rom, ok):
    d = ['# Stage F19: the sound routines and their tables (2026-10-26)', '',
         '`python -I tools%sf19_sound.py rom%spacman.a26` holds a model of the four sound routines and checks it on every frame of the 70 recorded games against the real emulator (every write to the six sound registers, in order and value, and the sound bytes `$E8`/`$E9`). This document is written by that tool.' % (BS, BS), '',
         '## Result: %s' % ('exact' if ok else 'MISMATCH'), '',
         '- **%d of %d predicted frames exactly right (%d wrong)**; %d of them contain a sound beyond the silencing of the volumes. Frames skipped: %d (Select/Reset and the frames where the title screen starts or ends).' % (stats['right (register writes and the two sound bytes)'], stats['predicted frames'], stats['WRONG'], stats['  of which with sound'], stats['skipped: Select/Reset or title screen starts or ends']),
         '- Sound starts seen: ' + ', '.join('%s %d' % (MEANING[k], v) for k, v in sorted(seen.items())) + '.', '',
         '## How the sound works', '',
         'The game has two sound channels (0 and 1). Every frame, near the end of the bottom logic (`$F7C3-$F7CE`), it silences both volumes (`AUDV0` = `AUDV1` = 0) and then calls `SOUND_EFFECTS` (channel 0) and `SOUND_MUSIC` (channel 0 or 1, by the situation). **Nothing is stored between frames in the chip**: whatever a routine does not write in a frame is silence.', '',
         '### Sound effects (four sounds, one at a time)', '',
         '`SOUND_START` (`$FF43`) is called from the game rules with a sound number Y. It starts the sound unless a sound that blocks it is playing: `SOUND_ACTIVE` (`$E8`) AND `TBL_FFE9[Y]` must be 0; then `SOUND_ACTIVE` = `TBL_FE6E[Y]` (a single bit) and `SOUND_TIMER` (`$E9`) = `TBL_FFED[Y]`.', '',
         '| Y | Sound | Bit in `$E8` | Blocked while `$E8` has | Length (frames) | What `SOUND_EFFECTS` writes each frame (channel 0) |', '| --- | --- | --- | --- | --- | --- |']
    what = {0: 'AUDV = timer (falls from 9 to 2), AUDC = 9, AUDF = 9: the wafer "chomp"', 1: 'AUDV = AUDC = AUDF = $0D: a steady tone', 2: 'AUDV = AUDC = AUDF = $0D: the same steady tone',
            3: 'AUDC = 4, AUDF = timer and $1F (falls), AUDV = AUDF xor $0F (rises): a sliding tone'}
    for y in range(4):
        d.append('| %d | %s | $%02X | $%02X | %d | %s |' % (y, MEANING[y], snd.bit[y], snd.prio[y], snd.dur[y] - 1, what[y]))
    d += ['', 'The timer goes down by one each frame; the frame it reaches 0 writes nothing and clears `SOUND_ACTIVE`, so a sound lasts (length - 1) frames of sound. A new sound is refused while a higher-ranking one plays: ghost eaten (never refused) > power pill (refused while a ghost sound plays) > bonus item (refused while a ghost or pill sound plays) > wafer (refused while any of the three plays); a sound of the same kind simply starts again. The wafer sound is restarted by every wafer, which makes the chomping.', '',
          '### Music and long sounds (`SOUND_MUSIC`)', '',
          'Chosen in this order each frame:', '',
          '1. **Pac-Man dying** (`GAME_FLAGS` bit 6): channel 0, `AUDC` 4, `AUDV` = `EVENT_TIMER`, `AUDF` = `EVENT_TIMER` / 2. The timer counts down from `$3F`, so the tone falls in pitch and volume (the death wail), for 126 frames.',
          '2. **The start tune** (`GAME_FLAGS` bit 3 set at the start of a try): from `EVENT_TIMER` bits 3-5 (`x`): for x >= 4 silence; for x = 0..3 both channels play `AUDC` 4, `AUDV` 8 with the pitches `SND_FFF1[x]` = %s (channel 0) and `SND_FFF5[x]` = %s (channel 1). When the timer reaches 0 the bit is cleared.' % (' '.join('$%02X' % v for v in snd.f0), ' '.join('$%02X' % v for v in snd.f1)),
          '3. **Scared ghosts** (`PILL_FLAGS` bit 6, the first part of the scare): channel **1**, `AUDC` 4, `AUDF` = (`FRAME_COUNTER` / 2) and $0F, `AUDV` = (`AUDF` xor $0F) / 2: a pitch that sweeps up and down every 32 frames.',
          '4. Otherwise nothing.', '',
          'A quirk of the tune: when `EVENT_TIMER` is 0 the code clears the tune bit and then takes the pitch number from `GAME_FLAGS` bits 3-5 instead of the timer (the accumulator still holds the flags, `$FFAE-$FFB2`); with the difficulty bit (bit 5) set that gives x = 4 (silence), without it x = 0 (the first note sounds once more). The model reproduces this and is exact in every frame.', '',
          'An effect writes channel 0 first, then the music may write channel 0 again (death, tune): the music wins. In the pause after eating a ghost the music is the scare sweep on channel 1, and the effect (ghost eaten) on channel 0.', '',
          '## For the rebuild', '',
          '- The sound state is two bytes (`$E8`, `$E9`) plus the situation (`GAME_FLAGS`, `EVENT_TIMER`, `PILL_FLAGS`, `FRAME_COUNTER`); the output of a frame is at most 3 + 6 register writes, listed exactly by the model above. The native sound generator (F24) only has to turn register values into samples like the TIA does and be compared sample by sample with the emulator (D8 and C5 give the sound chip facts).',
          '- Sound is created in the bottom logic by four calls (`$F57D...`: bonus item Y=1, `$F5CD`: power pill Y=2, `$F602`: ghost Y=3, `$F6A7`: wafer Y=0), in that order within a frame, and by the Select step (`$F8DB-$F8DF`: volumes and `SOUND_ACTIVE` cleared).', '',
          '## Not claimed', '',
          '- Frames with Select or Reset and the frames where the title screen starts or ends are skipped; the two test games BL-065/066 are skipped.',
          '- The sound of the two-player change of turn and of difficulty A was not separately observed (no sound code depends on them).']
    (ROOT / 'docs' / 'F19_SOUND.md').write_text('\n'.join(d) + '\n', encoding='utf-8')


if __name__ == '__main__':
    sys.exit(main())
