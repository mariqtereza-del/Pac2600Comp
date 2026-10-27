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

int pressesNeeded(std::uint8_t currentGameSelect, int game, bool twoPlayers)
{
  const int target = (4 + 2 * (std::clamp(game, 1, kGames) - 1) + (twoPlayers ? 1 : 0)) % 16;  // game 1, one player, is step 4 (byte value 6)
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
  static const std::vector<Item> all = {Item::Resume, Item::NewGame, Item::GameOptions, Item::Graphics, Item::Quit};
  return all;
}

const std::vector<Item>& Menu::gameItems()
{
  static const std::vector<Item> all = {Item::Game, Item::Players, Item::LeftDifficulty, Item::RightDifficulty, Item::FrameRate, Item::FrameCounter, Item::Back};
  return all;
}

const std::vector<Item>& Menu::graphicsItems()
{
  static const std::vector<Item> all = {Item::Background, Item::TvType, Item::Preview, Item::Back};
  return all;
}

const std::vector<Item>& Menu::shown() const
{
  switch(page_) {
    case Page::GameOptions: return gameItems();
    case Page::Graphics: return graphicsItems();
    case Page::Main: break;
  }
  return items();
}

void Menu::open(const Choice& current)
{
  choice_ = current;
  choice_.game = std::clamp(choice_.game, 1, kGames);
  cursor_ = 0;
  page_ = Page::Main;
  previewing_ = false;
  open_ = true;
}

Result Menu::press(Key key)
{
  if(!open_) return Result::None;
  const int count = static_cast<int>(shown().size());
  if(previewing_) { previewing_ = false; return Result::None; }  // any key brings the menu back
  if(key == Key::Escape && page_ != Page::Main) { leaveSubPage(); return Result::None; }
  if(key == Key::Escape) { open_ = false; return Result::Resume; }
  if(key == Key::Up) { cursor_ = (cursor_ + count - 1) % count; return Result::None; }
  if(key == Key::Down) { cursor_ = (cursor_ + 1) % count; return Result::None; }
  const int step = key == Key::Left ? -1 : 1;  // Right and Enter both move a value on
  switch(cursor()) {
    case Item::Game:
      choice_.game = (choice_.game - 1 + step + kGames) % kGames + 1;
      return Result::None;
    case Item::GameOptions: if(key == Key::Enter) { page_ = Page::GameOptions; cursor_ = 0; } return Result::None;
    case Item::Graphics: if(key == Key::Enter) { page_ = Page::Graphics; cursor_ = 0; } return Result::None;
    case Item::Back: if(key == Key::Enter) leaveSubPage(); return Result::None;
    case Item::Background: choice_.background = background::next(choice_.background, step); return Result::None;
    case Item::Preview: if(key == Key::Enter) previewing_ = true; return Result::None;
    case Item::Players: choice_.twoPlayers = !choice_.twoPlayers; return Result::None;
    case Item::LeftDifficulty: choice_.leftA = !choice_.leftA; return Result::None;
    case Item::RightDifficulty: choice_.rightA = !choice_.rightA; return Result::None;
    case Item::TvType: choice_.blackWhite = !choice_.blackWhite; return Result::None;
    case Item::FrameRate:
      do choice_.frameRate = framerate::next(choice_.frameRate, step);
      while(!choice_.smoothAvailable && framerate::smooth(choice_.frameRate));   // a program without smooth motion goes round 30 and 60 only
      return Result::None;
    case Item::FrameCounter: choice_.frameCounter = !choice_.frameCounter; return Result::None;
    case Item::Resume: if(key == Key::Enter) { open_ = false; return Result::Resume; } return Result::None;
    case Item::NewGame: if(key == Key::Enter) { open_ = false; return Result::StartGame; } return Result::None;
    case Item::Quit: if(key == Key::Enter) { open_ = false; return Result::Quit; } return Result::None;
  }
  return Result::None;
}

void Menu::leaveSubPage()
{
  const Item opener = page_ == Page::GameOptions ? Item::GameOptions : Item::Graphics;
  page_ = Page::Main;
  cursor_ = static_cast<int>(std::find(items().begin(), items().end(), opener) - items().begin());
}

