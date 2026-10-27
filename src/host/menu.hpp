// The Escape menu (Stage I5, DECISIONS.md D-010 and T-I5-1): what the menu offers, how its keys work, what each choice says it does,
// and the Select and Reset presses that carry a choice into the game. No window code, so it can be tested on its own.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "background.hpp"
#include "crt.hpp"
#include "framerate.hpp"

namespace menu {

enum class Key { Up, Down, Left, Right, Enter, Escape };
enum class Item { Resume, NewGame, GameOptions, Graphics, Quit,  // the main page
                  Game, Players, LeftDifficulty, RightDifficulty,  // the "Game options" page (Stage I8)
                  FrameRate, FrameCounter,                 // and its next two rows (Stage I9.5)
                  Crt, Hdr, HdrHighlights,                 // on the "Graphics options" page, after the TV type (Stages I11 and I10)
                  Background, TvType, Preview,                     // the "Graphics options" page (Stage I7; the TV type moved there in I8)
                  Back };                                          // the last row of both pages
enum class Page { Main, GameOptions, Graphics };
enum class Result { None, Resume, StartGame, Quit };

inline constexpr int kGames = 8;  // the console's Game Select offers 8 games, each for one or two players (docs\F17_STATES.md)

// What the player has chosen. The difficulty switches and the TV type are real switches: they take effect at once.
struct Choice {
  int game = 1;  // 1..8
  bool twoPlayers = false;  // two players take turns on the same controls (D-011)
  bool leftA = false, rightA = false, blackWhite = false;
  background::Kind background = background::Kind::Black;  // what fills the space around the picture (Stage I7); takes effect at once
  framerate::Rate frameRate = framerate::Rate::Fps60;      // how often the window shows a new picture (Stage I9.5); takes effect at once
  bool frameCounter = false;                               // a small counter of the pictures a second in the corner of the window; takes effect at once
  crt::Filter crt = crt::Filter::Off;                      // the CRT filter the picture goes through (Stage I11, D-017); takes effect at once
  bool hdr = true;                                         // HDR output when Windows is in HDR mode (Stage I10); on unless the owner turns it off
  int hdrStrength = 50;                                    // how bright the highlights are, 0 to 100 (percent of the way to the most the screen can show)
  bool hdrWindows = false;                                 // Windows is in HDR mode for the screen now (for the explanation only)
  bool smoothAvailable = true;                             // false in the reference program, which has no smooth motion: the rates above 60 are skipped
};

// What differs between the eight games (docs\F16_LEVELS.md, docs\F10_PACMAN.md; checked by `pacman2600 --menu-check`).
struct GameInfo {
  int startLevel;  // the level the game starts at (ghost speed and chase timer follow the level, up to 14)
  bool slowPacman;  // Pac-Man moves at 0.562 pixel a frame instead of 0.688
};
GameInfo gameInfo(int game);

// The game's own "game number" byte (`$84`, 2..$11) counts the 16 steps of Select: game 1 for one player is 6, then 7 is game 1 for two
// players, 8 is game 2, ... $11 is game 6 for two players, then 2 (game 7) ... 5 (game 8, two players), then 6 again.
int gameOfSelect(std::uint8_t gameSelect);  // 1..8 (a byte outside 2..$11 counts as the power-on value 6)
bool twoPlayersOfSelect(std::uint8_t gameSelect);
int pressesNeeded(std::uint8_t currentGameSelect, int game, bool twoPlayers = false);  // Select presses that lead to this game (0..15)

// Cuts a text into lines of at most `width` characters at spaces.
std::vector<std::string> wrap(const std::string& text, int width);

class Menu {
 public:
  void open(const Choice& current);
  bool isOpen() const { return open_; }
  void close() { open_ = false; }
  Result press(Key key);

  const Choice& choice() const { return choice_; }
  Item cursor() const { return shown()[static_cast<std::size_t>(cursor_)]; }
  static const std::vector<Item>& items();          // the rows of the main page
  static const std::vector<Item>& gameItems();      // the rows of the "Game options" page
  static const std::vector<Item>& graphicsItems();  // the rows of the "Graphics options" page
  const std::vector<Item>& shown() const;
  Page page() const { return page_; }
  bool previewing() const { return previewing_; }  // the menu is hidden so that the background can be looked at; any key brings it back

  // The rows of the list, the marked one starting with '>'.
  std::vector<std::string> rows() const;
  // What the marked row does, in plain words, cut to `width` characters.
  std::vector<std::string> explanation(int width) const;

 private:
  void leaveSubPage();  // back to the main page, the cursor on the row that opened the page
  bool open_ = false, previewing_ = false;
  Page page_ = Page::Main;
  int cursor_ = 0;
  Choice choice_;
};

// Carries "start a new game" into the game as the console does it: Game Select pressed once for every step, then Game Reset held.
// One call of apply() per frame; it changes the Select and Reset inputs of that frame while a start is under way.
class GameStarter {
 public:
  static constexpr int kSelectFrames = 2, kGapFrames = 3, kWaitFrames = 5, kResetFrames = 10;
  void begin(int presses);
  bool active() const { return step_ >= 0; }
  void apply(bool& select, bool& reset);
  static int framesFor(int presses) { return presses * (kSelectFrames + kGapFrames) + kWaitFrames + kResetFrames; }

 private:
  int presses_ = 0, step_ = -1;
};

}  // namespace menu
