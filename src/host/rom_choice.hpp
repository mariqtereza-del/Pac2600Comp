// Finding the ROM to play: the quiet search, the file dialog, the message about a problem.
// (Stage G4.7: moved out of main.cpp, the code is unchanged.)
#pragma once
#include "common.hpp"
#include "options.hpp"

namespace host {

void showError(const char* what);

// --find-rom: the quiet search, no window. Exit code 0 = good ROM found, 4 = a problem with the ROM, 5 = nothing found.
int findRomReport(const Options& o);

std::string fileNameOf(const std::string& path);

// Reads the 4096 ROM bytes of a file that has already passed the checks (read only).
bool readRomBytes(const std::string& path, std::vector<std::uint8_t>& bytes);

// Finds a usable ROM, asking the person when needed. Returns the chosen path, or empty if they gave up.
std::string resolveRom(SDL_Window* window, const Options& o);

}  // namespace host
