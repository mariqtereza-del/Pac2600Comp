// The renderer (Stage F23.3): the game's chip writes become a picture. It is the chip the game sees (TiaChip, for the logic parts, which have no exact
// timing and are placed at fixed scanlines) and the sink of the drawing code (TimedChip, exact scanline and colour clock); both feed the video-chip
// model of F23.1. After each game frame, finishPicture() closes the picture and gives it.
#pragma once
#include "kernel.hpp"
#include "routines.hpp"
#include "tia_video.hpp"

namespace game {

class Renderer : public TiaChip, public TimedChip {
 public:
  // The logic parts' writes (TiaChip): placed at the line given by position().
  void write(uint8_t reg, uint8_t value) override;
  void position(int line) override;
  // The drawing code's writes (TimedChip).
  void write(const ChipWrite& w) override;
  bool trace = false;   // for looking at differences: prints every write to the standard output

  // Closes the picture of the frame just played and returns it (228 rows of 160 colour values).
  const std::array<uint8_t, TiaVideo::kWidth * TiaVideo::kHeight>& finishPicture() {
    video_.endPicture();
    return video_.picture();
  }

 private:
  TiaVideo video_;
  int line_ = 0;
  bool powerOn_ = false;
};

}  // namespace game
