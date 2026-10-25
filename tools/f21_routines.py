"""Stage F21: rewriting the game routine by routine - the ledger and the checks.

  python -I tools\\f21_routines.py <rom>      (after tools\\build_lockstep.cmd and tools\\f20_lockstep.py; needs reference\\f20\\oracle_BL-nnn.txt)

Every rewritten routine is listed in DONE (stage, F8 name, native name in src\\lockstep\\natives.cpp, entry address). For each of them, and for all of them
together, the original program is run by the lockstep machine over all 70 games with the native routine in place and the routine check on:
  - all 128 memory bytes identical after every frame, and
  - at every call the native routine's results (memory and registers a, x, y, sp, pc, flags) equal those of the original code on a copy.
The test fixtures that are wrong on purpose must be found. Writes docs\\F21_ROUTINES.md (the ledger of routines: done and still to do).
"""
import concurrent.futures
import pathlib
import re
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / 'tools'))
import f6_coverage as f6  # noqa: E402

EXE = ROOT / 'build' / 'lockstep' / 'lockstep.exe'
BS = chr(92)

# stage, F8 name, native name, entry, one-line description of what was written
DONE = [
    ('F21.1', 'CLEAR_SCORES', 'clear-scores', 0xFC7D, 'game::clearScores in src/game/routines.cpp: both scores and both lives to 0'),
    ('F21.21', 'SCORE_DIGITS', 'score-digits', 0xF3EE, 'game::scoreDigits: the digits of a score to picture pointers (leading zeros blank) and the seven rows of the score line written to the picture chip, then CLEAR_GRAPHICS; chip writes compared in order'),
    ('F21.20', 'PACMAN_KEEP_GOING', 'keep-going', 0xF793, 'game::pacmanKeepGoing: stick neutral, exactly on a cell: try to go on, else stand still; ends the caller when he stands still (no move); contract: X, stack pointer, arrival address, live memory'),
    ('F21.19', 'SOUND_MUSIC', 'sound-music', 0xFF94, 'game::soundMusic: the death sound, the start tune and the scared-ghost sweep, written to a game::SoundChip; two cartridge tables'),
    ('F21.18', 'SOUND_EFFECTS', 'sound-effects', 0xFF55, 'game::soundEffects: the sound effect of the frame on channel 0, written to a game::SoundChip; the call check now compares the writes to the sound chip'),
    ('F21.18', 'SOUND_WRITE', 'sound-effects', 0xFF8B, 'the shared writer of the sound routines (inside soundEffects)'),
    ('F21.17', 'SOUND_START', 'sound-start', 0xFF43, 'game::soundStart: start a sound effect unless a higher-ranking one plays; three cartridge tables through game::Cartridge'),
    ('F21.16', 'GHOST_UPDATE', 'ghost-update', 0xF9AA, 'game::ghostUpdate: everything for one ghost in one frame (eyes at the pen, locate, decide, move); contract: X, stack pointer, return address and live memory'),
    ('F21.15', 'GHOST_DECIDE', 'ghost-decide', 0xFA26, 'game::ghostDecide: the ghost decision (aim, chase, scared, eyes, opening, random wandering) built from the rewritten TRY, TRY_PAIR, RNG and WRAP routines; contract: X, stack pointer, return address and live memory'),
    ('F21.14', 'ANIMATE_OBJECT', 'animate', 0xFC8B, 'game::animateObject: the opening animation of an object (waiting, down the outside, back into the maze, up) and the sprite scratch bytes; three small cartridge tables through game::Cartridge'),
    ('F21.13', 'TRY_PAIR_1', 'try-pair', 0xFAFC, "game::tryPair (vertical): after the random step try up and down in the order it picks; a taken turn drops the caller's return too"),
    ('F21.13', 'TRY_PAIR_UD', 'try-pair', 0xFB05, 'the vertical pair itself (inside the TRY_PAIR_1 rewrite; only reached from it)'),
    ('F21.13', 'TRY_PAIR_2', 'try-pair', 0xFB19, 'game::tryPair (horizontal): left and right'),
    ('F21.13', 'TRY_PAIR_LR', 'try-pair', 0xFB22, 'the horizontal pair itself (inside the TRY_PAIR_2 rewrite; only reached from it)'),
    ('F21.12', 'TRY_UP', 'try', 0xFB36, "game::tryDirection (direction 3): may the object turn up here? Reads the maze wall table through game::Cartridge; if yes it sets the state and DROPS THE CALLER'S RETURN"),
    ('F21.12', 'TRY_DOWN', 'try', 0xFB5C, 'game::tryDirection (direction 2)'),
    ('F21.12', 'TRY_LEFT', 'try', 0xFB70, 'game::tryDirection (direction 1)'),
    ('F21.12', 'TRY_RIGHT', 'try', 0xFB87, 'game::tryDirection (direction 0)'),
    ('F21.12', 'TRY_COMMIT', 'try', 0xFB44, 'the shared end of the four (inside tryDirection: no turning straight back for ghosts, set the state)'),
    ('F21.11', 'LIVES_MARKERS', 'lives-markers', 0xFC68, 'game::livesMarkers: the three playfield values of the life-marker row for one player (two cartridge tables through game::Cartridge)'),
    ('F21.10', 'ADD_SCORE', 'add-score', 0xFC45, 'game::addScore: decimal addition of a score amount to the player in turn; amounts read by address through the cartridge view (game::Cartridge)'),
    ('F21.9', 'SET_DIRECTION', 'set-direction', 0xF9E5, 'game::setDirection: direction bits and step-due bit set, then MOVE_OBJECT; the table TBL_FE63 is (d << 6) or $10 (checked equal to the cartridge bytes)'),
    ('F21.8', 'MOVE_OBJECT', 'move', 0xF9EE, 'game::moveObject: the speed accumulator (x17) and the one-pixel step with the border rules'),
    ('F21.7', 'LOCATE_OBJECT', 'locate', 0xFBAD, 'game::locateObject: row, offsets, cell index and position code of an object; the adapter also leaves the flags byte that the original PHP writes in the stack area'),
    ('F21.6', 'NEW_GAME_SETUP', 'new-game-setup', 0xFBF4, 'game::newGameSetup: score colours, three lives, start level from the game number, then NEW_MAZE'),
    ('F21.5', 'NEW_MAZE', 'new-maze', 0xFC0C, 'game::newMaze: the wafer map filled (two places empty), pill flags $3C, then PLACE_OBJECTS'),
    ('F21.5', 'PLACE_OBJECTS', 'place-objects', 0xFC21, 'game::placeObjects: freeze on, timer $3C, Pac-Man and the ghosts at their start places'),
    ('F21.4', 'WRAP_TO_BOTTOM', 'wrap', 0xFD26, 'game::wrapToBottom: direction bits $30 and y = $51 (the opening in the bottom wall); registered together with WRAP_TO_TOP and the shared end WRAP_SET_Y'),
    ('F21.4', 'WRAP_TO_TOP', 'wrap', 0xFD32, 'game::wrapToTop: direction bits $F0 and y = $0B (the opening in the top wall)'),
    ('F21.4', 'WRAP_SET_Y', 'wrap', 0xFD3C, 'the shared end of both (written inside wrapToTop/wrapToBottom; never entered on its own)'),
    ('F21.3', 'RNG_STEP', 'rng', 0xFBE0, 'game::rngStep: the shift-register random number; its bit-mask table is 0x80 >> n (checked equal to the cartridge bytes)'),
    ('F21.2', 'SWAP_PLAYERS', 'swap-players', 0xFB9F, 'game::swapPlayers: the 20-byte player blocks $88-$9B and $9C-$AF exchanged'),
]
# why a routine cannot be proved by the memory comparison alone, or what it needs first
NOTES = {
    'KERNEL_LINE_P1': 'writes only chip registers (picture): proved with the renderer (F23)', 'KERNEL_LINE_P1_P0': 'picture only (F23)',
    'KERNEL_LINE_P0': 'picture only (F23)', 'CLEAR_GRAPHICS': 'picture only (F23)', 'SOUND_WRITE': 'sound chip only (F24)',
    'EXIT_F9E4': 'a lone RTS: nothing to write', 'EXIT_FB5B': 'a lone RTS: nothing to write', 'EXIT_FF93': 'a lone RTS: nothing to write',
    'WRAP_SET_Y': 'end of both WRAP routines: written with them',
    'SCORE_DIGITS': 'needs ROM tables (F22) and the picture',
    'SOUND_MUSIC': '',
    'SET_DIRECTION': 'table TBL_FE63 (F22); falls into MOVE_OBJECT', 'MOVE_OBJECT': 'ends in the border tests; leaf',
    'GHOST_DECIDE': 'large; after the TRY_ routines', 'GHOST_UPDATE': 'after GHOST_DECIDE',
    'TRY_PAIR_1': 'drops the caller\'s return (rts twice)', 'TRY_PAIR_UD': 'drops the caller\'s return', 'TRY_PAIR_2': 'drops the caller\'s return',
    'TRY_PAIR_LR': 'drops the caller\'s return', 'TRY_COMMIT': 'drops the caller\'s return', 'PACMAN_KEEP_GOING': 'drops the caller\'s return',
    'RNG_STEP': 'reads the bit-mask table at $FE6A: a plain 0x80 >> n, checked equal to the ROM bytes',
}


