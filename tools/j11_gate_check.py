"""Stage J11: the checklist of the final gate, "go public" (DECISIONS.md T-J11-1).

    python -I tools\\j11_gate_check.py rom\\pacman.a26 [--write]

It reads the project and prints one line per item of the checklist with a status:
    OK        in order, and proved here
    TODO      Claude Code does it at the end (for example the release from the final commit)
    DECISION  the owner decides (the options are put to the owner in the session)
    OWNER     only the owner can do it (a click on github.com, a push, or something only the owner can see)
    FAIL      a real problem: nothing may be published until it is fixed
The exit code is 0 unless there is a FAIL. It never prints an e-mail address. With --write it writes the same table to docs/GATE_J.md (a record).
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
    add("OK" if ahead == 0 else "OWNER", "everything is pushed to the private repository", "%d commits are only on this PC (a push is the owner's)" % ahead if ahead else "the private repository has every commit")

    # ---- the decisions and the files that were promised ----
    decisions = open(os.path.join(ROOT, "DECISIONS.md"), encoding="utf-8").read()
    add("OK" if "### D-027" in decisions else "FAIL", "J1: the licence decision is recorded", "D-027: GNU GPL version 2 or later, private first")
    add("OK" if all(os.path.exists(os.path.join(ROOT, f)) for f in ("LICENSE", "THIRD_PARTY_NOTICES.md", "CONTRIBUTING.md", "README.md", "CHANGELOG.md")) else "FAIL",
        "the licence, the notices, the README, the changelog and the contribution rules exist", "LICENSE, THIRD_PARTY_NOTICES.md, README.md, CHANGELOG.md, CONTRIBUTING.md")

    # ---- the checks of the earlier stages, run again now ----
    for label, script, args in (("no game file in any commit that would be published (name and content of every version)", "j6_prepush_check.py", [rom]),
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
        add("OK" if built == head else "TODO", "the release was made from the final commit", "made from %s, the newest commit is %s: make it again after the last commit" % (built[:8], head[:8]) if built != head else "the zip is from the newest commit")

    # ---- what needs the owner (the facts, never an e-mail address) ----
    emails = {e for e in git("log", "--all", "--format=%ae").split() if e}
    add("DECISION", "the owner's e-mail address is in every commit", "%d commits carry %d author address(es); it is public once the history is public" % (int(git("rev-list", "--all", "--count").strip()), len(emails)))
    j3 = load("j3_provenance")
    files = j3.tracked_files()
    private = 0
    byte_lines = 0
    for f in files:
        if f.startswith(j3.THIRD_PARTY_PREFIXES) or f == "docs/PROVENANCE.md":
            continue
        try:
            body = open(os.path.join(ROOT, f), "rb").read()
        except OSError:
            continue
        if b"\0" in body[:8192]:
            continue
        text = body.decode("latin-1")
        if "mariqtereza" in text or "F:\\Ports" in text:
            private += 1
        if f.startswith("src/host/gdv_"):
            continue
        for line in text.splitlines():
            if j3.HEX_RUN.search(line) or j3.HEX_DOLLAR.search(line) or [m for m in j3.HEX_LONG.findall(line) if len(m) not in j3.HASH_LENGTHS]:
                byte_lines += 1
    add("DECISION", "our own notes name the owner's account or folders on this PC", "%d tracked files (the notes, the records); harmless while private" % private)
    add("DECISION", "bytes of the cartridge quoted as text in a note", "%d line(s) (docs\\ROM_IDENTITY.md: the last 23 bytes: table data and the reset vector); it is still in older commits" % byte_lines)
    j5 = load("j5_notices_check")
    table_rows, _ = j5.table()
    unstated = sum(1 for r in table_rows[2:] if r.rstrip().endswith("| not stated |"))
    add("DECISION", "shader files that state no licence", "%d of %d (the NTSC passes, the lookup pictures, the presets); the GDV NTSC 2600 filter uses them" % (unstated, len(table_rows) - 2))
    add("DECISION", "how to publish", "this repository as it is / a clean snapshot in a new public repository / the history rewritten first / not now")
    add("DECISION", "the internal notes (docs, DECISIONS.md, PLAN.md) name addresses of the cartridge's program (J3)", "they come with the repository unless the owner chooses a clean snapshot")

    # ---- the release page and GitHub ----
    tags = git("tag", "-l").split()
    add("TODO" if "v%s" % version not in tags else "OK", "the tag v%s" % version, "tags now: %s; v%s is made at the end, with the owner's approval; pushing it needs approval too" % (", ".join(tags), version))
    log = open(os.path.join(ROOT, "CHANGELOG.md"), encoding="utf-8").read()
    add("TODO" if ("## [%s] - unreleased" % version) in log else "OK", "the date of %s in the changelog" % version, "it says unreleased: the date of publication is written in at the end")
    add("OK", "the automatic checks on GitHub (J7)", "the first run was green in 5 min 2 s, as the owner reported")
    add("OWNER", "GitHub settings that make \"merged only with the owner's approval\" true", "no collaborators; approval for outside workflow runs; a rule for main (T-J10-1)")
    add("OWNER", "the repository's visibility (Private now) and the release page", "Settings > General > Danger Zone > Change visibility; Releases > Draft a new release")
    add("OWNER", "look at the public page without signing in", "a private browser window: the README, the pictures, the files, the Private badge gone")

    width = max(len(r[1]) for r in rows)
    lines = ["%-8s %-*s  %s" % (s, width, i, d) for s, i, d in rows]
    counts = {}
    for s, _, _ in rows:
        counts[s] = counts.get(s, 0) + 1
    summary = "OK %d, TODO %d, DECISION %d, OWNER %d, FAIL %d" % tuple(counts.get(k, 0) for k in ("OK", "TODO", "DECISION", "OWNER", "FAIL"))
    print("\n".join(lines))
    print(summary)
    if write:
        md = ["# Gate J: the checklist before going public (Stage J11)", "",
              "Made by `python -I tools\\j11_gate_check.py rom\\pacman.a26 --write`; do not edit by hand. Statuses: OK (in order, proved), TODO (Claude Code does it at the end), DECISION (the owner decides), OWNER (only the owner can do it), FAIL (a real problem).", "",
              "| Status | Item | Detail |", "| --- | --- | --- |"]
        md += ["| %s | %s | %s |" % (s, i.replace("|", "/"), d.replace("|", "/")) for s, i, d in rows]
        md += ["", summary, ""]
        with open(os.path.join(ROOT, "docs", "GATE_J.md"), "w", encoding="utf-8", newline="") as f:
            f.write("\n".join(md))
        print("wrote docs\\GATE_J.md")
    print("result:", "OK" if not counts.get("FAIL") else "FAILED")
    return 0 if not counts.get("FAIL") else 1


if __name__ == "__main__":
    sys.exit(main())
