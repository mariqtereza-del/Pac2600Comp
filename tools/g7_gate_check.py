r"""Stage G7 (Gate G): the checks of the gate that are not already a proof of an earlier stage.

python -I tools\g7_gate_check.py rom\pacman.a26

Needs the local files reference\f6\coverage_all.txt and the built build\host\pacman2600.exe (tools\build_host.cmd).
Reads the ROM, changes nothing. Prints one line per check and 'result: OK' or 'result: FAILED'.

 1  Git holds no ROM: no tracked .a26/.bin/.rom/archive, nothing under rom\ or reference\, no tracked file with the ROM's SHA-1 or its size.
 2  The tables in tables.cpp are the list of docs\EMBEDDED_DATA.md (names, addresses, sizes; 34 tables, 508 bytes), are the ROM's bytes at those
    addresses, and none of their bytes is program code in the F6 map.
 3  The program code of the cartridge is nowhere: no run of 8 consecutive code bytes (4 or more different values) is in pacman2600.exe or in a
    tracked source file (as 0x.. numbers).
 4  pacman2600.exe has no emulator in it (no Stella or 6502 CPU text).
 5  The game reads no file but the start-up check: in src\game only data\rom_check.cpp opens files.
 6  Rule 7 in CLAUDE.md is word for word the text the owner approved in DECISIONS.md (D-009).
 7  No LITERAL/TODO/FIXME marker in src\game or src\host.
 8  ARCHITECTURE.md names every source file of src\game and src\host.
 9  The conventions checker (tools\g2_conventions_check.py --strict) is clean.
"""
import hashlib
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SHA1 = '0940fea7f04cdb6d4b90c5ad1a7e344e68f6dbb1'
WINDOW = 8
results = []


def check(name, ok, detail=''):
    results.append(ok)
    print('%s  %s%s' % ('PASS' if ok else 'FAIL', name, ('  ' + detail) if detail else ''))


def git_files():
    out = subprocess.run(['git', 'ls-files'], cwd=ROOT, capture_output=True, text=True, check=True).stdout
    return [l for l in out.split('\n') if l]


def check_git(files):
    bad = []
    for f in files:
        low = f.lower()
        vendored = low.startswith('third_party/')   # upstream files of SDL3 and Stella, byte-identical to their releases (docs\DEPENDENCIES.md); their size and SHA-1 are still checked
        if (not vendored and low.endswith(('.a26', '.bin', '.rom', '.zip', '.rar', '.7z', '.tar', '.gz'))) or low.startswith(('rom/', 'reference/')):
            bad.append(f)
            continue
        p = ROOT / f
        if p.is_file() and p.stat().st_size == 4096 and hashlib.sha1(p.read_bytes()).hexdigest() == SHA1:
            bad.append(f)
    check('1 Git holds no ROM (%d tracked files)' % len(files), not bad, ', '.join(bad[:5]))


def parse_tables():
    text = (ROOT / 'src/game/data/tables.cpp').read_text(encoding='utf-8')
    tables = []
    for m in re.finditer(r'const Table<(\d+)> (\w+) = \{\{(.*?)\}, "(\w+)", 0x([0-9A-F]{4})\};', text, re.S):
        size, name, body, name2, addr = int(m.group(1)), m.group(2), m.group(3), m.group(4), int(m.group(5), 16)
        data = bytes(int(h, 16) for h in re.findall(r'0x([0-9A-Fa-f]{2})', body))
        tables.append((name, name2, addr, size, data))
    return tables


def check_tables(rom):
    tables = parse_tables()
    doc = (ROOT / 'docs/EMBEDDED_DATA.md').read_text(encoding='utf-8')
    sect = doc[doc.index('## The tables as they are in the program'):]
    rows = re.findall(r'^\| `(\w+)` \| \$([0-9A-F]{4})-\$([0-9A-F]{4}) \| (\d+) \|', sect, re.M)
    listed = [(n, int(a, 16), int(b, 16), int(s)) for n, a, b, s in rows]
    mine = [(n, a, a + s - 1, s) for n, n2, a, s, d in tables]
    problems = []
    if listed != mine:
        problems.append('tables.cpp and docs\\EMBEDDED_DATA.md differ')
    if len(tables) != 34 or sum(t[3] for t in tables) != 508:
        problems.append('%d tables, %d bytes (expected 34, 508)' % (len(tables), sum(t[3] for t in tables)))
    fmap = {}
    for line in (ROOT / 'reference/f6/coverage_all.txt').read_text().split('\n'):
        m = re.match(r'^(F[0-9A-F]{3})  (\S+)$', line)
        if m:
            for i, ch in enumerate(m.group(2)):
                fmap[int(m.group(1), 16) + i] = ch
    used = set()
    for n, n2, a, s, d in tables:
        if n != n2 or len(d) != s:
            problems.append('%s: name or size wrong' % n)
        if d != rom[a - 0xF000:a - 0xF000 + s]:
            problems.append('%s: bytes differ from the ROM' % n)
        for x in range(a, a + s):
            if x in used:
                problems.append('%s overlaps another table' % n)
            used.add(x)
            if fmap.get(x) in ('C', 'c'):
                problems.append('%s: $%04X is program code in the F6 map' % (n, x))
    check('2 the 34 tables (508 bytes) are the documented ones, equal the ROM there, are all data', not problems, '; '.join(problems[:4]))
    return used


def text_bytes(path):
    t = path.read_text(encoding='utf-8', errors='replace')
    return bytes(int(h, 16) for h in re.findall(r'0x([0-9A-Fa-f]{2})\b', t))