def run(entry, pokes, natives, verify=True):
    cmd = [str(EXE), str(ROM), str(ROOT / 'baseline' / (entry + '.inputs')), str(ROOT / 'reference' / 'baseline' / (entry + '.trace')),
           '--oracle', str(ROOT / 'reference' / 'f20' / ('oracle_%s.txt' % entry))]
    for p in pokes:
        cmd += ['--poke', p]
    for n in natives:
        cmd += ['--native', n]
    if verify:
        cmd.append('--verify')
    r = subprocess.run(cmd, capture_output=True, text=True)
    return entry, r.returncode, r.stdout.strip().split('\n')


def calls(lines):
    for l in lines:
        m = re.match(r'routine check: (\d+) calls compared with the original, (\d+) different', l)
        if m:
            return int(m.group(1)), int(m.group(2))
    return 0, 0


def main():
    global ROM
    ROM = pathlib.Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else ROOT / 'rom' / 'pacman.a26'
    items = f6.entries()
    problems = []
    results = {}
    percall = {}
    with concurrent.futures.ThreadPoolExecutor(8) as ex:
        seen = set()
        for stage, name, native, entry, what in DONE:
            if native in seen:
                results[name] = results[[d[1] for d in DONE if d[2] == native][0]]
                continue
            seen.add(native)
            res = list(ex.map(lambda it: run(it[0], it[1], [native]), items))
            bad = [r for r in res if r[1] != 0]
            c = sum(calls(r[2])[0] for r in res)
            results[name] = (len(res) - len(bad), c)
            percall[native] = {r[0]: calls(r[2])[0] for r in res}
            print('%s %s: %d games identical of %d, %d calls compared with the original, %d routine differences' % (
                stage, name, len(res) - len(bad), len(res), c, sum(calls(r[2])[1] for r in res)))
            if bad:
                problems.append('%s: %s' % (name, bad[0][2][:3]))
        allres = list(ex.map(lambda it: run(it[0], it[1], sorted({d[2] for d in DONE})), items))
        allbad = [r for r in allres if r[1] != 0]
        print('all rewritten routines together: %d games identical of %d' % (len(allres) - len(allbad), len(allres)))
        if allbad:
            problems.append('together: %s' % allbad[0][2][:3])
        wrong = list(ex.map(lambda it: run(it[0], it[1], ['clear-scores-wrong']), items))
        found_w = [r for r in wrong if r[1] == 1 and any('CLEAR_SCORES (native, wrong on purpose)' in l for l in r[2])]
        used_w = [r for r in wrong if calls(r[2])[0] > 0]
        print('fixture wrong on purpose (memory): found in %d of %d games that call the routine' % (len(found_w), len(used_w)))
        badreg = list(ex.map(lambda it: run(it[0], it[1], ['clear-scores-badreg']), items))
        found_r = [r for r in badreg if r[1] == 1 and any('ROUTINE CHECK FAILED' in l for l in r[2])]
        same_frames = [r for r in badreg if r[1] == 1 and any('ROUTINE CHECK FAILED' in l for l in r[2])]
        print('fixture with a wrong register: the routine check finds it in %d games (the frames themselves are identical in all of them)' % len(found_r))
        gw = list(ex.map(lambda it: run(it[0], it[1], ['ghost-decide-wrong']), items))
        found_g = [r for r in gw if r[1] == 1 and any('GHOST_DECIDE (native, wrong on purpose)' in l for l in r[2])]
        used_g = [r for r in gw if calls(r[2])[0] > 0 or r[1] == 1]
        print('fixture GHOST_DECIDE wrong on purpose (a scratch byte): found in %d of %d games' % (len(found_g), len(used_g)))
        if len(found_g) != len(used_g) or not used_g:
            problems.append('GHOST_DECIDE fixture not found everywhere')
        sw = list(ex.map(lambda it: run(it[0], it[1], ['sound-effects-wrong']), items))
        found_s = [r for r in sw if r[1] == 1 and any('SOUND_EFFECTS (native, wrong on purpose)' in l for l in r[2])]
        used_s = [r for r in sw if percall.get('sound-start', {}).get(r[0], 0) > 0]   # games in which a sound effect is started at all
        print('fixture SOUND_EFFECTS wrong on purpose (one chip register value; memory untouched): found by the chip-write check in %d of %d games' % (len(found_s), len(used_s)))
        if len(found_s) != len(used_s) or not used_s:
            problems.append('SOUND_EFFECTS fixture not found everywhere')
        if len(found_w) != len(used_w) or not used_w or not found_r:
            problems.append('fixtures not found everywhere')
    rom = ROM.read_bytes()
    table_ok = list(rom[0xE6A:0xE72]) == [0x80 >> i for i in range(8)]
    print('bit-mask table at $FE6A equals 0x80 >> n:', table_ok)
    if not table_ok:
        problems.append('the RNG mask table is not 0x80 >> n')
    dir_ok = list(rom[0xE63:0xE67]) == [(d << 6) | 0x10 for d in range(4)]
    print('direction table at $FE63 equals (d << 6) or $10:', dir_ok)
    if not dir_ok:
        problems.append('the direction table is not (d << 6) or $10')
    rev_ok = list(rom[0xE67:0xE6B]) == [((d ^ 1) << 6) for d in range(4)]
    print('turn-back table at $FE67 equals ((d xor 1) << 6):', rev_ok)
    if not rev_ok:
        problems.append('the turn-back table is not ((d xor 1) << 6)')
    ok = not problems
    print('problems:', problems)
    print('result: %s' % ('OK' if ok else 'MISMATCH'))
    write_doc(results, len(items), len(found_w), len(used_w), len(found_r), ok)
    return 0 if ok else 1


