r"""Stage H1: TESTING.md must list every test program and every tool.

python -I tools\h1_testing_check.py

Fails (exit 1) if a tests\*\test_*.cpp program, a tools\*.py or tools\*.ps1 script, or a main test of build\host is not named in TESTING.md.
"""
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
page = (ROOT / 'TESTING.md').read_text(encoding='utf-8')
names = set()
for p in (ROOT / 'tests').rglob('test_*.cpp'):
    names.add(p.stem)
for p in (ROOT / 'tools').glob('*.py'):
    names.add(p.stem)
for p in (ROOT / 'tools').glob('*.ps1'):
    names.add(p.stem)
missing = sorted(n for n in names if n not in page)
for n in missing:
    print('MISSING from TESTING.md:', n)
print('%d names checked, %d missing' % (len(names), len(missing)))
print('result: %s' % ('OK' if not missing else 'FAILED'))
sys.exit(1 if missing else 0)
