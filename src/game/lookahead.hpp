// Looking ahead (Stage I9.2, D-015): how the next frames will look if the controls stay as they are now. The game is copied, with the copy of the renderer that
// gives it its collision flags, and played on for a few frames; the copy makes no sound and draws into nothing the host shows. The real game is never touched.
// The smooth presenter (I9.3) glides each object towards the place it will have when its next change comes, so that nothing is ever shown later than the
// original shows it.
#pragma once
#include <vector>

#include "game.hpp"
#include "objects_view.hpp"
#include "renderer.hpp"

namespace game {

// The descriptions of the objects in each of the next `steps` frames (element 0 is the frame after the one just played), made with `in` held for all of
// them. Fewer are given if the game reaches the one path of the original that no recording runs.
std::vector<ObjectsView> lookAhead(const Game& game, const Renderer& renderer, const FrameInputs& in, int steps);

}  // namespace game
