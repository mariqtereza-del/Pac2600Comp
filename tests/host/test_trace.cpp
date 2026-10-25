// Tests for src\host\trace (Stage E1): the fingerprint, the file lines and reading them back.
// Usage: test_trace
#include <cstdio>
#include <string>

#include "../../src/host/trace.hpp"

static int failures = 0;
static int checks = 0;

static void expect(bool ok, const std::string& what)
{
  ++checks;
  if(!ok) { ++failures; std::printf("FAIL  %s\n", what.c_str()); }
}

int main()
{
  // The fingerprint is the 64-bit FNV-1a with the start value 1469598103934665603 that every Phase C fingerprint uses
  // (it is one digit short of the textbook start value 14695981039346656037; kept, so all fingerprints stay comparable).
  // Expected values computed independently in Python.
  expect(trace::fnv1a("", 0) == 0x14650fb0739d0383ULL, "fingerprint of nothing is the start value");
  expect(trace::fnv1a("a", 1) == 0x44bd8ad473cd9906ULL, "fingerprint of \"a\"");
  expect(trace::fnv1a("foobar", 6) == 0x88fad7c0a8ff07f2ULL, "fingerprint of \"foobar\"");

  const std::string sha(40, 'a');
  expect(trace::header(sha) == "PacMan2600-trace 1\nrom_sha1=" + sha + "\ncolumns=frame,input,switches,picture,audio,ram\n", "the header is the three lines");

  trace::Row r;
  r.frame = 1234; r.input = 0x2A8; r.switches = 0xf7; r.picture = 0x0123456789abcdefULL; r.audio = 0xfedcba9876543210ULL;
  for(int i = 0; i < 128; ++i) r.ram[static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(i * 3 + 1);
  const std::string text = trace::line(r);
  expect(text.rfind("1234,680,f7,0123456789abcdef,fedcba9876543210,", 0) == 0, "the line starts with the five numbers in the documented form");
  expect(text.size() == std::string("1234,680,f7,0123456789abcdef,fedcba9876543210,").size() + 256 + 1, "the line has 256 hex digits for the memory and a newline");
  expect(text.back() == '\n', "the line ends with a newline");

  trace::Row back;
  std::string error;
  expect(trace::parseLine(text, back, error), "the line reads back: " + error);
  expect(back == r, "every field survives");
  expect(trace::line(back) == text, "writing it again gives the same text");
  expect(trace::parseLine(text.substr(0, text.size() - 1) + "\r\n", back, error) && back == r, "a Windows line end is accepted");

  // Bad lines
  const std::string good = text.substr(0, text.size() - 1);
  auto refuses = [&](const std::string& bad, const std::string& what) {
    trace::Row t; std::string e;
    expect(!trace::parseLine(bad, t, e) && !e.empty(), "refused: " + what);
  };
  refuses("", "empty line");
  refuses("1,2,3", "too few fields");
  refuses(good.substr(0, good.size() - 2), "one memory byte short");
  refuses(good + "00", "one memory byte too many");
  refuses(good.substr(0, good.size() - 1) + "g", "a letter that is not a hex digit");
  refuses("x,680,f7,0123456789abcdef,fedcba9876543210," + good.substr(good.rfind(',') + 1), "frame number not a number");
  refuses("1,680,1ff,0123456789abcdef,fedcba9876543210," + good.substr(good.rfind(',') + 1), "switch register above 255");

  std::printf("%s (%d checks, %d failures)\n", failures == 0 ? "ALL TESTS PASSED" : "SOME TESTS FAILED", checks, failures);
  return failures == 0 ? 0 : 1;
}
