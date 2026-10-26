r"""Stage G5c (D-009): the fingerprint files of the ROM-free test.

python -I tools\g5c_make_fingerprints.py

Takes the golden traces of the reference emulator (reference\baseline\BL-nnn.trace, local) and writes, for a few recorded games, the picture and sound
fingerprint of every frame to tests\game\data\BL-nnn.fingerprints.txt (tracked in Git). They are hashes of the output, no cartridge data. The test
build\lockstep\test_no_rom.exe plays the recorded input (baseline\BL-nnn.inputs, tracked) and compares: no ROM is needed for it.
"""
import pathlib

ROOT = pathlib.Path(__file__).resolve().parent.parent
GAMES = [('BL-001', 700), ('BL-035', 3000),   # the idle title screen; the bot's route (start, play, wafers, ghosts, a power pill, deaths)
         # Stage H4: more kinds of play for the ROM-free set (frame counts are the first frames of the recorded game)
         ('BL-003', 1400),   # Select steps through the 16 game selections
         ('BL-044', 4000),   # the first power pill and the first ghosts eaten
         ('BL-047', 7500),   # the end of the first level: the last wafer, the extra life, the new map
         ('BL-048', 3000),   # an idle game to game over, then the fire button
         ('BL-062', 800),    # the right difficulty switch moved during play
         ('BL-063', 800),    # the colour / black-and-white switch moved during play
         ('BL-067', 3800),   # two players, an idle game to the end
         ('BL-068', 6000),   # random joystick and console buttons
         ('BL-070', 6000)]   # eaten ghosts walk back to the box

for name, limit in GAMES:
    lines = (ROOT / 'reference' / 'baseline' / (name + '.trace')).read_text().split('\n')[3:]
    out = ['# Picture and sound fingerprints of the first %d frames of the recorded game %s, from the reference emulator (Stage G5c).' % (limit, name),
           '# frame picture sound   (hashes of the output only; the input is baseline/%s.inputs)' % name]
    n = 0
    for line in lines:
        cells = line.split(',')
        if len(cells) < 6:
            continue
        out.append('%s %s %s' % (cells[0], cells[3], cells[4]))
        n += 1
        if n == limit:
            break
    (ROOT / 'tests' / 'game' / 'data' / (name + '.fingerprints.txt')).write_text('\n'.join(out) + '\n', encoding='utf-8', newline='\n')
    print('%s: %d frames' % (name, n))
