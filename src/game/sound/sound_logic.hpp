// What the game plays: which sound effect runs, and the long sounds (start tune, dying, scared sweep). The sound chip model itself is tia_audio.
// (Stage G4.3: moved out of routines.hpp. The functions are the F21 rewrites of the original routines named in their comments.)
#pragma once
#include <cstdint>

#include "../chips.hpp"
#include "../data/cartridge.hpp"
#include "../state/state.hpp"

namespace game {

// F21.17  SOUND_START ($FF43): starts sound effect number y (0 wafer, 1 bonus item, 2 power pill, 3 ghost eaten, F19) unless one that outranks it is playing.
// The cartridge tables: $FFE9 (the sounds that block it, as bits of SOUND_ACTIVE), $FE6E (the bit it sets in SOUND_ACTIVE), $FFED (its length, into
// SOUND_TIMER). Returns the value the original leaves in A: the blocking bits if it was refused, else the length.
uint8_t soundStart(GameState& st, const Cartridge& cart, uint8_t sound);

// F21.18  SOUND_EFFECTS ($FF55) with its shared writer SOUND_WRITE ($FF8B): each frame, plays the sound effect that SOUND_START chose, on channel 0 (the
// caller has X = 0). Ghost eaten: control 4, pitch = timer and $1F, volume = pitch xor $0F; power pill and bonus item: the steady tone $0D; wafer:
// control 9, pitch 9, volume = timer. The timer goes down; when it reaches 0 nothing is written and the effect ends (SOUND_ACTIVE = 0). TEMP_D3 is the
// scratch byte that holds the control value.
void soundEffects(GameState& st, SoundChip& chip);

// F21.19  SOUND_MUSIC ($FF94): each frame, the long sounds, in this order: Pac-Man dying (GAME_FLAGS bit 6): channel 0, volume = pitch x 2 = EVENT_TIMER,
// control 4; the start tune (GAME_FLAGS bit 3): from bits 3-5 of EVENT_TIMER (or, when the timer is 0, of the flags with the tune bit cleared) a note 0-3
// for both channels from the cartridge tables $FFF1 / $FFF5, control 4, volume 8, silent for 4-7; the scared-ghost sweep (PILL_FLAGS bit 6): channel 1,
// control 4, pitch = (FRAME_COUNTER / 2) and $0F, volume = (pitch xor $0F) / 2. The caller has X = 1 (channel 1) for the sweep. TEMP_D3 holds the sweep's
// control value.
void soundMusic(GameState& st, const Cartridge& cart, SoundChip& chip);

}  // namespace game
