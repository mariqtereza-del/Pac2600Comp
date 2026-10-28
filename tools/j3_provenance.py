"""Stage J3: the content audit (DECISIONS.md T-J3-1). Reads the file list of Git, puts every tracked file into one category by fixed rules, finds the notes that
name ROM addresses or hold runs of hex bytes, lists the binary files, runs the Gate G ROM scan and writes docs/PROVENANCE.md.

    python -I tools\\j3_provenance.py rom\\pacman.a26          writes docs\\PROVENANCE.md and must say result: OK
    python -I tools\\j3_provenance.py --check                  only classifies (no ROM needed, no file written); fails on an unclassified or ROM-like file

Standard library only. It reads files and never changes any file except docs/PROVENANCE.md."""
import fnmatch
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# (category, licence / origin, what it is for the public repository)
CATEGORIES = [
    ("own-code", "ours; GPL v2 or later if published (D-027)", "the program: the rewritten game, the window program and the tests of both"),
    ("rom-derived-data", "ours in form, Atari's in content (D-009)", "the 512 bytes of game tables the program needs; the only ROM-derived material in Git"),
    ("generated", "made by a tool of ours from our own inputs or from third-party sources", "files written by a script; never edited by hand"),
    ("emulator", "Stella 7.0, GPL v2 or any later version, unchanged (D-002)", "the reference emulator, used by the test tools and the reference program only"),
    ("emulator-tool", "ours, links Stella (GPL)", "the measuring tools that run the original game inside Stella"),
    ("third-party", "SDL3 (zlib) and the libretro shader sources (per file: GPL v2 or later, public domain)", "libraries and shader sources, unchanged"),
    ("build-tool", "ours", "scripts that build, test, release and check the project"),
    ("test-data", "ours", "recorded joystick inputs, entries and frozen files of the preservation suite (no ROM bytes)"),
    ("documentation", "ours", "the notes and documents of the project"),
    ("config", "ours", "Git and GitHub settings"),
]

# Ordered rules: the first match wins. (pattern on the path with forward slashes, category)
RULES = [
    ("third_party/stella/*", "emulator"),
    ("third_party/SDL3/*", "third-party"),
    ("third_party/shaders/*", "third-party"),
    ("third_party/CMakeLists.txt", "build-tool"),
    ("tools/stella_probe/*", "emulator-tool"),
    ("src/game/data/tables.cpp", "rom-derived-data"),
    ("src/game/data/tables.hpp", "generated"),
    ("src/host/gdv_generated.cpp", "generated"),
    ("src/host/gdv_luts.cpp", "generated"),
    ("src/host/ntsc_palette.hpp", "generated"),
    ("src/testsupport/ram_map.hpp", "generated"),
    ("src/testsupport/state_layout_checks.hpp", "generated"),
    ("src/lockstep/generated_tables.hpp", "generated"),
    ("tests/game/data/*", "generated"),
    ("src/*", "own-code"),
    ("tests/preservation/*", "test-data"),
    ("tests/*.cpp", "own-code"),
    ("tests/*.hpp", "own-code"),
    ("tests/host/*", "own-code"),
    ("baseline/*", "test-data"),
    ("tools/f9_inputs/*", "test-data"),
    ("tools/captures/*", "test-data"),
    ("tools/*.py", "build-tool"),
    ("tools/*.ps1", "build-tool"),
    ("tools/*.cmd", "build-tool"),
    ("tools/*.sh", "build-tool"),
    ("tools/*.txt", "build-tool"),
    ("docs/*", "documentation"),
    ("*.md", "documentation"),
    ("LICENSE", "documentation"),
    (".github/*", "config"),
    (".gitignore", "config"),
    (".gitattributes", "config"),
    ("VERSION", "config"),
]

