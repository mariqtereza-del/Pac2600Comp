#include "stats_driver.hpp"

namespace host {

namespace {
constexpr long long kSaveEveryMs = 10000;
}

StatsDriver::StatsDriver(const std::string& settingsFile, bool counting) : file_(settingsFile), counting_(counting), saved_(stats::loadTotals(settingsFile)), watch_(saved_) {}

void StatsDriver::update(long long nowMs)
{
  if(!counting_ || nowMs - lastSaveMs_ < kSaveEveryMs) return;
  lastSaveMs_ = nowMs;
  flush();
}

void StatsDriver::flush()
{
  if(!counting_) return;
  const stats::Totals now = watch_.snapshot();
  if(now != saved_ && stats::saveTotals(file_, now, saved_)) saved_ = now;
}

}  // namespace host
