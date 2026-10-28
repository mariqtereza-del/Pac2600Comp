// The test-only key pusher (see key_pusher.cpp).
#pragma once
#include <atomic>
#include <thread>

#include "common.hpp"
#include "options.hpp"

namespace host {

// Starts the helper thread of --push-keys or --push-menu; returns an empty thread when neither was asked for. `options` and `stop` must outlive the thread.
std::thread startKeyPusher(const Options& options, const std::atomic<bool>& stop);

// What --push-menu, --push-pad and --push-keys add to the report at the end of a window run (menu opens, the game's final state, the joystick and switch values it saw).
void writePushReports(std::ostream& log, const Options& options, const Emulator& emulator, int menuOpens, long long menuFramesRun, std::uint32_t joystickValuesSeen,
                      const std::set<int>& switchValuesSeen, int rendererRemakes);

}  // namespace host
