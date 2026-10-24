#include "emulator.hpp"

#include <cstring>

#include "StellaLIBRETRO.hxx"
#include "Console.hxx"
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
