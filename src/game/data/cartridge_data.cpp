#include "cartridge_data.hpp"

#include <algorithm>
#include <cstdio>
#include <vector>

namespace game {

namespace {
uint32_t rotl(uint32_t v, int n) { return (v << n) | (v >> (32 - n)); }
}  // namespace

std::string sha1Hex(const uint8_t* data, std::size_t size) {
  uint32_t h[5] = {0x67452301u, 0xEFCDAB89u, 0x98BADCFEu, 0x10325476u, 0xC3D2E1F0u};
  std::vector<uint8_t> m(data, data + size);
  m.push_back(0x80);
  while (m.size() % 64 != 56) m.push_back(0);
  const uint64_t bits = static_cast<uint64_t>(size) * 8;
  for (int i = 7; i >= 0; --i) m.push_back(static_cast<uint8_t>(bits >> (8 * i)));
  for (std::size_t off = 0; off < m.size(); off += 64) {
    uint32_t w[80];
    for (int i = 0; i < 16; ++i)
      w[i] = (uint32_t(m[off + 4 * i]) << 24) | (uint32_t(m[off + 4 * i + 1]) << 16) | (uint32_t(m[off + 4 * i + 2]) << 8) | m[off + 4 * i + 3];
    for (int i = 16; i < 80; ++i) w[i] = rotl(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
    for (int i = 0; i < 80; ++i) {
      uint32_t f, k;
      if (i < 20) { f = (b & c) | (~b & d); k = 0x5A827999u; }
      else if (i < 40) { f = b ^ c ^ d; k = 0x6ED9EBA1u; }
      else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDCu; }
      else { f = b ^ c ^ d; k = 0xCA62C1D6u; }
      const uint32_t t = rotl(a, 5) + f + e + k + w[i];
      e = d; d = c; c = rotl(b, 30); b = a; a = t;
    }
    h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e;
  }
  char out[41];
  for (int i = 0; i < 5; ++i) std::snprintf(out + 8 * i, 9, "%08x", h[i]);
  return std::string(out, 40);
}

const char* describe(LoadStatus status) {
  switch (status) {
    case LoadStatus::Ok: return "The ROM is the expected Pac-Man (1981) Atari cartridge.";
    case LoadStatus::Unreadable: return "The ROM file could not be read.";
    case LoadStatus::WrongSize: return "The ROM file is not 4096 bytes long, so it is not this cartridge.";
    case LoadStatus::UnknownFingerprint: return "The ROM file has the right size but a different fingerprint (SHA-1), "
                                         "so it is not this version of the cartridge.";
  }
  return "";
}

LoadStatus CartridgeData::load(const uint8_t* bytes, std::size_t size) {
  loaded_ = false;
  bytes_.fill(0);
  if (size != kCartridgeSize) return LoadStatus::WrongSize;
  if (sha1Hex(bytes, size) != kCartridgeSha1) return LoadStatus::UnknownFingerprint;
  std::copy(bytes, bytes + size, bytes_.begin());
  loaded_ = true;
  return LoadStatus::Ok;
}

LoadStatus CartridgeData::loadFile(const std::string& path) {
  loaded_ = false;
  bytes_.fill(0);
  std::FILE* f = std::fopen(path.c_str(), "rb");
  if (!f) return LoadStatus::Unreadable;
  std::vector<uint8_t> buf;
  uint8_t chunk[4096];
  std::size_t n;
  while ((n = std::fread(chunk, 1, sizeof chunk, f)) > 0 && buf.size() <= kCartridgeSize) buf.insert(buf.end(), chunk, chunk + n);
  const bool bad = std::ferror(f) != 0;
  std::fclose(f);
  if (bad) return LoadStatus::Unreadable;
  return load(buf.data(), buf.size());
}

}  // namespace game
