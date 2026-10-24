#pragma once

// Timer/scheduler: periodic application work through the timers/watchdog HAL.
namespace app::scheduler {

void wait_for_next_tick(int interval_ms);

} // namespace app::scheduler
