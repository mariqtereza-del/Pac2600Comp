// Test of the drawing code (Stage F23.2):
//   test_kernel <rom> <reference dir (reference\f23)> [--game BL-nnn]... [--show N]
// For every sampled frame the memory bytes at the start of the original drawing code ($F07F, r_BL-nnn.csv) are given to game::drawPicture. The list of
// writes it makes to the video chip must equal the emulator's list (t_BL-nnn.csv: scanline, colour clock, address of the writing instruction, register,
// value) write for write, and the memory it leaves must equal the emulator's at the end of the drawing code ($F4E7, e_BL-nnn.csv), except for the stack
// bytes $F6-$FF. Exit code 0 = everything equal.
#include <array>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "../../src/game/kernel.hpp"
#include "../../src/testsupport/state_bytes.hpp"

using game::ChipWrite;

class Recorder : public game::TimedChip {
 public:
  std::vector<ChipWrite> writes;
  void write(const ChipWrite& w) override { writes.push_back(w); }
};

static std::map<int, std::array<uint8_t, 128>> readDumps(const std::string& path) {
  std::map<int, std::array<uint8_t, 128>> out;
  std::ifstream in(path);
  for (std::string line; std::getline(in, line);) {
    const size_t comma = line.find(',');
    if (comma == std::string::npos || line.size() < comma + 1 + 256) continue;
    std::array<uint8_t, 128> r{};
    for (int i = 0; i < 128; ++i) r[i] = static_cast<uint8_t>(std::stoi(line.substr(comma + 1 + 2 * i, 2), nullptr, 16));
    out[std::stoi(line.substr(0, comma))] = r;
  }
  return out;
}

int main(int argc, char** argv) {
  if (argc < 3) { std::fprintf(stderr, "usage: test_kernel <rom> <f23 dir> [--game BL-nnn]... [--show N]\n"); return 2; }
  // argv[1] (the ROM) is not read: the drawing code needs none (G5c). The argument stays so that the tools keep their call.
  const std::string dir = argv[2];
  std::vector<std::string> games;
  int show = 4, shift = 0;   // --shift N adds N to every colour clock of ours first (a control: the test must then fail)
  for (int i = 3; i < argc; ++i) {
    const std::string a = argv[i];
    if (a == "--game" && i + 1 < argc) games.push_back(argv[++i]);
    else if (a == "--show" && i + 1 < argc) show = std::atoi(argv[++i]);
    else if (a == "--shift" && i + 1 < argc) shift = std::atoi(argv[++i]);
  }
  if (games.empty())
    for (int n = 1; n <= 70; ++n) { char b[16]; std::snprintf(b, sizeof b, "BL-%03d", n); games.push_back(b); }

  long frames = 0, listsEqual = 0, ramEqual = 0, shown = 0, writesChecked = 0;
  for (const auto& g : games) {
    const auto starts = readDumps(dir + "\\r_" + g + ".csv");
    const auto ends = readDumps(dir + "\\e_" + g + ".csv");
    std::map<int, std::vector<ChipWrite>> expected;
    {
      std::ifstream tf(dir + "\\t_" + g + ".csv");
      if (!tf) { std::fprintf(stderr, "missing %s\n", g.c_str()); return 2; }
      for (std::string row; std::getline(tf, row);) {
        int f, line, clock, pc, reg, val;
        if (std::sscanf(row.c_str(), "%d,%d,%d,%x,%x,%x", &f, &line, &clock, &pc, &reg, &val) != 6) continue;
        if (pc >= 0x053 && pc < 0x4E6)
          expected[f].push_back({line, clock, static_cast<uint8_t>(reg), static_cast<uint8_t>(val), static_cast<uint16_t>(0xF000 + pc)});
      }
    }
    for (const auto& [f, start] : starts) {
      if (f % 53 != 0) continue;
      auto exp = expected.find(f);
      auto end = ends.find(f);
      if (exp == expected.end() || end == ends.end()) continue;
      ++frames;
      testsupport::Bytes bytes;
      for (int i = 0; i < 128; ++i) bytes[i] = start[i];
      game::GameState state = testsupport::fromBytes(bytes);
      Recorder rec;
      int startLine = 16;
      for (const auto& w : exp->second) if (w.site == 0xF083) { startLine = w.line; break; }
      game::drawPicture(state, startLine, rec);
      bytes = testsupport::toBytes(state);
      for (auto& w : rec.writes) w.clock += shift;
      writesChecked += static_cast<long>(exp->second.size());
      bool same = rec.writes.size() == exp->second.size();
      size_t at = 0;
      for (; same && at < rec.writes.size(); ++at) {
        const auto& a = rec.writes[at];
        const auto& e = exp->second[at];
        if (a.line != e.line || a.clock != e.clock || a.reg != e.reg || a.value != e.value || a.site != e.site) { same = false; break; }
      }
      if (same) ++listsEqual;
      bool ramSame = true;
      int badByte = -1;
      for (int i = 0; i < 118; ++i)   // all but the stack bytes $F6-$FF
        if (bytes[i] != end->second[i]) { ramSame = false; badByte = i; break; }
      if (ramSame) ++ramEqual;
      if ((!same || !ramSame) && shown++ < show) {
        std::printf("%s frame %d (start line %d):", g.c_str(), f, startLine);
        if (!same) {
          std::printf(" writes differ: ours %zu, emulator %zu;", rec.writes.size(), exp->second.size());
          for (size_t k = at; k < at + 1 && (k < rec.writes.size() || k < exp->second.size()); ++k) {
            if (k < rec.writes.size()) std::printf(" ours[%zu] line %d clock %d reg $%02X val $%02X site $%04X;", k, rec.writes[k].line, rec.writes[k].clock, rec.writes[k].reg, rec.writes[k].value, rec.writes[k].site);
            if (k < exp->second.size()) std::printf(" emu[%zu] line %d clock %d reg $%02X val $%02X site $%04X;", k, exp->second[k].line, exp->second[k].clock, exp->second[k].reg, exp->second[k].value, exp->second[k].site);
          }
        }
        if (!ramSame) std::printf(" memory differs at $%02X: ours $%02X emulator $%02X;", 0x80 + badByte, bytes[badByte], end->second[badByte]);
        std::printf("\n");
      }
    }
  }
  std::printf("sampled frames: %ld; write lists equal: %ld; memory equal: %ld; writes checked: %ld\n", frames, listsEqual, ramEqual, writesChecked);
  const bool ok = frames > 0 && listsEqual == frames && ramEqual == frames;
  std::printf("%s\n", ok ? "result: OK" : "result: DIFFERENT");
  return ok ? 0 : 1;
}
