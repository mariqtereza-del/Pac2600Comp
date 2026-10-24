#include "settings.hpp"

#include <filesystem>
#include <fstream>
#include <vector>

namespace fs = std::filesystem;

namespace settings {

namespace {

fs::path toPath(const std::string& utf8)
{
  return fs::path(std::u8string(reinterpret_cast<const char8_t*>(utf8.data()), utf8.size()));
}

std::vector<std::string> readLines(const std::string& file)
{
  std::vector<std::string> lines;
  std::ifstream in(toPath(file));
  for(std::string line; std::getline(in, line); ) {
    if(!line.empty() && line.back() == '\r') line.pop_back();
    lines.push_back(line);
  }
  return lines;
}

}  // namespace

std::string get(const std::string& file, const std::string& key)
{
  const std::string prefix = key + "=";
  for(const std::string& line : readLines(file))
    if(line.rfind(prefix, 0) == 0) return line.substr(prefix.size());
  return {};
}

bool set(const std::string& file, const std::string& key, const std::string& value)
{
  std::vector<std::string> lines = readLines(file);
  const std::string prefix = key + "=";
  bool found = false;
  for(std::string& line : lines)
    if(line.rfind(prefix, 0) == 0) { line = prefix + value; found = true; }
  if(!found) lines.push_back(prefix + value);

  std::ofstream out(toPath(file), std::ios::trunc);
  if(!out) return false;
  for(const std::string& line : lines) out << line << '\n';
  return static_cast<bool>(out);
}

}  // namespace settings
