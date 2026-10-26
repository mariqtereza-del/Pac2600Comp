// The view of the cartridge's 4 KB that the game code reads (Stage G4.1). The data of the cartridge (maze, amounts, shapes, sounds) is never part of
// this source: the game reads it by its address in the original program ($F000-$FFFF) through this view, which the data loader (cartridge_data.hpp,
// F22) fills from the player's own ROM after checking its fingerprint. The tables the game reads are part of the program now (tables.hpp, G5b); this view only goes on until G5c.
#pragma once
#include <cstdint>

namespace game {

#ifdef PACMAN_COUNT_CARTRIDGE_READS
// Test tool only (Stage G5a): the build lockstep_native_reads counts every read of the cartridge by address, to list the data the game really uses.
// The game's own builds never define this.
inline unsigned long long gCartridgeReads[4096] = {};
#endif

class Cartridge {
 public:
  explicit Cartridge(const uint8_t* bytes4k) : bytes_(bytes4k) {}
  uint8_t at(uint16_t address) const {
#ifdef PACMAN_COUNT_CARTRIDGE_READS
    ++gCartridgeReads[address & 0x0FFF];
#endif
    return bytes_[address & 0x0FFF];
  }

 private:
  const uint8_t* bytes_;
};

}  // namespace game
