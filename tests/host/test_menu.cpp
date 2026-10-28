// Tests for src\host\menu (Stage I5): the game numbers, the keys, the explanations and the presses that start a game.
// The expected values are worked out from the written rules (docs\F10_PACMAN.md, F16_LEVELS.md, F17_STATES.md, CONTROLS.md), not copied from the code.
// Usage: test_menu
#include <cstdio>
#include <set>
#include <string>

#include "../../src/host/menu.hpp"
#include "../../src/host/menu_session.hpp"

using namespace menu;

static int failures = 0;
static int checks = 0;

static void expect(bool ok, const std::string& what)
{
  ++checks;
  if(!ok) { ++failures; std::printf("FAIL  %s\n", what.c_str()); }
}

// The console's 16 Select steps from power-on: byte $84 starts at 6 (game 1, one player), goes up by one per press, from $11 back to 2.
static std::uint8_t byteAfter(int presses)
{
  int value = 6;
  for(int i = 0; i < presses; ++i) value = value == 0x11 ? 2 : value + 1;
  return static_cast<std::uint8_t>(value);
}

int main()
{
  // Game numbers: press n of Select from power-on shows game 1 + n/2 (one player when n is even), wrapping after 16 presses.
  for(int n = 0; n < 33; ++n) {
    const std::uint8_t b = byteAfter(n);
    expect(gameOfSelect(b) == (n / 2) % 8 + 1, "press " + std::to_string(n) + ": game " + std::to_string((n / 2) % 8 + 1));
    expect(twoPlayersOfSelect(b) == (n % 2 == 1), "press " + std::to_string(n) + ": players");
  }
  expect(gameOfSelect(0) == 1 && gameOfSelect(0xCB) == 1, "a byte outside 2..$11 is taken as the power-on game 1");

  // Presses needed: from every one of the 16 states to every game for one player; pressing that many times lands on it, never more than 15.
  bool allLand = true, withinSixteen = true;
  for(int start = 0; start < 16; ++start)
    for(int game = 1; game <= kGames; ++game) {
      const int presses = pressesNeeded(byteAfter(start), game);
      std::uint8_t b = byteAfter(start);
      for(int i = 0; i < presses; ++i) b = b == 0x11 ? 2 : static_cast<std::uint8_t>(b + 1);
      if(gameOfSelect(b) != game || twoPlayersOfSelect(b)) allLand = false;
      if(presses < 0 || presses > 15) withinSixteen = false;
    }
  expect(allLand, "pressing Select the number of times given always lands on the game, for one player");
  expect(withinSixteen, "never more than 15 presses");
  expect(pressesNeeded(6, 1) == 0, "at power-on game 1 needs no press");
  expect(pressesNeeded(6, 2) == 2, "from game 1 to game 2 (one player) takes two presses");
  expect(pressesNeeded(7, 1) == 15, "from game 1 for two players back to game 1 for one player takes fifteen");

  // The table of the games: start levels 8, 10, 4, 6, 8, 10, 4, 6; Pac-Man is slower in games 7, 8 and 1.
  const int levels[8] = {8, 10, 4, 6, 8, 10, 4, 6};
  const std::set<int> slow = {1, 7, 8};
  for(int g = 1; g <= 8; ++g) {
    expect(gameInfo(g).startLevel == levels[g - 1], "game " + std::to_string(g) + " start level");
    expect(gameInfo(g).slowPacman == (slow.count(g) > 0), "game " + std::to_string(g) + " Pac-Man speed");
  }

  // Two players: one more Select press than the same game for one player (the game number byte steps game 1 one player, game 1 two players, game 2 ...).
  for(int g = 1; g <= kGames; ++g)
    for(int from = 2; from <= 0x11; ++from) {
      const int one = pressesNeeded(static_cast<std::uint8_t>(from), g, false), two = pressesNeeded(static_cast<std::uint8_t>(from), g, true);
      expect(two == (one + 1) % 16, "two players need one press more than one player (game " + std::to_string(g) + ", from " + std::to_string(from) + ")");
    }
  expect(pressesNeeded(6, 1, true) == 1 && pressesNeeded(7, 1, false) == 15 && pressesNeeded(7, 1, true) == 0, "game 1 for two players is byte 7");

  // The keys.
  Menu m;
  expect(m.press(Key::Down) == Result::None && !m.isOpen(), "a closed menu ignores keys");
  Choice start;
  start.game = 5;
  m.open(start);
  expect(m.isOpen() && m.cursor() == Item::Resume && m.choice().game == 5, "opens on Resume with the current choice");
  m.press(Key::Up);
  expect(m.cursor() == Item::Quit, "Up from the first row goes to the last");
  m.press(Key::Down);
  expect(m.cursor() == Item::Resume, "Down from the last row goes to the first");
  m.press(Key::Down); m.press(Key::Down);
  expect(m.cursor() == Item::GameOptions && m.press(Key::Right) == Result::None && m.page() == Page::Main, "third row is Game options; Right does not open it");
  expect(m.press(Key::Enter) == Result::None && m.page() == Page::GameOptions && m.cursor() == Item::Game, "Enter opens the Game options page on the game");
  m.press(Key::Right);
  expect(m.choice().game == 6, "Right: next game");
  for(int i = 0; i < 3; ++i) m.press(Key::Right);
  expect(m.choice().game == 1, "Right past game 8 wraps to game 1");
  m.press(Key::Left);
  expect(m.choice().game == 8, "Left from game 1 wraps to game 8");
  m.press(Key::Down); m.press(Key::Right);
  expect(m.cursor() == Item::Players && m.choice().twoPlayers, "Players row: Right gives two players");
  m.press(Key::Left);
  expect(!m.choice().twoPlayers, "and Left gives one player again");
  m.press(Key::Down); m.press(Key::Right);
  expect(m.choice().leftA && !m.choice().rightA, "left difficulty to A");
  m.press(Key::Left);
  expect(!m.choice().leftA, "and back to B");
  m.press(Key::Down); m.press(Key::Enter);
  expect(m.choice().rightA, "Enter also moves a value on (right difficulty to A)");
  expect(m.cursor() == Item::RightDifficulty, "the right difficulty is followed by the frame rate and the frame counter (Stage I9.5)");
  m.press(Key::Down);
  expect(m.cursor() == Item::FrameRate && m.choice().frameRate == framerate::Rate::Fps60, "the frame rate row starts at 60");
  m.press(Key::Right);
  expect(m.choice().frameRate == framerate::Rate::Fps75, "Right: 75");
  m.press(Key::Right); m.press(Key::Right); m.press(Key::Right);
  expect(m.choice().frameRate == framerate::Rate::Unlocked, "75, 120, 144, Unlocked");
  m.press(Key::Right);
  expect(m.choice().frameRate == framerate::Rate::Fps30, "Unlocked goes round to 30");
  m.press(Key::Left);
  expect(m.choice().frameRate == framerate::Rate::Unlocked, "and Left goes back");
  m.press(Key::Enter);
  expect(m.choice().frameRate == framerate::Rate::Fps30, "Enter also moves it on");
  bool named = false;
  for(const std::string& r : m.rows()) if(r.find("Frame rate:") != std::string::npos && r.find("< 30 >") != std::string::npos) named = true;
  expect(named, "the row shows the frame rate");
  m.press(Key::Down);
  expect(m.cursor() == Item::FrameCounter && !m.choice().frameCounter, "the frame counter row starts off");
  m.press(Key::Right);
  expect(m.choice().frameCounter, "Right: the counter is on");
  m.press(Key::Left);
  expect(!m.choice().frameCounter, "and off again");
  m.press(Key::Down);
  expect(m.cursor() == Item::Back && m.press(Key::Right) == Result::None && m.page() == Page::GameOptions, "Right on Back does nothing");
  expect(m.press(Key::Enter) == Result::None && m.page() == Page::Main && m.cursor() == Item::GameOptions, "Enter on Back returns to the main page, on Game options");
  m.press(Key::Down);
  expect(m.cursor() == Item::Graphics && m.press(Key::Enter) == Result::None && m.page() == Page::Graphics, "the Graphics options page");
  m.press(Key::Down); m.press(Key::Right);
  expect(m.cursor() == Item::TvType && m.choice().blackWhite, "TV type is on the Graphics options page: black and white");
  m.press(Key::Down);
  expect(m.cursor() == Item::Crt && m.choice().crt == crt::Filter::Off, "the CRT filter row follows the TV type, off at the start (Stage I11)");
  m.press(Key::Right);
  expect(m.choice().crt == crt::Filter::Lottes, "Right: CRT-Lottes (the first port)");
  m.press(Key::Right);
  expect(m.choice().crt == crt::Filter::Geom, "Right: CRT-Geom (the second port, Stage I11.3)");
  m.press(Key::Right);
  expect(m.choice().crt == crt::Filter::GuestNtsc, "Right: GDV NTSC 2600 (the third port, Stage I11.4)");
  m.press(Key::Right);
  expect(m.choice().crt == crt::Filter::Off, "and round to Off again");
  m.press(Key::Left);
  expect(m.choice().crt == crt::Filter::GuestNtsc, "Left goes back");
  m.press(Key::Left); m.press(Key::Left); m.press(Key::Left);
  expect(m.choice().crt == crt::Filter::Off, "to Off");
  m.press(Key::Down);
  expect(m.cursor() == Item::Hdr && m.choice().hdr, "the HDR row follows the TV type on the Graphics options page, on at the start (Stage I10, D-016)");
  m.press(Key::Right);
  expect(!m.choice().hdr, "Right: HDR off");
  m.press(Key::Left);
  expect(m.choice().hdr, "Left: on again");
  m.press(Key::Down);
  expect(m.cursor() == Item::HdrHighlights && m.choice().hdrStrength == 50, "the highlights slider starts half way");
  m.press(Key::Right);
  expect(m.choice().hdrStrength == 55, "Right: 5 more");
  for(int i = 0; i < 20; ++i) m.press(Key::Right);
  expect(m.choice().hdrStrength == 100, "the slider stops at 100");
  for(int i = 0; i < 30; ++i) m.press(Key::Left);
  expect(m.choice().hdrStrength == 0, "and at 0");
  m.press(Key::Enter);
  expect(m.choice().hdrStrength == 5, "Enter moves it on by 5");
  bool bar = false;
  for(const std::string& r : m.rows()) if(r.find("HDR highlights:") != std::string::npos && r.find("[#" + std::string(19, '-') + "]") != std::string::npos && r.find("5%") != std::string::npos) bar = true;
  expect(bar, "the row shows the slider as a bar and a percentage");
  for(int i = 0; i < 20; ++i) m.press(Key::Enter);
  expect(m.choice().hdrStrength == 0, "Enter goes round from 100 to 0");

  expect(m.press(Key::Escape) == Result::None && m.page() == Page::Main && m.cursor() == Item::Graphics, "Escape goes back to the main page");
  m.press(Key::Down);
  expect(m.cursor() == Item::SoundOptions && m.press(Key::Right) == Result::None && m.page() == Page::Main, "Sound options follows Graphics options; Right does not open it (Stage I12)");
  expect(m.press(Key::Enter) == Result::None && m.page() == Page::Sound && m.cursor() == Item::Volume, "Enter opens the Sound options page on the volume");
  expect(m.rows().size() == 3 && Menu::soundItems().size() == 3, "the page has only the volume, the mute button and Back");
  expect(m.choice().volume == 100 && !m.choice().muted, "the volume starts at 100 and the sound is not muted");
  m.press(Key::Right);
  expect(m.choice().volume == 100, "the slider stops at 100");
  m.press(Key::Left);
  expect(m.choice().volume == 95, "Left: 5 less");
  for(int i = 0; i < 30; ++i) m.press(Key::Left);
  expect(m.choice().volume == 0, "and it stops at 0");
  m.press(Key::Enter);
  expect(m.choice().volume == 5, "Enter moves it on by 5");
  for(int i = 0; i < 20; ++i) m.press(Key::Enter);
  expect(m.choice().volume == 0, "Enter goes round from 100 to 0");
  for(int i = 0; i < 10; ++i) m.press(Key::Right);
  bool volumeBar = false;
  for(const std::string& r : m.rows()) if(r.find("Volume:") != std::string::npos && r.find("[" + std::string(10, '#') + std::string(10, '-') + "]") != std::string::npos && r.find("50%") != std::string::npos) volumeBar = true;
  expect(volumeBar, "the row shows the slider as a bar and a percentage");
  m.press(Key::Down);
  expect(m.cursor() == Item::Mute && !m.choice().muted, "the mute button follows the volume, off at the start");
  m.press(Key::Enter);
  expect(m.choice().muted && m.choice().volume == 50, "Enter mutes and leaves the volume where it was");
  bool muteRow = false;
  for(const std::string& r : m.rows()) if(r.find("Mute:") != std::string::npos && r.find("< On >") != std::string::npos) muteRow = true;
  expect(muteRow, "the row shows Mute On");
  m.press(Key::Left);
  expect(!m.choice().muted, "Left unmutes");
  m.press(Key::Down);
  expect(m.cursor() == Item::Back && m.press(Key::Enter) == Result::None && m.page() == Page::Main && m.cursor() == Item::SoundOptions, "Back returns to the main page, on Sound options");
  m.press(Key::Down);
  expect(m.cursor() == Item::ControlOptions && m.press(Key::Right) == Result::None && m.page() == Page::Main, "Control options follows Sound options; Right does not open it (Stage I13)");
  expect(m.press(Key::Enter) == Result::None && m.page() == Page::Controls && m.cursor() == Item::KeyboardKeys, "Enter opens the Control options page on the keyboard row");
  expect(m.rows().size() == 3 && Menu::controlItems().size() == 3, "the page has the keyboard row, the rumble row (Stage I15) and Back");
  expect(m.choice().keyboard == controls::Scheme::Both, "both sets of keys at the start (as it always was)");
  m.press(Key::Right);
  expect(m.choice().keyboard == controls::Scheme::Arrows, "Right: the arrow keys only");
  m.press(Key::Enter);
  expect(m.choice().keyboard == controls::Scheme::Wasd, "Enter moves it on: W A S D only");
  m.press(Key::Right);
  expect(m.choice().keyboard == controls::Scheme::Both, "and round to both");
  m.press(Key::Left);
  expect(m.choice().keyboard == controls::Scheme::Wasd, "Left goes back");
  bool keysShown = false, rowShown = false;
  for(const std::string& w : m.explanation(60)) {
    if(w.find("  A ") == 0 && w.find("Joystick left") != std::string::npos) keysShown = true;
    if(w.find("Up arrow") != std::string::npos) rowShown = true;
  }
  expect(keysShown && !rowShown, "the explanation lists the keys of the choice (W A S D, not the arrows)");
  for(const std::string& r : m.rows()) if(r.find("Keyboard:") != std::string::npos) rowShown = r.find("W A S D") != std::string::npos;
  expect(rowShown, "the row names the choice");
  // Stage I15: the rumble row (the light bar is not a choice: it is always on).
  m.press(Key::Down);
  expect(m.cursor() == Item::RumbleMotors && m.choice().rumble == feedback::Rumble::Off, "second row: the rumble, off to begin with");
  m.press(Key::Right);
  expect(m.choice().rumble == feedback::Rumble::Light, "Right: light");
  m.press(Key::Enter);
  expect(m.choice().rumble == feedback::Rumble::Strong, "Enter moves it on: strong");
  m.press(Key::Right);
  expect(m.choice().rumble == feedback::Rumble::Off, "and round to off");
  m.press(Key::Down);
  expect(m.cursor() == Item::Back && m.press(Key::Enter) == Result::None && m.page() == Page::Main && m.cursor() == Item::ControlOptions, "Back returns to the main page, on Control options");
  m.press(Key::Down); m.press(Key::Down); m.press(Key::Down);   // past the Achievements and Statistics rows (Stages I20 and I21) to Quit
  expect(m.cursor() == Item::Quit && m.press(Key::Right) == Result::None, "Right on Quit does nothing");
  expect(m.press(Key::Enter) == Result::Quit && !m.isOpen(), "Enter on Quit quits and closes");
  m.open(start);
  expect(m.press(Key::Escape) == Result::Resume && !m.isOpen(), "Escape closes the menu");
  m.open(start);
  expect(m.press(Key::Enter) == Result::Resume && !m.isOpen(), "Enter on Resume closes the menu");
  m.open(start);
  m.press(Key::Down);
  expect(m.cursor() == Item::NewGame && m.press(Key::Right) == Result::None && m.isOpen(), "Right on New game does nothing");
  expect(m.press(Key::Enter) == Result::StartGame && !m.isOpen() && m.choice().game == 5, "Enter on New game starts, with the choice");

  // The rows and the explanations: one marked row, every row has an explanation, none too wide, the facts of the games are in it.
  for(int page = 0; page < 7; ++page) {  // the main page, then Game options, Graphics options, Sound options, Control options, Achievements and Statistics
    m.open(start);
    if(page > 0) { for(int i = 0; i < 1 + page; ++i) m.press(Key::Down); m.press(Key::Enter); }
    const std::vector<Item>& items = page == 0 ? Menu::items() : page == 1 ? Menu::gameItems() : page == 2 ? Menu::graphicsItems() : page == 3 ? Menu::soundItems() : page == 4 ? Menu::controlItems() : page == 5 ? Menu::achievementItems() : Menu::statItems();
    expect(m.rows().size() == items.size(), "one text row per item, page " + std::to_string(page));
    for(std::size_t i = 0; i < items.size(); ++i) {
      int marked = 0;
      for(const std::string& r : m.rows()) if(r.rfind("> ", 0) == 0) ++marked;
      expect(marked == 1, "exactly one marked row at item " + std::to_string(i) + " of page " + std::to_string(page));
      const std::vector<std::string> words = m.explanation(60);
      bool fits = !words.empty();
      for(const std::string& w : words) if(w.size() > 60) fits = false;
      expect(fits, "the explanation of item " + std::to_string(i) + " of page " + std::to_string(page) + " exists and fits 60 columns");
      m.press(Key::Down);
    }
  }
  m.open(start);
  m.press(Key::Down); m.press(Key::Down); m.press(Key::Enter);  // Game options, on the game
  // the table rows of the explanation: "<mark> <game> <level> <speed>", the marked one is game 5
  auto row = [&](const std::string& startsWith) {
    for(const std::string& w : m.explanation(64)) if(w.rfind(startsWith, 0) == 0) return w;
    return std::string();
  };
  auto has = [](const std::string& line, const std::string& word) { return line.find(word) != std::string::npos; };
  expect(has(row("> 5"), " 8 ") && has(row("> 5"), "faster"), "the table marks game 5 (level 8, faster)");
  expect(has(row("  1 "), " 8 ") && has(row("  1 "), "slower") && has(row("  7 "), " 4 ") && has(row("  7 "), "slower"), "games 1 and 7 are slower");
  expect(has(row("  2 "), "10") && has(row("  2 "), "faster") && has(row("  3 "), " 4 ") && has(row("  6 "), "10"), "games 2, 3 and 6 as in the table");

  // A program without smooth motion (the reference program) goes round 30 and 60 only.
  Choice limited;
  limited.smoothAvailable = false;
  m.open(limited);
  m.press(Key::Down); m.press(Key::Down); m.press(Key::Enter);
  m.press(Key::Down); m.press(Key::Down); m.press(Key::Down); m.press(Key::Down);
  expect(m.cursor() == Item::FrameRate && m.choice().frameRate == framerate::Rate::Fps60, "the frame rate row of a program without smooth motion starts at 60");
  m.press(Key::Right);
  expect(m.choice().frameRate == framerate::Rate::Fps30, "60 goes round to 30, past the smooth ones");
  m.press(Key::Right);
  expect(m.choice().frameRate == framerate::Rate::Fps60, "and back to 60");

  // The path the window uses (Session::open with what the window knows): a program with smooth motion goes through all six rates, one without only 30 and 60.
  {
    using framerate::Rate;
    for(const bool smoothAvailable : {true, false}) {
      Session session(true);
      session.open(controls::Input{}, Session::Window{background::Kind::Black, Rate::Fps60, false, smoothAvailable});
      session.key(Key::Down); session.key(Key::Down); session.key(Key::Enter);                        // Game options
      session.key(Key::Down); session.key(Key::Down); session.key(Key::Down); session.key(Key::Down);   // the frame rate row
      std::string seen;
      for(int i = 0; i < 7; ++i) {
        seen += std::string(framerate::settingName(session.menu().choice().frameRate)) + " ";
        session.key(Key::Right);
      }
      expect(seen == (smoothAvailable ? "60 75 120 144 unlocked 30 60 " : "60 30 60 30 60 30 60 "), std::string("the rates the window's menu goes through, smooth motion ") + (smoothAvailable ? "available: " : "not available: ") + seen);
    }
  }

  // Wrapping.
  const auto lines = wrap("one two three four five six seven", 9);
  expect(lines.size() == 4 && lines[0] == "one two" && lines[1] == "three" && lines[3] == "six seven", "wrap cuts at spaces");
  expect(wrap("", 10).empty(), "wrap of nothing is nothing");

  // The presses of a new game: Select held 2 frames and let go 3, per step; 5 frames of waiting; Reset 10 frames.
  GameStarter s;
  expect(!s.active(), "idle at first");
  s.begin(2);
  std::string selects, resets;
  int frames = 0;
  while(s.active() && frames < 100) {
    bool select = false, reset = false;
    s.apply(select, reset);
    selects += select ? 'S' : '.';
    resets += reset ? 'R' : '.';
    ++frames;
  }
  expect(frames == GameStarter::framesFor(2) && frames == 25, "a start with 2 presses lasts 25 frames");
  expect(selects == std::string("SS...SS...") + std::string(15, '.'), "Select shape: " + selects);
  expect(resets == ".........." ".....RRRRRRRRRR", "Reset shape: " + resets);
  s.begin(0);
  frames = 0;
  int resetFrames = 0;
  bool anySelect = false;
  while(s.active() && frames < 100) { bool sel = false, res = false; s.apply(sel, res); anySelect |= sel; resetFrames += res; ++frames; }
  expect(frames == 15 && resetFrames == 10 && !anySelect, "a start with no press is 5 frames of waiting and 10 of Reset");

  // The shared part: a start is queued for the game thread only when starting is allowed.
  Session allowed(true), refused(false);
  controls::Input in;
  std::uint8_t ram[128] = {};
  ram[0x84 - 0x80] = 6;
  allowed.publish(ram);
  refused.publish(ram);
  allowed.open(in);
  refused.open(in);
  expect(allowed.paused() && refused.paused(), "an open menu pauses the game");
  allowed.key(Key::Down); allowed.key(Key::Down); allowed.key(Key::Enter); allowed.key(Key::Right);  // Game options, game 2
  refused.key(Key::Down); refused.key(Key::Down); refused.key(Key::Enter); refused.key(Key::Right);
  allowed.key(Key::Escape); allowed.key(Key::Up);  // back, up to New game
  refused.key(Key::Escape); refused.key(Key::Up);
  expect(allowed.key(Key::Enter) == Result::StartGame && !allowed.paused(), "starting closes the menu and lets the game run");
  refused.key(Key::Enter);
  int selectFrames = 0, resetCount = 0;
  for(int f = 0; f < 40; ++f) { controls::Input x; allowed.apply(x); selectFrames += x.select; resetCount += x.reset; }
  expect(selectFrames == 4 && resetCount == 10, "the game thread gets 2 presses of 2 frames and then Reset");
  selectFrames = resetCount = 0;
  for(int f = 0; f < 40; ++f) { controls::Input x; refused.apply(x); selectFrames += x.select; resetCount += x.reset; }
  expect(selectFrames == 0 && resetCount == 0, "a refused start (a replay is running) adds no input");
  allowed.open(in);
  expect(allowed.menu().choice().game == 1, "the menu opens on the game the game number byte says (6 = game 1)");

  // Stage I20: the Achievements page. Fourteen rows and Back; an unlocked one is marked [x], with its day in the explanation.
  {
    menu::Choice withProgress = start;
    withProgress.progress.unlocked = 0x2001;   // A Little Snack and Blue Plate Special
    withProgress.progress.dates[0] = "2026-10-29";
    withProgress.progress.dates[13] = "2026-11-02";
    m.open(withProgress);
    for(int i = 0; i < 6; ++i) m.press(Key::Down);
    expect(m.cursor() == Item::Achievements, "the Achievements row follows Control options");
    bool countShown = false;
    for(const std::string& r : m.rows()) if(r.find("Achievements...") != std::string::npos && r.find("(2 of 14)") != std::string::npos) countShown = true;
    expect(countShown, "the row says how many are unlocked: 2 of 14");
    expect(m.press(Key::Right) == Result::None && m.page() == Page::Main, "Right does not open the page (only Enter does)");
    expect(m.press(Key::Enter) == Result::None && m.page() == Page::Achievements && m.cursorIndex() == 0, "Enter opens the page on the first achievement");
    expect(m.rows().size() == 15 && Menu::achievementItems().size() == 15, "fourteen achievements and Back");
    const std::vector<std::string> rows = m.rows();
    expect(rows[0] == "> [x] A Little Snack" && rows[1] == "  [ ] What's for Breakfast?" && rows[13] == "  [x] Blue Plate Special" && rows[9].find("Pac-Man Fever") != std::string::npos,
           "the rows show the title and [x] or [ ]; the marked row starts with >");
    bool dayShown = false, creditShown = false, descriptionShown = false;
    for(const std::string& w : m.explanation(60)) {
      if(w.find("Unlocked on 2026-10-29") != std::string::npos) dayShown = true;
      if(w.find("retroachievements.org/game/13094") != std::string::npos || w.find("RetroAchievements") != std::string::npos) creditShown = true;
      if(w.find("Score 500 points") != std::string::npos) descriptionShown = true;
    }
    expect(dayShown && creditShown && descriptionShown, "the explanation gives the description, the day it was unlocked and the credit to RetroAchievements");
    m.press(Key::Down);
    bool notYet = false;
    for(const std::string& w : m.explanation(60)) if(w.find("Not unlocked yet.") != std::string::npos) notYet = true;
    expect(m.cursorIndex() == 1 && notYet, "a locked one says it is not unlocked yet");
    expect(m.press(Key::Enter) == Result::None && m.page() == Page::Achievements, "Enter on an achievement does nothing");
    while(m.cursor() != Item::Back) m.press(Key::Down);
    expect(m.press(Key::Enter) == Result::None && m.page() == Page::Main && m.cursor() == Item::Achievements, "Back returns to the main page, on the Achievements row");
    m.press(Key::Enter);
    expect(m.press(Key::Escape) == Result::None && m.page() == Page::Main && m.isOpen(), "Escape on the page goes back");
  }

  // Stage I21: the Statistics page. Nine read-only rows and Back; the values come from the totals the menu was opened with.
  {
    menu::Choice withTotals = start;
    withTotals.totals.games = 12;
    withTotals.totals.playFrames = 60 * 3725;   // 3,725 seconds at about 60 frames a second: a little over an hour
    withTotals.totals.highScore = 12345;
    withTotals.totals.wafers = 1234567;
    m.open(withTotals);
    for(int i = 0; i < 7; ++i) m.press(Key::Down);
    expect(m.cursor() == Item::Statistics, "the Statistics row follows Achievements");
    expect(m.press(Key::Right) == Result::None && m.page() == Page::Main, "Right does not open the page (only Enter does)");
    bool summaryShown = false;
    std::string summary;
    for(const std::string& w : m.explanation(60)) summary += w + " ";   // the lines are cut at 60 columns: look at the whole text
    summaryShown = summary.find("12 games") != std::string::npos && summary.find("12,345") != std::string::npos;
    expect(summaryShown, "the main page's explanation gives the games and the high score");
    expect(m.press(Key::Enter) == Result::None && m.page() == Page::Statistics && m.cursorIndex() == 0, "Enter opens the page on the first row");
    expect(m.rows().size() == 10 && Menu::statItems().size() == 10, "nine totals and Back");
    const std::vector<std::string> rows = m.rows();
    expect(rows[0].find("> Games played:") == 0 && rows[0].find("12") != std::string::npos, "the first row is the games played: " + rows[0]);
    expect(rows[2].find("High score:") != std::string::npos && rows[2].find("12,345") != std::string::npos, "the high score has a thousands separator: " + rows[2]);
    expect(rows[3].find("1,234,567") != std::string::npos, "so do the wafers: " + rows[3]);
    expect(rows[1].find("Time played:") != std::string::npos && rows[1].find("1 h 02 min") != std::string::npos, "the time is in hours and minutes: " + rows[1]);
    bool explained = false, privacy = false;
    for(const std::string& w : m.explanation(60)) {
      if(w.find("started") != std::string::npos) explained = true;
      if(w.find("Nothing is sent") != std::string::npos) privacy = true;
    }
    expect(explained && privacy, "the explanation says what is counted and that nothing is sent anywhere");
    expect(m.press(Key::Enter) == Result::None && m.page() == Page::Statistics && m.press(Key::Left) == Result::None && m.page() == Page::Statistics, "Enter and Left on a total do nothing");
    while(m.cursor() != Item::Back) m.press(Key::Down);
    expect(m.press(Key::Enter) == Result::None && m.page() == Page::Main && m.cursor() == Item::Statistics, "Back returns to the main page, on the Statistics row");
    m.press(Key::Enter);
    expect(m.press(Key::Escape) == Result::None && m.page() == Page::Main && m.isOpen(), "Escape on the page goes back");
  }

  std::printf("%d checks, %d failures\n", checks, failures);
  if(failures == 0) std::printf("ALL TESTS PASSED\n");
  return failures == 0 ? 0 : 1;
}