std::vector<std::string> Menu::rows() const
{
  char text[96];
  std::vector<std::string> out;
  for(Item item : shown()) {
    switch(item) {
      case Item::Resume: std::snprintf(text, sizeof text, "Resume"); break;
      case Item::NewGame: std::snprintf(text, sizeof text, "New game with the Game options"); break;
      case Item::Game: std::snprintf(text, sizeof text, "Game:             < %d of %d >", choice_.game, kGames); break;
      case Item::Players: std::snprintf(text, sizeof text, "Players:          < %d >", choice_.twoPlayers ? 2 : 1); break;
      case Item::LeftDifficulty: std::snprintf(text, sizeof text, "Left difficulty:  < %c >", choice_.leftA ? 'A' : 'B'); break;
      case Item::RightDifficulty: std::snprintf(text, sizeof text, "Right difficulty: < %c >", choice_.rightA ? 'A' : 'B'); break;
      case Item::FrameRate: std::snprintf(text, sizeof text, "Frame rate:       < %s >", framerate::title(choice_.frameRate)); break;
      case Item::FrameCounter: std::snprintf(text, sizeof text, "Frame counter:    < %s >", choice_.frameCounter ? "On" : "Off"); break;
      case Item::TvType: std::snprintf(text, sizeof text, "TV type:      < %s >", choice_.blackWhite ? "Black & white" : "Colour"); break;
      case Item::GameOptions: std::snprintf(text, sizeof text, "Game options..."); break;
      case Item::Graphics: std::snprintf(text, sizeof text, "Graphics options..."); break;
      case Item::Quit: std::snprintf(text, sizeof text, "Quit the program"); break;
      case Item::Background: std::snprintf(text, sizeof text, "Background:   < %s >", background::title(choice_.background)); break;
      case Item::Preview: std::snprintf(text, sizeof text, "Hide this menu to look at the background"); break;
      case Item::Back: std::snprintf(text, sizeof text, "Back to the main menu"); break;
    }
    out.push_back(std::string(item == cursor() ? "> " : "  ") + text);
  }
  return out;
}

// What each background does, in plain words (the Background row's explanation).
static std::string backgroundText(background::Kind kind)
{
  switch(kind) {
    case background::Kind::Black: return "Black: nothing is drawn, as on the original TV. This is the usual setting.";
    case background::Kind::Glow: return "Ambient glow: the picture's own colours, enlarged and blurred, glow behind it and follow the action, like the backlight of a modern TV.";
    case background::Kind::Mirror: return "Mirrored edge: the picture is reflected as in a mirror on every side, darkened.";
    case background::Kind::Solid: return "Solid colour: one flat colour, the colour of the maze (the blue it is drawn on, not its walls), as it is right now.";
    case background::Kind::Gradient: return "Gradient: the colour of the maze, fairly bright next to the picture and fading to black at the edge of the window.";
    case background::Kind::Arcade:
      return "Arcade panel: in the style of an arcade cabinet, the title, the score of each player and the lives left, drawn large around the picture. "
             "Where the space is too thin for readable text only the frame is drawn.";
    case background::Kind::Bezel:
      return "TV bezel: a drawn TV set around the picture, with a wooden cabinet, a dark bevelled frame, knobs and a power light, and speaker slits, as far as "
             "the space allows.";
  }
  return "";
}

// The explanation of the Frame rate row: what each rate shows, and, in the reference program, why only two are offered.
static void appendFrameRateText(std::vector<std::string>& out, int width, bool smoothAvailable)
{
  auto paragraph = [&](const std::string& text) {
    for(const std::string& line : wrap(text, width)) out.push_back(line);
  };
  paragraph("How often the window shows a new picture. The game itself always runs at the console's own speed, with the same sound, whatever you choose: "
            "this only changes how smooth the movement looks.");
  out.push_back("");
  paragraph("60 shows the game's own pictures, one for each step of the game, exactly as the original console does (the usual setting). 30 shows every second one. "
            "75, 120, 144 and Unlocked also draw Pac-Man, the ghosts and the power pills in between, so they glide instead of jumping a pixel at a time; "
            "the ghosts still flicker as in the original, and nothing is ever shown later than the original shows it. A screen cannot show more pictures "
            "than its own refresh rate. Unlocked draws as often as the window can.");
  if(!smoothAvailable) {
    out.push_back("");
    paragraph("THIS IS THE REFERENCE PROGRAM (pacman2600_ref.exe, the one with the emulator inside, used only by the tests): it has no smooth motion, so only 30 and 60 "
              "are offered. Start pacman2600.exe for all six.");
  }
}

