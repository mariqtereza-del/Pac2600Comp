// The processor cycles the original code needs, counted along the path the rewritten routines take (Stage F24.2, moved here in G4.1). Only the
// bottom logic of the frame uses the count: its sound writes happen at an exact moment (cycles x 3 colour clocks after the end of the drawing code),
// which decides the samples. The calls to cy() move with their code and are never merged, reordered or dropped (docs/CONVENTIONS.md).
#pragma once

namespace game {

inline thread_local long gCycles = 0;
inline void cy(int cycles) { gCycles += cycles; }

}  // namespace game
