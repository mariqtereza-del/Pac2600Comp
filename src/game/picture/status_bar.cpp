#include "status_bar.hpp"
#include "../data/rom_tables.hpp"
#include "../state/cycles.hpp"

namespace game {

// LIVES_MARKERS ($FC68).
uint8_t livesMarkers(GameState& st, const Cartridge& cart, uint8_t lives, uint8_t offset) {
  st.scratch[kTempD2 + offset] = lives == 0 ? 0 : 0x40;
  st.scratch[kTempD3 + offset] = cart.at(rom::kLivesMarkerPf1 + lives);
  const uint8_t last = cart.at(rom::kLivesMarkerPf2 + lives);
  st.scratch[kTempD4 + offset] = last;
  return last;
}

// SCORE_DIGITS ($F3EE) and CLEAR_GRAPHICS ($F483).
void scoreDigits(GameState& st, const Cartridge& cart, TiaChip& chip, uint8_t offset) {
  auto buffer = [&](uint8_t slot) -> uint8_t& { return st.scratch[kTempD5 + slot]; };
  auto scoreByte = [&](uint8_t y) { return st.score[y]; };
  uint8_t& seen = st.scratch[kTempE1];      // how many digits that are not zero were met (leading zeros are blank)
  uint8_t& slotMemory = st.scratch[kTempE2];
  seen = 0;
  uint8_t slot = 8;
  uint8_t y = offset;
  auto digitShape = [&](uint8_t digit) {
    if (digit != 0) ++seen;
    if (seen == 0) digit = 10;
    return cart.at(rom::kDigitPointerLow + digit);
  };
  bool first = true;
  for (;;) {
    if (!first) {                                           // L_F3F6: the high nibble of the next lower byte
      buffer(slot) = digitShape(scoreByte(y) >> 4);
      slot = static_cast<uint8_t>(slot - 2);
      if (slot == 4) chip.write(chip::COLUBK, 0);
      else if (slot == 0) chip.write(chip::COLUBK, st.scratch[kTempD2]);
    }
    first = false;
    slotMemory = slot;                                      // L_F422: the low nibble
    buffer(slot) = digitShape(scoreByte(y) & 0x0F);
    slot = static_cast<uint8_t>(slot - 2);
    slotMemory = slot;
    y = static_cast<uint8_t>(y - 2);
    if (y & 0x80) break;
  }
  if (seen == 0) buffer(0) = 0xAC;                          // a score of 0 shows one 0
  for (uint8_t i = 1; i <= 9; i += 2) buffer(i) = 0xFE;     // the high bytes of the five shape pointers
  auto shapeByte = [&](uint8_t base, uint8_t row) {
    const uint16_t pointer = static_cast<uint16_t>((buffer(static_cast<uint8_t>(base + 1)) << 8) | buffer(base));
    return cart.at(static_cast<uint16_t>(pointer + row));
  };
  seen = 6;
  for (;;) {
    const uint8_t row = seen;
    chip.write(chip::GRP0, 0);
    chip.write(chip::GRP0, 0);
    chip.write(chip::GRP1, shapeByte(8, row));              // pointer at $DD
    chip.write(chip::GRP0, shapeByte(6, row));              // $DB
    const uint8_t third = shapeByte(4, row);                // $D9
    slotMemory = third;
    const uint8_t fourth = shapeByte(2, row);               // $D7
    const uint8_t fifth = shapeByte(0, row);                // $D5
    chip.write(chip::GRP1, third);
    chip.write(chip::GRP0, fourth);
    chip.write(chip::GRP1, fifth);
    chip.write(chip::GRP0, fifth);
    seen = static_cast<uint8_t>(seen - 1);
    if (seen & 0x80) break;
  }
  chip.write(chip::GRP0, 0);                                // CLEAR_GRAPHICS
  chip.write(chip::GRP1, 0);
  chip.write(chip::GRP0, 0);
  chip.write(chip::GRP1, 0);
}

}  // namespace game
