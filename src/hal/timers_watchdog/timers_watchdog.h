#pragma once
#include <cstdint>

// ESP-IDF timers, task delays, and watchdog adapter.
namespace hal::timers_watchdog {

// Yield the current FreeRTOS task for at least one tick.
void delay_ms(int interval_ms);
// Read monotonic milliseconds from the ESP-IDF timer.
std::uint64_t uptime_ms();

// TODO: Add watchdog lifecycle management.

} // namespace hal::timers_watchdog
