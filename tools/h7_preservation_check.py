r"""Stage H7 (Gate H): the preservation suite is frozen.

python -I tools\h7_preservation_check.py [--root <folder>] [--add] [--drill <new scratch folder>]

The frozen files are the 70 baseline entries and inputs in baseline\ and the fingerprint files in tests\game\data\. They are listed with
SHA-256 and size in tests\preservation\MANIFEST.txt. Checks:
  1. every listed file is present and unchanged;
  2. the manifest only grows: every line of the committed manifest (git HEAD) is still in it, unchanged;
  3. every frozen file that exists at the tag 1.0.0-preservation is byte-identical to the tag's version (skipped, and said so, where Git or
     the tag is missing, as in a zip download);
  4. frozen-looking files that are not listed yet are reported (new traces may be added, never silently).
--add appends the unlisted files to the manifest (it never changes or removes a line). --root checks another folder (tests, drills).
--drill copies the frozen files to a NEW scratch folder with its own Git repository and tag, damages the copies in six ways and shows that each
is caught (the real files are only read).
Exit 0 = OK, 1 = a check failed.
"""
import hashlib
import subprocess
import sys
from pathlib import Path

TAG = '1.0.0-preservation'
PATTERNS = ['baseline/BL-*.entry.txt', 'baseline/BL-*.inputs', 'tests/game/data/BL-*.fingerprints.txt']
MANIFEST = 'tests/preservation/MANIFEST.txt'


def content(path):
    # All frozen files are plain text; Git may store them with LF and a checkout may give CRLF (.gitattributes text=auto), so compare as LF.
    return path.read_bytes().replace(b'\r\n', b'\n')


def sha256(path):
    return hashlib.sha256(content(path)).hexdigest()


def git(root, *args):
    done = subprocess.run(['git', '-C', str(root), *args], capture_output=True)
    return done.returncode, done.stdout


def read_manifest(path):
    entries = {}
    lines = []
    for line in path.read_text(encoding='utf-8').splitlines():
        if not line.strip() or line.startswith('#'):
            continue
        digest, size, name = line.split(None, 2)
        entries[name] = (digest, int(size))
        lines.append(line)
    return entries, lines


def drill(scratch):
    """Tamper drill on a scratch copy (a new folder, nothing is deleted): every kind of damage must make the check say FAILED."""
    import shutil
    scratch = Path(scratch).resolve()
    if scratch.exists():
        print('the drill folder already exists, choose a new one:', scratch)
        return 2
    real = Path(__file__).resolve().parent.parent
    names = [p.relative_to(real).as_posix() for pattern in PATTERNS + ['tests/preservation/MANIFEST.txt'] for p in real.glob(pattern)]
    here = Path(__file__).resolve()
    ident = ['-c', 'user.name=drill', '-c', 'user.email=drill@example.invalid']

    def fresh(name):
        copy = scratch / name
        for n in names:
            (copy / n).parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(real / n, copy / n)
        git(copy, 'init', '-q')
        git(copy, 'add', '-A')
        git(copy, *ident, 'commit', '-q', '-m', 'drill')
        git(copy, *ident, 'tag', TAG)
        return copy

    def run(copy):
        return subprocess.run([sys.executable, '-I', str(here), '--root', str(copy)], capture_output=True, text=True)

    results = []
    copy = fresh('untouched')
    results.append(('untouched copy passes', run(copy).returncode == 0))
    copy = fresh('edited')
    target = copy / 'baseline/BL-001.inputs'
    target.write_bytes(target.read_bytes() + b'x')
    results.append(('edited file is caught', run(copy).returncode == 1))
    copy = fresh('edited_with_manifest')
    target = copy / 'baseline/BL-001.inputs'
    target.write_bytes(target.read_bytes() + b'x')
    manifest = copy / MANIFEST
    manifest.write_text(manifest.read_text(encoding='utf-8').replace(sha256_of_original(real, 'baseline/BL-001.inputs'), sha256(target)), encoding='utf-8')
    done = run(copy)
    results.append(('file and manifest edited together: caught against the tag and the commit', done.returncode == 1 and 'differs from tag' in done.stdout))
    copy = fresh('deleted')
    (copy / 'baseline/BL-002.entry.txt').unlink()
    results.append(('deleted file is caught', run(copy).returncode == 1))
    copy = fresh('shortened_manifest')
    manifest = copy / MANIFEST
    manifest.write_text(''.join(manifest.read_text(encoding='utf-8').splitlines(True)[:-1]), encoding='utf-8')
    done = run(copy)
    results.append(('removed manifest line is caught', done.returncode == 1 and 'manifest line removed' in done.stdout))
    copy = fresh('new_file')
    (copy / 'baseline/BL-999.inputs').write_text('PacMan2600-inputs 1\n', encoding='utf-8')
    done = run(copy)
    results.append(('new unlisted file is reported', done.returncode == 1 and 'NOT LISTED' in done.stdout))
    subprocess.run([sys.executable, '-I', str(here), '--root', str(copy), '--add'], capture_output=True)
    results.append(('after --add the new file is accepted', run(copy).returncode == 0))
    for text, ok in results:
        print('%s  %s' % ('PASS' if ok else 'FAIL', text))
    allok = all(ok for _, ok in results)
    print('drill result: %s' % ('OK' if allok else 'FAILED'))
    return 0 if allok else 1


