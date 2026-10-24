// settings.ini: lines of "key=value" (Stage D7, DECISIONS.md T-D7-1). No window code.
// Writing one key keeps every other line, so the ROM path and the window state can share the file.
#pragma once

#include <string>

namespace settings {

// The value of the key, or an empty string if the file or the key is missing. Paths are UTF-8.
std::string get(const std::string& file, const std::string& key);

// Sets the key, keeping all other lines. Creates the file if needed. False if it could not be written.
bool set(const std::string& file, const std::string& key, const std::string& value);

}  // namespace settings
