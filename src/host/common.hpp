// Shared by all the parts of the pacman2600 host program (Stage G4.7): the headers they use, the window title and the first window size.
// (Stage G4.7: moved out of main.cpp, the code is unchanged.)
#pragma once

#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <atomic>
#include <fstream>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <array>
#include <vector>
#include "audio.hpp"
#include "controls.hpp"
#include "emulator.hpp"
#include "inputlog.hpp"
#include "layout.hpp"
#include "rom_finder.hpp"
#include "settings.hpp"
#include "trace.hpp"
#include "windowstate.hpp"

namespace host {

inline constexpr const char* kTitle = "pacman2600 (working name)";  // real name is chosen in Stage J2

inline constexpr int kWindowW = 1280, kWindowH = 960;  // first window size; 4:3 like the picture

}  // namespace host
