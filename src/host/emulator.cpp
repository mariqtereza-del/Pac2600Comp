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
  impl_->core.setVideoPhosphor(1, 60);  // Stella's game list switches the glow on for this ROM; the port starts without it (D-007)
  return {};
}

void Emulator::randomisePowerOnRam(std::uint32_t seed)
{
  std::uint32_t x = seed * 2654435761u + 0x9E3779B9u;  // xorshift32; a zero state would stay zero
  if(x == 0) x = 1;
  std::uint8_t* ram = impl_->core.getRAM();
  for(int i = 0; i < 128; ++i) {
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    ram[i] = static_cast<std::uint8_t>(x >> 11);
  }
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

void Emulator::setRightJoystick(bool up, bool down, bool left, bool right)
{
  impl_->core.setInputEvent(Event::RightJoystickUp, up ? 1 : 0);
  impl_->core.setInputEvent(Event::RightJoystickDown, down ? 1 : 0);
  impl_->core.setInputEvent(Event::RightJoystickLeft, left ? 1 : 0);
  impl_->core.setInputEvent(Event::RightJoystickRight, right ? 1 : 0);
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

void Emulator::setSoftBlend(bool on) { impl_->core.setVideoPhosphor(on ? 2 : 1, 60); }

// Smooth motion needs the rebuilt game (Stage I9.4); this program, the reference, has none of it.
void Emulator::setSmooth(bool) {}
bool Emulator::smoothAvailable() const { return false; }
std::shared_ptr<const SmoothSnapshot> Emulator::smoothSnapshot() const { return {}; }

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

void Emulator::pokeRam(int address, std::uint8_t value)
{
  if(address >= 0x80 && address <= 0xFF) impl_->core.getRAM()[address - 0x80] = value;
}

const std::uint8_t* Emulator::ram() const { return impl_->core.osystem().console().system().m6532().getRAM(); }

std::uint64_t Emulator::pictureFingerprint() const
{
  const std::uint8_t* data = indexed();
  std::uint64_t h = 1469598103934665603ULL;
  for(std::size_t i = 0, n = static_cast<std::size_t>(height()) * kWidth; i < n; ++i) { h ^= data[i]; h *= 1099511628211ULL; }
  return h;
}
