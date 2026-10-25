# Stage F19: the sound routines and their tables (2026-10-26)

`python -I tools\f19_sound.py rom\pacman.a26` holds a model of the four sound routines and checks it on every frame of the 70 recorded games against the real emulator (every write to the six sound registers, in order and value, and the sound bytes `$E8`/`$E9`). This document is written by that tool.

## Result: exact

- **435986 of 435986 predicted frames exactly right (0 wrong)**; 123763 of them contain a sound beyond the silencing of the volumes. Frames skipped: 2226 (Select/Reset and the frames where the title screen starts or ends).
- Sound starts seen: wafer eaten 4276, bonus item eaten 18, power pill eaten 102, ghost eaten 193.

## How the sound works

The game has two sound channels (0 and 1). Every frame, near the end of the bottom logic (`$F7C3-$F7CE`), it silences both volumes (`AUDV0` = `AUDV1` = 0) and then calls `SOUND_EFFECTS` (channel 0) and `SOUND_MUSIC` (channel 0 or 1, by the situation). **Nothing is stored between frames in the chip**: whatever a routine does not write in a frame is silence.

### Sound effects (four sounds, one at a time)

`SOUND_START` (`$FF43`) is called from the game rules with a sound number Y. It starts the sound unless a sound that blocks it is playing: `SOUND_ACTIVE` (`$E8`) AND `TBL_FFE9[Y]` must be 0; then `SOUND_ACTIVE` = `TBL_FE6E[Y]` (a single bit) and `SOUND_TIMER` (`$E9`) = `TBL_FFED[Y]`.

| Y | Sound | Bit in `$E8` | Blocked while `$E8` has | Length (frames) | What `SOUND_EFFECTS` writes each frame (channel 0) |
| --- | --- | --- | --- | --- | --- |
| 0 | wafer eaten | $08 | $07 | 8 | AUDV = timer (falls from 9 to 2), AUDC = 9, AUDF = 9: the wafer "chomp" |
| 1 | bonus item eaten | $04 | $03 | 23 | AUDV = AUDC = AUDF = $0D: a steady tone |
| 2 | power pill eaten | $02 | $01 | 15 | AUDV = AUDC = AUDF = $0D: the same steady tone |
| 3 | ghost eaten | $01 | $00 | 47 | AUDC = 4, AUDF = timer and $1F (falls), AUDV = AUDF xor $0F (rises): a sliding tone |

The timer goes down by one each frame; the frame it reaches 0 writes nothing and clears `SOUND_ACTIVE`, so a sound lasts (length - 1) frames of sound. A new sound is refused while a higher-ranking one plays: ghost eaten (never refused) > power pill (refused while a ghost sound plays) > bonus item (refused while a ghost or pill sound plays) > wafer (refused while any of the three plays); a sound of the same kind simply starts again. The wafer sound is restarted by every wafer, which makes the chomping.

### Music and long sounds (`SOUND_MUSIC`)

Chosen in this order each frame:

1. **Pac-Man dying** (`GAME_FLAGS` bit 6): channel 0, `AUDC` 4, `AUDV` = `EVENT_TIMER`, `AUDF` = `EVENT_TIMER` / 2. The timer counts down from `$3F`, so the tone falls in pitch and volume (the death wail), for 126 frames.
2. **The start tune** (`GAME_FLAGS` bit 3 set at the start of a try): from `EVENT_TIMER` bits 3-5 (`x`): for x >= 4 silence; for x = 0..3 both channels play `AUDC` 4, `AUDV` 8 with the pitches `SND_FFF1[x]` = $08 $04 $0C $04 (channel 0) and `SND_FFF5[x]` = $14 $18 $14 $18 (channel 1). When the timer reaches 0 the bit is cleared.
3. **Scared ghosts** (`PILL_FLAGS` bit 6, the first part of the scare): channel **1**, `AUDC` 4, `AUDF` = (`FRAME_COUNTER` / 2) and $0F, `AUDV` = (`AUDF` xor $0F) / 2: a pitch that sweeps up and down every 32 frames.
4. Otherwise nothing.

A quirk of the tune: when `EVENT_TIMER` is 0 the code clears the tune bit and then takes the pitch number from `GAME_FLAGS` bits 3-5 instead of the timer (the accumulator still holds the flags, `$FFAE-$FFB2`); with the difficulty bit (bit 5) set that gives x = 4 (silence), without it x = 0 (the first note sounds once more). The model reproduces this and is exact in every frame.

An effect writes channel 0 first, then the music may write channel 0 again (death, tune): the music wins. In the pause after eating a ghost the music is the scare sweep on channel 1, and the effect (ghost eaten) on channel 0.

## For the rebuild

- The sound state is two bytes (`$E8`, `$E9`) plus the situation (`GAME_FLAGS`, `EVENT_TIMER`, `PILL_FLAGS`, `FRAME_COUNTER`); the output of a frame is at most 3 + 6 register writes, listed exactly by the model above. The native sound generator (F24) only has to turn register values into samples like the TIA does and be compared sample by sample with the emulator (D8 and C5 give the sound chip facts).
- Sound is created in the bottom logic by four calls (`$F57D...`: bonus item Y=1, `$F5CD`: power pill Y=2, `$F602`: ghost Y=3, `$F6A7`: wafer Y=0), in that order within a frame, and by the Select step (`$F8DB-$F8DF`: volumes and `SOUND_ACTIVE` cleared).

## Not claimed

- Frames with Select or Reset and the frames where the title screen starts or ends are skipped; the two test games BL-065/066 are skipped.
- The sound of the two-player change of turn and of difficulty A was not separately observed (no sound code depends on them).
