// The chips as the game code sees them (Stage G4.1, moved out of routines.hpp): the game is given somewhere to write the video and sound chip
// registers; what stands behind it (the picture and sound models, the lockstep tool's emulated chip) is not the game's business.
#pragma once
#include <cstdint>

namespace game {

// The sound chip as the game logic sees it: it is given register writes. In the finished game the sound generator of F24 stands behind it; in the
// lockstep tool the writes go to the emulated chip (and are compared with the original's writes).
class TiaChip {
 public:
  virtual ~TiaChip() = default;
  virtual void write(uint8_t reg, uint8_t value) = 0;
  // Where in the picture the following writes happen: a scanline number (the logic code has no exact timing), or -1 for the power-on set-up. Only a
  // video renderer cares; the default does nothing.
  virtual void position(int /*line*/) {}
  // From now on the writes follow the processor cycle count (gCycles, reset to 0 now) from this scanline and colour clock: the bottom logic of the frame,
  // whose length varies and decides the moment of its sound writes. Ended by the next position().
  virtual void timeBase(int /*line*/, int /*clock*/) {}
  // A write at an exact moment of the top logic: this many processor cycles after the write that switched the vertical sync off (the Select step of the
  // title screen silences the sound there). The default is an ordinary write.
  virtual void writeAtCycle(long /*cycles*/, uint8_t reg, uint8_t value) { write(reg, value); }
};
using SoundChip = TiaChip;   // the sound routines see the same chip

namespace chip {
inline constexpr uint8_t COLUBK = 0x09, GRP0 = 0x1B, GRP1 = 0x1C, COLUP0 = 0x06, COLUP1 = 0x07, REFP1 = 0x0C, VSYNC = 0x00, VBLANK = 0x01, CXCLR = 0x2C, RESM1 = 0x13, RESBL = 0x14, HMM1 = 0x23, HMBL = 0x24, HMOVE = 0x2A, HMCLR = 0x2B;
inline constexpr uint8_t AUDC0 = 0x15, AUDC1 = 0x16, AUDF0 = 0x17, AUDF1 = 0x18, AUDV0 = 0x19, AUDV1 = 0x1A;
}

}  // namespace game
