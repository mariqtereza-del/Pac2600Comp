// The emulator wrapper (Stage D4, DECISIONS.md T-D1-1 and T-D4-1). The only host code that includes Stella's headers;
// everything else talks to Stella through this class.
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class Emulator {
public:
  static constexpr int kWidth = 160;  // TIA pixels per line

  Emulator();
  ~Emulator();
  Emulator(const Emulator&) = delete;
  Emulator& operator=(const Emulator&) = delete;

  // Starts the console with these ROM bytes (a copy is made; the file is not touched). Empty string = ok.
  std::string load(const std::string& romName, const std::vector<std::uint8_t>& romBytes);

  void runFrame();  // one whole TV frame, no input

  int height() const;                       // picture rows (228 for this ROM)
  const std::uint8_t* indexed() const;      // height() x kWidth palette indexes, exactly what the TIA drew
  std::vector<std::uint32_t> colours() const;  // the same picture as 0xAARRGGBB, kWidth wide, as Stella shows it
  const std::uint8_t* ram() const;          // the 128 bytes of RIOT RAM

  // FNV-1a over the indexed picture; the same fingerprint the Phase C tools record (reference\c3).
  std::uint64_t pictureFingerprint() const;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
