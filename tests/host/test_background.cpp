// Tests for src\host\background (Stage I7): the choices, the colours, the blurred grid, the score and lives read from the game's memory,
// and the "Graphics options" page of the menu. No window code. Expected values are worked out by hand from the rules in background.hpp.
// Usage: test_background
#include <cstdio>
#include <string>
#include <vector>

#include "../../src/host/background.hpp"
#include "../../src/host/menu.hpp"

static int failures = 0;
static int checks = 0;

static void expect(bool ok, const std::string& what)
{
  ++checks;
  if(!ok) { ++failures; std::printf("FAIL  %s\n", what.c_str()); }
}

int main()
{
  using namespace background;

  // The choices: six (the Solid colour one has no colour to choose: it is the maze colour), each with its own setting name, written and read back; anything unknown is Black.
  expect(kKinds == 6, "six backgrounds (the owner's list without the player's own image, and without the Mirrored edge, removed on request)");
  std::vector<std::string> seen;
  for(int i = 0; i < kKinds; ++i) {
    const Kind k = static_cast<Kind>(i);
    expect(fromSetting(settingName(k)) == k, std::string("setting name round trip: ") + settingName(k));
    expect(std::string(title(k)).size() > 3, std::string("a title for ") + settingName(k));
    for(const std::string& s : seen) expect(s != settingName(k), "setting names differ");
    seen.push_back(settingName(k));
  }
  expect(fromSetting("") == Kind::Black && fromSetting("nonsense") == Kind::Black && fromSetting("GLOW") == Kind::Black, "unknown or empty setting means Black");
  expect(next(Kind::Bezel, 1) == Kind::Black && next(Kind::Black, -1) == Kind::Bezel && next(Kind::Black, 1) == Kind::Glow, "Left and Right wrap round the list");

  // Colours.
  expect(scaled({100, 200, 255}, 0.5) == Rgb{50, 100, 128}, "scaled halves the channels (rounded)");
  expect(scaled({200, 200, 200}, 2.0) == Rgb{255, 255, 255} && scaled({10, 10, 10}, -1.0) == Rgb{0, 0, 0}, "scaled stays within 0..255");

  // The maze colour: the most frequent colour that is not black. A 4 x 4 picture: 9 black, 5 blue, 2 red (rows 1..3 are looked at).
  std::vector<std::uint32_t> pic(16, 0);
  for(int i : {4, 5, 6, 7, 8}) pic[static_cast<std::size_t>(i)] = 0x000000C0;
  for(int i : {9, 10}) pic[static_cast<std::size_t>(i)] = 0x00C00000;
  const Rgb dom = mazeColour(pic.data(), 4, 0, 4);
  expect(dom.b > 150 && dom.r < 40 && dom.g < 40, "the dominant colour is the blue, not the black");
  expect(mazeColour(pic.data(), 4, 3, 1) == defaultMazeColour(), "all-black rows give the default navy");
  expect(mazeColour(pic.data(), 4, 2, 2).r > 150, "only the rows asked for are counted (rows 2-3 hold the red)");

  // The blurred grid: a 4 x 4 picture whose left half is 200 grey and right half 100 grey, cut into 2 x 2 blocks.
  std::vector<std::uint32_t> halves(16);
  for(int y = 0; y < 4; ++y) for(int x = 0; x < 4; ++x) halves[static_cast<std::size_t>(y * 4 + x)] = x < 2 ? 0x00C8C8C8 : 0x00646464;
  std::vector<float> grid;
  blockAverages(halves.data(), 4, 0, 4, 2, 2, grid);
  expect(grid.size() == 12 && grid[0] == 200.0f && grid[3] == 100.0f && grid[6] == 200.0f && grid[9] == 100.0f, "block averages of a two-tone picture");
  blockAverages(halves.data(), 4, 0, 4, 1, 1, grid);
  expect(grid.size() == 3 && grid[0] == 150.0f && grid[2] == 150.0f, "one block is the average of everything");
  blockAverages(halves.data(), 4, 0, 4, 8, 8, grid);
  expect(grid.size() == 8 * 8 * 3, "more blocks than pixels still gives a full grid");

  // Score and lives from the game's memory ($80 is ram[0]).
  std::vector<std::uint8_t> ram(128, 0);
  auto at = [&](int address) -> std::uint8_t& { return ram[static_cast<std::size_t>(address - 0x80)]; };
  at(0xCE) = 0x12; at(0xCC) = 0x34;  // player 1: 1234
  at(0xCF) = 0x00; at(0xCD) = 0x70;  // player 2: 70
  expect(scoreOf(ram.data(), 0) == 1234 && scoreOf(ram.data(), 1) == 70, "scores are four decimal digits");
  at(0xCE) = 0xED; at(0xCC) = 0xCB;  // the game's blank patterns before the first game
  // $ED: first nibble 14 -> 0, second nibble 13 -> 0; $CB: 12 -> 0, 11 -> 0; so the score is 0
  expect(scoreOf(ram.data(), 0) == 0, "a nibble above 9 counts as 0 (blank pattern), so the score is 0");
  at(0x82) = 0; at(0x98) = 3;
  expect(livesOf(ram.data()) == 3, "lives while in play");
  at(0x82) = 0x5F;
  expect(livesOf(ram.data()) == 0, "no lives shown on the title screen or at game over");
  at(0x82) = 0; at(0x98) = 200;
  expect(livesOf(ram.data()) == 9, "lives are capped at 9");
  at(0xE6) = 0x00;
  expect(!twoPlayers(ram.data()) && playerToPlay(ram.data()) == 0, "one player");
  at(0xE6) = 0x40;
  expect(twoPlayers(ram.data()) && playerToPlay(ram.data()) == 0, "two players, player 1's turn");
  at(0xE6) = 0xC0;
  expect(twoPlayers(ram.data()) && playerToPlay(ram.data()) == 1, "two players, player 2's turn");

  expect(fromSetting("mirror") == Kind::Black, "a remembered Mirrored edge (removed) comes up as Black");

  // The page of the menu.
  using menu::Item; using menu::Key; using menu::Page; using menu::Result;
  menu::Menu m;
  menu::Choice start;
  m.open(start);
  expect(m.page() == Page::Main && m.choice().background == Kind::Black, "the menu opens on the main page with the black background");
  m.press(Key::Up); m.press(Key::Up); m.press(Key::Up); m.press(Key::Up);
  expect(m.cursor() == Item::Graphics, "the Graphics row is above Sound options, Control options and Quit");
  expect(m.press(Key::Right) == Result::None && m.page() == Page::Main, "Right on the Graphics row does not open it (only Enter does)");
  expect(m.press(Key::Enter) == Result::None && m.page() == Page::Graphics && m.isOpen(), "Enter opens the Graphics options page");
  expect(m.cursor() == Item::Background && m.rows().size() == menu::Menu::graphicsItems().size(), "the page starts on Background and has its own rows");
  bool shownTitle = false;
  for(const std::string& r : m.rows()) if(r.find("Background:") != std::string::npos && r.find("Black") != std::string::npos) shownTitle = true;
  expect(shownTitle, "the Background row shows the current choice");
  m.press(Key::Right);
  expect(m.choice().background == Kind::Glow, "Right: Ambient glow");
  for(int i = 0; i < 5; ++i) m.press(Key::Right);
  expect(m.choice().background == Kind::Black, "six steps round the list are back at Black");
  m.press(Key::Left);
  expect(m.choice().background == Kind::Bezel, "Left from Black wraps to the TV bezel");
  m.press(Key::Down);
  expect(m.cursor() == Item::TvType && !m.choice().blackWhite, "second row: the TV type (moved here in Stage I8)");
  m.press(Key::Right);
  expect(m.choice().blackWhite, "Right: black and white");
  m.press(Key::Left);
  expect(!m.choice().blackWhite, "Left: colour again");
  m.press(Key::Down);
  m.press(Key::Down); m.press(Key::Down); m.press(Key::Down);   // past the CRT filter and the two HDR rows (Stages I11 and I10)
  expect(m.cursor() == Item::Preview && !m.previewing(), "fifth row: hide the menu");
  m.press(Key::Right);
  expect(!m.previewing(), "Right does not hide the menu");
  m.press(Key::Enter);
  expect(m.previewing() && m.isOpen(), "Enter hides the menu for a look");
  expect(m.press(Key::Left) == Result::None && !m.previewing() && m.choice().background == Kind::Bezel, "any key brings it back and does nothing else");
  m.press(Key::Down);
  expect(m.cursor() == Item::Back, "sixth row: back");
  expect(m.press(Key::Enter) == Result::None && m.page() == Page::Main && m.cursor() == Item::Graphics, "Enter on Back returns to the main page, on the Graphics row");
  expect(m.choice().background == Kind::Bezel, "the choice is kept");
  m.press(Key::Enter);
  expect(m.page() == Page::Graphics, "open the page again");
  expect(m.press(Key::Escape) == Result::None && m.page() == Page::Main && m.isOpen(), "Escape on the page goes back, it does not close the menu");
  expect(m.press(Key::Escape) == Result::Resume && !m.isOpen(), "Escape on the main page closes the menu");
  menu::Choice kept;
  kept.background = Kind::Arcade;
  m.open(kept);
  expect(m.choice().background == Kind::Arcade && m.page() == Page::Main, "the menu opens with the background that is in use, on the main page");

  // Every row of the page has an explanation that fits, for every background.
  m.open(kept);
  m.press(Key::Up); m.press(Key::Up); m.press(Key::Enter);
  for(int kind = 0; kind < kKinds; ++kind) {
    menu::Choice c;
    c.background = static_cast<Kind>(kind);
    m.open(c);
    m.press(Key::Up); m.press(Key::Up); m.press(Key::Up); m.press(Key::Up); m.press(Key::Enter);
    for(std::size_t row = 0; row < menu::Menu::graphicsItems().size(); ++row) {
      const std::vector<std::string> words = m.explanation(60);
      bool fits = !words.empty();
      for(const std::string& w : words) if(w.size() > 60) fits = false;
      expect(fits, std::string("explanation of row ") + std::to_string(row) + " for " + settingName(static_cast<Kind>(kind)) + " exists and fits");
      if(row == 0) {
        bool names = false;
        for(const std::string& w : words) if(w.rfind(std::string(title(static_cast<Kind>(kind))) + ":", 0) == 0) names = true;
        expect(names, std::string("the Background explanation describes ") + title(static_cast<Kind>(kind)));
      }
      m.press(Key::Down);
    }
  }

  std::printf("%s (%d checks, %d failures)\n", failures == 0 ? "ALL TESTS PASSED" : "SOME TESTS FAILED", checks, failures);
  return failures == 0 ? 0 : 1;
}