// The table of the eight games in the explanation of the Game row: the start level and Pac-Man's speed, the chosen game marked.
static void appendGameTable(std::vector<std::string>& out, int chosen)
{
      out.push_back("  Game   Start level   Pac-Man");
      for(int g = 1; g <= kGames; ++g) {
        const GameInfo info = gameInfo(g);
        char line[64];
        std::snprintf(line, sizeof line, "%s%d      %2d            %s", g == chosen ? "> " : "  ", g, info.startLevel, info.slowPacman ? "slower" : "faster");
        out.push_back(line);
      }
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
      paragraph("The console's Game Select offers 8 games, each for one or two players. They differ in only two things: how fast the ghosts are (the game starts at a level; the "
                "ghosts get faster with the level, and every maze you clear adds one level, up to level 14) and how fast Pac-Man moves.");
      out.push_back("");
      appendGameTable(out, choice_.game);
      out.push_back("");
      paragraph("Pac-Man: slower = 0.56 pixel a frame, faster = 0.69 pixel a frame (about 20 percent quicker). The game number is the same for one or two players (see Players).");
      break;
    }
    case Item::Players:
      paragraph("One player, or two players who take turns. In a two-player game player 1 plays until Pac-Man is caught, then player 2 plays on a maze and "
                "score of their own, and so on until the game is over. Both players use the same keys or pad (on the console, player 2 "
                "would use the second joystick); the game takes the input of whoever's turn it is.");
      paragraph("Player 1 plays with the left difficulty switch and player 2 with the right one.");
      break;
    case Item::LeftDifficulty:
      paragraph("The left switch of the console. B is the usual setting. It is the one that counts for player 1, so for every one-player game.");
      paragraph("A is the harder setting: after a power pill the ghosts stay scared half as long (about 2.7 seconds instead of 5.3, then they flash for 1 second), "
                "and the bonus item stays on the screen for 2 ticks instead of 3 (about 8.5 seconds instead of 12.8).");
      break;
    case Item::RightDifficulty:
      paragraph("The right switch of the console. It counts only for player 2 in a two-player game, so in a one-player game it changes nothing. "
                "A is the harder setting, as for the left switch.");
      break;
    case Item::FrameRate:
      appendFrameRateText(out, width, choice_.smoothAvailable);
      break;
    case Item::FrameCounter:
      paragraph("Shows a small counter in the top left corner of the window: how many different pictures are shown each second. Nothing else changes.");
      break;
    case Item::TvType:
      paragraph("The console's TV Type switch. Colour is the usual setting. Black & white makes the game use shades of grey, as it did on a black and white TV. "
                "Nothing else changes. It takes effect at once.");
      break;
    case Item::GameOptions:
      paragraph("The game to play (1 to 8), one or two players, and the left and right difficulty switches. New game starts with these choices. Enter opens the page.");
      break;
    case Item::Graphics:
      paragraph("How the picture looks: what fills the space around it, and the TV type (colour or black and white). Enter opens the page.");
      break;
    case Item::Quit:
      paragraph("Close the program. (The window's close button does the same.)");
      break;
    case Item::Background: {
      paragraph("What fills the space around the game picture. The picture itself, always in its true 12:7 pixel shape and as large as the window allows, is "
                "never changed or covered, and nothing here changes the game. The choice takes effect at once and is remembered.");
      out.push_back("");
      paragraph(backgroundText(choice_.background));
      break;
    }
    case Item::Preview:
      paragraph("Hides this menu so that the background can be seen in full; any key brings the menu back. The game stays paused.");
      break;
    case Item::Back:
      paragraph("Back to the main page of the menu. (Esc does the same.)");
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
