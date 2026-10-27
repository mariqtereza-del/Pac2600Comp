// The Escape menu: the choices, their keys, their explanations and the presses that start a game (Stage I5, T-I5-1).
#include "menu.hpp"

#include <algorithm>
#include <cstdio>

namespace menu {

// What each game changes: the start level (ghost speed, F16) and Pac-Man's speed (F10). Game 1 starts at level 8, game 2 at 10, 3 at 4, 4 at 6,
// then the same four again; Pac-Man is slower in games 7, 8 and 1 (his speed seed comes from the game number).
GameInfo gameInfo(int game)
{
  static constexpr int levels[kGames] = {8, 10, 4, 6, 8, 10, 4, 6};
  const int g = std::clamp(game, 1, kGames);
  return {levels[g - 1], g == 1 || g == 7 || g == 8};
}

// Where the game's counter stands as a step of the 16 (0 = byte value 2). A byte outside 2..$11 is treated as the power-on value 6.
static int stepOf(std::uint8_t gameSelect) { return (gameSelect < 2 || gameSelect > 0x11) ? 4 : gameSelect - 2; }

int gameOfSelect(std::uint8_t gameSelect) { return (stepOf(gameSelect) / 2 + 6) % kGames + 1; }

bool twoPlayersOfSelect(std::uint8_t gameSelect) { return (stepOf(gameSelect) & 1) != 0; }

int pressesNeeded(std::uint8_t currentGameSelect, int game)
{
  const int target = (4 + 2 * (std::clamp(game, 1, kGames) - 1)) % 16;  // game 1, one player, is step 4 (byte value 6)
  return (target - stepOf(currentGameSelect) + 16) % 16;
}

std::vector<std::string> wrap(const std::string& text, int width)
{
  std::vector<std::string> lines;
  std::string line, word;
  auto flush = [&]() {
    if(word.empty()) return;
    if(!line.empty() && static_cast<int>(line.size() + 1 + word.size()) > width) { lines.push_back(line); line.clear(); }
    line += (line.empty() ? "" : " ") + word;
    word.clear();
  };
  for(char c : text) {
    if(c == ' ') flush(); else word += c;
  }
  flush();
  if(!line.empty()) lines.push_back(line);
  return lines;
}

const std::vector<Item>& Menu::items()
{
  static const std::vector<Item> all = {Item::Resume, Item::NewGame, Item::Game, Item::LeftDifficulty, Item::RightDifficulty, Item::TvType, Item::Quit};
  return all;
}

void Menu::open(const Choice& current)
{
  choice_ = current;
  choice_.game = std::clamp(choice_.game, 1, kGames);
  cursor_ = 0;
  open_ = true;
}

Result Menu::press(Key key)
{
  if(!open_) return Result::None;
  const int count = static_cast<int>(items().size());
  if(key == Key::Escape) { open_ = false; return Result::Resume; }
  if(key == Key::Up) { cursor_ = (cursor_ + count - 1) % count; return Result::None; }
  if(key == Key::Down) { cursor_ = (cursor_ + 1) % count; return Result::None; }
  const int step = key == Key::Left ? -1 : 1;  // Right and Enter both move a value on
  switch(cursor()) {
    case Item::Game:
      choice_.game = (choice_.game - 1 + step + kGames) % kGames + 1;
      return Result::None;
    case Item::LeftDifficulty: choice_.leftA = !choice_.leftA; return Result::None;
    case Item::RightDifficulty: choice_.rightA = !choice_.rightA; return Result::None;
    case Item::TvType: choice_.blackWhite = !choice_.blackWhite; return Result::None;
    case Item::Resume: if(key == Key::Enter) { open_ = false; return Result::Resume; } return Result::None;
    case Item::NewGame: if(key == Key::Enter) { open_ = false; return Result::StartGame; } return Result::None;
    case Item::Quit: if(key == Key::Enter) { open_ = false; return Result::Quit; } return Result::None;
  }
  return Result::None;
}

std::vector<std::string> Menu::rows() const
{
  char text[96];
  std::vector<std::string> out;
  for(Item item : items()) {
    switch(item) {
      case Item::Resume: std::snprintf(text, sizeof text, "Resume"); break;
      case Item::NewGame: std::snprintf(text, sizeof text, "New game with the choices below"); break;
      case Item::Game: std::snprintf(text, sizeof text, "Game:             < %d of %d >", choice_.game, kGames); break;
      case Item::LeftDifficulty: std::snprintf(text, sizeof text, "Left difficulty:  < %c >", choice_.leftA ? 'A' : 'B'); break;
      case Item::RightDifficulty: std::snprintf(text, sizeof text, "Right difficulty: < %c >", choice_.rightA ? 'A' : 'B'); break;
      case Item::TvType: std::snprintf(text, sizeof text, "TV type:          < %s >", choice_.blackWhite ? "Black & white" : "Colour"); break;
      case Item::Quit: std::snprintf(text, sizeof text, "Quit the program"); break;
    }
    out.push_back(std::string(item == cursor() ? "> " : "  ") + text);
  }
  return out;
}

// The explanation of the marked row. Times are frames at the console's 59.92 a second. All of it comes from the original program's code
// (docs\F9_INPUT.md, F10, F15, F16); the game number, level and Pac-Man's speed are also checked by `pacman2600 --menu-check`.
std::vector<std::string> Menu::explanation(int width) const
{
  std::vector<std::string> out;
  auto paragraph = [&](const std::string& text) {
    for(const std::string& line : wrap(text, width)) out.push_back(line);
  };
  switch(cursor()) {
    case Item::Resume:
      paragraph("Close the menu and carry on from the same moment. (Esc does the same.)");
      break;
    case Item::NewGame:
      paragraph("Start a new game with the choices below, as if you had set the console's switches, chosen the game with Game Select and pressed Game Reset. "
                "The game in progress is lost.");
      break;
    case Item::Game: {
      paragraph("The console's Game Select offers 8 one-player games. They differ in only two things: how fast the ghosts are (the game starts at a level; the "
                "ghosts get faster with the level, and every maze you clear adds one level, up to level 14) and how fast Pac-Man moves.");
      out.push_back("");
      out.push_back("  Game   Start level   Pac-Man");
      for(int g = 1; g <= kGames; ++g) {
        const GameInfo info = gameInfo(g);
        char line[64];
        std::snprintf(line, sizeof line, "%s%d      %2d            %s", g == choice_.game ? "> " : "  ", g, info.startLevel, info.slowPacman ? "slower" : "faster");
        out.push_back(line);
      }
      out.push_back("");
      paragraph("Pac-Man: slower = 0.56 pixel a frame, faster = 0.69 pixel a frame (about 20 percent quicker). Two-player games are not in this menu yet.");
      break;
    }
    case Item::LeftDifficulty:
      paragraph("The left switch of the console. B is the usual setting. It is the one that counts for player 1, so for every one-player game.");
      paragraph("A is the harder setting: after a power pill the ghosts stay scared half as long (about 2.7 seconds instead of 5.3, then they flash for 1 second), "
                "and the bonus item stays on the screen for 2 ticks instead of 3 (about 8.5 seconds instead of 12.8).");
      break;
    case Item::RightDifficulty:
      paragraph("The right switch of the console. It counts only for player 2 in a two-player game, so in a one-player game it changes nothing. "
                "A is the harder setting, as for the left switch.");
      break;
    case Item::TvType:
      paragraph("The console's TV Type switch. Colour is the usual setting. Black & white makes the game use shades of grey, as it did on a black and white TV. "
                "Nothing else changes.");
      break;
    case Item::Quit:
      paragraph("Close the program. (The window's close button does the same.)");
      break;
  }
  return out;
}

void GameStarter::begin(int presses)
{
  presses_ = std::max(0, presses);
  step_ = 0;
}

// Frame by frame: Select held for 2 frames and let go for 3, once for every step; then 5 frames of waiting; then Reset held for 10 frames.
void GameStarter::apply(bool& select, bool& reset)
{
  if(step_ < 0) return;
  const int pressesEnd = presses_ * (kSelectFrames + kGapFrames);
  if(step_ < pressesEnd) select = (step_ % (kSelectFrames + kGapFrames)) < kSelectFrames;
  else if(step_ >= pressesEnd + kWaitFrames) reset = true;
  if(++step_ >= framesFor(presses_)) step_ = -1;
}

}  // namespace menu
