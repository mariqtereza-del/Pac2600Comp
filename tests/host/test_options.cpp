// The command line (after Stage I11.2). No window, no ROM. An argument that starts with "--" and is not an option of the program must be reported as unknown, not taken for the
// ROM file (that made "--crt-shot" look like a ROM that could not be found). The options of the program and a ROM given by name or path still work.
#include <cstdio>
#include <string>
#include <vector>

#include "../../src/host/options.hpp"

namespace {
int checks = 0, failures = 0;
void expect(bool ok, const std::string& what)
{
  ++checks;
  if(!ok) { ++failures; std::printf("FAIL  %s\n", what.c_str()); }
}
host::Options parse(std::vector<std::string> args)
{
  std::vector<char*> argv;
  static std::string program = "pacman2600.exe";
  argv.push_back(program.data());
  for(std::string& a : args) argv.push_back(a.data());
  return host::parseArguments(static_cast<int>(argv.size()), argv.data());
}
}  // namespace

int main()
{
  using host::Options;
  {
    const Options o = parse({"--no-such-option", "rom.a26"});
    expect(o.unknownOption == "--no-such-option", "an option the program does not know is reported");
    expect(o.romArg == "rom.a26", "and is not taken for the ROM, which is the next argument");
  }
  {
    const Options o = parse({"--crt-shot-typo", "x"});
    expect(o.unknownOption == "--crt-shot-typo" && o.romArg == "x", "a mistyped check name is unknown");
  }
  {
    const Options o = parse({"F:\\roms\\pacman.a26", "--fps", "144", "--crt", "lottes", "--hdr", "off", "--mute"});
    expect(o.unknownOption.empty(), "the options of the program are all known");
    expect(o.romArg == "F:\\roms\\pacman.a26" && o.fps == "144" && o.crt == "lottes" && o.hdr == "off" && o.mute, "and their values are taken");
  }
  {
    const Options o = parse({"--smooth-shot", "out", "rom.a26", "--hdr-report", "r.txt", "--crt-shot", "c", "--filter-check", "f", "--hdr-highlights", "h"});
    expect(o.unknownOption.empty() && o.smoothShotPng == "out" && o.hdrReportFile == "r.txt" && o.crtShotPng == "c" && o.filterCheckPng == "f" && o.hdrHighlightsPng == "h" && o.romArg == "rom.a26",
           "the check modes of Stages I9 to I11 are known");
  }
  {
    const Options o = parse({"--no-such-option", "--another"});
    expect(o.unknownOption == "--no-such-option", "the first unknown option is the one reported");
  }
  std::printf("%d checks, %d failures\n", checks, failures);
  if(failures == 0) std::printf("ALL TESTS PASSED\n");
  return failures == 0 ? 0 : 1;
}