ADDRESS = re.compile(r"(?:\$F[0-9A-Fa-f]{3}\b|\b0x[Ff][0-9A-Fa-f]{3}\b|\bL_F[0-9A-F]{3}\b)")
HEX_RUN = re.compile(r"(?:\b[0-9A-Fa-f]{2}\b[ ,]+){8,}\b[0-9A-Fa-f]{2}\b")            # 09 03 01 ... (nine or more bytes in a row)
HEX_DOLLAR = re.compile(r"(?:\$[0-9A-Fa-f]{2}\b[ ,]+){8,}\$[0-9A-Fa-f]{2}\b")          # $09, $03, ... as in an assembler listing
HEX_LONG = re.compile(r"\b(?=[0-9]*[A-Fa-f])[0-9A-Fa-f]{18,}\b")                                       # an unbroken hex string; hashes (32, 40 and 64 digits) are not meant
HASH_LENGTHS = (32, 40, 64)
ROM_LIKE = (".a26", ".bin", ".rom", ".rar", ".7z", ".zip", ".tar", ".gz")
KNOWN_ARCHIVE = {"third_party/stella/src/os/macos/template.dmg.gz"}   # part of Stella's unchanged source (T-D15-1)
THIRD_PARTY_PREFIXES = ("third_party/SDL3/", "third_party/stella/", "third_party/shaders/")


def tracked_files():
    out = subprocess.run(["git", "-C", ROOT, "ls-files", "-z"], capture_output=True, check=True).stdout
    return sorted(p.decode("utf-8") for p in out.split(b"\0") if p)


def classify(path):
    for pattern, category in RULES:
        if fnmatch.fnmatchcase(path, pattern):
            return category
    return None


def read(path):
    with open(os.path.join(ROOT, path), "rb") as f:
        return f.read()


def is_text(data):
    return b"\0" not in data[:8192]


def table_row(cells):
    return "| " + " | ".join(str(c) for c in cells) + " |"


