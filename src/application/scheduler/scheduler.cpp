#include "application/scheduler/scheduler.h"

#include "hal/timers_watchdog/timers_watchdog.h"

namespace app::scheduler {

std::uint64_t uptime_ms()
{
    return hal::timers_watchdog::uptime_ms();
}

void wait_for_next_tick(int interval_ms)
{
    hal::timers_watchdog::delay_ms(interval_ms);
}

} // namespace app::scheduler
