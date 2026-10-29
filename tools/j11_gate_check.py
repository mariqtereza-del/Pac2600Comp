"""Stage J11: the checklist of the final gate, "go public" (DECISIONS.md T-J11-1).

    python -I tools\\j11_gate_check.py rom\\pacman.a26 [--write]

It reads the project and prints one line per item of the checklist with a status:
    OK        in order, and proved here (or decided by the owner)
    TODO      Claude Code does it at the end
    OWNER     only the owner can do it (a click on github.com, a push, or something only the owner can see)
    PUBLIC    already public, so it cannot be taken back; the line says what it is
    FAIL      a real problem: it must be fixed (the exit code is then 1)
It never prints an e-mail address. With --write it writes the same table to docs/GATE_J.md (a record).
It reads files and Git only; the only file it can write is docs/GATE_J.md. Standard library only."""
import hashlib
import importlib.util
import os
import re
import subprocess
import sys
import zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
rows = []
# what goes into the release zip and its source zip: a change to any of these after the zip was made means the zip must be made again
RELEASE_PREFIXES = ("src/", "tests/", "third_party/")
RELEASE_FILES = {"LICENSE", "THIRD_PARTY_NOTICES.md", "VERSION", "CHANGELOG.md", "tools/make_release.ps1", "tools/build_host.cmd", "tools/build_third_party.cmd"}


def add(status, item, detail=""):
    rows.append((status, item, detail))


def git(*args):
    return subprocess.run(["git", "-C", ROOT] + list(args), capture_output=True, text=True).stdout


def load(name):
    spec = importlib.util.spec_from_file_location(name, os.path.join(ROOT, "tools", name + ".py"))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def run_tool(script, *args):
    out = subprocess.run([sys.executable, "-I", os.path.join(ROOT, "tools", script)] + list(args), cwd=ROOT, capture_output=True, text=True).stdout
    lines = [l for l in out.splitlines() if l.startswith("result:")]
    return (lines[-1] if lines else "no result line"), out


