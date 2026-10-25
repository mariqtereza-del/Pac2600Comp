// The video chip of the console as a small model (Stage F23.1): it is given the writes the program makes to the chip, each with the scanline and
// the colour clock at which it happened, and makes the 160 x 228 picture of the television window from them. It is the part of the renderer
// that knows how the chip draws (the playfield, the two players, the two missiles and the ball, their colours and priorities, the delayed
// graphics registers, the horizontal motion). The part that knows WHAT the game writes (the drawing code, F23.3) comes later.
// The picture is checked against the emulator's pictures (tools/f23_tia.py).
#pragma once
#include <array>
#include <cstdint>
#include <vector>

namespace game {

// When a write takes effect, in colour clocks after the end of the instruction that made it, and where the strobes put the objects. These are
// facts about the chip, found by comparing with the emulator (DECISIONS.md T-F23-1); they can be changed here for experiments only.
struct TiaTiming {
  int playfield = 0;   // PF0, PF1, PF2 (the game's pictures are the same for -8 to +6)
  int player = 2;      // GRP0, GRP1 (the same for 0 to +4)
  int enable = 0;      // ENAM0, ENAM1, ENABL (the same for -12 to +28)
  int other = 0;       // colours, CTRLPF, NUSIZ, REFP, VDEL, VBLANK (the same for -8 to +32)
  int playerPos = 5;   // first pixel column of a player = clock at the end of the strobe - 68 + playerPos
  int missilePos = 4;
  int ballPos = 4;
};

class TiaVideo {
 public:
  static constexpr int kWidth = 160;
  static constexpr int kHeight = 228;       // the window of the television picture
  static constexpr int kFirstLine = 23;     // scanline (counted from the end of the vertical sync) of the first picture row
  static constexpr int kClocks = 228;       // colour clocks in a scanline; the first 68 are the horizontal blank

  explicit TiaVideo(const TiaTiming& timing = TiaTiming()) : timing_(timing) {}

  // A write made at scanline `line` (0 at the end of the vertical sync), colour clock `clock` (of the end of the instruction). When the line
  // number is lower than the one before, a new picture has begun and the one before is finished.
  void write(int line, int clock, uint8_t reg, uint8_t value);
  // Finishes the picture being made (also done by write when a new picture begins).
  void endPicture();
  // Draws what the writes so far have drawn (the rows up to the latest write); the collision latches are then up to date. Later writes go on from there.
  void renderDrawn();
  // The collision latches since the last CXCLR: player 0 touched player 1, missile 1 touched player 1, the ball touched player 1.
  bool player0TouchedPlayer1() const { return state_.cxP0P1; }
  bool missile1TouchedPlayer1() const { return state_.cxM1P1; }
  bool ballTouchedPlayer1() const { return state_.cxP1BL; }
  // The last finished picture: kHeight rows of kWidth colour values (the chip's colour numbers, as the emulator shows them).
  const std::array<uint8_t, kWidth * kHeight>& picture() const { return picture_; }
  uint64_t pictureCount() const { return count_; }

 private:
  struct Event { long when; int order; uint8_t reg; uint8_t value; int clock; };
  struct State {
    uint8_t vblank = 0, colup0 = 0, colup1 = 0, colupf = 0, colubk = 0, ctrlpf = 0;
    uint8_t nusiz[2] = {0, 0}, refp[2] = {0, 0}, pf[3] = {0, 0, 0};
    uint8_t grp[2] = {0, 0}, grpOld[2] = {0, 0};
    uint8_t vdelp[2] = {0, 0}, vdelbl = 0;
    uint8_t enam[2] = {0, 0}, enabl = 0, enablOld = 0;
    int8_t hm[5] = {0, 0, 0, 0, 0};   // P0, P1, M0, M1, BL
    int pos[5] = {0, 0, 0, 0, 0};     // first pixel column
    bool cxP0P1 = false, cxM1P1 = false, cxP1BL = false;   // collision latches, cleared by CXCLR
  };
  void apply(const Event& e, State& s, bool& hmoveBlank) const;
  void renderThrough(int lastRow);
  int delayOf(uint8_t reg) const;

  TiaTiming timing_;
  State state_;
  std::vector<Event> events_;
  int lastLine_ = 0;
  int row_ = 0;           // the next row of the picture to draw
  size_t next_ = 0;       // the next event to apply
  int order_ = 0;
  uint64_t count_ = 0;
  std::array<uint8_t, kWidth * kHeight> picture_{};
};

}  // namespace game