def check_no_code(rom, tables_used, files):
    fmap = {}
    for line in (ROOT / 'reference/f6/coverage_all.txt').read_text().split('\n'):
        m = re.match(r'^(F[0-9A-F]{3})  (\S+)$', line)
        if m:
            for i, ch in enumerate(m.group(2)):
                fmap[int(m.group(1), 16) + i] = ch
    windows = set()
    for a in range(0xF000, 0x10000 - WINDOW + 1):
        span = range(a, a + WINDOW)
        if all(fmap.get(x) in ('C', 'c') and x not in tables_used for x in span):
            w = rom[a - 0xF000:a - 0xF000 + WINDOW]
            if len(set(w)) >= 4:
                windows.add(w)
    exe = (ROOT / 'build/host/pacman2600.exe').read_bytes()
    found_exe = [w for w in windows if w in exe]
    found_src = []
    for f in files:
        p = ROOT / f
        if p.suffix.lower() in ('.cpp', '.hpp', '.h', '.c', '.cmake', '.txt', '.md', '.py', '.cmd', '.ps1') and p.is_file():
            tb = text_bytes(p)
            if tb and any(w in tb for w in windows):
                found_src.append(f)
    check('3 no run of cartridge program code (%d windows of %d bytes) in pacman2600.exe or in %d tracked files' % (len(windows), WINDOW, len(files)),
          not found_exe and not found_src, 'in exe: %d, in files: %s' % (len(found_exe), ', '.join(found_src[:4])))
    markers = (b'M6502', b'OSystem', b'CartDebug', b'emucore', b'Cartridge')   # names inside the Stella core; the option '--stella-speed' is only a word
    bad = [s for s in markers if s in exe]
    ref = (ROOT / 'build/host/pacman2600_ref.exe').read_bytes()
    control = [s for s in markers if s in ref]
    check('4 pacman2600.exe has no emulator in it (control: pacman2600_ref.exe has %d of %d markers)' % (len(control), len(markers)),
          not bad and len(control) >= 2, 'found: %s' % b', '.join(bad).decode())


def check_reads_no_files():
    openers = []
    for p in sorted((ROOT / 'src/game').rglob('*.[ch]pp')):
        t = p.read_text(encoding='utf-8', errors='replace')
        if re.search(r'\b(fopen|ifstream|fstream|_wfopen|CreateFile|std::filesystem|fread)\b', t):
            openers.append(str(p.relative_to(ROOT)).replace('\\', '/'))
    check('5 in src\\game only the start-up check opens files', openers == ['src/game/data/rom_check.cpp'], ', '.join(openers))


def check_rule7():
    claude = (ROOT / 'CLAUDE.md').read_text(encoding='utf-8')
    dec = (ROOT / 'DECISIONS.md').read_text(encoding='utf-8')
    m = re.search(r'### Rule 7 .*?\n\n(.*?)\n\n### Rule 8', claude, re.S)
    rule = m.group(1).strip() if m else ''
    d = dec.index('Rule 7 now reads (in `CLAUDE.md`):')
    quote = []
    for line in dec[d:].split('\n')[1:]:
        if line.strip().startswith('>'):
            quote.append(line.strip()[1:].strip())
        elif quote and line.strip() == '':
            quote.append('')
        elif quote:
            break
    approved = '\n'.join(quote).strip()
    norm = lambda s: re.sub(r'\s+', ' ', s.replace('\r', '')).strip()
    check('6 Rule 7 in CLAUDE.md is the text the owner approved (D-009)', bool(rule) and norm(rule) == norm(approved),
          '' if rule else 'Rule 7 not found')


def check_markers(files):
    hits = []
    for f in files:
        if f.startswith(('src/game/', 'src/host/')) and f.endswith(('.cpp', '.hpp')):
            t = (ROOT / f).read_text(encoding='utf-8', errors='replace')
            if re.search(r'\b(LITERAL|TODO|FIXME)\b', t):
                hits.append(f)
    check('7 no LITERAL/TODO/FIXME marker in src\\game or src\\host', not hits, ', '.join(hits[:4]))


def check_architecture(files):
    arch = (ROOT / 'ARCHITECTURE.md').read_text(encoding='utf-8')
    missing = []
    for f in files:
        if f.startswith(('src/game/', 'src/host/')) and f.endswith(('.cpp', '.hpp')):
            stem = Path(f).stem
            if stem not in arch:
                missing.append(f)
    check('8 ARCHITECTURE.md names every source file of src\\game and src\\host', not missing, ', '.join(missing[:6]))


def check_conventions():
    r = subprocess.run([sys.executable, '-I', 'tools/g2_conventions_check.py', '--strict'], cwd=ROOT, capture_output=True, text=True)
    check('9 conventions checker --strict', r.returncode == 0, r.stdout.strip().split('\n')[-1] if r.returncode else '')


def main():
    if len(sys.argv) != 2:
        print(__doc__)
        return 2
    rom = Path(sys.argv[1]).read_bytes()
    if len(rom) != 4096 or hashlib.sha1(rom).hexdigest() != SHA1:
        print('this is not the right ROM (4,096 bytes, SHA-1 %s)' % SHA1)
        return 2
    files = git_files()
    check_git(files)
    used = check_tables(rom)
    check_no_code(rom, used, files)
    check_reads_no_files()
    check_rule7()
    check_markers(files)
    check_architecture(files)
    check_conventions()
    ok = all(results)
    print('result: %s (%d of %d checks)' % ('OK' if ok else 'FAILED', sum(results), len(results)))
    return 0 if ok else 1


sys.exit(main())