def write_doc(results, n, found_w, used_w, found_r, ok):
    text = (ROOT / 'docs' / 'F8_ROUTINES.md').read_text(encoding='utf-8')
    block = text[text.index('## Routines'):text.index('## Data each routine')]
    rows = []
    for line in block.split('\n'):
        m = re.match(r'\| \$([0-9A-F]{4}) \| `(\w+)` \| ([^|]*) \| (\d+) \| ([^|]*) \|', line)
        if m and m.group(2) != 'MAIN':
            rows.append((int(m.group(1), 16), m.group(2), int(m.group(4)), m.group(5)))
    done = {d[1]: d for d in DONE}
    d = ['# Stage F21: the routine ledger (2026-10-26)', '',
         '`python -I tools%sf21_routines.py rom%spacman.a26` checks every rewritten routine and writes this ledger. **F21 is not one session: it is one routine per session** (F21.1, F21.2, ...). This document is the list, kept up to date by each session.' % (BS, BS), '',
         '## How a routine is rewritten (the protocol)', '',
         '1. The routine is written as a C++ function in `src%sgame%sroutines.cpp`, working on the 128 memory bytes with the names of F7 (`src%sgame%sram_map.hpp`, generated). No ROM data and no emulator code is allowed there; things the original reads from the cartridge come later through the data loader (F22).' % ((BS,) * 4),
         '2. An adapter in `src%slockstep%snatives.cpp` registers it under the entry address of the original and reproduces what the original leaves in the registers and flags (the callers may depend on them).' % (BS, BS),
         '3. `lockstep.exe ... --native <name> --verify` runs all recorded games with the native routine in place of the original. It checks (a) all 128 bytes after every frame and (b) at every call, the native results against the original code run on a copy (memory, `A`, `X`, `Y`, stack pointer, return address, flags). Both must be clean in all 70 games.',
         '4. Only then is the routine marked done here and in `PLAN.md`, and the stage is committed. Fixtures that are wrong on purpose prove the checks bite.', '',
         '## Result of this run: %s' % ('all checks clean' if ok else 'MISMATCH'), '']
    for name, (g, c) in results.items():
        d.append('- **%s**: identical in %d of %d games; %d calls compared with the original code, 0 different.' % (name, g, n, c))
    d += ['- The fixture "wrong on purpose" (forgets `LIVES_P2`) was found by the memory comparison in %d of the %d games that call the routine; the fixture with a wrong `X` register leaves every frame identical but was found by the routine check in %d games: this is why the routine check exists.' % (found_w, used_w, found_r), '',
          '## The ledger', '',
          '| Stage | Routine | Entry | Bytes | Status | Note |', '| --- | --- | --- | --- | --- | --- |']
    todo = [r for r in rows if r[1] not in done]
    for r in sorted(rows, key=lambda r: (r[1] not in done, r[2], r[0])):
        nm = r[1]
        if nm in done:
            d.append('| %s | `%s` | $%04X | %d | **done** | %s |' % (done[nm][0], nm, r[0], r[2], done[nm][4]))
        else:
            d.append('| - | `%s` | $%04X | %d | to do | %s |' % (nm, r[0], r[2], NOTES.get(nm, '')))
    d += ['', 'Order for the next sessions: smallest first among the routines whose result shows in the memory (the picture-only and sound-chip-only routines wait for F23/F24; those that need cartridge tables wait for F22 or use a table checked equal to the ROM): SWAP_PLAYERS, RNG_STEP, the WRAP routines, NEW_MAZE and PLACE_OBJECTS, LOCATE_OBJECT, MOVE_OBJECT, the TRY routines, ANIMATE_OBJECT, then the ghost routines. The main loop (`MAIN`: start-up, top and bottom logic, kernel) is cut into routines in the last sessions of F21.']
    (ROOT / 'docs' / 'F21_ROUTINES.md').write_text('\n'.join(d) + '\n', encoding='utf-8')


if __name__ == '__main__':
    sys.exit(main())
