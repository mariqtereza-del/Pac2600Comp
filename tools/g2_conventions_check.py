"""Stage G2: measures docs\\CONVENTIONS.md on src\\game and src\\host.

python -I tools\\g2_conventions_check.py [--strict]

Reports files over 400 lines, functions over 80 lines (a definition that starts in column 0 and ends with '{'
up to the closing '}' in column 0), lines over 160 characters, the number of ORIGINAL: tags, and every ORIGINAL:
tag whose 'Evidence:' path does not exist. Exit 0 unless --strict and something was found.
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
FILE_LIMIT, FUNC_LIMIT, LINE_LIMIT = 400, 80, 160
DEF = re.compile(r'^( {0,2})(?!(?:if|for|while|switch|else|return|catch)\b)[A-Za-z_][\w:<>,&\* ]*\(.*\)\s*(const)?\s*\{\s*$')


def sources():
    for sub in ('game', 'host', 'lockstep'):
        for ext in ('*.cpp', '*.hpp'):
            yield from sorted((ROOT / 'src' / sub).rglob(ext))


def main():
    strict = '--strict' in sys.argv
    big_files, big_funcs, long_lines, tags, bad_tags = [], [], [], 0, []
    for f in sources():
        rel = f.relative_to(ROOT)
        lines = f.read_text(encoding='utf-8', errors='replace').splitlines()
        if len(lines) > FILE_LIMIT:
            big_files.append((str(rel), len(lines)))
        start = None
        for n, line in enumerate(lines, 1):
            if len(line) > LINE_LIMIT:
                long_lines.append((str(rel), n))
            m = DEF.match(line) if start is None else None
            if m:
                start = (n, m.group(1))
            elif start is not None and line.startswith(start[1] + '}') and not line.startswith(start[1] + '} '):
                if n - start[0] + 1 > FUNC_LIMIT:
                    big_funcs.append((str(rel), start[0], n - start[0] + 1))
                start = None
            if '// ORIGINAL:' in line:
                tags += 1
                m = re.search(r'Evidence:\s*([^\s;,)]+(?:\\[^\s;,)]+)*)', line)
                p = m.group(1).rstrip('.') if m else None
                if not p or not (ROOT / p.replace('\\', '/')).exists():
                    bad_tags.append((str(rel), n, p))
    print('files over %d lines: %d' % (FILE_LIMIT, len(big_files)))
    for x in big_files:
        print('  %s  %d' % x)
    print('functions over %d lines: %d' % (FUNC_LIMIT, len(big_funcs)))
    for x in big_funcs:
        print('  %s:%d  %d lines' % x)
    print('lines over %d characters: %d' % (LINE_LIMIT, len(long_lines)))
    print('ORIGINAL: tags: %d, with a missing evidence path: %d' % (tags, len(bad_tags)))
    for x in bad_tags:
        print('  %s:%d  %s' % x)
    found = len(big_files) + len(big_funcs) + len(long_lines) + len(bad_tags)
    print('result: %s' % ('CLEAN' if found == 0 else 'findings: %d (%s)' % (found, 'strict, failing' if strict else 'report only')))
    return 1 if strict and found else 0


if __name__ == '__main__':
    sys.exit(main())
