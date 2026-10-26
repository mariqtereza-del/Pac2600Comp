// Test of the data tables (Stage G5b): test_tables <rom file>. The ROM is only read. Every byte of every embedded table must equal the ROM byte at its
// original address, the lookup by address must agree, and the totals must be the ones of docs/EMBEDDED_DATA.md (34 tables, 508 bytes).
#include <cstdio>
#include <fstream>
#include <iterator>
#include <vector>

#include "../../src/game/data/tables.hpp"

int main(int argc, char** argv) {
  if (argc < 2) { std::fprintf(stderr, "usage: test_tables <rom>\n"); return 2; }
  std::ifstream in(argv[1], std::ios::binary);
  std::vector<unsigned char> rom((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  if (rom.size() != 4096) { std::fprintf(stderr, "the ROM must have 4096 bytes\n"); return 2; }
  int checks = 0, failures = 0;
  auto check = [&](bool ok, const char* what, unsigned a) {
    ++checks;
    if (!ok) { ++failures; std::printf("FAIL  %s at $%04X\n", what, a); }
  };
  unsigned total = 0;
  for (std::size_t s = 0; s < game::data::kSegmentCount; ++s) {
    const game::data::Segment& seg = game::data::kSegments[s];
    total += seg.size;
    check(seg.address >= 0xF000 && seg.address + seg.size <= 0x10000, "table inside the cartridge window", seg.address);
    for (unsigned i = 0; i < seg.size; ++i) {
      const unsigned a = seg.address + i;
      check(seg.bytes[i] == rom[a - 0xF000], "embedded byte equals the ROM byte", a);
      check(game::data::byAddress(static_cast<uint16_t>(a)) == rom[a - 0xF000], "lookup by address equals the ROM byte", a);
    }
    if (s > 0) {
      const game::data::Segment& before = game::data::kSegments[s - 1];
      check(before.address + before.size <= seg.address, "tables are sorted and do not overlap", seg.address);
    }
  }
  check(game::data::kSegmentCount == 34, "34 tables", 0);
  check(total == 508, "508 bytes", 0);
  check(game::data::kMazeWalls.size() == 80 && game::data::kMazeWalls[0] == rom[0xFE13 - 0xF000] && game::data::kMazeWalls[79] == rom[0xFE62 - 0xF000], "the maze wall table", 0xFE13);
  std::printf("%d checks, %d failures; %zu tables, %u bytes\n", checks, failures, game::data::kSegmentCount, total);
  std::printf("result: %s\n", failures == 0 ? "OK" : "FAILED");
  return failures == 0 ? 0 : 1;
}
