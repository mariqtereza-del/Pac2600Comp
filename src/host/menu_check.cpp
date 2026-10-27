// --menu-check (Stage I5, T-I5-1): the menu and the game, played together with no window. For three starting states (the title screen, a game in
// play, a two-player choice made with the Select key) and every game from 1 to 8 the menu is opened with real keys, the game is chosen, a switch
// is moved and "New game" is pressed. The game's own memory must then say: that game number, one player, the start level of the table the menu
// shows, the difficulty and TV switches as chosen, and Pac-Man at the speed the menu says. Also: the game does not run while the menu is open,
// and a recording of the inputs given (the presses of the start included) replays to the same picture and memory.
#include "menu_checks.hpp"
#include "menu_session.hpp"
#include "options.hpp"
#include "pads.hpp"
#include "rom_choice.hpp"

namespace host {
namespace {

enum class Start { Title, InPlay, TwoPlayerChosen };

struct Run {
  Emulator emulator;
  menu::Session session{true};
  controls::Keyboard keyboard;
  std::vector<std::uint32_t> given;  // the inputs the console was given, one per frame (what a recording would hold)
  long long framesRun = 0;

  // One pass of the game thread's loop: nothing happens while the menu is open.
  bool frame(const controls::Input& base)
  {
    if(session.paused()) return false;
    controls::Input in = base;
    session.apply(in);
    given.push_back(in.pack());
    applyInput(emulator, in, false);
    emulator.runFrame();
    session.publish(emulator.ram());
    ++framesRun;
    return true;
  }
  void idle(int frames) { for(int i = 0; i < frames; ++i) frame(keyboard.input()); }
  std::uint8_t ram(int address) const { return emulator.ram()[address - 0x80]; }
};

// How a console stood at the end of a run: its 128 bytes of memory and the fingerprint of its picture.
struct Snapshot {
  std::array<std::uint8_t, 128> ram{};
  std::uint64_t picture = 0;
  bool operator==(const Snapshot& o) const { return ram == o.ram && picture == o.picture; }
};

Snapshot snapshotOf(const Emulator& emulator)
{
  Snapshot s;
  for(std::size_t i = 0; i < s.ram.size(); ++i) s.ram[i] = emulator.ram()[i];
  s.picture = emulator.pictureFingerprint();
  return s;
}

// Plays one scenario; returns an empty string when everything was right, else what was wrong.
std::string scenario(const std::string& name, const std::vector<std::uint8_t>& rom, Start start, int game, std::vector<std::uint32_t>* recording, Snapshot* end)
{
  Run run;
  if(const std::string error = run.emulator.load(name, rom); !error.empty()) return error;
  controls::Input press;
  if(start == Start::TwoPlayerChosen) { for(int i = 0; i < 2; ++i) { press.select = true; run.frame(press); } press.select = false; run.idle(5); }
  if(start == Start::InPlay) { press.reset = true; for(int i = 0; i < 10; ++i) run.frame(press); press.reset = false; run.idle(240); }
  else run.idle(200);

  // Open the menu with Escape and check that the game stands still.
  run.session.open(run.keyboard.input());
  const long long before = run.framesRun;
  for(int i = 0; i < 100; ++i) run.frame(run.keyboard.input());
  if(run.framesRun != before) return "the game ran while the menu was open";
  if(run.session.menu().choice().game != menu::gameOfSelect(run.ram(0x84))) return "the menu did not open on the game number the game has";

  // Keys: down to Game, right to the wanted game; down to the left difficulty (A for even games), down twice to TV type (B&W for games 3 and 6);
  // up to New game; Enter.
  using menu::Key;
  const bool leftA = game % 2 == 0, blackWhite = game % 3 == 0;
  run.session.key(Key::Down); run.session.key(Key::Down);
  const int rights = (game - run.session.menu().choice().game + menu::kGames) % menu::kGames;
  for(int i = 0; i < rights; ++i) run.session.key(Key::Right);
  run.session.key(Key::Down);
  if(leftA) run.session.key(Key::Right);
  run.session.key(Key::Down); run.session.key(Key::Down);
  if(blackWhite) run.session.key(Key::Enter);
  for(int i = 0; i < 4; ++i) run.session.key(Key::Up);
  const menu::Choice chosen = run.session.menu().choice();
  run.keyboard.setSwitches(chosen.leftA, chosen.rightA, chosen.blackWhite);  // as the window does after each key
  if(chosen.game != game || chosen.leftA != leftA || chosen.blackWhite != blackWhite) return "the keys did not give the choice wanted";
  if(run.session.key(Key::Enter) != menu::Result::StartGame || run.session.isOpen()) return "Enter on New game did not start a game";

  const int presses = menu::pressesNeeded(run.ram(0x84), game);
  run.idle(menu::GameStarter::framesFor(presses) + 3);

  char text[160];
  const int expectedByte = 2 + (4 + 2 * (game - 1)) % 16;
  if(run.ram(0x84) != expectedByte) { std::snprintf(text, sizeof text, "game number byte is %02x, wanted %02x", run.ram(0x84), expectedByte); return text; }
  if(run.ram(0xE6) & 0x40) return "two players were started";
  if(run.ram(0x82) != 0) return "the game is not in play after the start";
  const std::uint8_t switches = run.emulator.consoleSwitches();
  if(((switches & 0x40) != 0) != leftA || (switches & 0x80) != 0 || ((switches & 0x08) == 0) != blackWhite) return "the console switches are not as chosen";

  // The game starts frozen for a moment; when it moves, the level and the difficulty bit are set. Then Pac-Man walks left to the wall.
  int guard = 0;
  while((run.ram(0xE7) & 0x80) && guard++ < 400) run.idle(1);
  if(guard >= 400) return "the game never started moving";
  if(run.ram(0x99) != menu::gameInfo(game).startLevel) { std::snprintf(text, sizeof text, "start level is %d, the menu says %d", run.ram(0x99), menu::gameInfo(game).startLevel); return text; }
  if(((run.ram(0xE7) & 0x20) != 0) != leftA) return "the difficulty bit of the game does not follow the left switch";
  const int x0 = run.ram(0xB1);
  controls::Input walk = run.keyboard.input();
  walk.left = true;
  // The time for pixels 2 to 12 of the walk (the first two include the turn): 10 pixels at 0.562 a frame is 17.8 frames, at 0.688 it is 14.5.
  int steps = 0, at2 = -1, at12 = -1;
  while(at12 < 0 && steps < 80) {
    run.frame(walk);
    ++steps;
    const int moved = std::abs(run.ram(0xB1) - x0);
    if(moved >= 2 && at2 < 0) at2 = steps;
    if(moved >= 12) at12 = steps;
  }
  const bool slow = menu::gameInfo(game).slowPacman;
  const int took = at12 - at2;
  if(at12 < 0 || (slow ? (took < 17 || took > 19) : (took < 14 || took > 16))) {
    std::snprintf(text, sizeof text, "Pac-Man took %d frames for 10 pixels (%s expected)", took, slow ? "slower: 17-19" : "faster: 14-16");
    return text;
  }
  if(recording) *recording = run.given;
  if(end) *end = snapshotOf(run.emulator);
  return "";
}

// The inputs of a finished run (the presses of the start included), given again to a fresh console, must end in the same picture and memory.
std::string replayCheck(const std::string& name, const std::vector<std::uint8_t>& rom, const std::vector<std::uint32_t>& inputs, const Snapshot& wanted)
{
  Emulator emulator;
  if(const std::string error = emulator.load(name, rom); !error.empty()) return error;
  for(std::uint32_t bits : inputs) { applyInput(emulator, controls::Input::unpack(bits), false); emulator.runFrame(); }
  return snapshotOf(emulator) == wanted ? "" : "a replay of the recorded inputs ended differently";
}

}  // namespace

int menuCheck(const Options& o)
{
  std::ofstream report(o.menuCheckFile);
  const auto found = rom::findQuietly(o.romArg, o.settingsFile, o.romsDir);
  std::vector<std::uint8_t> bytes;
  if(!report || !found || found->check.status != rom::Status::Ok || !readRomBytes(found->check.path, bytes)) { if(report) report << "result=no usable ROM\n"; return 5; }
  const std::string name = fileNameOf(found->check.path);

  const struct { Start start; const char* label; } starts[] = {{Start::Title, "title screen"}, {Start::InPlay, "game in play"}, {Start::TwoPlayerChosen, "two players chosen with Select"}};
  int passed = 0, failed = 0;
  for(const auto& s : starts)
    for(int game = 1; game <= menu::kGames; ++game) {
      std::vector<std::uint32_t> recording;
      Snapshot end;
      std::string problem = scenario(name, bytes, s.start, game, &recording, &end);
      if(problem.empty()) problem = replayCheck(name, bytes, recording, end);
      report << (problem.empty() ? "PASS  " : "FAIL  ") << s.label << ", game " << game << (problem.empty() ? "" : ": " + problem) << '\n';
      (problem.empty() ? passed : failed)++;
    }
  report << "scenarios_passed=" << passed << "\nscenarios_failed=" << failed << "\nresult=" << (failed == 0 ? "OK" : "FAIL") << '\n';
  return failed == 0 ? 0 : 2;
}

}  // namespace host
