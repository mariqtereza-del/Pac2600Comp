// The test-only key pusher (see key_pusher.cpp).
#pragma once
#include <atomic>
#include <thread>

#include "common.hpp"
#include "options.hpp"

namespace host {

// Starts the helper thread of --push-keys or --push-menu; returns an empty thread when neither was asked for. `options` and `stop` must outlive the thread.
std::thread startKeyPusher(const Options& options, const std::atomic<bool>& stop);

}  // namespace host
