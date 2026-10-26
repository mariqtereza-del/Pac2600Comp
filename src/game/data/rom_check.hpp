// The ROM check (Stage G5c, D-009). The program asks the player for the original cartridge file and checks that it is the right one: exactly 4096
// bytes with the SHA-1 fingerprint of the Pac-Man (1981) Atari cartridge. That is all the ROM is for: an indirect copy protection, so that nobody
// without a legal dump can play with this project. Nothing is kept and nothing is handed to the game: the game has all its data in tables.hpp.
// No window, no emulator and no file names in here, so it can be tested alone.
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

namespace game {

inline constexpr std::size_t kRomSize = 4096;
inline constexpr const char* kRomSha1 = "0940fea7f04cdb6d4b90c5ad1a7e344e68f6dbb1";  // docs\ROM_IDENTITY.md (A3)

enum class RomStatus { Ok, Unreadable, WrongSize, UnknownFingerprint };

std::string sha1Hex(const uint8_t* data, std::size_t size);   // lower-case hex
const char* describe(RomStatus status);                       // plain words

// Ok only if the size is 4096 and the fingerprint matches. Looks at the bytes and forgets them.
RomStatus checkRom(const uint8_t* bytes, std::size_t size);
// Reads the file (opened for reading only) and does the same.
RomStatus checkRomFile(const std::string& path);

}  // namespace game
