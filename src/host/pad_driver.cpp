#include "pad_driver.hpp"

namespace host {

controls::PadState PadDriver::update(Pads& pads, controls::Keyboard& keyboard, const PadMenuHooks& menu, long long nowMs)
{
  const controls::PadAsks asks = pads.asks(nowMs);
  if(menu.isOpen()) {
    static const menu::Key keys[] = {menu::Key::Up, menu::Key::Down, menu::Key::Left, menu::Key::Right, menu::Key::Enter, menu::Key::Escape};   // the order of controls::Nav
    for(const controls::Nav n : asks.nav)
      if(menu.isOpen()) menu.press(keys[static_cast<int>(n)]);
    for(int i = 0; asks.menu && i < 4 && menu.isOpen(); ++i) menu.press(menu::Key::Escape);   // back out of any page, then close
  } else if(asks.menu) {
    menu.open();
  } else {
    controls::flipSwitches(keyboard, asks);
  }
  controls::PadState now = pads.poll();
  if(menu.isOpen()) held_ = true; else if(held_ && !now.any()) held_ = false;
  return held_ ? controls::PadState{} : now;
}

}  // namespace host
