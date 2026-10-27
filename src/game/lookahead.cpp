#include "lookahead.hpp"

namespace game {

std::vector<ObjectsView> lookAhead(const Game& game, const Renderer& renderer, const FrameInputs& in, int steps) {
  std::vector<ObjectsView> views;
  Game copy = game;           // the state, the schedules and the notes of the last frame
  Renderer chip = renderer;   // the chip as it stands: the collision flags of a frame come from its picture
  copy.setPicture(&chip);
  copy.setBackgroundPicture(nullptr);
  copy.setBarePicture(nullptr);
  copy.trackObjects(true);
  for (int i = 0; i < steps; ++i) {
    if (!copy.runFrame(in, chip)) break;
    chip.finishPicture(copy.lastFrameLines());
    views.push_back(copy.objects());
  }
  return views;
}

}  // namespace game
