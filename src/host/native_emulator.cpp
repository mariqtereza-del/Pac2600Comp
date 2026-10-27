// The console of the finished game (Stage F26): the same class Emulator as in emulator.cpp, but made of our own code (src/game): the rebuilt game
// logic, the drawing
// code, the video-chip model and the sound-chip model. No Stella, no 6502 and no original program code in it; the cartridge file is only checked (size and
// fingerprint) before the game is made; the game has all its data inside. emulator.cpp (Stella) is kept only for the reference program pacman2600_ref, which the tests compare with.
#include "emulator.hpp"

#include <algorithm>
#include <array>
#include <optional>

#include "../game/data/rom_check.hpp"
#include "../game/game.hpp"
#include "../game/layers.hpp"
#include "../game/lookahead.hpp"
#include "../game/renderer.hpp"
#include "../game/smooth.hpp"
#include "ntsc_palette.hpp"
#include "../testsupport/state_bytes.hpp"

namespace {

// The snapshot of the native program: the smoother as it stood after the frame, and the palette that turns the chip's colour values into colours.
class NativeSnapshot : public SmoothSnapshot {
public:
  game::Smoother smoother;
  void compose(double progress, std::vector<SmoothRect>& out) const override
  {
    std::vector<game::Rect> rects;
    smoother.compose(progress, game::Smoother::Options{true, false}, rects);   // glide, and only the ghost of this step (the original's flicker, Stage I9.5)
    out.clear();
    for(const game::Rect& r : rects) out.push_back({r.x, r.y, r.w, r.h, ntsc::kPalette[r.colour >> 1]});
  }
};

}  // namespace

struct Emulator::Impl {
  std::unique_ptr<game::Game> game;
  game::Layers layers;                    // the game's own picture (layers.full) and, when smooth motion is on, the two layers without the moving objects
  game::Renderer& renderer = layers.full;
  bool smooth = false;
  game::Smoother smoother;
  std::shared_ptr<const SmoothSnapshot> snapshot;
  void makeSnapshot(const game::FrameInputs& in);   // after a frame, when smooth motion is on
  // the inputs
  bool up = false, down = false, left = false, right = false, fire = false, select = false, reset = false;
  bool rightUp = false, rightDown = false, rightLeft = false, rightRight = false;  // the right joystick (player 2)
  bool leftA = false, rightA = false, blackWhite = false;
  bool softBlend = false;
  mutable testsupport::Bytes memory{};   // the state as the original's 128 bytes, for the trace and the test options (src/testsupport)
  // what the last frame made
  std::vector<std::int16_t> stereo;
  const std::array<std::uint8_t, game::TiaVideo::kWidth * game::TiaVideo::kHeight>* picture = nullptr;
  // the picture with the phosphor blend (what the screen shows when it is on)
  std::array<std::uint32_t, game::TiaVideo::kWidth * game::TiaVideo::kHeight> glow{};
  std::array<std::uint32_t, game::TiaVideo::kWidth * game::TiaVideo::kHeight> shown{};
};

Emulator::Emulator() : impl_(std::make_unique<Impl>()) {}

Emulator::~Emulator() = default;

std::string Emulator::load(const std::string&, const std::vector<std::uint8_t>& romBytes)
{
  // The ROM is only a check (D-009): the game is not made unless the player has the right cartridge file. No byte of it reaches the game.
  const game::RomStatus status = game::checkRom(romBytes.data(), romBytes.size());
  if(status != game::RomStatus::Ok) return game::describe(status);
  impl_->game = std::make_unique<game::Game>();
  impl_->game->setPicture(&impl_->renderer);
  impl_->game->trackObjects(false);
  (void)game::spriteColumns();   // made once, about 0.1 s: now, so that switching smooth motion on in the middle of a game does not stall the game thread
  return {};
}

// The power-on memory makes no difference to this game: its start-up clears the whole page (T-D14-3).
void Emulator::randomisePowerOnRam(std::uint32_t) {}

