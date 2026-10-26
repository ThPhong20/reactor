#include "hal/timers_watchdog/timers_watchdog.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_task_wdt.h"

namespace hal::timers_watchdog {

std::uint64_t uptime_ms()
{
    return static_cast<std::uint64_t>(esp_timer_get_time() / 1000);
}

void delay_ms(int interval_ms)
{
    const auto ticks = pdMS_TO_TICKS(interval_ms > 0 ? interval_ms : 1);
    vTaskDelay(ticks > 0 ? ticks : 1);
}

} // namespace hal::timers_watchdog
