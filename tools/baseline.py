"""Stage E1: make and check baseline entries (docs\\BASELINE.md).

An entry is baseline\\<ID>.entry.txt (plain words, in Git) plus baseline\\<ID>.inputs (the input, in Git). Its golden trace
is made by the host, no window: pacman2600.exe --headless ... --replay baseline\\<ID>.inputs --trace <file>, and kept in
reference\\baseline\\<ID>.trace (local only: it holds Atari's memory contents). The entry stores the trace's SHA-256.

  python -I tools\\baseline.py make  <rom> <ID> [<ID> ...]   make the trace twice, require byte-identical, store its SHA-256 in the entry
  python -I tools\\baseline.py check <rom> <ID> [<ID> ...]   make the trace again, compare with the stored SHA-256 (exit 1 on any difference)
  python -I tools\\baseline.py list                          one line per entry

Run it from the project folder. The ROM is only given to the program by path (never copied).
"""
import hashlib
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
EXE = pathlib.Path(__import__('os').environ.get('PACMAN_EXE') or ROOT / 'build' / 'host' / 'pacman2600.exe')
BASE = ROOT / 'baseline'
OUT = ROOT / 'reference' / 'baseline'


def read_entry(entry_id):
    path = BASE / (entry_id + '.entry.txt')
    text = path.read_text(encoding='utf-8')
    fields = {}
    for line in text.split('\n'):
        if line.startswith('observations:'):
            break
        if ': ' in line:
            key, value = line.split(': ', 1)
            fields[key.strip()] = value.strip()
    return path, text, fields


def make_trace(rom, entry_id, fields, target):
    inputs = BASE / fields['inputs']
    OUT.mkdir(parents=True, exist_ok=True)
    scratch = OUT / (entry_id + '.scratch.ini')
    pokes = []
    for item in fields.get('pokes', '').split():  # TEST ONLY memory edits, Stage E8: <frame>:<hex address>:<hex value>
        pokes += ['--poke', item]
    r = subprocess.run([str(EXE), '--headless', str(OUT / (entry_id + '.png')), str(rom), '--replay', str(inputs), '--trace', str(target),
                        '--settings', str(scratch)] + pokes, capture_output=True, text=True)
    if r.returncode != 0:
        raise SystemExit('%s: the program failed (exit %d)' % (entry_id, r.returncode))


def sha256(path):
    h = hashlib.sha256()
    h.update(pathlib.Path(path).read_bytes())
    return h.hexdigest()


def check_shape(entry_id, fields, trace):
    lines = trace.read_text(encoding='ascii').split('\n')
    rows = [l for l in lines[3:] if l]
    frames = int(fields['frames'])
    if len(rows) != frames:
        raise SystemExit('%s: the trace has %d frames, the entry says %d' % (entry_id, len(rows), frames))
    for n, row in enumerate(rows):
        if int(row.split(',', 1)[0]) != n or len(row.split(',')[5]) != 256:
            raise SystemExit('%s: frame %d is not in order or its memory is not 128 bytes' % (entry_id, n))
    if lines[1] != 'rom_sha1=' + fields['rom_sha1']:
        raise SystemExit('%s: the trace says another ROM than the entry' % entry_id)


def main():
    cmd = sys.argv[1] if len(sys.argv) > 1 else ''
    if cmd == 'list':
        for p in sorted(BASE.glob('*.entry.txt')):
            _, _, f = read_entry(p.name.split('.')[0])
            print('%s  %5s frames  %s' % (f['id'], f['frames'], f['title']))
        return 0
    if cmd not in ('make', 'check') or len(sys.argv) < 4:
        print(__doc__)
        return 2
    rom = pathlib.Path(sys.argv[2]).resolve()
    bad = 0
    for entry_id in sys.argv[3:]:
        path, text, fields = read_entry(entry_id)
        trace = OUT / (entry_id + '.trace')
        make_trace(rom, entry_id, fields, trace)
        check_shape(entry_id, fields, trace)
        digest = sha256(trace)
        if cmd == 'make':
            second = OUT / (entry_id + '.second.trace')
            make_trace(rom, entry_id, fields, second)
            if second.read_bytes() != trace.read_bytes():
                print('%s: two replays gave DIFFERENT traces' % entry_id)
                bad += 1
                continue
            new = []
            for line in text.split('\n'):
                if line.startswith('trace_sha256:'):
                    line = 'trace_sha256: ' + digest
                if line.startswith('trace_bytes:'):
                    line = 'trace_bytes: %d' % trace.stat().st_size
                new.append(line)
            path.write_text('\n'.join(new), encoding='utf-8', newline='')
            print('%s: two replays gave byte-identical traces (%d frames, %d bytes); SHA-256 %s stored' % (entry_id, int(fields['frames']), trace.stat().st_size, digest[:16] + '...'))
        else:
            same = digest == fields.get('trace_sha256')
            print('%s: fresh trace %s the stored SHA-256 (%s...)' % (entry_id, 'equals' if same else 'DIFFERS FROM', digest[:16]))
            bad += 0 if same else 1
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
