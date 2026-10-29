#pragma once
#include <cstdint>

// Timer/scheduler: periodic application work through the timers/watchdog HAL.
namespace app::scheduler {

// Yield execution for the configured delay after application work.
void wait_for_next_tick(int interval_ms);
// Return milliseconds since startup.
std::uint64_t uptime_ms();

} // namespace app::scheduler
