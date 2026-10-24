// Tests for src\host\rom_finder (Stage D3). Usage: test_rom_finder <scratch_folder> <path_to_rom_copy>
// Creates small synthetic files in the scratch folder. The ROM copy is only read.
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "../../src/host/rom_finder.hpp"

namespace fs = std::filesystem;
static int failures = 0;

static void expect(bool ok, const std::string& what)
{
  std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what.c_str());
  if(!ok) ++failures;
}

static void writeFile(const fs::path& p, const std::vector<unsigned char>& bytes)
{
  std::ofstream out(p, std::ios::binary);
  out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
}

int main(int argc, char* argv[])
{
  if(argc < 3) { std::fprintf(stderr, "usage: test_rom_finder <scratch_folder> <rom_copy>\n"); return 2; }
  const fs::path scratch = argv[1];
  const std::string romCopy = argv[2];
  // Nothing is deleted: files are overwritten, so a second run starts from the same state.
  fs::create_directories(scratch / "roms_bad");
  fs::create_directories(scratch / "roms_empty");
  auto S = [&](const char* name) { return (scratch / name).string(); };

  // SHA-1 against published test vectors
  expect(rom::sha1Hex(reinterpret_cast<const unsigned char*>("abc"), 3) == "a9993e364706816aba3e25717850c26c9cd0d89d", "SHA-1 of \"abc\"");
  expect(rom::sha1Hex(nullptr, 0) == "da39a3ee5e6b4b0d3255bfef95601890afd80709", "SHA-1 of nothing");
  const std::string million(1000000, 'a');
  expect(rom::sha1Hex(reinterpret_cast<const unsigned char*>(million.data()), million.size()) == "34aa973cd4c4daa4f61eeb2bdbad27316534016f", "SHA-1 of a million 'a'");

  // the four failure cases and the good case, with the message shown to a person
  const auto good = rom::checkFile(romCopy);
  expect(good.status == rom::Status::Ok && good.sha1 == rom::kExpectedSha1, "the real ROM copy is accepted: " + good.sha1);

  const auto missing = rom::checkFile(S("nothing_here.a26"));
  expect(missing.status == rom::Status::Missing, "missing file");
  std::printf("---- message ----\n%s\n-----------------\n", rom::describe(missing).c_str());

  writeFile(scratch / "empty.a26", {});
  const auto empty = rom::checkFile(S("empty.a26"));
  expect(empty.status == rom::Status::Empty, "empty file");
  std::printf("---- message ----\n%s\n-----------------\n", rom::describe(empty).c_str());

  writeFile(scratch / "short.a26", std::vector<unsigned char>(100, 0x42));
  const auto wrongSize = rom::checkFile(S("short.a26"));
  expect(wrongSize.status == rom::Status::WrongSize && wrongSize.size == 100, "wrong size (100 bytes)");
  std::printf("---- message ----\n%s\n-----------------\n", rom::describe(wrongSize).c_str());

  writeFile(scratch / "other.a26", std::vector<unsigned char>(4096, 0x00));
  const auto unknown = rom::checkFile(S("other.a26"));
  expect(unknown.status == rom::Status::UnknownFingerprint && unknown.sha1.size() == 40, "right size, unknown fingerprint");
  std::printf("---- message ----\n%s\n-----------------\n", rom::describe(unknown).c_str());

  expect(rom::checkFile(scratch.string()).status == rom::Status::NotAFile, "a folder is not a file");
  expect(rom::checkFile("").status == rom::Status::Missing, "empty path");

  // a one-bit change in the real ROM must be rejected
  {
    std::ifstream in(romCopy, std::ios::binary);
    std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(in)), {});
    bytes[100] ^= 1;
    writeFile(scratch / "flipped.a26", bytes);
    expect(rom::checkFile(S("flipped.a26")).status == rom::Status::UnknownFingerprint, "one changed bit in the ROM is rejected");
  }

  // settings round trip
  const std::string ini = S("settings_test.ini");
  expect(rom::loadSavedPath(S("settings_never_written.ini")).empty(), "no settings file: no saved path");
  expect(rom::savePath(ini, romCopy) && fs::path(rom::loadSavedPath(ini)).is_absolute() && fs::equivalent(rom::loadSavedPath(ini), romCopy), "saved path is read back, and is absolute");

  // search order
  std::vector<std::string> log;
  auto f = rom::findQuietly(S("short.a26"), ini, S("roms_bad"), &log);
  expect(f && f->source == "command line" && f->check.status == rom::Status::WrongSize, "command line wins, even when it is bad");
  f = rom::findQuietly("", ini, S("roms_bad"), &log);
  expect(f && f->source == "saved path" && f->check.status == rom::Status::Ok, "saved path is used next");
  rom::savePath(ini, S("gone.a26"));
  f = rom::findQuietly("", ini, S("roms_empty"), &log);
  expect(!f, "a saved path that no longer exists and an empty roms folder: nothing found");
  // The ROM is never copied: the good roms-folder case points at the folder that already holds the working copy.
  writeFile(scratch / "roms_bad" / "aaa.a26", std::vector<unsigned char>(10, 1));
  writeFile(scratch / "roms_bad" / "BBB.A26", std::vector<unsigned char>(4096, 7));
  log.clear();
  f = rom::findQuietly("", ini, S("roms_bad"), &log);
  expect(!f && log.back().find("(2 .a26 files)") != std::string::npos, "roms folder with only bad files (either letter case): nothing found");
  f = rom::findQuietly("", ini, fs::path(romCopy).parent_path().string(), &log);
  expect(f && f->source == "roms folder" && f->check.status == rom::Status::Ok, "roms folder holding the good ROM: found");
  expect(!rom::findQuietly("", S("no_such.ini"), S("no_such_folder"), nullptr), "no settings and no roms folder: nothing found");

  std::printf("\n%s (%d failure%s)\n", failures ? "TESTS FAILED" : "ALL TESTS PASSED", failures, failures == 1 ? "" : "s");
  return failures ? 1 : 0;
}
