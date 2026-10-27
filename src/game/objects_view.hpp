// What the five moving objects look like in the picture of a frame (Stage I9.2, D-015): where each stands, its picture lines, its colour. It is read from
// the game's state at the moment the drawing code starts (after the top logic), the way the drawing code itself finds it, so the sprites it describes are
// the ones the picture shows; the presenter (I9.3) draws all five every frame and glides them between their positions. The game never uses this.
#pragma once
#include <array>
#include <cstdint>

#include "kernel.hpp"
#include "state/state.hpp"

namespace game {

struct SpriteImage {
  bool drawn = false;        // the object is in the picture of this frame (the original draws Pac-Man and one ghost per frame)
  bool visible = false;      // it has lines to show (a ghost or Pac-Man inside an opening of the wall can be out of sight)
  bool held = false;         // a ghost not drawn this frame that is inside an opening: its picture is not known, the last one stays
  int column = 0;            // picture column of the left edge of the 8-pixel-wide picture
  int row = 0;               // picture row of the first line shown
  int lines = 0;             // number of lines shown (up to 8)
  // the picture row where line i begins to show; rowStart[lines] is where the last one ends (a line is not always two rows high)
  std::array<int, 9> rowStart{};
  std::array<uint8_t, 8> bits{};   // one byte a line, the first line first; bit 7 is the left pixel (before any mirroring)
  uint8_t colour = 0;        // the chip's colour value
  bool mirrored = false;     // the picture is shown mirrored (Pac-Man looking left)
};

// Object 0 is Pac-Man, 1 to 4 the ghosts (as in GameState).
using ObjectsView = std::array<SpriteImage, 5>;

// `schedule` says on which picture rows the drawing code shows each line of a sprite (spriteSchedule in kernel.hpp, for the start lines of the frame).
// `st` is the state when the drawing code starts. `pacmanMirrored` says whether the chip's reflect bit for Pac-Man was set when the frame began: the bottom
// logic of the frame before writes it, but only on the paths that reach the end of the frame, so it is not the same as the state bit (Game notes the write).
ObjectsView viewObjects(const GameState& st, const SpriteSchedule& schedule, bool pacmanMirrored);

}  // namespace game
