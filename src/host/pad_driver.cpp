#include "pad_driver.hpp"

#include "settings.hpp"

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

FeedbackDriver::FeedbackDriver(const std::string& settingsFile) : rumble_(feedback::rumbleFromSetting(settings::get(settingsFile, "rumble"))) {}

void FeedbackDriver::update(Pads& pads, feedback::Hub& hub)
{
  if(const unsigned events = hub.takeEvents(); events && rumble_ != feedback::Rumble::Off) {
    const feedback::Pulse p = feedback::pulseFor(events, rumble_);
    pads.rumble(p.low, p.high, p.ms);
  }
  pads.setLight(hub.light());   // sent only when it changed, and only to a PlayStation pad
}

}  // namespace host
