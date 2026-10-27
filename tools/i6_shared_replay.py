"""Stage I6 (D-011): two players on one set of controls, replayed end to end.

Makes an input recording (a two-player game, then seeded random joystick moves) twice: with the optional line
`shared_controls=1` and without it. Each is replayed without a window in the native program (pacman2600.exe) and in the
program with the original ROM inside (pacman2600_ref.exe), and the traces are compared frame by frame.

Checks (must end with "result: OK"):
  1. With shared controls, the picture and sound of every frame are the same in both programs (the native game reads the
     right joystick for player 2 exactly as the original does).
  2. Player 2's turn happened ($E6 bits 6 and 7 set for some frames) and, in it, the game's joystick byte ($ED) took a
     direction in the reference program, i.e. the input really reached player 2.
  3. Without the line, the same inputs give a different game once player 2's turn begins (the old behaviour), and
     both programs still agree with each other.

Usage: python -I tools\\i6_shared_replay.py <rom> <scratch folder> [frames] [seed]
Only reads the ROM; writes only into the scratch folder.
"""
import hashlib
import os
import random
import subprocess
import sys


def make_inputs(path, sha1, frames, seed, shared):
    rng = random.Random(seed)
    changes = [(0, 0), (60, 32), (62, 0), (80, 64), (90, 0)]  # one Select press (game 1 -> two players), then Reset
    frame = 120
    while frame < frames:
        bits = rng.choice([0, 1, 2, 4, 8, 1, 2, 4, 8])
        changes.append((frame, bits))
        frame += rng.randint(8, 50)
    with open(path, "w", newline="\n") as f:
        f.write("PacMan2600-inputs 1\nrom_sha1=%s\n" % sha1)
        if shared:
            f.write("shared_controls=1\n")
        f.write("frames=%d\n" % frames)
        for fr, bits in changes:
            f.write("%d %d\n" % (fr, bits))


def run(exe, rom, inputs, trace, png, settings):
    cmd = [exe, "--headless", png, rom, "--replay", inputs, "--trace", trace, "--settings", settings]
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0:
        print("FAILED:", " ".join(cmd), r.returncode, r.stdout[-300:], r.stderr[-300:])
        sys.exit(2)


def rows(path):
    out = []
    with open(path) as f:
        for line in f:
            p = line.rstrip("\n").split(",")
            if len(p) == 6 and p[0].isdigit():
                out.append(p)
    return out


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 2
    rom, work = sys.argv[1], sys.argv[2]
    frames = int(sys.argv[3]) if len(sys.argv) > 3 else 6000
    seed = int(sys.argv[4]) if len(sys.argv) > 4 else 7
    os.makedirs(work, exist_ok=True)
    sha1 = hashlib.sha1(open(rom, "rb").read()).hexdigest()
    host = os.path.join("build", "host")
    ok = True

    def check(name, good, detail=""):
        nonlocal ok
        print(("ok    " if good else "FAIL  ") + name + (" " + detail if detail else ""))
        ok = ok and good

    traces = {}
    for shared in (True, False):
        tag = "shared" if shared else "separate"
        inputs = os.path.join(work, "i6_%s.inputs" % tag)
        make_inputs(inputs, sha1, frames, seed, shared)
        for prog, exe in (("native", "pacman2600.exe"), ("ref", "pacman2600_ref.exe")):
            t = os.path.join(work, "i6_%s_%s.trace" % (tag, prog))
            run(os.path.join(host, exe), rom, inputs, t, os.path.join(work, "i6_%s_%s.png" % (tag, prog)), os.path.join(work, "i6_settings.ini"))
            traces[(tag, prog)] = rows(t)

    for tag in ("shared", "separate"):
        a, b = traces[(tag, "native")], traces[(tag, "ref")]
        same = len(a) == len(b) == frames and all(x[3] == y[3] and x[4] == y[4] for x, y in zip(a, b))
        diff = next((i for i, (x, y) in enumerate(zip(a, b)) if x[3] != y[3] or x[4] != y[4]), None)
        check("%s: native and original agree on the picture and sound of all %d frames" % (tag, frames), same, "" if same else "first difference at frame %s" % diff)

    ref = traces[("shared", "ref")]

    def ram(row):
        return bytes.fromhex(row[5])

    turn2 = [i for i, r in enumerate(ref) if ram(r)[0xE6 - 0x80] & 0xC0 == 0xC0]
    check("player 2's turn happened", len(turn2) > 0, "(%d frames)" % len(turn2))
    moved = sum(1 for i in turn2 if ram(ref[i])[0xED - 0x80] & 0x0F not in (0, 0x0F))
    check("in player 2's turn the game's joystick byte took a direction", moved > 0, "(%d frames)" % moved)

    s, u = traces[("shared", "native")], traces[("separate", "native")]
    first = next((i for i, (x, y) in enumerate(zip(s, u)) if x[3] != y[3]), None)
    check("without shared controls the game differs once player 2 plays", first is not None and len(turn2) > 0 and first >= turn2[0] - 2,
          "(first difference at frame %s, player 2 starts at %s)" % (first, turn2[0] if turn2 else None))

    print("result: " + ("OK" if ok else "FAIL"))
    return 0 if ok else 1


sys.exit(main())
