#pragma once
#include <cstdint>

// Timer/scheduler: periodic application work through the timers/watchdog HAL.
namespace app::scheduler {

void wait_for_next_tick(int interval_ms);
std::uint64_t uptime_ms();

} // namespace app::scheduler
