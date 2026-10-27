// The picture in two layers (Stage I9.1, D-015): the picture the game makes, and the same picture without Pac-Man and the ghosts. Both renderers are given
// every write the game logic makes to the chip (so both stay in step), the drawing code's writes go to each its own (Game::setPicture,
// Game::setBackgroundPicture). The game, its memory and its sound are the same with or without the second layer; only the first layer's picture and the
// sound of `full` are ever used for play. The smooth presenter (Stage I9.3) draws the moving objects on `background`.
#pragma once
#include "game.hpp"
#include "renderer.hpp"

namespace game {

class Layers : public TiaChip {
 public:
  Renderer full;         // the picture of the game, as the original makes it; also the game's collision flags and its sound
  Renderer background;   // the same without Pac-Man and the ghosts
  Renderer bare;         // the same without the power pills either (they are above Pac-Man; the difference to `background` is where they are)

  void write(uint8_t reg, uint8_t value) override { full.write(reg, value); background.write(reg, value); bare.write(reg, value); }
  void position(int line) override { full.position(line); background.position(line); bare.position(line); }
  void timeBase(int line, int clock) override { full.timeBase(line, clock); background.timeBase(line, clock); bare.timeBase(line, clock); }
  void writeAtCycle(long cycles, uint8_t reg, uint8_t value) override {
    full.writeAtCycle(cycles, reg, value);
    background.writeAtCycle(cycles, reg, value);
    bare.writeAtCycle(cycles, reg, value);
  }

  // Switches the two extra layers on in the middle of a game, between frames: they start as copies of the game's own layer (the places of the bonus item are
  // written to the chip once, at power-on, and the copy has them) and from then on get everything the game writes.
  void enable(Game& game) {
    background = full;
    bare = full;
    attach(game);
  }

  // Connects the layers to a game.
  void attach(Game& game) { game.setPicture(&full); game.setBackgroundPicture(&background); game.setBarePicture(&bare); }
};

}  // namespace game
