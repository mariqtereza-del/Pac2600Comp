#include "emulator.hpp"

#include <cstring>

#include "StellaLIBRETRO.hxx"
#include "Console.hxx"
#include "Event.hxx"
#include "Switches.hxx"
#include "OSystem.hxx"
#include "System.hxx"
#include "M6532.hxx"
#include "TIA.hxx"

namespace {
StellaLIBRETRO* activeCore = nullptr;  // Stella's file layer asks for the ROM through libretro_read_rom below
}

// Stella's FSNodeLIBRETRO reads the ROM through this function (same definition as in libretro.cxx and the Phase C tools).
uInt32 libretro_read_rom(void* data)
{
  std::memcpy(data, activeCore->getROM(), activeCore->getROMSize());
  return activeCore->getROMSize();
}

struct Emulator::Impl {
  StellaLIBRETRO core;
};

Emulator::Emulator() : impl_(std::make_unique<Impl>()) {}

Emulator::~Emulator()
{
  if(activeCore == &impl_->core) activeCore = nullptr;
  impl_->core.destroy();
}

std::string Emulator::load(const std::string& romName, const std::vector<std::uint8_t>& romBytes)
{
  if(romBytes.empty() || romBytes.size() > impl_->core.getROMMax()) return "The ROM has an unusable size.";
  activeCore = &impl_->core;
  impl_->core.setROM(romName.c_str(), romBytes.data(), romBytes.size());
  if(!impl_->core.create(false)) return "Stella could not start the console for this ROM.";
  return {};
}

void Emulator::runFrame() { impl_->core.runFrame(); }

const std::int16_t* Emulator::audio() const { return impl_->core.getAudioBuffer(); }

int Emulator::audioFrames() const { return static_cast<int>(impl_->core.getAudioSize()); }

int Emulator::audioRate() const { return static_cast<int>(impl_->core.getAudioRate() + 0.5f); }

void Emulator::setJoystick(bool up, bool down, bool left, bool right, bool fire)
{
  impl_->core.setInputEvent(Event::LeftJoystickUp, up ? 1 : 0);
  impl_->core.setInputEvent(Event::LeftJoystickDown, down ? 1 : 0);
  impl_->core.setInputEvent(Event::LeftJoystickLeft, left ? 1 : 0);
  impl_->core.setInputEvent(Event::LeftJoystickRight, right ? 1 : 0);
  impl_->core.setInputEvent(Event::LeftJoystickFire, fire ? 1 : 0);
}

void Emulator::setConsoleSelect(bool pressed) { impl_->core.setInputEvent(Event::ConsoleSelect, pressed ? 1 : 0); }

void Emulator::setDifficulty(bool leftA, bool rightA)
{
  impl_->core.setInputEvent(Event::ConsoleLeftDiffA, leftA ? 1 : 0);
  impl_->core.setInputEvent(Event::ConsoleLeftDiffB, leftA ? 0 : 1);
  impl_->core.setInputEvent(Event::ConsoleRightDiffA, rightA ? 1 : 0);
  impl_->core.setInputEvent(Event::ConsoleRightDiffB, rightA ? 0 : 1);
}

void Emulator::setBlackWhite(bool blackWhite)
{
  impl_->core.setInputEvent(Event::ConsoleColor, blackWhite ? 0 : 1);
  impl_->core.setInputEvent(Event::ConsoleBlackWhite, blackWhite ? 1 : 0);
}

std::uint8_t Emulator::consoleSwitches() const { return impl_->core.osystem().console().switches().read(); }

void Emulator::setConsoleReset(bool pressed) { impl_->core.setInputEvent(Event::ConsoleReset, pressed ? 1 : 0); }

int Emulator::height() const { return static_cast<int>(impl_->core.osystem().console().tia().height()); }

const std::uint8_t* Emulator::indexed() const { return impl_->core.osystem().console().tia().frameBuffer(); }

std::vector<std::uint32_t> Emulator::colours() const
{
  const int h = height();
  std::vector<std::uint32_t> out(static_cast<std::size_t>(h) * kWidth);
  const auto* src = static_cast<const std::uint32_t*>(impl_->core.getVideoBuffer());
  const std::size_t pitch = impl_->core.getVideoPitch() / 4;  // pixels per buffer row
  for(int y = 0; y < h; ++y)
    std::memcpy(&out[static_cast<std::size_t>(y) * kWidth], src + y * pitch, kWidth * sizeof(std::uint32_t));
  return out;
}

const std::uint8_t* Emulator::ram() const { return impl_->core.osystem().console().system().m6532().getRAM(); }

std::uint64_t Emulator::pictureFingerprint() const
{
  const std::uint8_t* data = indexed();
  std::uint64_t h = 1469598103934665603ULL;
  for(std::size_t i = 0, n = static_cast<std::size_t>(height()) * kWidth; i < n; ++i) { h ^= data[i]; h *= 1099511628211ULL; }
  return h;
}