def sha256_of_original(real, name):
    return sha256(real / name)


def main():
    args = sys.argv[1:]
    if '--drill' in args:
        return drill(args[args.index('--drill') + 1])
    root = Path(__file__).resolve().parent.parent
    if '--root' in args:
        root = Path(args[args.index('--root') + 1]).resolve()
    manifest_path = root / MANIFEST
    if not manifest_path.exists() and '--add' not in args:
        print('no manifest at', manifest_path)
        print('result: FAILED')
        return 1
    problems = []
    entries, lines = read_manifest(manifest_path) if manifest_path.exists() else ({}, [])

    found = sorted(p.relative_to(root).as_posix() for pattern in PATTERNS for p in root.glob(pattern))
    unlisted = [f for f in found if f not in entries]
    if '--add' in args:
        header = '' if manifest_path.exists() else '# SHA-256  bytes  path (frozen files; lines are only ever added). See tests/preservation/README.md\n'
        manifest_path.parent.mkdir(parents=True, exist_ok=True)
        with open(manifest_path, 'a', encoding='utf-8', newline='\n') as out:
            out.write(header)
            for name in unlisted:
                out.write('%s %d %s\n' % (sha256(root / name), len(content(root / name)), name))
        print('added %d lines to the manifest' % len(unlisted))
        return 0

    # 1. listed files present and unchanged
    for name, (digest, size) in entries.items():
        path = root / name
        if not path.exists():
            problems.append('MISSING frozen file: ' + name)
        elif sha256(path) != digest or len(content(path)) != size:
            problems.append('CHANGED frozen file: ' + name)
    print('1. %d frozen files listed: %d problems' % (len(entries), len(problems)))

    # 2. the manifest only grows
    before = len(problems)
    code, out = git(root, 'show', 'HEAD:' + MANIFEST)
    if code != 0:
        print('2. manifest only grows: SKIPPED (no committed manifest in Git here)')
    else:
        committed = [l for l in out.decode('utf-8').splitlines() if l.strip() and not l.startswith('#')]
        for l in committed:
            if l not in lines:
                problems.append('manifest line removed or changed: ' + l.split(None, 2)[2])
        print('2. manifest only grows (%d committed lines): %d problems' % (len(committed), len(problems) - before))

    # 3. equal to the tag
    before = len(problems)
    code, _ = git(root, 'rev-parse', '--verify', '-q', 'refs/tags/' + TAG)
    if code != 0:
        print('3. frozen files equal tag %s: SKIPPED (no Git or no tag here)' % TAG)
    else:
        checked = 0
        for name in entries:
            code, blob = git(root, 'show', TAG + ':' + name)
            if code != 0:
                continue  # added after the tag: allowed, it is listed and hashed
            checked += 1
            if hashlib.sha256(blob.replace(b'\r\n', b'\n')).hexdigest() != sha256(root / name):
                problems.append('differs from tag %s: %s' % (TAG, name))
        print('3. %d frozen files that exist at tag %s are identical to it: %d problems' % (checked, TAG, len(problems) - before))

    # 4. unlisted files
    if unlisted:
        print('4. NOT LISTED yet (run with --add after review):')
        for f in unlisted:
            print('     ' + f)
        problems.append('%d unlisted frozen-looking files' % len(unlisted))
    else:
        print('4. no unlisted files')

    for p in problems:
        print('PROBLEM:', p)
    print('result: %s' % ('OK' if not problems else 'FAILED'))
    return 1 if problems else 0


sys.exit(main())
