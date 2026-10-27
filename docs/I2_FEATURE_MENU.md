# Feature menu (Stage I2, 2026-10-27)

This is a menu, not a plan. **Nothing here is decided, and nothing has been built.** You decide in I3. **"Add nothing" is a complete and fine answer:** the project already works as a preservation (see `docs\I1_STATUS_REPORT.md`), and Phase J can follow directly.

## How to read it

- **The eight answers** are the ones your brief asks for. The last one, *now / later / never*, is **my proposal only**.
- **Your own list is missing.** The list of candidate features in your original brief is not stored in the project files, so I could not read it. These candidates come from the project documents (flicker, "a new mode", saving, filters), from what the code already has, and from what I3 of the plan needs. **If your brief named something that is not here, tell me and I will add it to the menu.**
- **Already in the program, so not offered:** soft glow to soften the flicker (F7, D-007), five picture modes (keys 1-5), fullscreen (Alt+Enter), remembered window and switches, F1 key list, keyboard and PS4 pad, recording and replaying your play (`--record`, `--replay`), console switches (F4-F6), the console's own speed and sound pitch.
- **Effort words** (small, medium, large) are **my estimates**, not measurements.
- **The rules every feature must obey** (from the plan): the Original mode stays the default and is exactly the 1.0.0 baseline; a feature that changes play is a separate, clearly labelled option and is tested to change nothing when it is off; the frozen preservation suite (`tests\preservation`) must stay green; new libraries are explained first (Rule 5).

## The groups

| Group | What it means for the frozen preservation suite |
| --- | --- |
| **A. Around the game** | Touches only the window program. Cannot change what the game does. The suite stays green by construction. |
| **B. Reads the game's state** | Looks at, or saves, the game's memory but does not change a rule. Needs a check that nothing is missed. |
| **C. Changes how the game plays** | Needs one small, planned opening in the game code, default = original. Its "off" setting is proved equal to the original by the existing proofs; its "on" setting needs its own tests. |
| **D. Other** | Outside the game and the window. |

## Summary table

| # | Feature | Group | Effort (estimate) | New library | My proposal |
| --- | --- | --- | --- | --- | --- |
| A1 | Pause key | A | small | no | **now** |
| A2 | Volume control | A | small | no | **now** |
| A3 | Screenshot key | A | small | no | **now** |
| A4 | Scanline / CRT look | A | small to medium | no | later |
| A5 | Colour palette choices | A | small | no | later |
| A6 | Key and pad remapping | A | medium | no | later |
| A7 | Slow motion and fast forward | A | medium | no | later |
| A8 | Video capture | A | medium to large | probably yes | later |
| B1 | Save and load a position | B | medium | no | later |
| B2 | Rewind | B | medium (needs B1) | no | later |
| B3 | Best-score table | B | medium | no | later |
| B4 | Flicker-free view (picture filter) | B | medium, needs an experiment | no | later (see F7 first) |
| C1 | Starting lives and practice mode | C | small to medium | no | later |
| C2 | Extra life at a score | C | small | no | later |
| C3 | More difficulty settings | C | medium | no | never |
| C4 | New mazes or game modes | C | very large | no | never (for now) |
| D1 | Other operating systems | D | medium to large | no | later (Phase J era) |
| D2 | Online scores or network play | D | very large | yes, plus a server | never |

---

## A. Around the game

### A1. Pause key
1. **What it does:** a key (for example P) freezes the game and its sound until pressed again.
2. **Why it is useful:** the console has no pause; a PC player expects one.
3. **Changes game behaviour?** No. The game simply is not asked for its next frame while paused.
4. **Effect on fidelity:** none; the frames the game plays are the same ones in the same order. A recording made with pauses replays the same (pauses are not frames).
5. **Difficulty:** small. Touches `src\host` (the game thread and the sound feed) and the F1 key list.
6. **New dependencies:** none.
7. **Optional?** It does nothing unless pressed.
8. **Proposal:** now.

### A2. Volume control
1. **What it does:** keys to turn the sound down or up, and to mute; remembered in `settings.ini`.
2. **Why:** the game's volume is whatever the PC is set to; a quick control is convenient. The sound path already has a mute (used by tests).
3. **Changes game behaviour?** No.
4. **Fidelity:** none at the default volume; the samples are unchanged, only scaled on the way to the sound device.
5. **Difficulty:** small (`src\host\audio.*`).
6. **Dependencies:** none.
7. **Optional?** Default = full volume as now.
8. **Proposal:** now.

### A3. Screenshot key
1. **What it does:** a key saves the current picture as a PNG file in a `screenshots` folder next to the program.
2. **Why:** for sharing and for documentation; the program already knows how to write PNG files (the test mode `--headless` does it).
3. **Changes game behaviour?** No.
4. **Fidelity:** none (what is saved is the game picture, either as the game draws it or as shown, to be decided).
5. **Difficulty:** small.
6. **Dependencies:** none (SDL3 writes PNG).
7. **Optional?** Does nothing until pressed. It writes files, so it will tell you where.
8. **Proposal:** now.

