// The explanations of the menu rows that need more than a paragraph (Stages I7, I9.5, I10, I11 and I12), moved out of menu.cpp (Stage I12).
#include "menu_text.hpp"

#include "menu.hpp"

namespace menu {

// What each background does, in plain words (the Background row's explanation).
std::string backgroundText(background::Kind kind)
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


}  // namespace menu