def main():
    write = "--write" in sys.argv
    rom = next((a for a in sys.argv[1:] if not a.startswith("--")), None)
    if not rom or not os.path.exists(os.path.join(ROOT, rom)):
        print("usage: python -I tools\\j11_gate_check.py rom\\pacman.a26 [--write]")
        return 2
    version = open(os.path.join(ROOT, "VERSION"), encoding="utf-8").read().strip()

    # ---- the state of the repository ----
    dirty = git("status", "--porcelain").strip()
    add("OK" if not dirty else "FAIL", "the working tree is clean", "nothing waiting to be committed" if not dirty else "there are uncommitted changes")
    branch = git("branch", "--show-current").strip()
    add("OK" if branch == "main" else "FAIL", "the branch is main", branch)
    counts = git("rev-list", "--left-right", "--count", "HEAD...origin/main").split()
    ahead = int(counts[0]) if len(counts) == 2 else -1
    add("OK" if ahead == 0 else "OWNER", "everything is pushed to GitHub", "%d commits are only on this PC (a push is the owner's)" % ahead if ahead else "GitHub has every commit")

    # ---- the decisions and the files that were promised ----
    decisions = open(os.path.join(ROOT, "DECISIONS.md"), encoding="utf-8").read()
    add("OK" if "### D-027" in decisions else "FAIL", "J1: the licence decision is recorded", "D-027: GNU GPL version 2 or later")
    add("OK" if all(os.path.exists(os.path.join(ROOT, f)) for f in ("LICENSE", "THIRD_PARTY_NOTICES.md", "CONTRIBUTING.md", "README.md", "CHANGELOG.md")) else "FAIL",
        "the licence, the notices, the README, the changelog and the contribution rules exist", "LICENSE, THIRD_PARTY_NOTICES.md, README.md, CHANGELOG.md, CONTRIBUTING.md")

    # ---- the checks of the earlier stages, run again now ----
    for label, script, args in (("no game file in any commit (name and content of every version)", "j6_prepush_check.py", [rom]),
                                ("every tracked file has a category (J3)", "j3_provenance.py", ["--check"]),
                                ("the public documents are true and clean (J4, J10)", "j4_docs_check.py", []),
                                ("the licence and the notices match the files (J5)", "j5_notices_check.py", []),
                                ("the version and the changelog agree (J8)", "j8_version_check.py", []),
                                ("the conventions (file sizes, line lengths)", "g2_conventions_check.py", ["--strict"]),
                                ("Gate G: no cartridge code in Git or the program", "g7_gate_check.py", [rom]),
                                ("the test list is complete", "h1_testing_check.py", []),
                                ("the frozen preservation files are unchanged", "h7_preservation_check.py", [])):
        result, out = run_tool(script, *args)
        detail = result
        if script == "j6_prepush_check.py":
            m = re.search(r"commits that would be pushed: (\d+)", out)
            detail = "%s (%s commits scanned)" % (result, m.group(1) if m else "?")
        add("OK" if re.search(r"result: (OK|CLEAN)", result) else "FAIL", label, detail)

    # ---- no cartridge bytes or code as text, and no address of the owner as text, in the files that are public now ----
    j3 = load("j3_provenance")
    emails = {e for e in git("log", "--all", "--format=%ae").split() if e}
    private_emails = {e for e in emails if not e.lower().endswith("@users.noreply.github.com")}   # the no-reply form is meant to be shown
    byte_lines, address_files, private = 0, 0, 0
    for f in j3.tracked_files():
        if f.startswith(j3.THIRD_PARTY_PREFIXES) or f == "docs/PROVENANCE.md":
            continue
        try:
            body = open(os.path.join(ROOT, f), "rb").read()
        except OSError:
            continue
        if b"\0" in body[:8192]:
            continue
        text = body.decode("latin-1")
        if any(e.lower() in text.lower() for e in private_emails):
            address_files += 1
        if "mariqtereza" in text or "F:\\Ports" in text:
            private += 1
        if f.startswith("src/host/gdv_"):
            continue
        byte_lines += sum(1 for line in text.splitlines() if j3.bytes_as_text(line))
    add("OK" if byte_lines == 0 else "FAIL", "no bytes or instructions of the cartridge quoted as text in a file", "%d line(s) found (Rule 7: the cartridge's program code is never in the repository)" % byte_lines)
    add("OK" if address_files == 0 else "FAIL", "the owner's e-mail address is not written as text in any file", "%d file(s) contain it" % address_files)
    add("PUBLIC", "the owner's e-mail address is in the commits", "%d commits carry %d author address(es), %d of them a private one; they are public with the history. New commits use the GitHub no-reply form" % (int(git("rev-list", "--all", "--count").strip()), len(emails), len(private_emails)))
    add("PUBLIC", "the notes name the owner's account and folders on this PC, and discuss the legal position", "%d tracked files name the account or a folder; the legal briefing and D-027, D-028 are in the history" % private)
    add("PUBLIC", "the notes name addresses inside the cartridge's program (J3)", "they are in the public history; no bytes or code of the cartridge are in the current files")
    j5 = load("j5_notices_check")
    table_rows, _ = j5.table()
    unstated = sum(1 for r in table_rows[2:] if r.rstrip().endswith("| not stated |"))
    add("OK", "shader files that state no licence", "%d of %d: the owner chose to publish with the 'not stated' notice" % (unstated, len(table_rows) - 2))
    add("OK", "how to publish", "the owner made this repository public as it is (reported by the owner, 2026-10-29)")

    # ---- the release ----
    zip_path = os.path.join(ROOT, "out", "Pac2600Comp-%s-windows.zip" % version)
    head = git("rev-parse", "HEAD").strip()
    if not os.path.exists(zip_path):
        add("TODO", "the release zip exists", "out\\Pac2600Comp-%s-windows.zip is not there: make it from the final commit" % version)
    else:
        sha = hashlib.sha256(open(zip_path, "rb").read()).hexdigest()
        side = open(zip_path + ".sha256", encoding="utf-8").read().split() if os.path.exists(zip_path + ".sha256") else []
        add("OK" if side and side[0] == sha else "FAIL", "the release zip matches its .sha256 file", "SHA-256 %s..." % sha[:16])
        with zipfile.ZipFile(zip_path) as z:
            info = [n for n in z.namelist() if n.endswith("/BUILD-INFO.txt")]
            text = z.read(info[0]).decode("utf-8", "replace") if info else ""
        built = re.search(r"Git commit: (\w+)", text)
        built = built.group(1) if built else ""
        changed = [n for n in git("diff", "--name-only", built, head).split() if n.startswith(RELEASE_PREFIXES) or n in RELEASE_FILES] if built else ["?"]
        add("OK" if not changed else "TODO", "the release holds the newest files", "made from %s; nothing that goes into the zip changed since" % built[:8] if not changed else "made from %s; %d file(s) that go into the zip changed since (first: %s): make it again" % (built[:8], len(changed), changed[0]))

    # ---- the release page and GitHub ----
    tags = git("tag", "-l").split()
    typo = "v." + version
    if "v%s" % version in tags:
        add("OK", "the tag v%s" % version, "it exists")
    elif typo in tags:
        at = git("rev-list", "-n1", typo).strip()[:8]
        add("OWNER", "the tag v%s" % version, "the release page made the tag %s (a stray dot) at %s; the release and its download links carry that name; harmless, and fixing it takes the owner's clicks (T-J11-1)" % (typo, at))
    else:
        add("OWNER", "the tag v%s" % version, "tags now: %s; the owner creates it on the release page (Create new tag on publish)" % ", ".join(tags))
    log = open(os.path.join(ROOT, "CHANGELOG.md"), encoding="utf-8").read()
    add("OWNER" if ("## [%s] - unreleased" % version) in log else "OK", "the date of %s in the changelog" % version, "it says unreleased: the date of publication is written in when the release is published")
    add("OK", "the automatic checks on GitHub (J7)", "the first run was green in 5 min 2 s, as the owner reported")
    add("OWNER", "GitHub settings that make \"merged only with the owner's approval\" true", "no collaborators; approval for outside workflow runs; a rule for main (T-J10-1)")
    add("OK", "the release page with the zip and its .sha256 file", "published by the owner, who reported that the download works (not seen from here)")
    add("OK", "the public page looks right to a guest", "the owner looked at it signed out and said it looks great (not seen from here)")
    add("OWNER", "the bug-report form shows under New issue", "not reported yet: GitHub ignores an invalid form silently, so it has to be looked at (J10)")
    add("OWNER", "the three milestone tags are on GitHub", "0.5.0-native, 1.0.0-preservation (the permanent baseline) and v0.1.0 were made on this PC; check the Tags page of the repository; this PC cannot tell")

    width = max(len(r[1]) for r in rows)
    lines = ["%-7s %-*s  %s" % (s, width, i, d) for s, i, d in rows]
    counts = {}
    for s, _, _ in rows:
        counts[s] = counts.get(s, 0) + 1
    summary = "OK %d, TODO %d, OWNER %d, PUBLIC %d, FAIL %d" % tuple(counts.get(k, 0) for k in ("OK", "TODO", "OWNER", "PUBLIC", "FAIL"))
    print("\n".join(lines))
    print(summary)
    if write:
        md = ["# Gate J: the checklist before and after going public (Stage J11)", "",
              "Made by `python -I tools\\j11_gate_check.py rom\\pacman.a26 --write`; do not edit by hand. Statuses: OK (in order, proved or decided by the owner), TODO (Claude Code does it), OWNER (only the owner can do it), PUBLIC (already public, cannot be taken back), FAIL (a real problem).", "",
              "| Status | Item | Detail |", "| --- | --- | --- |"]
        md += ["| %s | %s | %s |" % (s, i.replace("|", "/"), re.sub(r"\d+ commits are only", "some commits are only", d).replace("|", "/")) for s, i, d in rows]
        md += ["", summary, ""]
        with open(os.path.join(ROOT, "docs", "GATE_J.md"), "w", encoding="utf-8", newline="") as f:
            f.write("\n".join(md))
        print("wrote docs\\GATE_J.md")
    print("result:", "OK" if not counts.get("FAIL") else "FAILED")
    return 0 if not counts.get("FAIL") else 1


if __name__ == "__main__":
    sys.exit(main())
