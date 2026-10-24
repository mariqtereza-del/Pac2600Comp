# Sound facts (Stage C5, measured 2026-10-24)

All values were **measured** by `tools\stella_probe\sound.cpp` → `stella_sound.exe` (DECISIONS.md T-C5-1), which runs the ROM in the same Stella 7.0 emulator code as the reference app. None is copied from memory. The names of the sounds are **interpretations**, each backed by the evidence given.

- **What the tool records:** every change of the six TIA sound registers (at CPU-instruction precision), the full audio output as WAV, the picture fingerprint, a status-bar fingerprint and the colours of every frame.
- **Proof it doesn't change the emulation:** the idle run's 24,000 picture fingerprints equal C3's. Every re-run with extra recording reproduced its earlier data exactly.
- **Runs** (24,000 frames each, about 6 min 40 s, data in `reference\audio\<run>\`, local only):
  - `idle`: the C3 scenario (attract 0–1799, Game Reset 1800–1809, no input).
  - `play1`: Game Reset at 1800, then a scripted joystick (seed 1) holding a pseudo-random direction for 30–119 frames at a time.
  - `seed1` … `seed6`: scripted joystick with seeds 1–6, plus a new Game Reset every 4,000 frames to start new games.
  - Together that's 8 runs, about 53 minutes of emulated time. Frames with any sound: `idle` 738, `play1` 833, `seed1`–`seed6` 4,763–5,005 each. All runs were silent in attract mode.
- **Commands:**
  - `tools\build_stella_probe.cmd`
  - `build\stella_probe\stella_sound.exe <rom> reference\audio\<run> 24000 1800 <seed or -1> [- 4000]`
  - `python -I tools\c5_report.py reference\audio\<run> third_party\stella\src 1800 10`

## Audio format

| Fact | Value | Source |
| --- | --- | --- |
| Sample rate | **31,440 Hz** (262 × 76 × 60 ÷ 38) | `StellaLIBRETRO::getAudioRate()`; measured 524 samples per frame on all 23,999 full frames = 31,440 / 60 |
| Format | 16-bit, 2 channels (mono game: both sides identical) | `SoundLIBRETRO::dequeue()` |
| Channels used by the game | Both TIA voices: channel 0 for every sound, channel 1 only for the second part of the tune | Register data below |
| Effective register bits | AUDC 4 bits, AUDF 5 bits, AUDV 4 bits. The game writes bytes with higher bits set (for example AUDV = 56–62), which the TIA ignores | `TIAConstants.hxx` comments; values in `aud_changes.csv` |
| Attract mode | **Silent**: no volume above 0 on any frame before the first Game Reset | `idle` run, frames 0–1799 |

## The sounds

Only **three different sounds** occurred in all 8 runs. Across every run, the only (tone type, pitch) pairs ever played at non-zero volume were:
- channel 0: AUDC 4 with pitches 0–31, and AUDC 9 with pitch 9;
- channel 1: AUDC 4 with pitches 20 and 24.

Register notation: `C4 F12 V8 ×16` means AUDC = 4, AUDF = 12, AUDV = 8, held for 16 frames.

### 1. Start tune: at the start of a game and after each lost life

| | Channel 0 | Channel 1 |
| --- | --- | --- |
| Sequence | `C4 F4 V8 ×16, C4 F12 V8 ×16, C4 F4 V8 ×16, C4 F8 V8 ×15` | `C4 F24 V8 ×16, C4 F20 V8 ×16, C4 F24 V8 ×16, C4 F20 V8 ×15` |
| Length | 63 frames = 1.05 s (both channels together) | 63 frames |

- **When:** about 55 frames after Game Reset is released (frame 1865 in every run), and after each lost life. In `idle` that was frames 2407, 2907 and 3409, about 180 frames after each death.
- **Picture at frame 1865** (`seed1\png\frame_001865_160x228.png`): new game, Pac-Man at the start position, a ghost in the pen, score 0, three life markers.
- **Clips:** `idle\clips\sound_02_ch1_frame001865.wav` (the whole tune) and `idle\clips\sound_04_ch0_frame002407.wav` (the tune after a death).

### 2. Scoring blip: when the score goes up (eating a wafer)

| Channel | Sequence | Length |
| --- | --- | --- |
| 0 | `C9 F9 V9, V8, V7, V6, V5, V4, V3, V2` (one frame each), then silence | 8 frames = 0.13 s |

- **Evidence:** in all runs together, **all 376** starts of this sound fall within 2 frames of a change in the status bar (score area). The status bar changed 844 times in all. The other 468 changes weren't examined one by one; they include life-marker changes, game starts and the colour shifting after game over.
- In the first game of each run it directly follows the start tune (frame 1928), when the score first changes.
- **Picture:** `seed1\png\frame_002068_160x228.png` shows score 2 with Pac-Man on the bottom row of wafers.
- **Clip:** `seed1\clips\sound_03_ch0_frame002068.wav`. `sound_06_…002508.wav` is two blips in a row.

### 3. Death sound: when a ghost catches Pac-Man

| Channel | Sequence | Length |
| --- | --- | --- |
| 0 | Tone type C4 throughout. **AUDF steps down by 1 every 4 frames, F31 → F0, so the note rises.** Stella's `AudioChannel.cxx` lines 70–75 advance a channel once every AUDF + 1 clocks, so a smaller AUDF is a higher note. The **volume ramps down 15 → 0 every 32 frames (1 step per 2 frames), four times over.** In full: `C4 F31 V15 ×1, F31 V14 ×2, F30 V13 ×2, F30 V12 ×2, …, F16 V0 ×2, F15 V15 ×2, …, F0 V0 ×2` | 125–126 frames ≈ 2.1 s |

- **Evidence:**
  - Picture at its start (`seed1\png\frame_002138_160x228.png`): Pac-Man touching a ghost.
  - In `idle` it occurs exactly 4 times (frames 2224, 2723, 3225, 3729), matching the four lives lost before game over (C3 pictures).
- **Clips:** `idle\clips\sound_03_ch0_frame002224.wav` and `sound_05_ch0_frame002723.wav`. Some `seed` clips join a blip and a death that follow each other (for example `seed1\clips\sound_08_ch0_frame002862.wav`).

## How the game drives the sound (for the native rebuild)

- **Once per frame.** Registers are updated once per frame, at the end of the drawing area:
  - every frame with sound, the game writes **AUDV = 0 at scanline 226** and then the real volume at **scanline 227**;
  - that's about one scanline (≈ 64 µs) of silence per frame;
  - it happened 1,004 times in `idle` (both channels), and no other register changes more than once in a frame apart from 2 isolated cases.
- **Where it shows:** the WAV files include this short dip, so a faithful port must include it too.
- **Timing:** sounds are measured in frames, so their real duration follows the frame rate (C4: 60.000 Hz in Stella, 59.92 Hz on Stella's console clock).

## Not yet captured

| Sound | Why not | Where it will be captured |
| --- | --- | --- |
| Eating a **power pill** / ghosts turning vulnerable | The scripted joystick never produced a frame with changed ghost colours. In 67,892 frames showing the normal maze colours, the only extra colours were attract-mode and Game-Reset frames. So it is unknown whether a power pill was ever eaten, and whether this event has its own sound | Phase E recordings of real play (E3/E7), or a smarter scripted route |
| **Eating a ghost** | Never happened | Same |
| **Clearing a level** (all wafers eaten) | Never happened | Same |
| A continuous background sound while playing | **None observed**: between the sounds above, both volumes stayed 0 throughout play | Re-checked in Phase E |
| Sounds in other game variations (Game Select) | Only the default variation was run | C6 / Phase E |

## Notes

- **What a clip contains:** each clip is the game's whole audio output during that time (both channels mixed), from 6 frames before the sound to 6 frames after it. A clip named "ch0" can therefore also contain channel 1.
- **Full-run audio:** `audio.wav` in each run folder (50 MB each) is the whole 6 min 40 s.
- **About `idle_check`:** that folder is a repeat of `idle`, made to prove that a tool change kept the idle run byte-identical.

## Played by the host (Stage D8)

The host's own audio for 2,500 frames (Game Reset at frame 1800) is byte-identical to the first 5,237,904 bytes of `reference\audio\idle\audio.wav`, so the sound the PC port plays is the sound recorded in C5, including the one-scanline volume dip each frame. See `DECISIONS.md` T-D8-1.

## Heard by the owner (Stage D10)

With the keyboard of D10 the owner played and heard the wafer-eating blip, a **power pellet sound** and the sound of **eating a ghost**. The last two were listed above as "not yet captured": the scripted joystick of C5 never produced them. They are heard, not measured, so their registers and lengths are still to be recorded (Phase E recordings of real play).
