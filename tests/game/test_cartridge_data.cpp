// Tests of the data loader (Stage F22): test_cartridge_data <rom file>. The ROM is only read; nothing is written or copied to disk.
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <vector>

#include "../../src/game/data/cartridge_data.hpp"

static int failed = 0;
static void check(bool ok, const char* what) {
  std::printf("%s: %s\n", ok ? "ok  " : "FAIL", what);
  if (!ok) ++failed;
}

int main(int argc, char** argv) {
  if (argc < 2) { std::fprintf(stderr, "usage: test_cartridge_data <rom>\n"); return 2; }
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

  CartridgeData d;
  check(!d.loaded(), "nothing is loaded at first");
  check(d.load(rom.data(), rom.size()) == LoadStatus::Ok && d.loaded(), "the right ROM loads");
  bool same = true;
  for (int a = 0xF000; a <= 0xFFFF; ++a) same = same && d.view().at(static_cast<uint16_t>(a)) == rom[a - 0xF000];
  check(same, "the view gives every byte $F000-$FFFF as in the file");
  rom[1234] ^= 1;
  check(d.view().at(0xF000 + 1234) != rom[1234], "the loader keeps its own copy (a later change to the buffer does not reach it)");
  check(d.load(rom.data(), rom.size()) == LoadStatus::UnknownFingerprint && !d.loaded(), "one changed bit: refused (fingerprint)");
  check(d.view().at(0xF000) == 0 && d.view().at(0xFFFF) == 0, "after a refusal the view holds no data");
  rom[1234] ^= 1;
  check(d.load(rom.data(), rom.size() - 1) == LoadStatus::WrongSize, "one byte short: refused (size)");
  std::vector<uint8_t> big(rom);
  big.push_back(0);
  check(d.load(big.data(), big.size()) == LoadStatus::WrongSize, "one byte too long: refused (size)");
  check(d.load(rom.data(), 0) == LoadStatus::WrongSize, "empty: refused (size)");
  check(d.loadFile(argv[1]) == LoadStatus::Ok && d.loaded(), "loadFile reads the right ROM");
  check(d.loadFile("no\\such\\file.a26") == LoadStatus::Unreadable && !d.loaded(), "loadFile: a missing file is reported");
  check(d.loadFile(argv[0]) == LoadStatus::WrongSize, "loadFile: some other file (this program) is refused");
  std::printf("%s\n", failed ? "result: FAILED" : "result: OK");
  return failed ? 1 : 0;
}
