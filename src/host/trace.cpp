#include "trace.hpp"

#include <cstdio>
#include <cstdlib>

namespace trace {

// The start value is the one every Phase C fingerprint uses (a digit short of the textbook 14695981039346656037):
// kept on purpose so that fingerprints of all stages stay equal.
std::uint64_t fnv1a(const void* data, std::size_t size)
{
  const auto* bytes = static_cast<const std::uint8_t*>(data);
  std::uint64_t h = 1469598103934665603ULL;
  for(std::size_t i = 0; i < size; ++i) { h ^= bytes[i]; h *= 1099511628211ULL; }
  return h;
}

std::string header(const std::string& romSha1)
{
  return std::string(kHeaderLine) + "\nrom_sha1=" + romSha1 + "\n" + kColumnsLine + "\n";
}

std::string line(const Row& row)
{
  char head[96];
  std::snprintf(head, sizeof head, "%lld,%u,%02x,%016llx,%016llx,", row.frame, row.input, row.switches,
                static_cast<unsigned long long>(row.picture), static_cast<unsigned long long>(row.audio));
  std::string out = head;
  static const char digits[] = "0123456789abcdef";
  for(std::uint8_t b : row.ram) { out += digits[b >> 4]; out += digits[b & 15]; }
  out += '\n';
  return out;
}

bool parseLine(const std::string& text, Row& out, std::string& error)
{
  Row r;
  char* end = nullptr;
  const char* p = text.c_str();
  auto field = [&](unsigned long long& v, int base) {
    v = std::strtoull(p, &end, base);
    if(end == p || (*end != ',' && *end != '\0' && *end != '\n' && *end != '\r')) return false;
    p = *end == ',' ? end + 1 : end;
    return true;
  };
  unsigned long long frame = 0, input = 0, sw = 0, pic = 0, aud = 0;
  if(!field(frame, 10) || !field(input, 10) || !field(sw, 16) || !field(pic, 16) || !field(aud, 16) || sw > 255) { error = "a trace line does not have its five numbers: " + text; return false; }
  r.frame = static_cast<long long>(frame);
  r.input = static_cast<std::uint32_t>(input);
  r.switches = static_cast<std::uint8_t>(sw);
  r.picture = pic;
  r.audio = aud;
  for(int i = 0; i < 128; ++i) {
    auto nibble = [](char c) { return c >= '0' && c <= '9' ? c - '0' : (c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1); };
    const int hi = nibble(p[0]), lo = hi < 0 ? -1 : nibble(p[1]);
    if(hi < 0 || lo < 0) { error = "a trace line has fewer than 128 memory bytes: " + text; return false; }
    r.ram[static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(hi * 16 + lo);
    p += 2;
  }
  if(*p != '\0' && *p != '\n' && *p != '\r') { error = "a trace line has more than 128 memory bytes: " + text; return false; }
  out = r;
  return true;
}

}  // namespace trace