### A4. Scanline / CRT look
1. **What it does:** an option that darkens every other row of the enlarged picture, perhaps with a soft glow, to look like a TV tube.
2. **Why:** some players like the look; it is also a matter of authenticity (a 1981 TV showed scanlines).
3. **Changes game behaviour?** No, picture only.
4. **Fidelity:** the game is unchanged; the look is a matter of taste. It would be **off by default** (authentic-first, D-004 concerns the console's behaviour; a TV effect is a different thing).
5. **Difficulty:** small to medium (drawing in `src\host\screen.*`; only looks right at larger window sizes).
6. **Dependencies:** none expected with SDL's drawing; a full "shader" look would need more and is not proposed.
7. **Optional?** Yes, a key, off by default.
8. **Proposal:** later.

### A5. Colour palette choices
1. **What it does:** choose between Stella's standard NTSC colours (now), other TV-like palettes, a black-and-white option, or a colour-blind-friendly set.
2. **Why:** there is no single authentic palette (TV knobs varied; `DISCREPANCIES.md`), and some players find ghost colours hard to tell apart.
3. **Changes game behaviour?** No, only which screen colour each of the game's colour numbers becomes.
4. **Fidelity:** the game's colour numbers are unchanged; the default stays as now.
5. **Difficulty:** small (the palette is one table, `ntsc_palette.hpp`).
6. **Dependencies:** none.
7. **Optional?** Yes.
8. **Proposal:** later.

### A6. Key and pad remapping
1. **What it does:** lets you choose which keys or pad buttons do what, stored in `settings.ini`.
2. **Why:** other keyboard layouts and other pads. The F1 list shows the current fixed map; the PS4 pad is the only pad tested.
3. **Changes game behaviour?** No.
4. **Fidelity:** none.
5. **Difficulty:** medium (the rules live in `controls.cpp` and are covered by 119 unit tests; remapping needs a screen or a text file to choose, and care so a bad file cannot lock you out).
6. **Dependencies:** none.
7. **Optional?** Default map stays as now.
8. **Proposal:** later.

### A7. Slow motion and fast forward
1. **What it does:** keys to run the game slower or faster than the console speed.
2. **Why:** to watch a tricky moment, or to skip a long wait.
3. **Changes game behaviour?** No, the same frames play, only the time between them changes.
4. **Fidelity:** none to the frames; **the sound would be wrong** at other speeds unless muted or re-pitched, so the proposal is: muted when not at normal speed.
5. **Difficulty:** medium (the pacing in `src\host\pacer.*` is steered by the sound card, which must be handled).
6. **Dependencies:** none.
7. **Optional?** Yes; normal speed on start.
8. **Proposal:** later.

### A8. Video capture
1. **What it does:** records your play to a video file.
2. **Why:** sharing clips.
3. **Changes game behaviour?** No.
4. **Fidelity:** none to the game.
5. **Difficulty:** medium to large.
6. **Dependencies:** **probably yes.** A usable video file needs an encoder (a library, with its own licence). Without one the program could only write a numbered series of PNG files and a WAV sound file (huge, and joined by another tool). Rule 5 means I would explain the choice before adding anything. Note: your existing recordings (`--record`) replay the game exactly and are tiny; a clip can already be made by replaying one under a screen recorder.
7. **Optional?** Yes.
8. **Proposal:** later (A3 screenshots first; you may find a screen recorder enough).

---

## B. Reads the game's state

### B1. Save and load a position
1. **What it does:** a key saves the exact moment of the game; another loads it again.
2. **Why:** practice a hard part; stop and come back.
3. **Changes game behaviour?** Not the rules, but it lets a player redo a moment, so it is a play aid and should be clearly labelled.
4. **Fidelity:** the game's main state is one 128-byte block plus a few small values, but whether the picture and sound models hold more that must be saved **has not been checked** (I1, section 6). The first task would be that check, then a test: save in the middle of a recorded game, load, and the rest of the game must be identical to playing straight through. If the test fails the feature does not ship.
5. **Difficulty:** medium.
6. **Dependencies:** none.
7. **Optional?** Yes; nothing is saved unless you press the key.
8. **Proposal:** later.

### B2. Rewind
1. **What it does:** hold a key to run the game backwards for a few seconds.
2. **Why:** undo a mistake.
3. **Changes game behaviour?** Like B1, a play aid.
4. **Fidelity:** same proof as B1 (every saved moment must restore exactly). The memory needed is small (the state is a few hundred bytes a frame; a minute of history is well under a megabyte, an estimate).
5. **Difficulty:** medium; **needs B1 first**.
6. **Dependencies:** none.
7. **Optional?** Yes.
8. **Proposal:** later, only if B1 is chosen.

### B3. Best-score table
1. **What it does:** remembers the best scores of your sessions (the original shows no high-score table) and shows them on request.
2. **Why:** a reason to play again; the score is in the game's memory (decimal digits in two bytes).
3. **Changes game behaviour?** No; it reads the score at game over and writes a small file.
4. **Fidelity:** none to the game. Care is needed to know when a game is really over (a lesson from Phase E: lives reading 0 happens outside play too) and for two-player games.
5. **Difficulty:** medium (reading the state, a file, a screen to show it; the program already draws simple text for the F1 list).
6. **Dependencies:** none.
7. **Optional?** Yes; it could be a key that shows the table.
8. **Proposal:** later.

### B4. Flicker-free view (picture filter)
1. **What it does:** shows each ghost on every frame, not one in four, by combining the last few pictures in the window.
2. **Why:** the flicker is the best-known thing about this port of Pac-Man; some people dislike it.
3. **Changes game behaviour?** **No**, if done as a picture filter in the window program only; the game still draws one ghost a frame and still decides collisions the same way. (Changing the game's own drawing would change which ghost can catch Pac-Man, F12, and is a Group C change; **not proposed**.)
4. **Fidelity:** the game is unchanged; the look is not the original's. **Already half there:** the soft glow (F7, D-007) blends 60 % of the previous picture. A stronger filter might leave a short trail behind Pac-Man (he moves about 0.57 pixel a frame); whether it looks right is unknown until tried.
5. **Difficulty:** medium, and it needs a trial first.
6. **Dependencies:** none.
7. **Optional?** Yes, off by default.
8. **Proposal:** later; try the F7 glow first and tell me whether it is enough.

---

## C. Changes how the game plays

*All of these need one new "options" value handed to the game, whose default means "exactly the original". The existing proofs (70 + 120 games) then show the default is unchanged. Each "on" setting gets its own tests, kept apart from the frozen suite.*

### C1. Starting lives and practice mode
1. **What it does:** choose the number of lives at the start (the original gives 3), and perhaps a practice setting where Pac-Man cannot be caught.
2. **Why:** learning the maze; easier play for guests.
3. **Changes game behaviour?** Yes, when on.
4. **Fidelity:** none when off. When on it is a different game and is labelled as such. Scores from such games should not count in B3 (a rule to decide).
5. **Difficulty:** small to medium (the game's start-up in `src\game\logic\flow.cpp` and the death handling).
6. **Dependencies:** none.
7. **Optional?** Yes; off by default.
8. **Proposal:** later.

### C2. Extra life at a score
1. **What it does:** gives a bonus life when the score passes a limit (the original gives none for score, only one per cleared map, `docs\F15_SCORING.md`).
2. **Why:** a familiar feature of other arcade games.
3. **Changes game behaviour?** Yes, when on.
4. **Fidelity:** none when off.
5. **Difficulty:** small (the scoring module).
6. **Dependencies:** none.
7. **Optional?** Yes; off by default.
8. **Proposal:** later.

### C3. More difficulty settings
1. **What it does:** extra ghost-speed or scared-time settings beyond the console's own A/B switches and the eight Select games (which choose one of four starting levels).
2. **Why:** finer tuning.
3. **Changes game behaviour?** Yes.
4. **Fidelity:** none when off.
5. **Difficulty:** medium (ghost speed comes from small seeds that depend on the level; changes ripple through the movement tests).
6. **Dependencies:** none.
7. **Optional?** Yes.
8. **Proposal:** never. The console already has difficulty controls, and the benefit is small for the risk and the tests it adds.

### C4. New mazes or game modes
1. **What it does:** other mazes, or new kinds of game.
2. **Why:** variety.
3. **Changes game behaviour?** Yes, completely.
4. **Fidelity:** none when off.
5. **Difficulty:** very large. The maze is a small table, but the picture code that draws it, the wafer storage and the ghost routes are all tied to this one maze and to the exact timing of the original's drawing.
6. **Dependencies:** none, but a lot of new tests.
7. **Optional?** Yes.
8. **Proposal:** never, for now. This would be a different project built on this one.

---

## D. Other

### D1. Other operating systems
1. **What it does:** builds the program for Linux or macOS.
2. **Why:** more players. The libraries (SDL3) and the game code are not Windows-specific.
3. **Changes game behaviour?** No.
4. **Fidelity:** the same game; the proofs would be re-run on the new system.
5. **Difficulty:** medium to large (the build scripts are written for Microsoft's compiler and Windows; I have no way to test other systems on this PC; I would need a machine or a helper).
6. **Dependencies:** none new, but a different compiler and build files.
7. **Optional?** n/a.
8. **Proposal:** later, after Phase J decisions (a published project invites the request).

### D2. Online scores or network play
1. **What it does:** a shared score list or play over the internet.
2. **Why:** competition.
3. **Changes game behaviour?** Not the rules, but it needs trust in the scores.
4. **Fidelity:** none to the game.
5. **Difficulty:** very large (a server, accounts, cheating protection, privacy duties; it also conflicts with keeping the program small and offline).
6. **Dependencies:** yes, network libraries and a server.
7. **Optional?** n/a.
8. **Proposal:** never.

---

## How to answer in I3

Reply in your own words, or in one line, for example: *"A1, A2, A3 now. B1 later. A4 never. Everything else never."* I will record your exact words in `DECISIONS.md` (the approved ones as decisions, the rejected ones in a **Considered and declined** list so they are not proposed again) and plan one small stage per approved feature (I4). You may also add features of your own, ask me to explain any item further, or say **"nothing"**.