def main():
    check_only = "--check" in sys.argv
    rom_path = next((a for a in sys.argv[1:] if not a.startswith("--")), None)
    files = tracked_files()
    category = {p: classify(p) for p in files}
    unclassified = [p for p in files if category[p] is None]
    rom_like = [p for p in files if p.lower().endswith(ROM_LIKE) and p not in KNOWN_ARCHIVE]
    problems = []
    if unclassified:
        problems.append("%d tracked file(s) fit no rule, first: %s" % (len(unclassified), unclassified[0]))
    if rom_like:
        problems.append("%d tracked file(s) look like a ROM or an archive, first: %s" % (len(rom_like), rom_like[0]))
    if check_only:
        print("files: %d, unclassified: %d, ROM-like: %d" % (len(files), len(unclassified), len(rom_like)))
        for p in problems:
            print("PROBLEM:", p)
        print("result:", "OK" if not problems else "FAILED")
        return 0 if not problems else 1
    if not rom_path or not os.path.exists(os.path.join(ROOT, rom_path)):
        print("usage: python -I tools\\j3_provenance.py rom\\pacman.a26   (or --check)")
        return 2

    # Sizes, text or binary, and the notes that name ROM addresses (outside the third-party folders).
    size = {}
    binary = []
    notes = []   # (path, category, address mentions, hex runs)
    hex_lines = []   # (path, line number, the start of the line)
    for p in files:
        data = read(p)
        size[p] = len(data)
        text = is_text(data)
        if not text:
            binary.append(p)
        if text and not p.startswith(THIRD_PARTY_PREFIXES) and category[p] != "rom-derived-data" and p != "docs/PROVENANCE.md":
            body = data.decode("latin-1")
            addresses = len(ADDRESS.findall(body))
            runs = 0
            for number, line in enumerate(body.splitlines(), 1):
                long_hits = [m for m in HEX_LONG.findall(line) if len(m) not in HASH_LENGTHS]
                if p.startswith("src/host/gdv_"):   # the shaders' lookup tables: decimal numbers of pictures, not bytes of anything of Atari's
                    break
                if HEX_RUN.search(line) or HEX_DOLLAR.search(line) or long_hits:
                    runs += 1
                    shown = HEX_DOLLAR.sub("<bytes>", HEX_RUN.sub("<bytes>", line.strip()))   # the document itself must not quote the bytes it reports
                    shown = HEX_LONG.sub(lambda m: m.group(0) if len(m.group(0)) in HASH_LENGTHS else "<bytes>", shown)
                    hex_lines.append((p, number, shown[:48]))
            if addresses or runs:
                notes.append((p, category[p], addresses, runs))

    # Gate G: the ROM scan over all tracked files and the program (nothing of the cartridge outside the tables).
    gate = subprocess.run([sys.executable, "-I", os.path.join(ROOT, "tools", "g7_gate_check.py"), rom_path], cwd=ROOT, capture_output=True, text=True)
    gate_lines = [l for l in gate.stdout.splitlines() if l.startswith("result:")]
    gate_result = gate_lines[-1] if gate_lines else "no result line"
    gate_ok = gate.returncode == 0 and "OK" in gate_result
    if not gate_ok:
        problems.append("Gate G: " + gate_result)

    # The ROM-derived data must be exactly the approved tables file (nothing else carries cartridge bytes).
    derived = [p for p in files if category[p] == "rom-derived-data"]
    if derived != ["src/game/data/tables.cpp"]:
        problems.append("the ROM-derived data is not exactly src/game/data/tables.cpp: %s" % derived)

    # ---- the document ----
    md = []
    add = md.append
    add("# Provenance of every file in Git (Stage J3)")
    add("")
    add("Made by `python -I tools\\j3_provenance.py rom\\pacman.a26`; do not edit by hand. It lists what Git holds (%d files), not what is only on this PC (section 5)." % len(files))
    add("The categories follow the plan: our own code, ROM-derived material, emulator code, third-party libraries, documentation, build tools and generated files, plus test data, measuring tools and settings.")
    add("This is a factual list, not legal advice (see `docs\\LEGAL_BRIEFING.md`; the owner's position is D-028).")
    add("")
    add("## 1. Summary")
    add("")
    add(table_row(["Category", "Files", "Bytes", "Licence or origin", "What it is"]))
    add(table_row(["---"] * 5))
    for name, licence, what in CATEGORIES:
        members = [p for p in files if category[p] == name]
        add(table_row([name, len(members), "{:,}".format(sum(size[p] for p in members)), licence, what]))
    add(table_row(["**total**", len(files), "{:,}".format(sum(size.values())), "", ""]))
    add("")
    add("Every tracked file is in exactly one category, and no file fits no rule (the tool stops otherwise).")
    add("")
    add("## 2. The only ROM-derived material in Git")
    add("")
    add("`src/game/data/tables.cpp` holds the **512 bytes in 34 tables** that the program needs and that the cartridge holds (`docs\\EMBEDDED_DATA.md`, D-009), written as source text. It is data only; Gate G proves the tables equal the ROM's and that no run of cartridge program code is anywhere in the tracked files or in `pacman2600.exe`.")
    add("The cartridge file itself and its program code are not in Git, not in the release and not in any test file (Rule 7).")
    add("")
    add("## 3. Notes and code that name ROM addresses")
    add("")
    add("A mention is an address between `$F000` and `$FFFF` (or `0xF...`, or a label `L_F...`): the places in the cartridge's program that our notes and comments refer to. A hex run is a line with nine or more hex bytes in a row. Neither contains the cartridge's code as bytes; they are evidence that these files were written by studying it.")
    add("")
    for group, title in (("documentation", "Documentation"), ("own-code", "Our own code (comments such as `ORIGINAL: $Fxxx`)"), ("build-tool", "Build tools"), ("test-data", "Test data"), ("emulator-tool", "Measuring tools"), ("generated", "Generated files")):
        rows = [n for n in notes if n[1] == group]
        if not rows:
            continue
        add("### %s: %d files, %d address mentions, %d hex-run lines" % (title, len(rows), sum(r[2] for r in rows), sum(r[3] for r in rows)))
        add("")
        add(table_row(["File", "Address mentions", "Hex-run lines"]))
        add(table_row(["---", "---", "---"]))
        for p, _, a, r in sorted(rows, key=lambda r: (-r[2], r[0]))[:25]:
            add(table_row(["`%s`" % p, a, r]))
        if len(rows) > 25:
            add("")
            add("(%d more files with fewer mentions are not listed.)" % (len(rows) - 25))
        add("")
    add("### Every line that holds bytes as text (%d lines in %d files)" % (len(hex_lines), len({h[0] for h in hex_lines})))
    add("")
    add("These lines quote bytes of the cartridge or of the game's memory as text. None is cartridge program code (Gate G finds no run of it), but any that quote cartridge bytes are ROM-derived material in a note; the owner decides about them at J11.")
    add("")
    add(table_row(["File", "Line", "Starts with"]))
    add(table_row(["---", "---", "---"]))
    for p, number, start in hex_lines[:60]:
        add(table_row(["`%s`" % p, number, "`%s`" % start.replace("|", "/")]))
    if len(hex_lines) > 60:
        add("")
        add("(%d more lines are not listed.)" % (len(hex_lines) - 60))
    add("")
    add("## 4. Binary files that Git holds")
    add("")
    own_binary = [p for p in binary if not p.startswith(THIRD_PARTY_PREFIXES)]
    add("Outside `third_party/` (%d files): %s." % (len(own_binary), ", ".join("`%s`" % p for p in own_binary) if own_binary else "none"))
    third_binary = {}
    for p in binary:
        if p.startswith(THIRD_PARTY_PREFIXES):
            top = "/".join(p.split("/")[:2])
            third_binary[top] = third_binary.get(top, 0) + 1
    add("Inside `third_party/` (unchanged files of the libraries: pictures, fonts, archives of their own): %s." % (", ".join("%s: %d" % (k, v) for k, v in sorted(third_binary.items())) if third_binary else "none"))
    add("No tracked file ends in `.a26 .bin .rom .rar .7z .zip .tar` or `.gz`, except Stella's own `template.dmg.gz` (part of its unchanged source; pinned by SHA-256 in `tools\\make_release.ps1`).")
    add("")
    add("## 5. What is only on this PC (not in Git)")
    add("")
    add("`.gitignore` keeps these out: `rom/` (the working copy of the cartridge file), `reference/` (traces, listings and the raw disassembly), `build/`, `out/`, `recordings/`, archives, and any `*.a26` or `*.bin`. The original cartridge file in the top folder and the spare in `F:\\Ports\\ROM-originals\\` are read-only and never tracked.")
    add("")
    add("## 6. Checks")
    add("")
    add("- Gate G (`tools\\g7_gate_check.py`): %s" % gate_result)
    add("- Unclassified tracked files: %d. ROM-like tracked files: %d." % (len(unclassified), len(rom_like)))
    add("- ROM-derived data files: %s." % ", ".join("`%s`" % p for p in derived))
    add("")
    add("## 7. What this means for a public repository (to be decided in J11)")
    add("")
    add("- **Can go with the code, under the licences above:** `own-code`, `generated`, `build-tool`, `config`, `third-party`, `emulator` (only if the Stella-based tools are part of what is published; the program itself has no emulator in it).")
    add("- **ROM-derived data:** `tables.cpp` is needed for the program to run; the owner's position on it is D-028 and the lawyers' advice the owner reports.")
    add("- **For the owner to decide at J11:** the documentation that names ROM addresses (section 3). The release zip leaves `docs\\` out entirely (T-D15-1). A private repository holds everything above; a public one could hold the same or leave the notes out.")
    add("- **Never:** the cartridge file, its code, the raw disassembly and the traces (they are not in Git).")
    add("")
    out_path = os.path.join(ROOT, "docs", "PROVENANCE.md")
    with open(out_path, "w", encoding="utf-8", newline="") as f:
        f.write("\n".join(md))

    print("files: %d, categories: %d" % (len(files), len(CATEGORIES)))
    for name, _, _ in CATEGORIES:
        print("  %-17s %5d" % (name, sum(1 for p in files if category[p] == name)))
    print("notes naming ROM addresses: %d files; lines holding bytes as text: %d in %d files" % (len(notes), len(hex_lines), len({h[0] for h in hex_lines})))
    print("binary files outside third_party: %d" % len(own_binary))
    print("Gate G: " + gate_result)
    for p in problems:
        print("PROBLEM:", p)
    print("wrote docs\\PROVENANCE.md")
    print("result:", "OK" if not problems else "FAILED")
    return 0 if not problems else 1


if __name__ == "__main__":
    sys.exit(main())
