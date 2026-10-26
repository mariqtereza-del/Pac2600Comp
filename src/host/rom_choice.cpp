// Finding the ROM to play: the quiet search, the file dialog, the message about a problem.
// (Stage G4.7: moved out of main.cpp, the code is unchanged.)
#include "rom_choice.hpp"
#include "options.hpp"

namespace host {

void showError(const char* what)
{
  const char* detail = SDL_GetError();
  char text[512];
  std::snprintf(text, sizeof text, "%s\n\n%s", what, detail);
  SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, kTitle, text, nullptr);
}

// --find-rom: the quiet search, no window. Exit code 0 = good ROM found, 4 = a problem with the ROM, 5 = nothing found.
int findRomReport(const Options& o)
{
  std::vector<std::string> log;
  const auto found = rom::findQuietly(o.romArg, o.settingsFile, o.romsDir, &log);
  std::ofstream out(o.findRomFile);
  if(!out) return 1;
  for(const std::string& line : log) out << "looked: " << line << '\n';
  if(!found) {
    out << "result=none (the Open file dialog would be shown)\n";
    return 5;
  }
  out << "source=" << found->source << "\nstatus=" << static_cast<int>(found->check.status) << "\nsize=" << found->check.size
      << "\nsha1=" << found->check.sha1 << "\nmessage:\n" << rom::describe(found->check) << '\n';
  return found->check.status == rom::Status::Ok ? 0 : 4;
}

std::string fileNameOf(const std::string& path)
{
  const std::size_t cut = path.find_last_of("/\\");
  return cut == std::string::npos ? path : path.substr(cut + 1);
}

// Reads the 4096 ROM bytes of a file that has already passed the checks (read only).
bool readRomBytes(const std::string& path, std::vector<std::uint8_t>& bytes)
{
  bytes.assign(rom::kExpectedSize, 0);
  std::ifstream in(path, std::ios::binary);
  return static_cast<bool>(in.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size())));
}

namespace {

// The Open file dialog reports back from SDL, possibly on another thread.
struct PickResult {
  std::mutex lock;
  bool done = false;
  std::string path;  // empty = cancelled
};

}  // namespace

namespace {

void SDLCALL onPicked(void* userdata, const char* const* files, int)
{
  auto* r = static_cast<PickResult*>(userdata);
  std::lock_guard<std::mutex> guard(r->lock);
  r->path = (files && files[0]) ? files[0] : "";
  r->done = true;
}

}  // namespace

namespace {

// Shows a ROM problem. Returns true for "Use anyway".
bool askAboutProblem(SDL_Window* window, const rom::Check& check)
{
  std::vector<SDL_MessageBoxButtonData> buttons;
  buttons.push_back({SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT | SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 1, "Choose another"});
  if(check.status == rom::Status::UnknownFingerprint) buttons.push_back({0, 2, "Use anyway"});
  const std::string text = rom::describe(check);
  SDL_MessageBoxData box{SDL_MESSAGEBOX_WARNING, window, "This is not the expected game file", text.c_str(),
                         static_cast<int>(buttons.size()), buttons.data(), nullptr};
  int pressed = 1;
  if(!SDL_ShowMessageBox(&box, &pressed)) pressed = 1;
  return pressed == 2;
}

}  // namespace

// Finds a usable ROM, asking the person when needed. Returns the chosen path, or empty if they gave up.
std::string resolveRom(SDL_Window* window, const Options& o)
{
  std::optional<rom::Found> found = rom::findQuietly(o.romArg, o.settingsFile, o.romsDir);
  const std::string savedBefore = rom::loadSavedPath(o.settingsFile);
  for(;;) {
    if(found) {
      if(found->check.status == rom::Status::Ok) {
        if(found->check.path != savedBefore) rom::savePath(o.settingsFile, found->check.path);  // remember only a good ROM
        return found->check.path;
      }
      if(askAboutProblem(window, found->check)) return found->check.path;  // this run only, not remembered
      found.reset();
    }

    static const SDL_DialogFileFilter filters[] = {{"Atari 2600 game (*.a26)", "a26"}, {"All files", "*"}};
    PickResult result;
    SDL_ShowOpenFileDialog(onPicked, &result, window, filters, 2, nullptr, false);
    for(;;) {
      {
        std::lock_guard<std::mutex> guard(result.lock);
        if(result.done) break;
      }
      for(SDL_Event e; SDL_PollEvent(&e); )
        if(e.type == SDL_EVENT_QUIT) return {};
      SDL_Delay(20);
    }
    if(result.path.empty()) return {};  // cancelled
    found = rom::Found{"dialog", rom::checkFile(result.path)};
  }
}

}  // namespace host
