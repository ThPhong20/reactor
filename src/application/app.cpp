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
state_machine::Machine reactor({pump_manager::start_all, pump_manager::stop_all, nullptr});

// Collect the latest sample and hardware readiness for the FSM.
state_machine::Inputs control_inputs()
{
    state_machine::Inputs inputs;
    history.latest(inputs.sample);
    inputs.now_ms = scheduler::uptime_ms();
    inputs.pumps_ready = pump_manager::is_ready();
    inputs.duties_ready = pump_manager::operating_duties_ready();
    // No verified combined flow calibration, interlock inputs, or PSU backend
    // exists yet. Keep these guards false; do not simulate readiness on hardware.
    return inputs;
}

// Log the current FSM state and its reason.
void report_state()
{
    telemetry::report_state(state_machine::name(reactor.state()),
                            state_machine::description(reactor.reason()));
}
} // namespace

// Return the current reactor state.
state_machine::State reactor_state()
{
    return reactor.state();
}

// Handle one FSM event and report the result on serial.
bool command(state_machine::Event event)
{
    const bool accepted = reactor.handle(event, control_inputs());
    if (system_config::default_config().logging_enabled) report_state();
    return accepted;
}

// Initialize monitoring and the FSM with pump outputs off.
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
    reactor.initialize(pumps_ready);
    reactor.handle(state_machine::Event::Tick, control_inputs());
    display_ready = display_manager::init(display, sensors);
    if (display_ready) display_ready = display_manager::update({}, scheduler::uptime_ms());
    if (system_config::default_config().logging_enabled) {
        telemetry::report_startup();
        telemetry::report_pump_initialization(pumps_ready);
        telemetry::report_sensor_initialization(sensors_ready);
        telemetry::report_display_status(display_ready);
        report_state();
    }
    // Hardware interlocks and electrolysis power control remain unimplemented.
}

// Continuously sample, store, evaluate the FSM, and update outputs.
[[noreturn]] void run()
{
    const auto config = system_config::default_config();
    while (true) {
        // Sampling continues even with logging disabled. Sensor errors are
        // reported per input and do not start pumps or enable electrode power.
        // Use the start time: a slow later sensor must not make earlier data
        // look newly acquired when the batch finally completes.
        const auto sampled_at_ms = scheduler::uptime_ms();
        const auto readings = sensor_manager::sample();
        history.push(readings, sampled_at_ms);
        sensor_data::Record latest;
        history.latest(latest);
        const auto previous_state = reactor.state();
        const auto previous_reason = reactor.reason();
        if (reactor.state() == state_machine::State::Init) {
            reactor.initialize(pump_manager::is_ready());
        }
        reactor.handle(state_machine::Event::Tick, control_inputs());
        if (config.logging_enabled && (previous_state != reactor.state() || previous_reason != reactor.reason())) {
            report_state();
        }
        if (display_ready && !display_manager::update(latest, scheduler::uptime_ms(),
                state_machine::name(reactor.state()), state_machine::description(reactor.reason()))) {
            display_ready = false;
            if (config.logging_enabled) telemetry::report_display_status(false);
        }
        if (config.logging_enabled) {
            telemetry::report_sensors(latest.sensors);
        }
        // FSM checks are still in the monitoring task, not a fast safety task.
        scheduler::wait_for_next_tick(config.sample_interval_ms);
    }
}

} // namespace app
