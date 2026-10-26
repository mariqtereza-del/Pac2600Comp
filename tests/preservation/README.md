# The preservation suite (frozen at Gate H, Stage H7)

These files define "the game still does what the original Atari 2600 Pac-Man does". The proofs and tests compare the program against them:

- `baseline\BL-nnn.entry.txt` and `baseline\BL-nnn.inputs`: the 70 recorded behaviours of the original and the button presses that reproduce them.
- `tests\game\data\BL-nnn.fingerprints.txt`: the expected picture and sound fingerprints of eleven of those games (they let the public tests run without a ROM).

`MANIFEST.txt` lists every one of them with its SHA-256 (computed with LF line endings, so a Windows checkout gives the same value) and size.

## The rule

**Frozen files are never edited, renamed or deleted. New ones may be added.**

- To add a new recording: put it in `baseline\` (or a fingerprint file in `tests\game\data\`), run `python -I tools\h7_preservation_check.py`, read the "NOT LISTED" report, then `python -I tools\h7_preservation_check.py --add`. `--add` only appends lines.
- If a frozen file is wrong, do not fix it: add a new entry that says what was wrong, and record the reason in `DECISIONS.md`.
- A change that makes a frozen file differ is a change to what the game does. The permanent baseline is the Git tag `1.0.0-preservation`; no modern feature may change what the game does at that tag (new features are separate code, off by default).

## What checks it

`tools\h7_preservation_check.py` (part of `tools\test_quick.ps1` and `tools\test_public.ps1`): every listed file is present and unchanged; the manifest only grows compared with the last commit; every frozen file that exists at the tag is identical to the tag's version; new unlisted files are reported.

## What it does not do

It is a check, not a lock. Someone who edits a frozen file and the manifest together, and commits both, passes check 1; check 3 (against the tag) still catches it for every file the tag has, and the 9 files added after the tag are covered by the manifest and by Git history. Where Git or the tag is missing (a zip download) checks 2 and 3 are skipped and say so.
