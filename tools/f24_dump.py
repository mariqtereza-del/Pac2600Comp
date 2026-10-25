r"""Stage F24.1: makes the reference data for the sound-chip model.

  python -I tools\f24_dump.py <rom>      (needs build\stella_probe\stella_coverage.exe and the 70 baseline traces)

For every baseline game and EVERY frame, the real emulator's list of writes to the six sound registers (AUDC0, AUDC1, AUDF0, AUDF1, AUDV0, AUDV1) is saved in
reference\f24\a_BL-nnn.csv: frame, scanline, colour clock after the instruction, program address, register, value. The golden traces hold the fingerprint
of each frame's sound. The ROM is only read. Local only (reference\ is ignored by Git).
"""
import concurrent.futures
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / 'tools'))
import f6_coverage as f6  # noqa: E402

EXE = ROOT / 'build' / 'stella_probe' / 'stella_coverage.exe'
OUT = ROOT / 'reference' / 'f24'


def dump(item, rom):
    entry, pokes = item[0], item[1]
    a = OUT / ('a_%s.csv' % entry)
    c = OUT / ('c_%s.csv' % entry)
    args = [str(EXE), str(rom), str(ROOT / 'baseline' / (entry + '.inputs')), str(c), str(ROOT / 'reference' / 'baseline' / (entry + '.trace'))] + list(pokes) + [
        'tiaw=%s:0:99999999' % a, 'tiaregs=15,16,17,18,19,1a']
    r = subprocess.run(args, capture_output=True, text=True)
    if c.exists():
        c.unlink()
    return entry, r.returncode, (r.stdout.strip().splitlines() or [r.stderr.strip()])[-1]


def main():
    rom = pathlib.Path(sys.argv[1]).resolve()
    OUT.mkdir(parents=True, exist_ok=True)
    with concurrent.futures.ThreadPoolExecutor(8) as ex:
        res = list(ex.map(lambda it: dump(it, rom), f6.entries()))
    bad = [r for r in res if r[1] != 0]
    print('dumped %d games; failures: %s' % (len(res), bad or 'none'))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
