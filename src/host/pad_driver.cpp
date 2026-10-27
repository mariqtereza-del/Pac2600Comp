#include "pad_driver.hpp"

#include "layout.hpp"
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

FeedbackDriver::FeedbackDriver(const std::string& settingsFile)
    : light_(feedback::lightFromSetting(settings::get(settingsFile, "light_bar"))), rumble_(feedback::rumbleFromSetting(settings::get(settingsFile, "rumble")))
{
}

void FeedbackDriver::update(Pads& pads, feedback::Hub& hub, const std::uint32_t* pixels, int rows)
{
  if(const unsigned events = hub.takeEvents(); events && rumble_ != feedback::Rumble::Off) {
    const feedback::Pulse p = feedback::pulseFor(events, rumble_);
    pads.rumble(p.low, p.high, p.ms);
  }
  if(light_ == feedback::Light::Off) {   // back to the pad's own colour once, then left alone
    if(lightSent_) { pads.setLight(feedback::defaultLightColour()); lightSent_ = false; }
    return;
  }
  lightSent_ = true;
  if(light_ == feedback::Light::Maze && turns_++ % 30 == 0 && pixels && rows >= layout::kGameBottom)   // the maze colour is looked for twice a second or so
    maze_ = background::mazeColour(pixels, Emulator::kWidth, layout::kGameTop, layout::kGameBottom - layout::kGameTop);
  pads.setLight(feedback::lightColour(light_, maze_, hub.lives()));
}

}  // namespace host
