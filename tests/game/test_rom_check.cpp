// Tests of the ROM check (Stage G5c): test_rom_check <rom file>. The ROM is only read; nothing is written or copied to disk.
// The check says yes or no and keeps nothing: the game gets no byte of the ROM (its data is in tables.hpp).
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <vector>

#include "../../src/game/data/rom_check.hpp"

static int failed = 0;
static void check(bool ok, const char* what) {
  std::printf("%s: %s\n", ok ? "ok  " : "FAIL", what);
  if (!ok) ++failed;
}

int main(int argc, char** argv) {
  if (argc < 2) { std::fprintf(stderr, "usage: test_rom_check <rom>\n"); return 2; }
  using namespace game;
  const char* abc = "abc";
  check(sha1Hex(reinterpret_cast<const uint8_t*>(abc), 3) == "a9993e364706816aba3e25717850c26c9cd0d89d", "SHA-1 of \"abc\" (standard test value)");
  check(sha1Hex(nullptr, 0) == "da39a3ee5e6b4b0d3255bfef95601890afd80709", "SHA-1 of nothing (standard test value)");
  std::vector<uint8_t> million(1000000, 'a');
  check(sha1Hex(million.data(), million.size()) == "34aa973cd4c4daa4f61eeb2bdbad27316534016f", "SHA-1 of a million letters a (standard test value)");

  std::ifstream in(argv[1], std::ios::binary);
  std::vector<char> raw((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  std::vector<uint8_t> rom(raw.begin(), raw.end());
  check(rom.size() == 4096, "the ROM file is 4096 bytes");

  check(checkRom(rom.data(), rom.size()) == RomStatus::Ok, "the right ROM is accepted");
  rom[1234] ^= 1;
  check(checkRom(rom.data(), rom.size()) == RomStatus::UnknownFingerprint, "one changed bit: refused (fingerprint)");
  rom[1234] ^= 1;
  check(checkRom(rom.data(), rom.size() - 1) == RomStatus::WrongSize, "one byte short: refused (size)");
  std::vector<uint8_t> big(rom);
  big.push_back(0);
  check(checkRom(big.data(), big.size()) == RomStatus::WrongSize, "one byte too long: refused (size)");
  check(checkRom(rom.data(), 0) == RomStatus::WrongSize, "empty: refused (size)");
  check(checkRomFile(argv[1]) == RomStatus::Ok, "checkRomFile accepts the right ROM");
  check(checkRomFile("no/such/file.a26") == RomStatus::Unreadable, "checkRomFile: a missing file is reported");
  check(checkRomFile(argv[0]) == RomStatus::WrongSize, "checkRomFile: some other file (this program) is refused");
  check(std::strlen(describe(RomStatus::Ok)) > 0 && std::strlen(describe(RomStatus::UnknownFingerprint)) > 0, "every answer has a plain message");
  std::printf("%s\n", failed ? "result: FAILED" : "result: OK");
  return failed ? 1 : 0;
}
