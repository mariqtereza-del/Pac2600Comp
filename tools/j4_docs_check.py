"""Stage J4: checks the public documents (DECISIONS.md T-J4-1).

    python -I tools\\j4_docs_check.py          must say result: OK

The public documents are README.md, BUILDING.md, docs/HOW_IT_WORKS.md, docs/PRESERVATION.md, and the two texts inside tools/make_release.ps1 that go into the
release (README.txt and NOTICE.txt). It checks that: the first screen of the README says the game is not included and who owns it; the SHA-1 written in the
documents is the one the program checks; every key of the program and every page of its menu is mentioned; relative links lead somewhere; the rights notice is
the same in the README and the release; and no public document names a cartridge address, holds a run of bytes or shows a private path or address.
Standard library only; it reads files and writes nothing."""
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PUBLIC = ["README.md", "BUILDING.md", "docs/HOW_IT_WORKS.md", "docs/PRESERVATION.md"]
PENDING = set()   # links to files that a later stage creates (none now: LICENSE and THIRD_PARTY_NOTICES.md came with Stage J5)

ADDRESS = re.compile(r"(?:\$F[0-9A-Fa-f]{3}\b|\b0x[Ff][0-9A-Fa-f]{3}\b|\bL_F[0-9A-F]{3}\b)")
HEX_RUN = re.compile(r"(?:\b[0-9A-Fa-f]{2}\b[ ,]+){8,}\b[0-9A-Fa-f]{2}\b")
HEX_DOLLAR = re.compile(r"(?:\$[0-9A-Fa-f]{2}\b[ ,]+){8,}\$[0-9A-Fa-f]{2}\b")
HEX_LONG = re.compile(r"\b(?=[0-9]*[A-Fa-f])[0-9A-Fa-f]{18,}\b")
PRIVATE = re.compile(r"F:\\Ports|C:\\Users|@gmail|mariqtereza|\\AppData\\", re.IGNORECASE)
LINK = re.compile(r"\]\(([^)\s]+)\)")
MENU_PAGES = ["Game options", "Graphics options", "Sound options", "Control options", "Achievements", "Statistics"]

problems = []
notes = []


def read(path):
    with open(os.path.join(ROOT, path), encoding="utf-8") as f:
        return f.read()


def check(ok, what):
    if not ok:
        problems.append(what)


def here_string(script, name):
    """The text of $name = @" ... "@ in the release script."""
    m = re.search(r"\$" + name + r' = @"\n(.*?)\n"@', script, re.DOTALL)
    return m.group(1) if m else None


def main():
    texts = {}
    for p in PUBLIC:
        if not os.path.exists(os.path.join(ROOT, p)):
            problems.append("missing public document: " + p)
            continue
        texts[p] = read(p)
    script = read("tools/make_release.ps1")
    release_readme = here_string(script, "readme")
    release_notice = here_string(script, "rightsNotice")
    check(release_readme is not None, "the release README text was not found in make_release.ps1")
    check(release_notice is not None, "the release NOTICE text was not found in make_release.ps1")
    if release_readme:
        texts["release README.txt"] = release_readme
    if release_notice:
        texts["release NOTICE.txt"] = release_notice
    if problems:
        return finish()

    # The first screen of the README: the game is not included, the file is checked, the rights notice.
    first = "\n".join(texts["README.md"].splitlines()[:14]).lower()
    for phrase in ("not included", "your own", "sha-1", "trademark", "rights holders", "unofficial"):
        check(phrase in first, "README.md, first screen: no '%s'" % phrase)

    # The fingerprint the documents state is the one the program checks.
    sha1 = re.search(r'kExpectedSha1 = "([0-9a-f]{40})"', read("src/host/rom_finder.hpp")).group(1)
    for name in ("README.md", "BUILDING.md", "release README.txt"):
        check(sha1 in texts[name].lower(), "%s does not state the SHA-1 the program checks (%s)" % (name, sha1))

    # Every key of the program, every page of the menu.
    controls = read("src/host/controls.cpp")
    keys = sorted(set(re.findall(r"SDL_SCANCODE_(F\d+)", controls)) | set(re.findall(r'\{"(F\d+)",', controls)))   # the bindings, and F1 and F7 of the help list
    check(len(keys) >= 7, "found only %d function keys in controls.cpp" % len(keys))
    menu_source = read("src/host/menu.cpp").lower()
    for name in ("README.md", "release README.txt"):
        body = texts[name]
        for k in keys:
            check(re.search(r"\b" + k + r"\b", body) is not None, "%s does not mention the key %s" % (name, k))
        for needle in ("Alt + Enter", "Esc", "Space"):
            check(needle in body, "%s does not mention '%s'" % (name, needle))
        for page in MENU_PAGES:
            check(page.lower().split()[0] in body.lower(), "%s does not mention the menu page '%s'" % (name, page))
    for page in MENU_PAGES:
        check(page.lower() in menu_source or page.lower() + "..." in menu_source, "the menu has no page called '%s' (the documents are out of date)" % page)

    # The release text must not still tell the player that Esc quits.
    check("Esc                     quit" not in release_readme and "Esc to quit" not in release_readme, "the release README still says Esc quits")

    # The rights notice is the same everywhere.
    for sentence in ("unofficial, fan-made program", "Bandai Namco Entertainment Inc."):
        for name in ("README.md", "release README.txt", "release NOTICE.txt"):
            check(sentence in " ".join(texts[name].split()), "%s lacks the rights-notice words '%s'" % (name, sentence))

    # Links.
    for name in PUBLIC:
        base = os.path.dirname(name)
        for target in LINK.findall(texts[name]):
            if target.startswith(("http:", "https:", "#", "mailto:")):
                continue
            path = target.split("#")[0]
            full = os.path.normpath(os.path.join(ROOT, base, path))
            if os.path.exists(full):
                continue
            if os.path.normpath(os.path.join(base, path)).replace("\\", "/") in PENDING:
                notes.append("%s links to %s, which Stage J5 creates" % (name, path))
            else:
                problems.append("%s: the link %s leads nowhere" % (name, target))

    # Nothing in them that belongs only to the owner or to the cartridge.
    for name, body in texts.items():
        for number, line in enumerate(body.splitlines(), 1):
            if ADDRESS.search(line):
                problems.append("%s line %d names a cartridge address" % (name, number))
            long_hits = [m for m in HEX_LONG.findall(line) if len(m) not in (32, 40, 64)]
            if HEX_RUN.search(line) or HEX_DOLLAR.search(line) or long_hits:
                problems.append("%s line %d holds a run of bytes" % (name, number))
            if PRIVATE.search(line):
                problems.append("%s line %d shows a private path or address" % (name, number))
    return finish(len(texts))


def finish(count=0):
    print("public texts checked: %d" % count)
    for n in notes:
        print("pending:", n)
    for p in problems:
        print("PROBLEM:", p)
    print("result:", "OK" if not problems else "FAILED")
    return 0 if not problems else 1


if __name__ == "__main__":
    sys.exit(main())
