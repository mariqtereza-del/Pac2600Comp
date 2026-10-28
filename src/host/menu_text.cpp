// The explanations of the menu rows that need more than a paragraph (Stages I7, I9.5, I10, I11 and I12), moved out of menu.cpp (Stage I12).
#include "menu_text.hpp"

#include "menu.hpp"
#include "pad_layout.hpp"

#include <algorithm>

namespace menu {

// What each background does, in plain words (the Background row's explanation).
std::string backgroundText(background::Kind kind)
{
  switch(kind) {
    case background::Kind::Black: return "Black: nothing is drawn, as on the original TV. This is the usual setting.";
    case background::Kind::Glow: return "Ambient glow: the picture's own colours, enlarged and blurred, glow behind it and follow the action, like the backlight of a modern TV.";
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
void appendFrameRateText(std::vector<std::string>& out, int width, bool smoothAvailable)
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

// The explanation of the CRT filter row. The filters are the real shaders of the libretro collection, ported to HLSL (third_party\shaders, DECISIONS.md D-017).
void appendCrtText(std::vector<std::string>& out, int width, crt::Filter filter)
{
  auto paragraph = [&](const std::string& text) {
    for(const std::string& line : wrap(text, width)) out.push_back(line);
  };
  paragraph("Makes the picture look as it did on a cathode-ray tube television or monitor: scanlines, the glow of the phosphor, a screen mask, a little curvature. "
            "Off is the program's own picture, as it always was.");
  out.push_back("");
  switch(filter) {
    case crt::Filter::Off: paragraph("Off: no filter."); break;
    case crt::Filter::Lottes:
      paragraph("CRT-Lottes: the shader of Timothy Lottes (public domain), in the style of a good arcade monitor: crisp scanlines, a visible shadow mask, a soft glow around "
                "bright parts and slightly curved glass.");
      break;
    case crt::Filter::Geom:
      paragraph("CRT-Geom: the classic curved-screen shader of cgwg, Themaister and DOLLS (GPL): strongly rounded glass with shaded corners, scanlines whose width follows the "
                "brightness of the picture, smooth horizontal blending between pixels and a fine vertical-stripe screen mask.");
      break;
    case crt::Filter::GuestNtsc: paragraph("GDV NTSC 2600: the crt-guest-advanced shader (guest.r and Dr. Venom) with its NTSC colour."); break;
  }
  out.push_back("");
  paragraph("While a filter is on, the picture is the game's own at 60 steps a second: the smooth glide between steps (Frame rate) and the HDR highlights are not shown "
            "inside a filter yet. The game itself is never changed.");
}

// The explanation of the two HDR rows. The original game has no HDR: the picture stays exactly as it was, at normal white, and only Pac-Man, the ghosts and the
// power pills are drawn brighter, on a screen that is in HDR mode.
void appendHdrText(std::vector<std::string>& out, int width, bool windowsHdr, bool slider)
{
  auto paragraph = [&](const std::string& text) {
    for(const std::string& line : wrap(text, width)) out.push_back(line);
  };
  if(slider)
    paragraph("How bright the highlights are: 0 draws no highlight at all (the plain picture), 100 takes them as bright as the screen can show, up to four times "
              "brighter than white. Left and Right change it by 5. The maze, the score and the menu always stay at normal white.");
  else
    paragraph("On a screen that Windows has in HDR mode, the picture goes out through the HDR pipeline: the same picture at normal white, with Pac-Man, the ghosts and "
              "the power pills drawn brighter than white (see HDR highlights). The 1981 game has no HDR; this is only an addition. Off (or Windows not in HDR "
              "mode) gives the plain picture. The game, its speed and its sound are never changed.");
  out.push_back("");
  paragraph(windowsHdr ? "Windows is in HDR mode for this screen, so these choices show now."
                       : "Windows is NOT in HDR mode for this screen, so these rows change nothing yet. Turn HDR on in Windows (Settings, System, Display, HDR); "
                         "this program does not change that setting. It then switches itself on while the program runs.");
}

// The explanation of the rows of the Sound options page (Stage I12). Only the loudness of the played sound changes, never the sound the game makes.
void appendSoundText(std::vector<std::string>& out, int width, Item item)
{
  const char* text = "How loud the game is, and a mute button. Enter opens the page.";
  if(item == Item::Volume)
    text = "How loud the game's sound is played, from 0 to 100. 100 is the sound exactly as the game makes it; lower values only make it quieter, so the pitch, "
           "the pace and the game itself never change. Left and Right change it by 5. It takes effect at once and is remembered.";
  else if(item == Item::Mute)
    text = "On plays no sound at all. The volume keeps its place, so turning Mute off brings the sound back at the same loudness. The game keeps running at "
           "its usual speed. It takes effect at once and is remembered.";
  for(const std::string& line : wrap(text, width)) out.push_back(line);
}


// The explanation of the Control options rows (Stage I13, D-019): what the Keyboard choice does and, always, every key of the game window as it is now.
void appendControlsText(std::vector<std::string>& out, int width, controls::Scheme scheme, bool keysRow, controls::PadFamily pad, bool padConnected)
{
  auto paragraph = [&](const std::string& text) {
    for(const std::string& line : wrap(text, width)) out.push_back(line);
  };
  if(keysRow)
    paragraph("Which keys move the joystick: the arrow keys and W A S D together (the usual setting), only the arrow keys, or only W A S D. "
              "Left and Right change it. It takes effect at once and is remembered. The other keys below work whatever you choose.");
  else
    paragraph("The keys that play the game. Enter opens the page.");
  out.push_back("");
  out.push_back(std::string("Keys now (") + controls::schemeTitle(scheme) + "):");
  const int keyColumn = 17;
  for(const controls::Binding& b : controls::bindings()) {
    const std::string keys = controls::keysText(b, scheme);
    const std::vector<std::string> meaning = wrap(b.meaning, std::max(10, width - keyColumn - 2));
    for(std::size_t i = 0; i < meaning.size(); ++i) {
      const std::string label = i == 0 ? keys : std::string();
      out.push_back("  " + label + std::string(static_cast<std::size_t>(std::max(0, keyColumn - static_cast<int>(label.size()))), ' ') + meaning[i]);
    }
  }
  out.push_back("");
  out.push_back(padConnected ? std::string("Game pad - connected, ") + controls::familyTitle(pad) + ":" : "Game pad - none connected, a DualShock 4 shown:");
  for(const controls::HelpLine& b : controls::padLines(pad)) {
    const std::vector<std::string> meaning = wrap(b.meaning, std::max(10, width - 24 - 2));
    for(std::size_t i = 0; i < meaning.size(); ++i) {
      const std::string label = i == 0 ? b.keys : std::string();
      out.push_back(label + std::string(static_cast<std::size_t>(std::max(0, 24 - static_cast<int>(label.size()))), ' ') + meaning[i]);
    }
  }
  out.push_back("");
  paragraph("A pad and the keyboard work at the same time. F1 shows these lists in the game; Esc (or the pad's menu button) opens this menu.");
}


// The explanation of the Rumble row (Stage I15, D-021), which also tells how the light bar behaves. Both only send things out to the pad; the game is never changed.
void appendFeedbackText(std::vector<std::string>& out, int width, bool padConnected)
{
  auto paragraph = [&](const std::string& text) {
    for(const std::string& line : wrap(text, width)) out.push_back(line);
  };
  paragraph("Makes the pad rumble when something happens in the game: a tiny tap for each wafer, a short tick for a power pill, a thump for a ghost eaten and a long rumble when Pac-Man is "
            "caught. Light is gentle, Strong is firm. It takes effect at once and is remembered.");
  out.push_back("");
  paragraph("The light bar of a DualShock 4 or a DualSense is always on and follows the game: yellow, blue while the ghosts are scared (flashing white when they are about to "
            "recover), a white flash when a ghost is eaten and red when Pac-Man is caught.");
  out.push_back("");
  paragraph(padConnected ? "A pad is connected. A pad without motors simply ignores the rumble."
                         : "No pad is connected now: this works as soon as one is plugged in.");
}

// The explanation of an achievement row (index 0 to 13: what it asks and whether it is done), or of the row on the main page (-1) (Stage I20, D-025).
void appendAchievementText(std::vector<std::string>& out, int width, int index, const achievements::Progress& progress)
{
  auto paragraph = [&](const std::string& text) {
    for(const std::string& line : wrap(text, width)) out.push_back(line);
  };
  if(index < 0) {
    paragraph(std::string("The ") + std::to_string(achievements::kCount) + " achievements of the game, and which of them you have unlocked (" + std::to_string(progress.count()) +
              " so far). Each one unlocks while you play, and a notice appears in the window. Enter opens the page.");
  } else {
    const achievements::Definition& d = achievements::definition(index);
    paragraph(std::string(d.title) + ": " + d.description + ".");
    out.push_back("");
    paragraph(progress.has(index) ? "Unlocked on " + progress.dates[static_cast<std::size_t>(index)] + "." : "Not unlocked yet.");
  }
  out.push_back("");
  paragraph("These are the achievements of the RetroAchievements set for Pac-Man on the Atari 2600 (retroachievements.org/game/13094). This program works them out on this "
            "computer from the game's memory; it does not connect to RetroAchievements, and it never changes the game.");
}

// The explanation of a row of the Statistics page (index 0 to 8: what is counted and the value), or of the row on the main page (-1) (Stage I21, D-026).
void appendStatsText(std::vector<std::string>& out, int width, int index, const stats::Totals& totals)
{
  auto paragraph = [&](const std::string& text) {
    for(const std::string& line : wrap(text, width)) out.push_back(line);
  };
  if(index < 0) {
    paragraph(std::string("Your totals: ") + stats::groupedText(totals.games) + " games, " + stats::timeText(totals.playFrames) + " played, a high score of " +
              stats::groupedText(totals.highScore) + ". Enter opens the page.");
  } else {
    paragraph(stats::definition(index).explanation);
  }
  out.push_back("");
  paragraph("The totals are counted on this computer while you play, and kept between runs in settings.ini. A recording that is replayed is not counted. Nothing is sent "
            "anywhere, and the game itself is never changed.");
}

// The explanation of the Players and the two difficulty rows (moved out of menu.cpp in Stage I13).
void appendPlayersText(std::vector<std::string>& out, int width, Item item)
{
  auto paragraph = [&](const std::string& text) {
    for(const std::string& line : wrap(text, width)) out.push_back(line);
  };
  switch(item) {
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
    default: break;
  }
}

}  // namespace menu

