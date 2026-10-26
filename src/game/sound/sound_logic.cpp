#include "sound_logic.hpp"
#include "../data/tables.hpp"
#include "../state/cycles.hpp"

namespace game {

// SOUND_START ($FF43).
uint8_t soundStart(GameState& st, const Cartridge& cart, uint8_t sound) {
  cy(4 + 3);   // LDA, AND
  const uint8_t blocked = data::kSoundBlockers[sound] & st.soundActive;
  if (blocked != 0) { cy(3 + 6); return blocked; }
  cy(2 + 4 + 3 + 4 + 3 + 6);
  st.soundActive = data::kBitMasksAndReloads[4 + sound];
  const uint8_t length = data::kSoundLength[sound];
  st.soundTimer = length;
  return length;
}

// SOUND_EFFECTS ($FF55) and SOUND_WRITE ($FF8B).
void soundEffects(GameState& st, SoundChip& out) {
  const uint8_t active = st.soundActive;
  uint8_t volume, pitch, control;
  if (active & 0x01) {                                   // ghost eaten: a sliding tone
    cy(24);
    control = 4;
    pitch = st.soundTimer & 0x1F;
    volume = pitch ^ 0x0F;
  } else if (active & 0x06) {                            // power pill (bit 1) and bonus item (bit 2): the steady tone
    cy((active & 0x02) ? 22 : 30);
    control = pitch = volume = 0x0D;
  } else if (active & 0x08) {                            // wafer
    const uint8_t timer = st.soundTimer;
    cy(timer != 0 ? 33 : 37);
    if (timer != 0) { control = 9; pitch = 9; volume = timer; }
    else { control = pitch = volume = 0; }              // a timer of 0 cannot occur; the original then writes zeros
  } else {
    cy(29);
    return;
  }
  st.scratch[kTempD3] = control;
  uint8_t& timer = st.soundTimer;
  timer = static_cast<uint8_t>(timer - 1);
  cy(6);   // DEC SOUND_TIMER,X
  if (timer == 0) { cy(2 + 2 + 3 + 6); st.soundActive = 0; return; }
  cy(3);   // BNE
  cy(4); out.write(chip::AUDV0, volume);
  cy(3 + 4); out.write(chip::AUDC0, control);
  cy(4); out.write(chip::AUDF0, pitch);
  cy(6);
}

// SOUND_MUSIC ($FF94).
void soundMusic(GameState& st, const Cartridge& cart, SoundChip& out) {
  uint8_t& flags = st.gameFlags;
  const uint8_t timer = st.eventTimer;
  cy(2 + 3);   // LDA #8, BIT GAME_FLAGS
  if (flags & 0x40) {                                   // dying: a falling tone
    cy(2 + 3 + 3);
    out.write(chip::AUDV0, timer);
    cy(2 + 3);
    out.write(chip::AUDF0, timer >> 1);
    cy(2 + 3);
    out.write(chip::AUDC0, 4);
    cy(6);
    return;
  }
  cy(3);   // BVC taken
  if (flags & 0x08) {                                   // the start tune
    cy(2 + 3);   // BEQ not taken, LDA EVENT_TIMER
    uint8_t a = timer;
    // the tune ends; the note number is then taken from the flags themselves (as the original does)
    if (timer == 0) { cy(2 + 2 + 3 + 3); flags &= 0xF7; a = flags; }
    else cy(3);
    const uint8_t note = (a >> 3) & 7;
    cy(6 + 2 + 2 + 2);   // LSR x3, AND, TAX, CPX
    if (note >= 4) { cy(3 + 6); return; }
    cy(2);
    cy(4 + 3);
    out.write(chip::AUDF0, data::kStartTuneA[note]);
    cy(4 + 3);
    out.write(chip::AUDF1, data::kStartTuneB[note]);
    cy(2 + 3);
    out.write(chip::AUDC0, 4);
    cy(3);
    out.write(chip::AUDC1, 4);
    cy(2 + 3);
    out.write(chip::AUDV0, 8);
    cy(3);
    out.write(chip::AUDV1, 8);
    cy(6);
    return;
  }
  cy(3 + 3 + 2);   // BEQ taken, LDA PILL_FLAGS, ASL
  if (!((st.player[0].pillFlags << 1) & 0x80)) { cy(3 + 6); return; }   // scared (first part): the sweep on channel 1
  cy(2);
  st.scratch[kTempD3] = 4;
  const uint8_t pitch = (st.frameCounter >> 1) & 0x0F;
  cy(2 + 3 + 3 + 2 + 2 + 2 + 2 + 2 + 3);   // LDA #4, STA, LDA, LSR, AND, TAY, EOR, LSR, JMP
  cy(4); out.write(chip::AUDV1, (pitch ^ 0x0F) >> 1);
  cy(3 + 4); out.write(chip::AUDC1, 4);
  cy(4); out.write(chip::AUDF1, pitch);
  cy(6);
}

}  // namespace game