// Stage I9.4: after a frame, the picture without the moving objects, the next four frames with the controls held, and the smoother brought up to date.
void Emulator::Impl::makeSnapshot(const game::FrameInputs& in)
{
  Impl& m = *this;
  const auto& background = m.layers.background.finishPicture(m.game->lastFrameLines());
  const auto& bare = m.layers.bare.finishPicture(m.game->lastFrameLines());
  const std::vector<game::ObjectsView> ahead = game::lookAhead(*m.game, m.layers.full, in, 4);
  m.smoother.step(m.game->objects(), ahead, background, bare);
  auto next = std::make_shared<NativeSnapshot>();
  next->smoother = m.smoother;
  next->background.resize(background.size());
  for(std::size_t i = 0; i < background.size(); ++i) next->background[i] = ntsc::kPalette[background[i] >> 1];
  m.snapshot = std::move(next);
}

void Emulator::runFrame()
{
  Impl& m = *impl_;
  game::FrameInputs in;
  in.console.fire = m.fire;
  in.console.select = m.select;
  in.console.reset = m.reset;
  in.console.leftDifficultyA = m.leftA;
  in.console.rightDifficultyA = m.rightA;
  in.console.colour = !m.blackWhite;
  // Up and down together give down, left and right together give right: what the reference emulator did with such input, so that recordings replay
  // the same (T-F25-1).
  bool u = m.up, d = m.down, l = m.left, r = m.right;
  if(u && d) u = false;
  if(l && r) l = false;
  std::uint8_t swcha = 0xFF;
  if(u) swcha &= static_cast<std::uint8_t>(~0x10);
  if(d) swcha &= static_cast<std::uint8_t>(~0x20);
  if(l) swcha &= static_cast<std::uint8_t>(~0x40);
  if(r) swcha &= static_cast<std::uint8_t>(~0x80);
  // The right joystick is the lower half of the port: up 1, down 2, left 4, right 8 (the same opposite-direction rule).
  bool ru = m.rightUp, rd = m.rightDown, rl = m.rightLeft, rr = m.rightRight;
  if(ru && rd) ru = false;
  if(rl && rr) rl = false;
  if(ru) swcha &= static_cast<std::uint8_t>(~0x01);
  if(rd) swcha &= static_cast<std::uint8_t>(~0x02);
  if(rl) swcha &= static_cast<std::uint8_t>(~0x04);
  if(rr) swcha &= static_cast<std::uint8_t>(~0x08);
  in.frame.swcha = swcha;
  if(m.smooth) m.game->runFrame(in, m.layers);   // the logic's writes reach all three layers
  else m.game->runFrame(in, m.renderer);
  m.picture = &m.renderer.finishPicture(m.game->lastFrameLines());
  if(m.smooth) m.makeSnapshot(in);
  // the sound handed over (mono) as left and right
  const std::vector<std::int16_t>& mono = m.renderer.sound();
  m.stereo.resize(mono.size() * 2);
  for(std::size_t i = 0; i < mono.size(); ++i) m.stereo[2 * i] = m.stereo[2 * i + 1] = mono[i];
  // the picture as it is shown, with the glow of the phosphor if switched on (the larger of the new colour and 60 % of the one shown before, per channel)
  for(std::size_t i = 0; i < m.shown.size(); ++i) {
    const std::uint32_t c = ntsc::kPalette[(*m.picture)[i] >> 1];
    if(!m.softBlend) { m.shown[i] = c; m.glow[i] = c; continue; }
    std::uint32_t out = 0;
    for(int shift = 16; shift >= 0; shift -= 8) {
      const std::uint8_t cur = static_cast<std::uint8_t>(c >> shift);
      const std::uint8_t old = static_cast<std::uint8_t>(static_cast<std::uint8_t>(m.glow[i] >> shift) * 0.6f);
      out |= static_cast<std::uint32_t>(std::max(cur, old)) << shift;
    }
    m.glow[i] = out;
    m.shown[i] = out;
  }
}

