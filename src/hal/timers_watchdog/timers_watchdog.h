#pragma once
#include <cstdint>

// ESP-IDF timers, task delays, and watchdog adapter.
namespace hal::timers_watchdog {

void delay_ms(int interval_ms);
std::uint64_t uptime_ms();

// TODO: Add watchdog lifecycle management.

} // namespace hal::timers_watchdog
