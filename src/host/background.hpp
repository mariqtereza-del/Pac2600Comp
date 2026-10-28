// What fills the space around the game picture (Stage I7, DECISIONS.md D-012 and T-I7-1): the choices, their names in settings.ini and in the
// menu, the colours, and the small calculations the drawing needs (dominant colour, blurred grid, score and lives read from the game's memory).
// No window code, so all of it can be tested on its own. The drawing itself is in backdrop.cpp.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace background {

enum class Kind { Black, Glow, Solid, Gradient, Arcade, Bezel };
inline constexpr int kKinds = 6;

const char* settingName(Kind kind);          // "black", "glow", "solid", "gradient", "arcade", "bezel" (settings.ini)
Kind fromSetting(const std::string& text);   // anything else (also an empty text) is Black
const char* title(Kind kind);                // as the menu shows it
Kind next(Kind kind, int step);              // the menu's Left/Right, wrapping round

struct Rgb {
  std::uint8_t r = 0, g = 0, b = 0;
  bool operator==(const Rgb& o) const { return r == o.r && g == o.g && b == o.b; }
};
Rgb scaled(Rgb colour, double factor);       // every channel times factor, kept in 0..255

// The colour of the maze: the most frequent colour that is not black among the given rows of a picture of 0x00RRGGBB pixels (each channel counted in
// 16 steps). That is the blue the maze is drawn on, not the walls (they take up less of the picture). The Solid colour and Gradient backgrounds use it.
// Returns defaultMazeColour() if the rows are all black (the title screen before anything is drawn).
Rgb defaultMazeColour();
Rgb mazeColour(const std::uint32_t* pixels, int width, int firstRow, int rows);

// The picture rows cut into cols x gridRows blocks and averaged: three floats (0..255) per block, row by row. A tiny picture that, drawn large
// with smooth scaling, gives the blurred glow.
void blockAverages(const std::uint32_t* pixels, int width, int firstRow, int rows, int cols, int gridRows, std::vector<float>& out);

// What the game's memory says (the 128 bytes $80-$FF, `ram[0]` is $80): the score of player 0 or 1 (four decimal digits; a nibble above 9, the
// game's "blank" pattern, counts as 0), the lives of the player whose turn it is, which player that is, and whether a two-player game is on.
// Lives are 0 while the game is not in play ($82 not 0: title screen or game over).
int scoreOf(const std::uint8_t* ram, int player);
int fullScoreOf(const std::uint8_t* ram, int player);   // the six digits of the score ($CC/$CE/$D0 or $CD/$CF/$D1), 0 to 999999; a blank pattern counts as 0 (Stage I20)
int livesOf(const std::uint8_t* ram);
int playerToPlay(const std::uint8_t* ram);  // 0 or 1
bool twoPlayers(const std::uint8_t* ram);

// How many whole copies of a picture `size` long are needed to cover a bar of `bar` length (at least 0, at most 6).

}  // namespace background
