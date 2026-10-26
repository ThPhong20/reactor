#include "application/app.h"

#include "application/data_store/data_store.h"
#include "application/display_manager/display_manager.h"
#include "application/pump_manager/pump_manager.h"
#include "application/scheduler/scheduler.h"
#include "application/sensor_manager/sensor_manager.h"
#include "application/telemetry/telemetry.h"
#include "common/system_config.h"

namespace app {
namespace {
data_store::Store history;
bool display_ready = false;
} // namespace

void init()
{
    // Prepare PWM outputs at zero duty before normal application work. The saved
    // inlet calibration is not a startup command; the main loop does not start pumps.
    const bool pumps_ready = pump_manager::init();
    // Give both managers the same pin map so custom sensor/display settings
    // cannot silently take ownership of each other's GPIOs.
    const auto sensors = system_config::default_sensor_config();
    const auto display = system_config::default_display_config();
    const bool sensors_ready = sensor_manager::init(sensors, display);
    history.clear();
    display_ready = display_manager::init(display, sensors);
    if (display_ready) display_ready = display_manager::update({}, scheduler::uptime_ms());
    if (system_config::default_config().logging_enabled) {
        telemetry::report_startup();
        telemetry::report_pump_initialization(pumps_ready);
        telemetry::report_sensor_initialization(sensors_ready);
        telemetry::report_display_status(display_ready);
    }
    // TODO: Initialize safety and the state machine
    // once board wiring and operating requirements are defined.
}

[[noreturn]] void run()
{
    const auto config = system_config::default_config();
    while (true) {
        // Sampling continues even with logging disabled. Sensor errors are
        // reported per input and do not start pumps or enable electrode power.
        const auto readings = sensor_manager::sample();
        history.push(readings, scheduler::uptime_ms());
        sensor_data::Record latest;
        history.latest(latest);
        if (display_ready && !display_manager::update(latest, scheduler::uptime_ms())) {
            display_ready = false;
            if (config.logging_enabled) telemetry::report_display_status(false);
        }
        if (config.logging_enabled) {
            telemetry::report_sensors(latest.sensors);
        }
        // TODO: Dispatch safety and state-machine work.
        scheduler::wait_for_next_tick(config.sample_interval_ms);
    }
}

} // namespace app
