// lockstep: runs the original program (small 6502 core, src/lockstep/machine.*), optionally with natively written routines replacing the
// original ones, over a recorded game, and compares all 128 memory bytes with the golden trace after every frame (Stage F20).
//
// Usage: lockstep <rom> <inputs_file> <trace_file> [--oracle <file>] [--poke <frame>:<hex addr>:<hex value>]... [--native <name>]... [--all]
//   --oracle  per frame the collision flags the chip would give: lines "frame bonus ghost" (0/1), made by tools\f20_lockstep.py
//   --native  use a natively written routine instead of the original: rng, rng-broken (test fixtures of Stage F20), clear-scores (F21.1)
//   --verify  also run every call of a native routine through the original code on a copy and compare memory and registers
//   --all     do not stop at the first different frame; count them
// Output: "result: IDENTICAL (n frames)" or the first difference: frame, byte, name, golden value, our value, the routine that wrote it last.
// Exit code 0 = identical, 1 = different, 2 = bad arguments or files.
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "machine.hpp"
#include "natives.hpp"

using lockstep::Machine;

int main(int argc, char** argv) {
  if (argc < 4) {
    std::fprintf(stderr, "usage: lockstep <rom> <inputs> <trace> [--oracle f] [--poke f:a:v]... [--native name]... [--all]\n");
    return 2;
  }
  std::ifstream romFile(argv[1], std::ios::binary);
  std::vector<char> rom((std::istreambuf_iterator<char>(romFile)), std::istreambuf_iterator<char>());
  if (rom.size() != 4096) { std::fprintf(stderr, "the ROM must be 4096 bytes (%zu read)\n", rom.size()); return 2; }

  int frames = 0;
  std::vector<std::pair<int, unsigned>> changes;
  {
    std::ifstream in(argv[2]);
    if (!in) { std::fprintf(stderr, "cannot read %s\n", argv[2]); return 2; }
    for (std::string line; std::getline(in, line);) {
      if (!line.empty() && line.back() == '\r') line.pop_back();
      if (line.rfind("frames=", 0) == 0) { frames = std::stoi(line.substr(7)); continue; }
      if (line.empty() || line[0] == '#' || line.rfind("PacMan2600", 0) == 0 || line.rfind("rom_sha1=", 0) == 0) continue;
      std::istringstream ls(line);
      int f; unsigned b;
      if (ls >> f >> b) changes.emplace_back(f, b);
    }
  }
  std::vector<std::string> golden;
  {
    std::ifstream in(argv[3]);
    if (!in) { std::fprintf(stderr, "cannot read %s\n", argv[3]); return 2; }
    std::string line;
    std::getline(in, line); std::getline(in, line); std::getline(in, line);
    while (std::getline(in, line)) {
      std::vector<std::string> cells;
      std::istringstream ls(line);
      for (std::string c; std::getline(ls, c, ',');) cells.push_back(c);
      if (cells.size() >= 6) golden.push_back(cells[5]);
    }
  }
  std::map<int, std::pair<bool, bool>> oracle;
  std::vector<std::array<int, 3>> pokes;
  std::vector<std::string> natives;
  bool all = false, verify = false;
  for (int i = 4; i < argc; ++i) {
    const std::string a = argv[i];
    if (a == "--oracle" && i + 1 < argc) {
      std::ifstream in(argv[++i]);
      if (!in) { std::fprintf(stderr, "cannot read the oracle\n"); return 2; }
      int f, b, g;
      while (in >> f >> b >> g) oracle[f] = {b != 0, g != 0};
    } else if (a == "--poke" && i + 1 < argc) {
      int pf = 0, pa = 0, pv = 0;
      if (std::sscanf(argv[++i], "%d:%x:%x", &pf, &pa, &pv) == 3) pokes.push_back({pf, pa, pv});
    } else if (a == "--native" && i + 1 < argc) {
      natives.push_back(argv[++i]);
    } else if (a == "--all") {
      all = true;
    } else if (a == "--verify") {
      verify = true;
    }
  }

  Machine m(reinterpret_cast<const uint8_t*>(rom.data()));
  for (const auto& n : natives)
    if (!lockstep::registerNativeByName(m, n)) { std::fprintf(stderr, "unknown native routine '%s'\n", n.c_str()); return 2; }

  m.setVerify(verify);
  const int total = std::min<int>(frames > 0 ? frames : static_cast<int>(golden.size()), static_cast<int>(golden.size()));
  unsigned bits = 0;
  size_t next = 0;
  int different = 0;
  int firstFrame = -1;
  for (int f = 0; f < total; ++f) {
    while (next < changes.size() && changes[next].first <= f) bits = changes[next++].second;
    m.setInputs(bits);
    auto o = oracle.find(f);
    m.setOracle(o != oracle.end() && o->second.first, o != oracle.end() && o->second.second);
    for (const auto& pk : pokes)
      if (pk[0] == f && pk[1] >= 0x80 && pk[1] <= 0xFF) m.ramMutable()[pk[1] - 0x80] = static_cast<uint8_t>(pk[2]);
    if (!m.runFrame()) {
      std::printf("frame %d: the original code did something this program does not know (pc $%04X) or ran too long\n", f, m.pc);
      return 1;
    }
    std::string ours;
    char buf[3];
    for (int i = 0; i < 128; ++i) { std::snprintf(buf, sizeof buf, "%02x", m.ram()[i]); ours += buf; }
    if (ours != golden[f]) {
      ++different;
      if (firstFrame < 0) {
        firstFrame = f;
        bool first = true;
        for (int i = 0; i < 128; ++i) {
          const unsigned want = std::stoul(golden[f].substr(2 * i, 2), nullptr, 16);
          if (want != m.ram()[i]) {
            const uint16_t wpc = m.lastWriterPc(i);
            std::printf("%s: frame %d, byte $%02X (%s): golden $%02X, ours $%02X; last written by %s", first ? "first difference" : "  also differs", f,
                        0x80 + i, Machine::ramName(static_cast<uint8_t>(0x80 + i)), want, m.ram()[i], m.lastWriterName(i));
            if (wpc) std::printf(" (pc $%04X)", wpc);
            std::printf("\n");
            first = false;
          }
        }
      }
      if (!all) break;
    }
  }
  if (verify) {
    std::printf("routine check: %llu calls compared with the original, %llu different\n", static_cast<unsigned long long>(m.verifyCalls()), static_cast<unsigned long long>(m.verifyFailures()));
    if (m.verifyFailures()) std::printf("  first: %s\n", m.verifyReport().c_str());
  }
  if (different == 0 && !(verify && m.verifyFailures())) {
    std::printf("result: IDENTICAL (%d frames)\n", total);
    return 0;
  }
  if (different == 0) {
    std::printf("result: ROUTINE CHECK FAILED (the frames are identical but a native routine differs from the original)\n");
    return 1;
  }
  std::printf("result: DIFFERENT (%d of %d frames differ%s; first at frame %d)\n", different, total, all ? "" : ", stopped at the first", firstFrame);
  return 1;
}
