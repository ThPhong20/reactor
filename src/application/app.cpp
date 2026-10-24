#include "application/app.h"

#include "application/scheduler/scheduler.h"
#include "application/telemetry/telemetry.h"
#include "common/system_config.h"

namespace app {

void init()
{
    if (system_config::default_config().logging_enabled) {
        telemetry::report_startup();
    }
    // TODO: Initialize safety, sensors, the state machine, and three pumps
    // once board wiring and operating requirements are defined.
}

[[noreturn]] void run()
{
    const auto config = system_config::default_config();
    while (true) {
        // TODO: Dispatch scheduled sensor, safety, state-machine, and telemetry work.
        scheduler::wait_for_next_tick(config.sample_interval_ms);
    }
}

} // namespace app
