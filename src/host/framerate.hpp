// The frame rate of the picture (Stage I9.4, DECISIONS.md D-015 and T-I9-4): how often the window shows a new picture. The game itself always runs at the
// console's own speed; this only decides how smooth the motion on the screen is. No window code, so it can be tested on its own.
#pragma once
#include <string>

namespace framerate {

enum class Rate { Fps30, Fps60, Fps75, Fps120, Fps144, Unlocked };

inline constexpr int kRates = 6;

// 30 shows every second picture of the game (half the original's rate); 60 shows the game's own pictures, one for each step (the original's look, the
// default); the rest draw the moving objects between their places (game::Smoother) at that many pictures a second; Unlocked as often as the window can
// (and the screen's own wait is switched off).
const char* settingName(Rate rate);     // "30", "60", "75", "120", "144", "unlocked": the value in settings.ini and on the command line
const char* title(Rate rate);           // as shown to the player
Rate fromSetting(const std::string& text);   // anything unknown is 60
Rate next(Rate rate, int step = 1);     // round the list, either way
bool smooth(Rate rate);                 // the moving objects are drawn between their places
bool unlocked(Rate rate);
int perSecond(Rate rate);               // 0 for Unlocked

// The moments at which pictures are made: at a fixed rate a picture is made at every 1/rate second of the clock (not at each game step, so the pictures
// are evenly spaced) and stays until the next one; Unlocked makes one whenever the window is drawn.
// pictureIndex is the number of the picture the time `seconds` belongs to (-1 for Unlocked, which has no grid) and pictureTime the moment it was made.
long long pictureIndex(Rate rate, double seconds);
double pictureTime(Rate rate, double seconds);

// How far through a game step that began at `stepStart` and lasts `period` seconds a picture made at `at` is, 0 up to just below 1 (a picture made before
// the step began is at its start, one made long after it at its end).
double progress(double at, double stepStart, double period);

// What the picture on screen is for this step number: 30 shows every second step, the others each one.
long long shownStep(Rate rate, long long step);

}  // namespace framerate
