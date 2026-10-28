"""Stage J5: the licence and the third-party notices (DECISIONS.md T-J5-1).

    python -I tools\\j5_notices_check.py            checks LICENSE and THIRD_PARTY_NOTICES.md; must say result: OK
    python -I tools\\j5_notices_check.py --write    rewrites the shader table inside THIRD_PARTY_NOTICES.md from the shader files themselves

The shader table says, for each of the files in third_party/shaders/MANIFEST.txt, what the file says about its own author and licence: "GNU GPL version 2 or later",
"public domain", or "not stated" (nothing is guessed). The manifest's SHA-256 of each file is checked too, so the table is about the unchanged files.
Standard library only; the only file it writes is THIRD_PARTY_NOTICES.md, and only with --write."""
import hashlib
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
NOTICES = os.path.join(ROOT, "THIRD_PARTY_NOTICES.md")
SHADER_DIR = os.path.join(ROOT, "third_party", "shaders")
BEGIN, END = "<!-- shader-table:begin (made by tools/j5_notices_check.py --write) -->", "<!-- shader-table:end -->"
GPL2_LATER = "either version 2 of the License, or (at your option) any later version"


def read_text(path):
    with open(path, encoding="utf-8", errors="replace") as f:
        return f.read()


def norm(text):
    return " ".join(text.split())


def manifest():
    rows = []
    for line in read_text(os.path.join(SHADER_DIR, "MANIFEST.txt")).splitlines():
        m = re.match(r"^([0-9a-f]{64})\s+(\S+)\s+\((\d+) bytes\)", line)
        if m:
            rows.append((m.group(1), m.group(2), int(m.group(3))))
    return rows


def describe(path, relative):
    """(credit, licence) as the file itself states them."""
    if relative.endswith(".png"):
        return "(a picture: no text)", "not stated"
    if relative.endswith(".slangp"):
        return "(a preset: a list of passes)", "not stated"
    text = read_text(path)
    flat = norm(re.sub(r"^[ \t]*(//+|\*+|/\*+)", " ", text, flags=re.M))
    m = re.search(r"Copyright \(C\)[^\n]*", text)
    credit = re.sub(r"\s*-?\s*\S+@\S+", "", " ".join(m.group(0).split())) if m else ""   # an author's e-mail address is not republished
    if not credit and "based on Themaister's NTSC shader" in text:
        credit = "\"based on Themaister's NTSC shader\""
    if not credit and re.search(r"by Timothy Lottes", text):
        credit = "by Timothy Lottes"
    if not credit:
        credit = "(no author line)"
    if GPL2_LATER in flat:
        licence = "GNU GPL version 2 or later"
    elif "PUBLIC DOMAIN" in text:
        licence = "public domain"
    else:
        licence = "not stated"
    return credit, licence


def table():
    lines = ["| File | Credit in the file | Licence the file states |", "| --- | --- | --- |"]
    problems = []
    for sha, relative, size in manifest():
        path = os.path.join(SHADER_DIR, "libretro", "crt", relative.replace("/", os.sep))
        if not os.path.exists(path):
            problems.append("the manifest names %s, which is missing" % relative)
            continue
        with open(path, "rb") as f:
            data = f.read()
        if hashlib.sha256(data).hexdigest() != sha:
            problems.append("%s is not the file the manifest recorded (SHA-256 differs)" % relative)
        credit, licence = describe(path, relative)
        lines.append("| `%s` | %s | %s |" % (relative, credit.replace("|", "/"), licence))
    return lines, problems


def main():
    write = "--write" in sys.argv
    problems = []
    rows, bad = table()
    problems += bad
    block = BEGIN + "\n" + "\n".join(rows) + "\n" + END
    notices = read_text(NOTICES) if os.path.exists(NOTICES) else ""
    if BEGIN not in notices or END not in notices:
        problems.append("THIRD_PARTY_NOTICES.md has no shader-table markers")
    elif write:
        a, b = notices.index(BEGIN), notices.index(END) + len(END)
        notices = notices[:a] + block + notices[b:]
        with open(NOTICES, "w", encoding="utf-8", newline="") as f:
            f.write(notices)
        print("rewrote the shader table (%d files)" % (len(rows) - 2))
    else:
        a, b = notices.index(BEGIN), notices.index(END) + len(END)
        if notices[a:b] != block:
            problems.append("the shader table in THIRD_PARTY_NOTICES.md differs from the files: run --write")

    # LICENSE: the notice, then the full text of the GNU GPL version 2 exactly as Stella carries it.
    licence_path = os.path.join(ROOT, "LICENSE")
    if not os.path.exists(licence_path):
        problems.append("LICENSE is missing")
    else:
        licence = read_text(licence_path)
        gpl = read_text(os.path.join(ROOT, "third_party", "stella", "License.txt"))
        if norm(gpl) not in norm(licence):
            problems.append("LICENSE does not hold the full GPL version 2 text of third_party/stella/License.txt")
        if GPL2_LATER not in norm(licence).replace("either version 2 of the License, or any later version", GPL2_LATER) and "or (at your option) any later version" not in norm(licence):
            problems.append("LICENSE does not say 'version 2 or (at your option) any later version'")
        if "Pac2600Comp" not in licence:
            problems.append("LICENSE does not name the program")

    # The notices carry the texts they must carry, word for word.
    flat = norm(notices)
    sdl = norm(read_text(os.path.join(ROOT, "third_party", "SDL3", "LICENSE.txt")))
    if sdl not in flat:
        problems.append("the SDL3 zlib licence text is not in THIRD_PARTY_NOTICES.md word for word")
    stella_line = next((l.strip() for l in read_text(os.path.join(ROOT, "third_party", "stella", "Copyright.txt")).splitlines() if l.startswith("Copyright (c)")), "")
    if not stella_line or norm(stella_line) not in flat:
        problems.append("Stella's copyright line is not in THIRD_PARTY_NOTICES.md: %r" % stella_line)
    for needle in ("Stella 7.0", "SDL3", "Timothy Lottes", "guest(r)", "Dr. Venom", "cgwg", "Themaister", "Microsoft", "Tod Frye", "Toru Iwatani", "Atari", "Bandai Namco",
                   "RetroAchievements", "not stated", "libretro"):
        if needle not in notices:
            problems.append("THIRD_PARTY_NOTICES.md does not mention '%s'" % needle)
    geom_consent = "Feel free to distribute my shaders under the GPL"
    if geom_consent not in notices:
        problems.append("the CRT-Geom consent of cgwg is not quoted")

    # The release and the source zip carry both files.
    script = read_text(os.path.join(ROOT, "tools", "make_release.ps1"))
    for needle in ("LICENSE", "THIRD_PARTY_NOTICES.md"):
        if script.count(needle) < 2:
            problems.append("tools/make_release.ps1 does not put %s into the release and the source zip" % needle)

    print("shader files accounted for: %d" % (len(rows) - 2))
    states = {}
    for r in rows[2:]:
        licence = r.rsplit("|", 2)[-2].strip()
        states[licence] = states.get(licence, 0) + 1
    for k, v in sorted(states.items()):
        print("  %-28s %d" % (k, v))
    for p in problems:
        print("PROBLEM:", p)
    print("result:", "OK" if not problems else "FAILED")
    return 0 if not problems else 1


if __name__ == "__main__":
    sys.exit(main())
