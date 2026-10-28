"""Stage J8: the version number and the changelog (DECISIONS.md T-J8-1).

    python -I tools\\j8_version_check.py          must say result: OK

The file VERSION holds the version (three numbers, big.feature.fix) and is the only place it is written for the tools: tools/make_release.ps1 and
tools/d16_gate_check.ps1 read it. CHANGELOG.md lists the versions newest first, the newest one is the one in VERSION, and the release and its source zip carry
the version file and the changelog. Standard library only; it reads files and writes nothing."""
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
problems = []


def read(path):
    with open(os.path.join(ROOT, path), encoding="utf-8") as f:
        return f.read()


def check(ok, what):
    if not ok:
        problems.append(what)


def number(text):
    return tuple(int(x) for x in text.split("."))


def main():
    version_text = read("VERSION")
    check(re.fullmatch(r"\d+\.\d+\.\d+\n?", version_text) is not None, "VERSION must hold one version such as 1.0.0 and nothing else: %r" % version_text)
    version = version_text.strip()

    log = read("CHANGELOG.md")
    headings = re.findall(r"^## \[(\d+\.\d+\.\d+)\] - (\d{4}-\d{2}-\d{2}|unreleased)\s*$", log, re.MULTILINE)
    check(len(headings) >= 1, "CHANGELOG.md has no entry of the form '## [x.y.z] - date'")
    if headings:
        check(headings[0][0] == version, "the newest entry of CHANGELOG.md is %s, VERSION says %s" % (headings[0][0], version))
        numbers = [number(h[0]) for h in headings]
        check(numbers == sorted(numbers, reverse=True) and len(set(numbers)) == len(numbers), "the entries of CHANGELOG.md are not newest first, each version once")
        check(all(h[1] != "unreleased" for h in headings[1:]), "only the newest entry may be 'unreleased'")
        dates = [h[1] for h in headings if h[1] != "unreleased"]
        check(dates == sorted(dates, reverse=True), "the dates of CHANGELOG.md are not newest first")
    for needle in ("big.feature.fix", "1.0.0-preservation"):
        check(needle in log, "CHANGELOG.md does not explain '%s'" % needle)

    # The scripts read the version from the file; none of them has a number of its own.
    release = read("tools/make_release.ps1")
    gate = read("tools/d16_gate_check.ps1")
    check("'VERSION'" in release and "'VERSION'" in gate, "make_release.ps1 and d16_gate_check.ps1 must read the file VERSION")
    for name, text in (("tools/make_release.ps1", release), ("tools/d16_gate_check.ps1", gate)):
        own = re.search(r"\$Version\s*=\s*'\d+\.\d+\.\d+'|Pac2600Comp-\d+\.\d+\.\d+|\[-Version \d+\.\d+\.\d+\]", text)
        check(own is None, "%s holds a version number of its own: %s" % (name, own.group(0) if own else ""))
    check("CHANGELOG.txt" in release, "the release zip does not carry CHANGELOG.txt")
    check(release.count("VERSION") >= 3 and "CHANGELOG.md" in release, "the source zip does not carry VERSION and CHANGELOG.md")
    check("CHANGELOG.txt" in gate, "the gate check does not expect CHANGELOG.txt in the release")

    # The README tells where the list of changes is.
    check("CHANGELOG.md" in read("README.md"), "README.md does not link to CHANGELOG.md")

    print("version: %s; entries in the changelog: %d (%s)" % (version, len(headings), ", ".join(h[0] for h in headings)))
    for p in problems:
        print("PROBLEM:", p)
    print("result:", "OK" if not problems else "FAILED")
    return 0 if not problems else 1


if __name__ == "__main__":
    sys.exit(main())
