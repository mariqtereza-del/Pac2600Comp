r"""Stage H5: look inside a release zip (and the source zip inside it) for the cartridge's program code.

python -I tools\h5_zip_code_scan.py <release zip> rom\pacman.a26

Uses the code/data map of Stage F6 (reference\f6\coverage_all.txt, local) to take every run of 8 consecutive program-code bytes of the
cartridge (4 or more different values, not inside the 34 embedded tables) and searches every file in the zip for it: as raw bytes, and in
source files as 0x.. numbers. Also looks for the whole ROM (by SHA-1 of any 4,096-byte file). Reads the zip and the ROM, changes nothing.
Prints 'result: OK' when nothing is found, else 'result: FAILED' (exit 1).
"""
import hashlib
import io
import re
import sys
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SHA1 = '0940fea7f04cdb6d4b90c5ad1a7e344e68f6dbb1'
WINDOW = 8


def code_windows(rom):
    fmap = {}
    for line in (ROOT / 'reference/f6/coverage_all.txt').read_text().split('\n'):
        m = re.match(r'^(F[0-9A-F]{3})  (\S+)$', line)
        if m:
            for i, ch in enumerate(m.group(2)):
                fmap[int(m.group(1), 16) + i] = ch
    text = (ROOT / 'src/game/data/tables.cpp').read_text(encoding='utf-8')
    used = set()
    for m in re.finditer(r'const Table<(\d+)> \w+ = \{\{.*?\}, "\w+", 0x([0-9A-F]{4})\};', text, re.S):
        used.update(range(int(m.group(2), 16), int(m.group(2), 16) + int(m.group(1))))
    windows = set()
    for a in range(0xF000, 0x10000 - WINDOW + 1):
        if all(fmap.get(x) in ('C', 'c') and x not in used for x in range(a, a + WINDOW)):
            w = rom[a - 0xF000:a - 0xF000 + WINDOW]
            if len(set(w)) >= 4:
                windows.add(w)
    return windows


def scan(zf, where, windows, found, counts):
    for info in zf.infolist():
        if info.is_dir():
            continue
        data = zf.read(info)
        label = where + info.filename
        counts['files'] += 1
        if len(data) == 4096 and hashlib.sha1(data).hexdigest() == SHA1:
            found.append(label + ' : IS THE ROM')
        if any(w in data for w in windows):
            found.append(label + ' : cartridge code as raw bytes')
        if info.filename.lower().endswith(('.cpp', '.hpp', '.h', '.c', '.cmake', '.txt', '.md', '.py', '.cmd', '.ps1')):
            tb = bytes(int(h, 16) for h in re.findall(r'0x([0-9A-Fa-f]{2})\b', data.decode('utf-8', errors='replace')))
            if tb and any(w in tb for w in windows):
                found.append(label + ' : cartridge code as 0x.. numbers')
        if info.filename.lower().endswith('-source.zip'):
            scan(zipfile.ZipFile(io.BytesIO(data)), label + '!/', windows, found, counts)


def main():
    if len(sys.argv) != 3:
        print(__doc__)
        return 2
    rom = Path(sys.argv[2]).read_bytes()
    if len(rom) != 4096 or hashlib.sha1(rom).hexdigest() != SHA1:
        print('this is not the right ROM')
        return 2
    windows = code_windows(rom)
    found, counts = [], {'files': 0}
    scan(zipfile.ZipFile(sys.argv[1]), '', windows, found, counts)
    print('%d code windows of %d bytes searched in %d files (the zip and the source zip inside it)' % (len(windows), WINDOW, counts['files']))
    for f in found:
        print('FOUND  ' + f)
    print('result: %s' % ('OK' if not found else 'FAILED'))
    return 0 if not found else 1


sys.exit(main())
