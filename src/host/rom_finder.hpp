// Finding and checking the player's ROM (Stage D3, DECISIONS.md T-D3-1). No window code here, so it can be tested alone.
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace rom {

constexpr std::uint64_t kExpectedSize = 4096;
constexpr const char* kExpectedSha1 = "0940fea7f04cdb6d4b90c5ad1a7e344e68f6dbb1";  // docs\ROM_IDENTITY.md (A3)
constexpr const char* kExpectedName = "Pac-Man (1981) (Atari).a26";

enum class Status { Ok, Missing, NotAFile, Unreadable, Empty, WrongSize, UnknownFingerprint };

struct Check {
  Status status = Status::Missing;
  std::string path;       // UTF-8
  std::uint64_t size = 0;
  std::string sha1;       // lower-case hex; only filled when the size was right and the file was read
};

// Paths are UTF-8 strings. The file is only ever opened for reading.
std::string sha1Hex(const std::uint8_t* data, std::size_t size);
Check checkFile(const std::string& path);
std::string describe(const Check& check);  // plain-words message for a person

// settings.ini: one line "rom_path=<UTF-8 path>".
std::string loadSavedPath(const std::string& settingsFile);  // empty if none
// Stores the absolute form of `path`.
bool savePath(const std::string& settingsFile, const std::string& path);

struct Found {
  std::string source;  // "command line", "saved path" or "roms folder"
  Check check;
};

// The quiet part of the search (no dialog). A path given on the command line is returned as it is, good or bad, so
// the caller can say what is wrong with it. The saved path and the roms folder are only returned when they pass.
// `log` (optional) receives one line per place looked at.
std::optional<Found> findQuietly(const std::string& commandLinePath, const std::string& settingsFile,
                                 const std::string& romsDir, std::vector<std::string>* log = nullptr);

}  // namespace rom
