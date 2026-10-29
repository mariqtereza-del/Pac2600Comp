## What does this change, and why?

(One thing per proposal, please.)

## Checklist

Nothing is merged without the owner's review and approval. Please read CONTRIBUTING.md in the repository first.

- [ ] It contains **no game file**, no part of the game's code and no dump of its bytes.
- [ ] It does **not change what the game does by default**, and no file in `baseline/`, `tests/game/data/` or `tests/preservation/` was edited, renamed or deleted.
- [ ] Anything new is **optional and off by default**, with a test that shows the game is the same with it on and off.
- [ ] I ran `tools\test_public.ps1 -Build` and it said `result: OK`.
- [ ] I added a line to `CHANGELOG.md` if people will notice the change.
- [ ] My change is under the project's licence (GNU GPL version 2 or later), and I wrote it or have the right to share it.
