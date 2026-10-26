// lockstep_native: runs a recorded game with the finished native game code only (src/game: no 6502 core, no original program code) and compares the 128 memory
// bytes with the golden trace after every frame (Stage F21.27). The ROM is read only to give the game its cartridge data by address.
//
// Usage: lockstep_native <rom> <inputs_file> <trace_file> [--oracle <file>] [--poke <frame>:<hex addr>:<hex value>]... [--ignore <hex list>] [--all]
//   The stack bytes $F6-$FF are always left out (the original leaves old return addresses there; the native game has no stack) and so are the bytes of --ignore
//   (the scratch bytes of the drawing code, tools/f21_scratch_bytes.txt).
// Exit code 0 = identical, 1 = different, 2 = bad arguments or files.
#include <algorithm>
#include <array>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "../game/data/cartridge_data.hpp"
#include "../game/game.hpp"
#include "../game/renderer.hpp"
#include "../testsupport/state_bytes.hpp"

// --cart-reads <file> (only in the build lockstep_native_reads, Stage G5a): at the end, one line per cartridge address that was read: "<hex address> <times>".
static std::string gReadsFile;
#ifdef PACMAN_COUNT_CARTRIDGE_READS
static struct ReadsWriter {
  ~ReadsWriter() {
    if (gReadsFile.empty()) return;
    std::ofstream out(gReadsFile);
    for (int a = 0; a < 4096; ++a)
      if (game::gCartridgeReads[a]) out << std::hex << (0xF000 + a) << std::dec << ' ' << game::gCartridgeReads[a] << "\n";
  }
} gReadsWriter;
#endif

class NullChip : public game::TiaChip {
 public:
  void write(uint8_t, uint8_t) override {}
};

