"""Stage J6: what a push would send, and whether the game file was ever in it (DECISIONS.md T-J6-1). Also used at the final gate (J11).

    python -I tools\\j6_prepush_check.py rom\\pacman.a26 [--list <file>]       must say result: OK

It looks at ALL commits, not just the newest: a ROM that was committed once and deleted later is still in the history and would be pushed. For every file that ever existed
it checks the name (a cartridge or archive extension) and, for every version of every file of 4096 bytes or more, whether the content is the cartridge file (same SHA-1) or
holds it whole. It lists what the newest commit contains by folder, and with --list writes every file and its size to a text file. It reads Git and the ROM only; it changes
nothing except the --list file. Standard library only."""
import hashlib
import os
import subprocess
import sys
import threading

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ROM_LIKE = (".a26", ".bin", ".rom", ".rar", ".7z", ".zip", ".tar", ".gz")
KNOWN_ARCHIVE = {"third_party/stella/src/os/macos/template.dmg.gz"}   # part of Stella's unchanged source (T-D15-1)
ROM_SHA1 = "0940fea7f04cdb6d4b90c5ad1a7e344e68f6dbb1"


def git(*args, check=True):
    return subprocess.run(["git", "-C", ROOT] + list(args), capture_output=True, check=check).stdout


def main():
    rom_arg = next((a for i, a in enumerate(sys.argv[1:], 1) if not a.startswith("--") and sys.argv[i - 1] not in ("--list", "--expect-sha1")), None)
    list_file = sys.argv[sys.argv.index("--list") + 1] if "--list" in sys.argv else None
    if not rom_arg or not os.path.exists(os.path.join(ROOT, rom_arg)):
        print("usage: python -I tools\\j6_prepush_check.py rom\\pacman.a26 [--list <file>]")
        return 2
    with open(os.path.join(ROOT, rom_arg), "rb") as f:
        rom = f.read()
    expect = sys.argv[sys.argv.index("--expect-sha1") + 1] if "--expect-sha1" in sys.argv else ROM_SHA1   # --expect-sha1 is for the tool's own test with an invented file
    if hashlib.sha1(rom).hexdigest() != expect:
        print("the file given is not the cartridge file (SHA-1 differs); the check needs the real one to know what to look for")
        return 2
    problems = []

    # Every object that any commit reaches, with the path it had.
    listing = git("rev-list", "--objects", "--all").decode("utf-8", "replace").splitlines()
    oids, paths = [], {}
    for line in listing:
        oid, _, path = line.partition(" ")
        oids.append(oid)
        if path:
            paths.setdefault(oid, set()).add(path)
    commits = int(git("rev-list", "--all", "--count").decode().strip())

    # Names: any cartridge or archive extension, in any commit.
    named = sorted({p for ps in paths.values() for p in ps if p.lower().endswith(ROM_LIKE) and p not in KNOWN_ARCHIVE})
    for p in named:
        problems.append("a file named like a ROM or an archive is in the history: " + p)

    # Contents: size and type of everything, then every blob of 4096 bytes or more is read.
    check = subprocess.run(["git", "-C", ROOT, "cat-file", "--batch-check"], input="\n".join(oids).encode(), capture_output=True, check=True).stdout.decode().splitlines()
    blobs = []
    for line in check:
        parts = line.split()
        if len(parts) == 3 and parts[1] == "blob":
            blobs.append((parts[0], int(parts[2])))
    big = [b for b in blobs if b[1] >= len(rom)]
    proc = subprocess.Popen(["git", "-C", ROOT, "cat-file", "--batch"], stdin=subprocess.PIPE, stdout=subprocess.PIPE)

    def feed():
        for oid, _ in big:
            proc.stdin.write((oid + "\n").encode())
        proc.stdin.close()
    threading.Thread(target=feed, daemon=True).start()
    scanned = 0
    for oid, size in big:
        header = proc.stdout.readline().split()
        data = proc.stdout.read(int(header[2]))
        proc.stdout.read(1)   # the newline after each object
        scanned += 1
        if hashlib.sha1(data).hexdigest() == expect or rom in data:
            problems.append("the cartridge file is in the history: blob %s (paths: %s)" % (oid[:10], ", ".join(sorted(paths.get(oid, ["?"])))[:120]))
    proc.wait()

    # What the newest commit holds.
    files = git("ls-files", "-z").decode("utf-8").split("\0")
    files = sorted(f for f in files if f)
    sizes = {}
    for f in files:
        sizes[f] = os.path.getsize(os.path.join(ROOT, f))
    folders = {}
    for f in files:
        top = f.split("/")[0] if "/" in f else "(top folder)"
        count, total = folders.get(top, (0, 0))
        folders[top] = (count + 1, total + sizes[f])
    own = [f for f in files if not f.startswith("third_party/")]
    personal = 0
    for f in own:
        try:
            with open(os.path.join(ROOT, f), "rb") as fh:
                body = fh.read()
        except OSError:
            continue
        if b"mariqtereza" in body or b"F:\\Ports" in body:
            personal += 1

    if list_file:
        with open(list_file, "w", encoding="utf-8", newline="\n") as out:
            out.write("Files in the newest commit (%d), with their size in bytes\n" % len(files))
            for f in files:
                out.write("%9d  %s\n" % (sizes[f], f))

    print("commits that would be pushed: %d" % commits)
    print("objects reached: %d; files ever named like a ROM or archive: %d; versions of 4096 bytes or more searched for the cartridge: %d" % (len(oids), len(named), scanned))
    print("files in the newest commit: %d, %s bytes" % (len(files), "{:,}".format(sum(sizes.values()))))
    for top, (count, total) in sorted(folders.items(), key=lambda kv: -kv[1][0]):
        print("  %-14s %5d files  %12s bytes" % (top, count, "{:,}".format(total)))
    print("our own files that mention the owner's account name or a path on this PC: %d (fine in a private repository; to be decided at J11)" % personal)
    for p in problems:
        print("PROBLEM:", p)
    print("result:", "OK" if not problems else "FAILED")
    return 0 if not problems else 1


if __name__ == "__main__":
    sys.exit(main())
