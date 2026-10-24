#include "application/app.h"

#include "application/pump_manager/pump_manager.h"
#include "application/scheduler/scheduler.h"
#include "application/telemetry/telemetry.h"
#include "common/system_config.h"

namespace app {

void init()
{
    // Prepare PWM outputs at zero duty before normal application work. The saved
    // inlet calibration is not a startup command; the main loop does not start pumps.
    const bool pumps_ready = pump_manager::init();
    if (system_config::default_config().logging_enabled) {
        telemetry::report_startup();
        telemetry::report_pump_initialization(pumps_ready);
    }
    // TODO: Initialize safety, sensors, and the state machine
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
