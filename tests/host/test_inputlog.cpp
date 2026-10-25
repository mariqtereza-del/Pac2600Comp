// Tests for src\host\inputlog (Stage D13): recording, saving, reading back and rejecting bad files.
// Usage: test_inputlog <scratch folder>
#include <cstdio>
#include <string>

#include "../../src/host/inputlog.hpp"

static int failures = 0;
static int checks = 0;

static void expect(bool ok, const std::string& what)
{
  ++checks;
  if(!ok) { ++failures; std::printf("FAIL  %s\n", what.c_str()); }
}

static const std::string kSha(40, 'a');

static bool rejects(const std::string& text, const std::string& what)
{
  inputlog::Recording r;
  std::string error;
  const bool refused = !inputlog::parse(text, r, error) && !error.empty();
  expect(refused, "rejected: " + what);
  return refused;
}

int main(int argc, char** argv)
{
  inputlog::Recorder rec;
  for(int f = 0; f < 10; ++f) rec.add(0);
  for(int f = 0; f < 5; ++f) rec.add(8);   // right
  for(int f = 0; f < 3; ++f) rec.add(24);  // right + fire
  rec.add(0);
  const inputlog::Recording r = rec.finish(kSha);
  expect(r.frames == 19 && rec.frames() == 19, "19 frames recorded");
  expect(r.changes.size() == 4, "only the changes are kept (4 of 19 frames)");
  expect(r.bitsAt(0) == 0 && r.bitsAt(9) == 0 && r.bitsAt(10) == 8 && r.bitsAt(14) == 8 && r.bitsAt(15) == 24 && r.bitsAt(17) == 24 && r.bitsAt(18) == 0,
         "bitsAt gives the input of every frame");
  expect(r.bitsAt(100) == 0, "after the end: 0");

  // Text round trip
  inputlog::Recording back;
  std::string error;
  expect(inputlog::parse(r.toText(), back, error), "the text reads back: " + error);
  expect(back.romSha1 == kSha && back.frames == 19 && back.changes.size() == 4, "header and changes survive");
  expect(back.toText() == r.toText(), "writing it again gives the same text");
  bool same = true;
  for(long long f = 0; f < 19; ++f) same = same && back.bitsAt(f) == r.bitsAt(f);
  expect(same, "every frame's input survives");

  // Windows line ends and comments are fine
  {
    std::string crlf = "# a note\r\n";
    for(char c : r.toText()) { if(c == '\n') crlf += '\r'; crlf += c; }
    inputlog::Recording t;
    expect(inputlog::parse(crlf, t, error) && t.toText() == r.toText(), "CRLF line ends and a comment line are accepted");
  }

  // An empty recording
  {
    inputlog::Recorder none;
    inputlog::Recording t;
    expect(inputlog::parse(none.finish(kSha).toText(), t, error) && t.frames == 0 && t.bitsAt(0) == 0, "a recording of no frames is fine");
  }

  // Bad files
  rejects("", "empty file");
  rejects("hello\n", "wrong first line");
  rejects("PacMan2600-inputs 1\nframes=3\n0 0\n", "no ROM line");
  rejects("PacMan2600-inputs 1\nrom_sha1=abc\nframes=3\n0 0\n", "short SHA-1");
  rejects("PacMan2600-inputs 1\nrom_sha1=" + kSha + "\n0 0\n", "no frames line");
  rejects("PacMan2600-inputs 1\nrom_sha1=" + kSha + "\nframes=x\n0 0\n", "frames is not a number");
  rejects("PacMan2600-inputs 1\nrom_sha1=" + kSha + "\nframes=3\n", "frames but no inputs");
  rejects("PacMan2600-inputs 1\nrom_sha1=" + kSha + "\nframes=3\n1 0\n", "first change not at frame 0");
  rejects("PacMan2600-inputs 1\nrom_sha1=" + kSha + "\nframes=9\n0 0\n5 8\n5 4\n", "frames not increasing");
  rejects("PacMan2600-inputs 1\nrom_sha1=" + kSha + "\nframes=9\n0 0\n3 99999\n", "bits too big");
  rejects("PacMan2600-inputs 1\nrom_sha1=" + kSha + "\nframes=9\n0 0\n3\n", "line without bits");
  rejects("PacMan2600-inputs 1\nrom_sha1=" + kSha + "\nframes=9\n0 0\n9 8\n", "change after the last frame");

  // Files
  if(argc > 1) {
    const std::string path = std::string(argv[1]) + "/d13_test.inputs";
    inputlog::Recording t;
    expect(inputlog::save(path, r), "saved to a file");
    expect(inputlog::load(path, t, error) && t.toText() == r.toText(), "loaded from the file: " + error);
    expect(!inputlog::load(std::string(argv[1]) + "/does_not_exist.inputs", t, error) && !error.empty(), "a missing file gives an error");
  }

  std::printf("%s (%d checks, %d failures)\n", failures == 0 ? "ALL TESTS PASSED" : "SOME TESTS FAILED", checks, failures);
  return failures == 0 ? 0 : 1;
}