const std::int16_t* Emulator::audio() const { return impl_->stereo.data(); }

int Emulator::audioFrames() const { return static_cast<int>(impl_->stereo.size() / 2); }

int Emulator::audioRate() const { return 31440; }

void Emulator::setJoystick(bool up, bool down, bool left, bool right, bool fire)
{
  impl_->up = up; impl_->down = down; impl_->left = left; impl_->right = right; impl_->fire = fire;
}

void Emulator::setRightJoystick(bool up, bool down, bool left, bool right)
{
  impl_->rightUp = up; impl_->rightDown = down; impl_->rightLeft = left; impl_->rightRight = right;
}

void Emulator::setConsoleSelect(bool pressed) { impl_->select = pressed; }

void Emulator::setDifficulty(bool leftA, bool rightA) { impl_->leftA = leftA; impl_->rightA = rightA; }

void Emulator::setBlackWhite(bool blackWhite) { impl_->blackWhite = blackWhite; }

// SWCHB as the game reads it: bit 0 Reset and bit 1 Select are 0 while pressed, bit 3 is 1 for colour, bits 6 and 7 are 1 for difficulty A; bits 2, 4, 5 are 1.
std::uint8_t Emulator::consoleSwitches() const
{
  std::uint8_t v = 0x3F;
  if(impl_->reset) v &= static_cast<std::uint8_t>(~0x01);
  if(impl_->select) v &= static_cast<std::uint8_t>(~0x02);
  if(impl_->blackWhite) v &= static_cast<std::uint8_t>(~0x08);
  if(impl_->leftA) v |= 0x40;
  if(impl_->rightA) v |= 0x80;
  return v;
}

void Emulator::setSoftBlend(bool on) { impl_->softBlend = on; }

// Smooth motion (Stage I9.4). Switched on between frames: the two extra layers start as copies of the game's own, which has the places of the bonus item that
// the power-on set-up gave the chip once, and from then on get everything the game writes to the chip.
void Emulator::setSmooth(bool on)
{
  Impl& m = *impl_;
  if(on == m.smooth || !m.game) return;
  m.smooth = on;
  m.snapshot.reset();
  m.game->trackObjects(on);
  if(on) {
    m.layers.enable(*m.game);
    m.smoother = game::Smoother();
  } else {
    m.game->setBackgroundPicture(nullptr);
    m.game->setBarePicture(nullptr);
  }
}

bool Emulator::smoothAvailable() const { return true; }

std::shared_ptr<const SmoothSnapshot> Emulator::smoothSnapshot() const { return impl_->snapshot; }

void Emulator::setConsoleReset(bool pressed) { impl_->reset = pressed; }

int Emulator::height() const { return game::TiaVideo::kHeight; }

const std::uint8_t* Emulator::indexed() const { return impl_->picture ? impl_->picture->data() : nullptr; }

std::vector<std::uint32_t> Emulator::colours() const
{
  return std::vector<std::uint32_t>(impl_->shown.begin(), impl_->shown.end());
}

void Emulator::pokeRam(int address, std::uint8_t value)
{
  if(address >= 0x80 && address <= 0xFF && impl_->game) testsupport::byteAt(impl_->game->state(), address) = value;
}

const std::uint8_t* Emulator::ram() const { impl_->memory = testsupport::toBytes(impl_->game->state()); return impl_->memory.data(); }

std::uint64_t Emulator::pictureFingerprint() const
{
  const std::uint8_t* data = indexed();
  std::uint64_t h = 1469598103934665603ULL;
  for(std::size_t i = 0, n = static_cast<std::size_t>(height()) * kWidth; i < n; ++i) { h ^= data[i]; h *= 1099511628211ULL; }
  return h;
}
