// The renderer (Stage F23.3): the game's chip writes become a picture. It is the chip the game sees (TiaChip, for the logic parts, which have no exact
// timing and are placed at fixed scanlines) and the sink of the drawing code (TimedChip, exact scanline and colour clock); both feed the video-chip
// model of F23.1. After each game frame, finishPicture() closes the picture and gives it.
#pragma once
#include "kernel.hpp"
#include "chips.hpp"
#include "data/cartridge.hpp"
#include "state/state.hpp"
#include "tia_audio.hpp"
#include "tia_video.hpp"

namespace game {

class Renderer : public TiaChip, public TimedChip {
 public:
  // The logic parts' writes (TiaChip): placed at the line given by position().
  void write(uint8_t reg, uint8_t value) override;
  void position(int line) override;
  void timeBase(int line, int clock) override;
  void writeAtCycle(long cycles, uint8_t reg, uint8_t value) override;
  // The drawing code's writes (TimedChip).
  void write(const ChipWrite& w) override;
  bool collisions(bool& ghostTouched, bool& bonusTouched) override {
    video_.renderDrawn();
    ghostTouched = video_.player0TouchedPlayer1();
    bonusTouched = video_.missile1TouchedPlayer1() && video_.ballTouchedPlayer1();
    return true;
  }
  int skewCycles = 0;   // for tests only: added to the cycle count of every write of the bottom logic (a control: the sound must then differ)
  bool trace = false;   // for looking at differences: prints every write to the standard output

  // Closes the picture of the frame just played and returns it (228 rows of 160 colour values).
  // `lines` is the number of scanlines the frame had (Game::lastFrameLines()).
  const std::array<uint8_t, TiaVideo::kWidth * TiaVideo::kHeight>& finishPicture(int lines = TiaAudio::kLines) {
    video_.endPicture();
    audio_.endFrame(lines);
    return video_.picture();
  }
  // The sound handed over after the frame just played (call after finishPicture): whole fragments of 524 mono samples, none after the first frame.
  const std::vector<int16_t>& sound() const { return audio_.output(); }

 private:
  TiaVideo video_;
  TiaAudio audio_;
  long frames_ = 0;
  bool timed_ = false;     // the writes follow the cycle count of the bottom logic from (baseLine_, baseClock_)
  int baseLine_ = 0, baseClock_ = 0;
  int line_ = 0;
  bool powerOn_ = false;
};

}  // namespace game
