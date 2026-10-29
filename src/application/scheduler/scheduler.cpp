#include "application/scheduler/scheduler.h"

#include "hal/timers_watchdog/timers_watchdog.h"

namespace app::scheduler {

// Return milliseconds since startup.
std::uint64_t uptime_ms()
{
    return hal::timers_watchdog::uptime_ms();
}

// Yield execution for the configured delay after application work.
void wait_for_next_tick(int interval_ms)
{
    hal::timers_watchdog::delay_ms(interval_ms);
}

} // namespace app::scheduler
