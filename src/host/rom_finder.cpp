#include "rom_finder.hpp"

#include "settings.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <system_error>

namespace fs = std::filesystem;

namespace rom {

namespace {

fs::path toPath(const std::string& utf8)
{
  return fs::path(std::u8string(reinterpret_cast<const char8_t*>(utf8.data()), utf8.size()));
}

std::string toUtf8(const fs::path& p)
{
  const std::u8string u = p.u8string();
  return std::string(reinterpret_cast<const char*>(u.data()), u.size());
}

std::uint32_t rotl(std::uint32_t v, int n) { return (v << n) | (v >> (32 - n)); }

}  // namespace

std::string sha1Hex(const std::uint8_t* data, std::size_t size)
{
  std::uint32_t h[5] = {0x67452301u, 0xEFCDAB89u, 0x98BADCFEu, 0x10325476u, 0xC3D2E1F0u};
  std::vector<std::uint8_t> msg(data, data + size);
  msg.push_back(0x80);
  while(msg.size() % 64 != 56) msg.push_back(0);
  const std::uint64_t bits = static_cast<std::uint64_t>(size) * 8;
  for(int i = 7; i >= 0; --i) msg.push_back(static_cast<std::uint8_t>(bits >> (i * 8)));

  for(std::size_t off = 0; off < msg.size(); off += 64) {
    std::array<std::uint32_t, 80> w{};
    for(int i = 0; i < 16; ++i)
      w[i] = (static_cast<std::uint32_t>(msg[off + 4 * i]) << 24) | (static_cast<std::uint32_t>(msg[off + 4 * i + 1]) << 16)
             | (static_cast<std::uint32_t>(msg[off + 4 * i + 2]) << 8) | static_cast<std::uint32_t>(msg[off + 4 * i + 3]);
    for(int i = 16; i < 80; ++i) w[i] = rotl(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    std::uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
    for(int i = 0; i < 80; ++i) {
      std::uint32_t f, k;
      if(i < 20) { f = (b & c) | (~b & d); k = 0x5A827999u; }
      else if(i < 40) { f = b ^ c ^ d; k = 0x6ED9EBA1u; }
      else if(i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDCu; }
      else { f = b ^ c ^ d; k = 0xCA62C1D6u; }
      const std::uint32_t t = rotl(a, 5) + f + e + k + w[i];
      e = d; d = c; c = rotl(b, 30); b = a; a = t;
    }
    h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e;
  }
  static const char* digits = "0123456789abcdef";
  std::string out;
  for(std::uint32_t v : h)
    for(int shift = 28; shift >= 0; shift -= 4) out += digits[(v >> shift) & 15];
  return out;
}

Check checkFile(const std::string& path)
{
  Check c;
  c.path = path;
  std::error_code ec;
  const fs::path p = toPath(path);
  if(path.empty() || !fs::exists(p, ec)) { c.status = Status::Missing; return c; }
  if(!fs::is_regular_file(p, ec)) { c.status = Status::NotAFile; return c; }
  const std::uintmax_t size = fs::file_size(p, ec);
  if(ec) { c.status = Status::Unreadable; return c; }
  c.size = size;
  if(size == 0) { c.status = Status::Empty; return c; }
  if(size != kExpectedSize) { c.status = Status::WrongSize; return c; }  // not read: it could be huge

  std::ifstream in(p, std::ios::binary);  // read only
  std::vector<std::uint8_t> bytes(kExpectedSize);
  if(!in.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()))) {
    c.status = Status::Unreadable;
    return c;
  }
  c.sha1 = sha1Hex(bytes.data(), bytes.size());
  c.status = c.sha1 == kExpectedSha1 ? Status::Ok : Status::UnknownFingerprint;
  return c;
}

std::string describe(const Check& c)
{
  std::ostringstream o;
  const std::string expected = std::string("The game file must be the original Atari Pac-Man cartridge (") + kExpectedName
                               + "): exactly 4096 bytes, fingerprint (SHA-1) " + kExpectedSha1 + ".";
  switch(c.status) {
    case Status::Ok: o << "ROM found and checked: " << c.path << " (4096 bytes, fingerprint matches)."; break;
    case Status::Missing: o << "The file was not found:\n" << c.path << "\n\n" << expected; break;
    case Status::NotAFile: o << "This is not a file (it may be a folder):\n" << c.path << "\n\n" << expected; break;
    case Status::Unreadable: o << "The file could not be read:\n" << c.path << "\n\n" << expected; break;
    case Status::Empty: o << "The file is empty (0 bytes):\n" << c.path << "\n\n" << expected; break;
    case Status::WrongSize:
      o << "The file has the wrong size (" << c.size << " bytes):\n" << c.path << "\n\n" << expected;
      break;
    case Status::UnknownFingerprint:
      o << "The file has the right size but a different fingerprint:\n" << c.path << "\nIts SHA-1 is " << c.sha1
        << ".\n\n" << expected << "\n\nIt may be another version or game, or a damaged copy.";
      break;
  }
  return o.str();
}

std::string loadSavedPath(const std::string& settingsFile)
{
  return settings::get(settingsFile, "rom_path");
}

bool savePath(const std::string& settingsFile, const std::string& path)
{
  std::error_code ec;
  const fs::path absolute = fs::absolute(toPath(path), ec);  // a relative path would break when started from another folder
  return settings::set(settingsFile, "rom_path", ec ? path : toUtf8(absolute));  // other lines (window state) are kept
}

std::optional<Found> findQuietly(const std::string& commandLinePath, const std::string& settingsFile,
                                 const std::string& romsDir, std::vector<std::string>* log)
{
  auto note = [&](const std::string& text) { if(log) log->push_back(text); };

  if(!commandLinePath.empty()) {
    note("command line: " + commandLinePath);
    return Found{"command line", checkFile(commandLinePath)};
  }

  const std::string saved = loadSavedPath(settingsFile);
  if(!saved.empty()) {
    Check c = checkFile(saved);
    note("saved path: " + saved + (c.status == Status::Ok ? " (ok)" : " (not usable)"));
    if(c.status == Status::Ok) return Found{"saved path", c};
  } else {
    note("saved path: none");
  }

  std::error_code ec;
  std::vector<std::string> candidates;
  for(fs::directory_iterator it(toPath(romsDir), ec), end; !ec && it != end; it.increment(ec)) {
    std::string ext = toUtf8(it->path().extension());
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    if(ext == ".a26") candidates.push_back(toUtf8(it->path()));
  }
  std::sort(candidates.begin(), candidates.end());
  note("roms folder: " + romsDir + " (" + std::to_string(candidates.size()) + " .a26 files)");
  for(const std::string& file : candidates) {
    Check c = checkFile(file);
    if(c.status == Status::Ok) return Found{"roms folder", c};
  }
  return std::nullopt;
}

}  // namespace rom
