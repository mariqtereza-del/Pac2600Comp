# Gate J: the checklist before and after going public (Stage J11)

Made by `python -I tools\j11_gate_check.py rom\pacman.a26 --write`; do not edit by hand. Statuses: OK (in order, proved or decided by the owner), TODO (Claude Code does it), OWNER (only the owner can do it), PUBLIC (already public, cannot be taken back), FAIL (a real problem).

| Status | Item | Detail |
| --- | --- | --- |
| OK | the working tree is clean | nothing waiting to be committed |
| OK | the branch is main | main |
| OWNER | everything is pushed to GitHub | some commits are only on this PC (a push is the owner's) |
| OK | J1: the licence decision is recorded | D-027: GNU GPL version 2 or later |
| OK | the licence, the notices, the README, the changelog and the contribution rules exist | LICENSE, THIRD_PARTY_NOTICES.md, README.md, CHANGELOG.md, CONTRIBUTING.md |
| OK | no game file in any commit (name and content of every version) | result: OK (185 commits scanned) |
| OK | every tracked file has a category (J3) | result: OK |
| OK | the public documents are true and clean (J4, J10) | result: OK |
| OK | the licence and the notices match the files (J5) | result: OK |
| OK | the version and the changelog agree (J8) | result: OK |
| OK | the conventions (file sizes, line lengths) | result: CLEAN |
| OK | Gate G: no cartridge code in Git or the program | result: OK (9 of 9 checks) |
| OK | the test list is complete | result: OK |
| OK | the frozen preservation files are unchanged | result: OK |
| OK | no bytes or instructions of the cartridge quoted as text in a file | 0 line(s) found (Rule 7: the cartridge's program code is never in the repository) |
| OK | the owner's e-mail address is not written as text in any file | 0 file(s) contain it |
| PUBLIC | the owner's e-mail address is in the commits | 185 commits carry 2 author address(es), 1 of them a private one; they are public with the history. New commits use the GitHub no-reply form |
| PUBLIC | the notes name the owner's account and folders on this PC, and discuss the legal position | 19 tracked files name the account or a folder; the legal briefing and D-027, D-028 are in the history |
| PUBLIC | the notes name addresses inside the cartridge's program (J3) | they are in the public history; no bytes or code of the cartridge are in the current files |
| OK | shader files that state no licence | 11 of 26: the owner chose to publish with the 'not stated' notice |
| OK | how to publish | the owner made this repository public as it is (reported by the owner, 2026-10-29) |
| OK | the release zip matches its .sha256 file | SHA-256 d7c8a99872643f05... |
| OK | the release holds the newest files | made from da6bf82a; nothing that goes into the zip changed since |
| OWNER | the tag v1.0.0 | tags now: 0.5.0-native, 1.0.0-preservation, v0.1.0; the owner creates it on the release page (Create new tag on publish), so no tag is pushed from here |
| OWNER | the date of 1.0.0 in the changelog | it says unreleased: the date of publication is written in when the release is published |
| OK | the automatic checks on GitHub (J7) | the first run was green in 5 min 2 s, as the owner reported |
| OWNER | GitHub settings that make "merged only with the owner's approval" true | no collaborators; approval for outside workflow runs; a rule for main (T-J10-1) |
| OWNER | the release page with the zip and its .sha256 file | Releases > Draft a new release (clicks given in the session) |
| OWNER | look at the public page without signing in | a private browser window: the README, the pictures, the issue form (New issue) |

OK 20, TODO 0, OWNER 6, PUBLIC 3, FAIL 0