int main(int argc, char** argv) {
  if (argc < 4) {
    std::fprintf(stderr, "usage: lockstep_native <rom> <inputs> <trace> [--oracle f] [--poke f:a:v]... [--ignore hex,hex] [--all]\n");
    return 2;
  }
  game::CartridgeData data;  // the data loader (F22): size and fingerprint are checked, the game gets a private copy
  const game::LoadStatus loaded = data.loadFile(argv[1]);
  if (loaded != game::LoadStatus::Ok) { std::fprintf(stderr, "%s\n", game::describe(loaded)); return 2; }
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
  std::vector<std::string> golden, goldenPicture, goldenSound;
  {
    std::ifstream in(argv[3]);
    if (!in) { std::fprintf(stderr, "cannot read %s\n", argv[3]); return 2; }
    std::string line;
    std::getline(in, line); std::getline(in, line); std::getline(in, line);
    while (std::getline(in, line)) {
      std::vector<std::string> cells;
      std::istringstream ls(line);
      for (std::string c; std::getline(ls, c, ',');) cells.push_back(c);
      if (cells.size() >= 6) { golden.push_back(cells[5]); goldenPicture.push_back(cells[3]); goldenSound.push_back(cells[4]); }
    }
  }
  std::map<int, std::pair<bool, bool>> oracle;
  std::vector<std::array<int, 3>> pokes;
  std::vector<int> ignore;
  bool all = false, pictures = false, sound = false;
  int saveFrame = -1, traceFrame = -1, skew = 0;
  std::string saveFile;
  for (int i = 4; i < argc; ++i) {
    const std::string a = argv[i];
    if (a == "--oracle" && i + 1 < argc) {
      std::ifstream in(argv[++i]);
      int f, b, g;
      while (in >> f >> b >> g) oracle[f] = {b != 0, g != 0};
    } else if (a == "--poke" && i + 1 < argc) {
      int pf = 0, pa = 0, pv = 0;
      if (std::sscanf(argv[++i], "%d:%x:%x", &pf, &pa, &pv) == 3) pokes.push_back({pf, pa, pv});
    } else if (a == "--ignore" && i + 1 < argc) {
      std::istringstream ls(argv[++i]);
      for (std::string t; std::getline(ls, t, ',');) ignore.push_back(static_cast<int>(std::stoul(t, nullptr, 16)) - 0x80);
    } else if (a == "--cart-reads" && i + 1 < argc) {
      gReadsFile = argv[++i];
    } else if (a == "--all") {
      all = true;
    } else if (a == "--trace-frame" && i + 1 < argc) {
      traceFrame = std::atoi(argv[++i]);
    } else if (a == "--pictures") {
      pictures = true;
    } else if (a == "--skew" && i + 1 < argc) {   // a control: every write of the bottom logic this many cycles late
      skew = std::atoi(argv[++i]);
    } else if (a == "--sound") {   // with --pictures: the fingerprint of the sound handed over after each frame is compared too
      sound = true;
    } else if (a == "--save-picture" && i + 1 < argc) {   // <frame>:<file>: saves that frame's picture (228 x 160 colour bytes) for looking at
      const std::string v = argv[++i];
      saveFrame = std::atoi(v.c_str());
      saveFile = v.substr(v.find(':') + 1);

    }
  }
  for (int a = 0xF6; a <= 0xFF; ++a) ignore.push_back(a - 0x80);

  const game::Cartridge cart = data.view();
  game::Game g(cart);
  NullChip nullChip;
  game::Renderer renderer;   // with --pictures: the drawing code and the video-chip model make every frame's picture, compared by fingerprint
  game::TiaChip& chip = pictures ? static_cast<game::TiaChip&>(renderer) : static_cast<game::TiaChip&>(nullChip);
  if (pictures) g.setPicture(&renderer);
  renderer.skewCycles = skew;
  int picturesDifferent = 0, firstPicture = -1, soundDifferent = 0, firstSound = -1;
  const int total = std::min<int>(frames > 0 ? frames : static_cast<int>(golden.size()), static_cast<int>(golden.size()));
  unsigned bits = 0;
  size_t next = 0;
  int different = 0, firstFrame = -1;
  std::array<long, 128> diffCount{};
  for (int f = 0; f < total; ++f) {
    while (next < changes.size() && changes[next].first <= f) bits = changes[next++].second;
    game::FrameInputs in;
    in.console.fire = (bits & 16) != 0;
    in.console.select = (bits & 32) != 0;
    in.console.reset = (bits & 64) != 0;
    in.console.leftDifficultyA = (bits & 128) != 0;
    in.console.rightDifficultyA = (bits & 256) != 0;
    in.console.colour = !(bits & 512);
    // The reference emulator drops up when up and down are pressed together and left when left and right are (found in Stage F25 by reading SWCHA in
    // random games; a real joystick cannot do it). The game gets what the emulator gave it.
    unsigned dirs = bits & 15;
    if ((dirs & 3) == 3) dirs &= ~1u;
    if ((dirs & 12) == 12) dirs &= ~4u;
    uint8_t swcha = 0xFF;
    if (dirs & 1) swcha &= ~0x10;
    if (dirs & 2) swcha &= ~0x20;
    if (dirs & 4) swcha &= ~0x40;
    if (dirs & 8) swcha &= ~0x80;
    in.frame.swcha = swcha;
    auto o = oracle.find(f);
    in.frame.bonusTouched = o != oracle.end() && o->second.first;
    in.frame.ghostTouched = o != oracle.end() && o->second.second;
    for (const auto& pk : pokes)
      if (pk[0] == f && pk[1] >= 0x80 && pk[1] <= 0xFF) testsupport::byteAt(g.state(), pk[1]) = static_cast<uint8_t>(pk[2]);
    renderer.trace = (f == traceFrame);
    if (!g.runFrame(in, chip)) { std::printf("frame %d: the game met the one path of the original that no recording runs\n", f); return 1; }
    if (pictures) {
      const auto& px = renderer.finishPicture(g.lastFrameLines());
      uint64_t h = 1469598103934665603ULL;   // the fingerprint of the whole project
      for (uint8_t v : px) { h ^= v; h *= 1099511628211ULL; }
      char hex[20];
      std::snprintf(hex, sizeof hex, "%016llx", static_cast<unsigned long long>(h));
      if (f == saveFrame) { std::ofstream sf(saveFile, std::ios::binary); sf.write(reinterpret_cast<const char*>(px.data()), static_cast<std::streamsize>(px.size())); }
      if (sound) {
        uint64_t sh = 1469598103934665603ULL;
        for (int16_t v : renderer.sound())
          for (int rep = 0; rep < 2; ++rep)   // mono: left and right are the same
            for (int b = 0; b < 2; ++b) { sh ^= static_cast<uint8_t>((static_cast<uint16_t>(v) >> (8 * b)) & 0xFF); sh *= 1099511628211ULL; }
        char shex[20];
        std::snprintf(shex, sizeof shex, "%016llx", static_cast<unsigned long long>(sh));
        if (goldenSound[f] != shex && soundDifferent++ == 0) { firstSound = f; std::printf("first sound difference: frame %d, golden %s, ours %s\n", f, goldenSound[f].c_str(), shex); }
      }
      if (f > 0 && goldenPicture[f] != hex) {   // frame 0 is the partial first picture after power-on
        if (picturesDifferent++ == 0) { firstPicture = f; std::printf("first picture difference: frame %d, golden %s, ours %s\n", f, goldenPicture[f].c_str(), hex); }
      }
    }
    std::string ours;
    char buf[3];
    const testsupport::Bytes bytes = testsupport::toBytes(g.state());
    for (int i = 0; i < 128; ++i) { std::snprintf(buf, sizeof buf, "%02x", bytes[i]); ours += buf; }
    for (int i : ignore)
      if (i >= 0 && i < 128) ours.replace(2 * i, 2, golden[f].substr(2 * i, 2));
    if (ours != golden[f]) {
      ++different;
      bool first = firstFrame < 0;
      if (first) firstFrame = f;
      for (int i = 0; i < 128; ++i) {
        if (ours.compare(2 * i, 2, golden[f], 2 * i, 2) == 0) continue;
        ++diffCount[i];
        if (first) std::printf("first difference: frame %d, byte $%02X: golden $%s, ours $%s\n", f, 0x80 + i, golden[f].substr(2 * i, 2).c_str(), ours.substr(2 * i, 2).c_str());
      }
      if (!all) break;
    }
  }
  if (all) {
    std::printf("bytes that differ in some frame:");
    for (int i = 0; i < 128; ++i)
      if (diffCount[i]) std::printf(" $%02X x%ld", 0x80 + i, diffCount[i]);
    std::printf("\n");
  }
  if (pictures) {
    std::printf("pictures: %s (%d of %d frames differ", picturesDifferent ? "DIFFERENT" : "IDENTICAL", picturesDifferent, total - 1);
    if (picturesDifferent) std::printf(", first at frame %d", firstPicture);
    std::printf(")\n");
  }
  if (sound) {
    std::printf("sound: %s (%d of %d frames differ", soundDifferent ? "DIFFERENT" : "IDENTICAL", soundDifferent, total);
    if (soundDifferent) std::printf(", first at frame %d", firstSound);
    std::printf(")\n");
  }
  if (different == 0 && picturesDifferent == 0 && soundDifferent == 0) {
    std::printf("result: IDENTICAL (%d frames)\n", total);
    return 0;
  }
  if (different == 0) return 1;
  std::printf("result: DIFFERENT (%d of %d frames differ; first at frame %d)\n", different, total, firstFrame);
  return 1;
}
