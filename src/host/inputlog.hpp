// Input recordings: what the console was given on every frame (Stage D13, DECISIONS.md T-D13-1). No window code.
//
// A recording is a small text file:
//     PacMan2600-inputs 1
//     rom_sha1=<40 hex digits>      (the ROM it was recorded with)
//     frames=<number of frames recorded>
//     <frame> <bits>                (one line each time the input changes; frame 0 is always there;
//                                    bits = controls::Input::pack, in decimal)
// The game starts from power-on with empty memory, so the same recording always gives the same pictures.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace inputlog {

struct Change {
  long long frame = 0;
  std::uint32_t bits = 0;
};

struct Recording {
  std::string romSha1;
  long long frames = 0;
  std::vector<Change> changes;  // frame numbers strictly increasing, the first one is 0

  std::uint32_t bitsAt(long long frame) const;  // the input in force on that frame (0 if there is none)
  std::string toText() const;
};

// Collects the input of each frame, in order; remembers only the changes.
class Recorder {
public:
  void add(std::uint32_t bits);
  long long frames() const { return frames_; }
  Recording finish(const std::string& romSha1) const;

private:
  std::vector<Change> changes_;
  long long frames_ = 0;
};

// False with a plain-words error if the text is not a recording.
bool parse(const std::string& text, Recording& out, std::string& error);
bool save(const std::string& path, const Recording& recording);
bool load(const std::string& path, Recording& out, std::string& error);

}  // namespace inputlog
