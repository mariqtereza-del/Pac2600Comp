// Running the game with no window: headless runs, the key replay and the frame trace.
// (Stage G4.7: moved out of main.cpp, the code is unchanged.)
#pragma once
#include "common.hpp"
#include "options.hpp"

namespace host {

// One line of a frame trace (Stage E1): the input given on this frame and what the console did.
void writeTrace(std::ofstream& out, const Emulator& emulator, long long frame, std::uint32_t input);

// --keys-replay: runs the game with no window and presses the keys that a Phase C input script (lines "from to NAME",
// frames counted from 0, both ends included) asks for, in the real keyboard code, so the recorded Phase C runs can be
// compared frame by frame with what the keys do. Colour/B&W is the F4 switch (D12).
int keysReplay(const Options& o);

// --headless: run the game with no window. Exit code 0 = done, 4 = problem with the ROM, 5 = no ROM found, 6 = other failure.
int headlessRun(const Options& o);

}  // namespace host
