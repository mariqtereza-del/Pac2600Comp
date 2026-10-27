// The menu shared by the window thread and the game thread (Stage I5, T-I5-1).
#include "menu_session.hpp"

namespace menu {

// Opens the menu with the choices where the game and the console switches are now (the game number is read from the game's memory, byte $84).
void Session::open(const controls::Input& switches)
{
  Choice now;
  now.game = gameOfSelect(static_cast<std::uint8_t>(gameSelect_.load()));
  now.leftA = switches.leftDifficultyA;
  now.rightA = switches.rightDifficultyA;
  now.blackWhite = switches.blackWhite;
  menu_.open(now);
  open_ = true;
}

// A key for the open menu. Start Game queues the Select presses the game needs to get from its game number to the chosen one.
Result Session::key(Key key)
{
  const Result result = menu_.press(key);
  if(result == Result::StartGame && allowStart_) pendingPresses_ = pressesNeeded(static_cast<std::uint8_t>(gameSelect_.load()), menu_.choice().game);
  open_ = menu_.isOpen();
  return result;
}

// The game's number byte ($84 is the fifth byte of the memory, which starts at $80).
void Session::publish(const std::uint8_t* ram) { gameSelect_ = ram[0x84 - 0x80]; }

void Session::apply(controls::Input& in)
{
  const int presses = pendingPresses_.exchange(-1);
  if(presses >= 0) starter_.begin(presses);
  starter_.apply(in.select, in.reset);
}

}  // namespace menu
