// The only place that turns game::GameState into the original's 128 bytes of memory and back (Stage G3). The test tools need it because they compare
// the game with the original byte for byte; the game code itself never includes this file. Including it also runs state_layout_checks.hpp, which
// fails the build if any field of GameState is not where the F7 map says.
#pragma once
#include <array>
#include <cstdint>
#include <cstring>

#include "../game/state.hpp"
#include "state_layout_checks.hpp"

namespace testsupport {

using Bytes = std::array<uint8_t, 128>;   // memory $80-$FF

inline Bytes toBytes(const game::GameState& state) {
  Bytes bytes;
  std::memcpy(bytes.data(), &state, sizeof state);
  return bytes;
}

inline game::GameState fromBytes(const Bytes& bytes) {
  game::GameState state;
  std::memcpy(&state, bytes.data(), sizeof state);
  return state;
}

// One byte by its address ($80-$FF), for the test options that poke a value into the running game.
inline uint8_t& byteAt(game::GameState& state, int address) { return reinterpret_cast<uint8_t*>(&state)[address - 0x80]; }

}  // namespace testsupport
