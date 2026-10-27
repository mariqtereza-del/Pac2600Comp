#include "objects_view.hpp"

#include "data/tables.hpp"

namespace game {

namespace {

// Where the picture puts a sprite comes from the drawing code: the column from its positioning code (spriteColumns), the rows from its schedule.

uint8_t colourOf(const GameState& st, uint8_t index) {
  const bool colourTv = (st.colourMask & 0x80) != 0;
  const std::size_t table = colourTv ? data::kColourTvAt : data::kColourBlackWhiteAt;
  return static_cast<uint8_t>((data::kColourTables[table + index] ^ st.attractShift) & st.colourMask & 0xFE);   // the picture holds the colour without bit 0
}

// The picture of one sprite as the drawing code reads it: the lines are fetched from the last one down, from the address in the pointer, and the first one
// is fetched on the first line of the drawing code whose counter is not below the sprite's y.
void fill(SpriteImage& s, const std::vector<SpriteFetch>& fetches, int column, uint8_t y, uint8_t lines, uint8_t pointerLo) {
  const int wanted = (lines & 0x80) ? 0 : lines + 1;
  std::size_t first = fetches.size();
  for (std::size_t i = 0; i < fetches.size(); ++i)
    if (!((fetches[i].x - y) & 0x80)) { first = i; break; }
  int count = 0;
  auto lineAt = [&](int n) { return first + static_cast<std::size_t>(n); };
  while (count < wanted && count < 8 && lineAt(count) < fetches.size() && !fetches[lineAt(count)].clear) ++count;
  s.lines = count;
  s.visible = count > 0;
  s.column = column;
  for (int i = 0; i < count; ++i) {
    s.rowStart[static_cast<std::size_t>(i)] = fetches[first + static_cast<std::size_t>(i)].row;
    s.bits[static_cast<std::size_t>(i)] = data::byAddress(static_cast<uint16_t>(0xFD00 + pointerLo + (lines - i)));
  }
  if (count > 0) {   // the last line shows until the next fetch of the drawing code, which writes 0
    const std::size_t after = first + static_cast<std::size_t>(count);
    s.rowStart[static_cast<std::size_t>(count)] = after < fetches.size() ? fetches[after].row : fetches.back().row + 2;
    s.row = s.rowStart[0];
  }
}

}  // namespace

ObjectsView viewObjects(const GameState& st, const SpriteSchedule& schedule, bool pacmanMirrored) {
  ObjectsView view;
  // Pac-Man: player 1 of the chip; his x is read from the object, the rest from the set-up of the frame start (and of the animation in an opening).
  SpriteImage& pacman = view[0];
  pacman.drawn = true;
  fill(pacman, schedule.player[1], spriteColumns().player[1][st.objX[0]], st.scratch[kSprite1Y], st.scratch[kSprite1Lines], st.scratch[kSprite1PtrLo]);
  pacman.colour = st.colour[kColourSprite1] & 0xFE;
  pacman.mirrored = pacmanMirrored;

  // The ghost of this frame is player 0: one ghost a frame, (frame counter and 3) + 1.
  const int drawnGhost = (st.frameCounter & 3) + 1;
  for (int ghost = 1; ghost <= 4; ++ghost) {
    SpriteImage& s = view[static_cast<std::size_t>(ghost)];
    if (ghost == drawnGhost) {
      s.drawn = true;
      fill(s, schedule.player[0], spriteColumns().player[0][st.spriteX], st.scratch[kSprite0Y], st.scratch[kSprite0Lines], st.scratch[kSprite0PtrLo]);
      s.colour = st.colour[kColourSprite0] & 0xFE;
      continue;
    }
    // A ghost that is not drawn this frame looks as it will when its turn comes: the same picture, at its own place (the frame start has set the ghost
    // picture for the frame; eyes have another one). Inside an opening its lines depend on its animation, which only runs on its turn: the last picture stays.
    if (st.objState[ghost] & kObjInOpening) { s.held = true; continue; }
    const bool eyes = (st.objState[ghost] & kObjEyes) != 0;
    const uint8_t counterAtStart = static_cast<uint8_t>(st.frameCounter - 1);   // the frame start sets the pictures before it counts the frame
    const uint8_t pointer = eyes ? 0xC6 : data::kGhostPictures[((counterAtStart >> 3) >> 1) & 3];
    fill(s, schedule.player[0], spriteColumns().player[0][st.objX[ghost]], static_cast<uint8_t>(st.objY[ghost] + 0x0B), 7, pointer);
    const uint8_t pill = st.player[0].pillFlags;
    const uint8_t index = (pill & kPillScared) ? ((pill & kPillScareFirstPart) ? 4 : 5) : static_cast<uint8_t>(ghost - 1);
    s.colour = colourOf(st, index);
  }
  return view;
}

}  // namespace game
