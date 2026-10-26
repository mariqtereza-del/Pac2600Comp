// Where the tables the game reads sit in the cartridge (Stage G4.1). The game reads the cartridge by the address it has in the original program
// ($F000-$FFFF, see cartridge.hpp); these are the start addresses of the tables, named for what the game uses them for (F4 listed the tables, the
// F-stage documents say what each one is for). The values are addresses, not data. `Tbl`/`Pf` names with an address are tables of the picture code
// whose use is described in docs/F18_KERNEL.md, kept under their address until G5 finds better words.
#pragma once
#include <cstdint>

namespace game::rom {

// Maze and wafers (F10, F11)
inline constexpr uint16_t kMazeWalls = 0xFE13;            // 80 bytes: bit 7 side wall, 6 up allowed, 5 down allowed, per cell
inline constexpr uint16_t kWaferBitMask = 0xFE6A;         // the bit of a wafer inside its byte, from the cell column
inline constexpr uint16_t kPillMask = 0xFE6C;             // the power pills' bits in the pill flags
inline constexpr uint16_t kWaferByteIndex = 0xFE79;       // which byte of the wafer map a row and half-row use

// Scoring and lives (F15)
inline constexpr uint16_t kScoreAmountLow = 0xFF06;       // low decimal byte of the amount added, per amount number
inline constexpr uint16_t kScoreAmountMid = 0xFF0C;       // middle decimal byte
inline constexpr uint16_t kLivesMarkerPf1 = 0xFF1B;       // the playfield pattern of the life markers, per number of lives
inline constexpr uint16_t kLivesMarkerPf2 = 0xFEEC;
inline constexpr uint16_t kDigitPointerLow = 0xFEF6;      // low byte of the picture pointer of each score digit shape

// Objects and ghosts (F13, F16)
inline constexpr uint16_t kAnimPointerStep = 0xFF0D;      // opening/pen animation: picture pointer steps
inline constexpr uint16_t kAnimWhichPointer = 0xFF19;     // which sprite pointer the animation sets
inline constexpr uint16_t kAnimNextY = 0xFF25;            // start y of the next animation phase
inline constexpr uint16_t kChaseTimerReload = 0xFE6F;     // the ghosts' chase timer reload, by level / 2

// Pictures chosen at the frame start (F18)
inline constexpr uint16_t kGhostPictures = 0xFDAB;        // four ghost pictures, by FRAME_COUNTER bits 4-5
inline constexpr uint16_t kPacmanPictures = 0xFDC2;       // four Pac-Man pictures, by bits 3-4
inline constexpr uint16_t kPacmanDyingPictures = 0xFDFA;  // the dying Pac-Man, by the event timer

// Colours (F9, F26)
inline constexpr uint16_t kColourTableTv = 0xFF2B;        // colour switch: colour
inline constexpr uint16_t kColourTableBlackWhite = 0xFF36;

// Sound (F19)
inline constexpr uint16_t kSoundBlockers = 0xFFE9;        // per effect: the sounds (bits of SOUND_ACTIVE) that outrank it
inline constexpr uint16_t kSoundBit = 0xFE6E;             // per effect: its bit in SOUND_ACTIVE
inline constexpr uint16_t kSoundLength = 0xFFED;          // per effect: its length in frames
inline constexpr uint16_t kStartTuneA = 0xFFF1;           // the start tune's four notes, first channel table
inline constexpr uint16_t kStartTuneB = 0xFFF5;           // and the second

// The picture code (F18, F23.2): the playfield patterns and the maze-row tables
inline constexpr uint16_t kPlayfieldFE0A = 0xFE0A, kPlayfieldFE0C = 0xFE0C, kPlayfieldFE0D = 0xFE0D, kPlayfieldFE0F = 0xFE0F;
inline constexpr uint16_t kPlayfieldFE11 = 0xFE11, kPlayfieldFF41 = 0xFF41;
inline constexpr uint16_t kMazeRowFD3F = 0xFD3F, kMazeRowFD47 = 0xFD47, kMazeRowFD57 = 0xFD57, kMazeRowFD5B = 0xFD5B;
inline constexpr uint16_t kMazeRowFD63 = 0xFD63, kMazeRowFD6B = 0xFD6B, kMazeRowFD7B = 0xFD7B, kMazeRowFD83 = 0xFD83;

}  // namespace game::rom
