// Natively written routines that can replace the original ones in the lockstep machine. Stage F20 holds only two test fixtures
// (RNG_STEP, correct and deliberately broken) to prove that the harness finds a wrong routine; F21 adds the real rewrites.
#pragma once
#include <string>

#include "machine.hpp"

namespace lockstep {
bool registerNativeByName(Machine& m, const std::string& name);
}
