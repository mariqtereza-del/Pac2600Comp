#include "inputlog.hpp"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace inputlog {

namespace {
constexpr std::uint32_t kLargestBits = 1023;  // ten switches (controls::Input::pack)
}

std::uint32_t Recording::bitsAt(long long frame) const
{
  const auto it = std::upper_bound(changes.begin(), changes.end(), frame, [](long long f, const Change& c) { return f < c.frame; });
  return it == changes.begin() ? 0 : std::prev(it)->bits;
}

std::string Recording::toText() const
{
  std::ostringstream out;
  out << "PacMan2600-inputs 1\nrom_sha1=" << romSha1 << "\nframes=" << frames << '\n';
  for(const Change& c : changes) out << c.frame << ' ' << c.bits << '\n';
  return out.str();
}

void Recorder::add(std::uint32_t bits)
{
  if(changes_.empty() || changes_.back().bits != bits) changes_.push_back({frames_, bits});
  ++frames_;
}

Recording Recorder::finish(const std::string& romSha1) const
{
  Recording r;
  r.romSha1 = romSha1;
  r.frames = frames_;
  r.changes = changes_;
  return r;
}

bool parse(const std::string& text, Recording& out, std::string& error)
{
  std::istringstream in(text);
  std::string line;
  auto next = [&]() {  // the next line that is not empty and not a comment
    while(std::getline(in, line)) {
      if(!line.empty() && line.back() == '\r') line.pop_back();
      if(!line.empty() && line[0] != '#') return true;
    }
    return false;
  };
  Recording r;
  if(!next() || line != "PacMan2600-inputs 1") { error = "This is not an input recording of this program (the first line is wrong)."; return false; }
  if(!next() || line.rfind("rom_sha1=", 0) != 0 || line.size() != 9 + 40) { error = "The recording does not say which ROM it was made with."; return false; }
  r.romSha1 = line.substr(9);
  if(!next() || line.rfind("frames=", 0) != 0) { error = "The recording does not say how many frames it holds."; return false; }
  char* end = nullptr;
  r.frames = std::strtoll(line.c_str() + 7, &end, 10);
  if(end == line.c_str() + 7 || *end != '\0' || r.frames < 0) { error = "The number of frames in the recording is not a number."; return false; }
  while(next()) {
    long long frame = std::strtoll(line.c_str(), &end, 10);
    if(end == line.c_str() || *end != ' ') { error = "A line of the recording is not 'frame bits': " + line; return false; }
    const char* second = end + 1;
    const unsigned long long bits = std::strtoull(second, &end, 10);
    if(end == second || *end != '\0' || bits > kLargestBits) { error = "A line of the recording is not 'frame bits': " + line; return false; }
    const bool first = r.changes.empty();
    if(first ? frame != 0 : frame <= r.changes.back().frame) { error = "The frame numbers of the recording are not in order: " + line; return false; }
    if(frame >= r.frames) { error = "The recording has a change after its last frame: " + line; return false; }
    r.changes.push_back({frame, static_cast<std::uint32_t>(bits)});
  }
  if(r.frames > 0 && r.changes.empty()) { error = "The recording has frames but no inputs."; return false; }
  out = std::move(r);
  return true;
}

bool save(const std::string& path, const Recording& recording)
{
  std::ofstream out(path, std::ios::binary);
  out << recording.toText();
  return static_cast<bool>(out);
}

bool load(const std::string& path, Recording& out, std::string& error)
{
  std::ifstream in(path, std::ios::binary);
  if(!in) { error = "The recording file could not be opened: " + path; return false; }
  std::ostringstream text;
  text << in.rdbuf();
  return parse(text.str(), out, error);
}

}  // namespace inputlog
