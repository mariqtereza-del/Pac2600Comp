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

  // Fills the 128 bytes of RIOT RAM with pseudo-random bytes from this seed, as a real console's memory is at power-on.
  // Only before the first runFrame (Stella's libretro wrapper otherwise starts it with zeros). Same seed = same bytes.
  void randomisePowerOnRam(std::uint32_t seed);

  void runFrame();  // one whole TV frame; the console's Reset switch is as last set by setConsoleReset

  // Sound made by the last runFrame (Stage D8): 16-bit stereo, left and right interleaved, audioFrames() pairs.
  // For this ROM that is 31,440 Hz and about 524 pairs per frame (docs\SOUND.md). Valid until the next runFrame.
  const std::int16_t* audio() const;
  int audioFrames() const;
  int audioRate() const;

  // The left joystick and the console switches (Stage D10). Each call sets the state for the next frames; the
  // difficulty switches are real switches (true = position A), the others are held while true.
  void setJoystick(bool up, bool down, bool left, bool right, bool fire);
  // The right joystick (Stage I6, T-I6-1): the game reads it for player 2 in a two-player game. Directions only; held while true.
  void setRightJoystick(bool up, bool down, bool left, bool right);
  void setConsoleSelect(bool pressed);
  void setDifficulty(bool leftA, bool rightA);
  void setBlackWhite(bool blackWhite);  // the Colour / B&W switch
  std::uint8_t consoleSwitches() const;  // the console switch register (SWCHB) as the game reads it

  // Stella's "phosphor" glow: each picture is blended 60 % with the one before (softer ghost flicker, trails behind movers).
  // Off from the start (D-007: authentic, as on a real TV); only the colours() picture changes, never indexed() or the fingerprint.
  void setSoftBlend(bool on);

  void setConsoleReset(bool pressed);  // the console's Reset switch (starts a game); held until set again

  int height() const;                       // picture rows (228 for this ROM)
  const std::uint8_t* indexed() const;      // height() x kWidth palette indexes, exactly what the TIA drew
  std::vector<std::uint32_t> colours() const;  // the same picture as 0xAARRGGBB, kWidth wide, as Stella shows it
  const std::uint8_t* ram() const;          // the 128 bytes of RIOT RAM
  // TEST ONLY (Stage E8, DECISIONS.md T-E8-1): writes one byte of RIOT RAM (address 0x80-0xFF). Used by `--poke` in headless
  // tests that need a state the game cannot reach in reasonable time (a high score). Never used in play.
  void pokeRam(int address, std::uint8_t value);

  // FNV-1a over the indexed picture; the same fingerprint the Phase C tools record (reference\c3).
  std::uint64_t pictureFingerprint() const;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
