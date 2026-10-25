// The data loader (Stage F22): takes the player's own ROM file, checks that it is the right one (size and SHA-1 fingerprint) and keeps a private
// copy of its 4096 bytes for the game to read by address through game::Cartridge. The source of the game holds no Atari data, only this check.
// No window, no emulator and no file names in here, so it can be tested alone.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

#include "routines.hpp"

namespace game {

inline constexpr std::size_t kCartridgeSize = 4096;
inline constexpr const char* kCartridgeSha1 = "0940fea7f04cdb6d4b90c5ad1a7e344e68f6dbb1";  // docs\ROM_IDENTITY.md (A3)

enum class LoadStatus { Ok, Unreadable, WrongSize, UnknownFingerprint };

std::string sha1Hex(const uint8_t* data, std::size_t size);   // lower-case hex
const char* describe(LoadStatus status);                      // plain words

// Owns the bytes. Cartridge views made by view() point into this object and must not outlive it.
class CartridgeData {
 public:
  // Copies the bytes only if the size is 4096 and the fingerprint matches; otherwise the object stays empty and the status says why.
  LoadStatus load(const uint8_t* bytes, std::size_t size);
  // Reads the file (opened for reading only) and does the same.
  LoadStatus loadFile(const std::string& path);
  bool loaded() const { return loaded_; }
  Cartridge view() const { return Cartridge(bytes_.data()); }

 private:
  std::array<uint8_t, kCartridgeSize> bytes_{};
  bool loaded_ = false;
};

}  // namespace game
