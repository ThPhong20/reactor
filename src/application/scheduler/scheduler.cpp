#include "application/scheduler/scheduler.h"

#include "hal/timers_watchdog/timers_watchdog.h"

namespace app::scheduler {

void wait_for_next_tick(int interval_ms)
{
    hal::timers_watchdog::delay_ms(interval_ms);
}

} // namespace app::scheduler
