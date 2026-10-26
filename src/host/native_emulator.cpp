// The console of the finished game (Stage F26): the same class Emulator as in emulator.cpp, but made of our own code (src/game): the rebuilt game logic, the drawing
// code, the video-chip model and the sound-chip model. No Stella, no 6502 and no original program code in it; the cartridge data is read through the data
// loader after the fingerprint check. emulator.cpp (Stella) is kept only for the reference program pacman2600_ref, which the tests compare with.
#include "emulator.hpp"

#include <algorithm>
#include <array>
#include <optional>

#include "../game/data/cartridge_data.hpp"
#include "../game/game.hpp"
#include "../game/renderer.hpp"
#include "ntsc_palette.hpp"
#include "../testsupport/state_bytes.hpp"

struct Emulator::Impl {
  game::CartridgeData data;
  std::optional<game::Cartridge> cart;
  std::unique_ptr<game::Game> game;
  game::Renderer renderer;
  // the inputs
  bool up = false, down = false, left = false, right = false, fire = false, select = false, reset = false;
  bool leftA = false, rightA = false, blackWhite = false;
  bool softBlend = false;
  mutable testsupport::Bytes memory{};   // the state as the original's 128 bytes, for the trace and the test options (src/testsupport)
  // what the last frame made
  std::vector<std::int16_t> stereo;
  const std::array<std::uint8_t, game::TiaVideo::kWidth * game::TiaVideo::kHeight>* picture = nullptr;
  std::array<std::uint32_t, game::TiaVideo::kWidth * game::TiaVideo::kHeight> glow{};   // the picture with the phosphor blend (what the screen shows when it is on)
  std::array<std::uint32_t, game::TiaVideo::kWidth * game::TiaVideo::kHeight> shown{};
};

Emulator::Emulator() : impl_(std::make_unique<Impl>()) {}

Emulator::~Emulator() = default;

std::string Emulator::load(const std::string&, const std::vector<std::uint8_t>& romBytes)
{
  const game::LoadStatus status = impl_->data.load(romBytes.data(), romBytes.size());
  if(status != game::LoadStatus::Ok) return game::describe(status);
  impl_->cart.emplace(impl_->data.view());
  impl_->game = std::make_unique<game::Game>(*impl_->cart);
  impl_->game->setPicture(&impl_->renderer);
  return {};
}

// The power-on memory makes no difference to this game: its start-up clears the whole page (T-D14-3).
void Emulator::randomisePowerOnRam(std::uint32_t) {}

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
  // Up and down together give down, left and right together give right: what the reference emulator did with such input, so that recordings replay the same (T-F25-1).
  bool u = m.up, d = m.down, l = m.left, r = m.right;
  if(u && d) u = false;
  if(l && r) l = false;
  std::uint8_t swcha = 0xFF;
  if(u) swcha &= static_cast<std::uint8_t>(~0x10);
  if(d) swcha &= static_cast<std::uint8_t>(~0x20);
  if(l) swcha &= static_cast<std::uint8_t>(~0x40);
  if(r) swcha &= static_cast<std::uint8_t>(~0x80);
  in.frame.swcha = swcha;
  m.game->runFrame(in, m.renderer);
  m.picture = &m.renderer.finishPicture(m.game->lastFrameLines());
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
