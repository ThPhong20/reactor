#pragma once

// ESP-IDF timers, task delays, and watchdog adapter.
namespace hal::timers_watchdog {

void delay_ms(int interval_ms);

// TODO: Add monotonic timers and watchdog lifecycle management.

} // namespace hal::timers_watchdog
