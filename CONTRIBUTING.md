# Contributing to Pac2600Comp

Thank you for wanting to help. Proposals are welcome, as **pull requests**. **Nothing is merged without the owner's review and approval**: the owner reads every change, and may ask for changes or decline a proposal.

The aim of the project is to play the Atari 2600 *Pac-Man* **exactly as the original does**, and to keep that provable. Anything extra must be optional and must not change the game.

## The rules

A proposal that breaks one of these will not be merged.

1. **Never include the game file.** No copy of the cartridge file, no part of its program code, no dump or listing of its bytes, in files, tests, notes, screenshots or issues. The public tests need none. A request for the game file will be closed.
2. **Never change what the game does by default.** The rebuilt game (`src/game`) is proved identical to the original. The 70 recorded games and their fingerprints (the files in `baseline/` and `tests/game/data/`, fixed by `tests/preservation/MANIFEST.txt`) are **never edited, renamed or deleted**; a new one may be added, with the reason.
3. **Extras are separate, optional and off by default**, and come with a test that shows the game is the same with them on and off.
4. **No new library without a reason**, written in the proposal. Anything taken from someone else keeps its licence, and is added to `THIRD_PARTY_NOTICES.md`.
5. **The tests must pass.** Run `powershell -NoProfile -ExecutionPolicy Bypass -File tools\test_public.ps1 -Build` (about 3 minutes, no game file needed) and it must end with `result: OK`. GitHub runs the same tests on your proposal; for a first contribution the owner may have to approve that run.
6. **The licence.** The project is under the GNU General Public License, version 2 or later (see [LICENSE](LICENSE)). By sending a proposal you agree that your change is under it, and that you wrote it or have the right to share it.
7. **Texts the program shows stay as they are**, unless changing them is the point of your proposal.

## How to propose a change

1. For a small repair, a better document or a test, just send a pull request. For anything bigger, open an issue first and describe what and why, so that nobody's time is wasted.
2. Make your own copy (a "fork") on GitHub, make a branch, and change **one thing** per proposal.
3. Build it as [BUILDING.md](BUILDING.md) says, and run the public tests (rule 5).
4. If it changes something people notice, add a line to the newest entry of [CHANGELOG.md](CHANGELOG.md).
5. Open the pull request; the template asks a few questions. Say what you changed and why.

Please be patient: this is a small project and a review takes the time it takes.

## Reporting a problem

Use the **bug report** form when you open an issue. It asks for:

- the **version** (the name of the zip you downloaded, the first line of `BUILD-INFO.txt`, or the first line of `README.txt`),
- your **Windows version** (press the Windows key, type `winver`, press Enter),
- the **fingerprint of your game file**: only the number, never the file. In a Command Prompt: `certutil -hashfile "Pac-Man (1981) (Atari).a26" SHA1`. The right one is `0940fea7f04cdb6d4b90c5ad1a7e344e68f6dbb1`,
- what happened, what you expected, and how to make it happen again.

A **recording** helps a lot: double-click `Record a game.cmd`, play until the problem shows, close the game from its menu (Esc, then Quit), and attach the file from the `recordings` folder. It holds only the buttons you pressed and the fingerprint of your game file, nothing of the game itself.

`settings.ini` holds the path of your game file on your PC. If you want to attach it, look at it first and remove anything you do not want to share.

## Be kind

Treat everyone with respect, in issues and in reviews. Harassment is not accepted, and the owner may remove such comments and block the person.
