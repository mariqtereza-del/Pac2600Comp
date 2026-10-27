// Smooth motion (Stage I9.3, D-015): the moving objects drawn between their places, with no delay. After every game step the smoother is given the
// description of the objects now (objects_view.hpp) and of the next few steps (lookahead.hpp, the controls held). Each object moves, along each axis, from
// the place it has now to the place it will have when it next changes, and arrives there at the moment the original shows it: so what is shown is never
// behind the original, and between two steps it is somewhere between the place the original shows and the place it shows next.
// An object that is not going to change inside the look-ahead stays where it is. A jump that is not an ordinary step (a new game, a death, a ghost sent
// home, a sprite cut off by an opening of the wall) is not glided across. The game never uses this.
#pragma once
#include <array>
#include <cstdint>
#include <vector>

#include "objects_view.hpp"
#include "tia_video.hpp"

namespace game {

// A filled rectangle in picture units (columns and rows of the 160 x 228 picture), in a colour value of the chip.
struct Rect {
  float x, y, w, h;
  uint8_t colour;
};

class Smoother {
 public:
  // After a game step: the objects as this step's picture shows them, the objects of the next steps (element 0 = the step after this one), and the
  // pictures of this step without the moving objects (`background`) and without the power pills either (`bare`), to find the pills.
  void step(const ObjectsView& now, const std::vector<ObjectsView>& ahead, const std::array<uint8_t, TiaVideo::kWidth * TiaVideo::kHeight>& background,
            const std::array<uint8_t, TiaVideo::kWidth * TiaVideo::kHeight>& bare);

  struct Options {
    bool glide = true;        // false: every object stays where this step's picture has it
    bool allGhosts = true;    // false: only the objects the original draws in this step (Pac-Man and one ghost): the picture of the original
  };
  // The rectangles that are drawn on the picture without the moving objects, in drawing order, `tau` (0 up to 1) of the way through this step.
  void compose(double tau, const Options& options, std::vector<Rect>& out) const;

  // How far the object has moved from the place the original shows now: used by the tests; columns and rows, 0 when it does not move.
  void offset(int object, double tau, double& columns, double& rows) const;

 private:
  struct Axis {
    int since = 0;     // steps since the object last changed along this axis
    int ahead = 0;     // steps until it next changes (0: not inside the look-ahead, or not to be glided)
    int delta = 0;     // how far it moves then
  };
  struct Track {
    SpriteImage image;   // what is shown now (a ghost that is inside an opening keeps its last picture)
    bool known = false;
    Axis column, row;
  };
  std::array<Track, 5> track_{};
  std::vector<Rect> pills_;
  int horizon_ = 0;   // how many steps the look-ahead of the last step reached
};

}  // namespace game
