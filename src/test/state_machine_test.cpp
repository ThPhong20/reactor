#include "application/state_machine/state_machine.h"

#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace {
using namespace app::state_machine;
bool pumps_on = false;
bool power_on = false;
bool fail_start = false;
bool fail_power_on = false;
bool fail_power_off = false;
bool fail_stop = false;
std::vector<std::string> actions;

// Fail the test immediately when a required condition is false.
void require(bool condition, const char* message)
{
    if (!condition) { std::cerr << message << '\n'; std::exit(1); }
}
// Simulate pump startup, including a possible partial failure.
bool start()
{
    actions.emplace_back("pumps on");
    pumps_on = true; // Model a partial change even when the callback fails.
    return !fail_start;
}
// Simulate pump shutdown and its configured failure.
bool stop()
{
    actions.emplace_back("pumps off");
    if (!fail_stop) pumps_on = false;
    return !fail_stop;
}
// Simulate electrode power changes and injected failures.
bool power(bool on)
{
    actions.emplace_back(on ? "power on" : "power off");
    if (!on && fail_power_off) return false;
    power_on = on;
    return !on || !fail_power_on;
}
// Create fresh calibrated sensor data and satisfied readiness guards.
Inputs healthy()
{
    Inputs in;
    in.sample.sequence = 1;
    in.sample.sampled_at_ms = in.now_ms = 1000;
    in.pumps_ready = in.duties_ready = in.flows_verified = true;
    in.interlocks_available = in.interlocks_ok = true;
    for (auto* reading : {&in.sample.sensors.ph, &in.sample.sensors.ec, &in.sample.sensors.turbidity}) {
        reading->status = sensor_data::Status::Ok;
        reading->value = 1;
    }
    return in;
}
// Verify that both simulated outputs are off.
void off()
{
    require(!power_on && !pumps_on, "outputs must be off");
}
// Verify power-off is requested before pump shutdown.
void shutdown_order()
{
    require(actions.size() >= 2 && actions[actions.size()-2] == "power off" &&
            actions.back() == "pumps off", "disable power before stopping pumps");
}
} // namespace

// Run the requested test scenario and return its result.
int main(int argc, char** argv)
{
    require(argc == 2, "provide scenario");
    const std::string scenario = argv[1];
    Machine machine({start, stop, power});
    auto in = healthy();
    if (scenario == "missing_outputs") {
        Machine no_stop({start, nullptr, power});
        require(!no_stop.initialize(true) && no_stop.reason() == Reason::ShutdownFailed,
                "missing stop callback must prevent initialization");
        Machine no_start({nullptr, stop, power});
        require(no_start.initialize(true), "shutdown is possible without a start callback");
        require(!no_start.handle(Event::Start, in) && no_start.reason() == Reason::PumpUnavailable,
                "missing start callback must block operation");
        off(); return 0;
    }
    require(!machine.handle(Event::Start, in), "START cannot bypass INIT");
    require(machine.handle(Event::Stop, in) && machine.state() == State::Init,
            "STOP before init must not bypass initialization");
    if (scenario == "init_failure") {
        require(!machine.initialize(false) && machine.state() == State::Fault, "init failure latches fault");
        require(!machine.handle(Event::Start, in), "cannot start from fault");
        off();
        return 0;
    }
    require(machine.initialize(true) && machine.state() == State::Idle, "initialize to IDLE");
    off();
    shutdown_order();
    if (scenario == "stop_blocked") {
        in.flows_verified = false;
        require(machine.handle(Event::Tick, in) && machine.reason() == Reason::FlowCalibrationMissing,
                "show missing flow verification before STOP");
        require(machine.handle(Event::Stop, in) && machine.state() == State::Idle,
                "STOP remains available with incomplete readiness");
        require(machine.reason() == Reason::FlowCalibrationMissing,
                "STOP must not report READY while startup is blocked");
        shutdown_order(); off(); return 0;
    }
    if (scenario == "guards") {
        const auto reject = [&](const Inputs& bad, Reason reason) {
            actions.clear();
            require(machine.start_blocker(bad) == reason, "wrong blocked-start reason");
            require(!machine.handle(Event::Start, bad) && machine.state() == State::Idle, "reject unsafe start");
            require(actions.empty(), "rejected start must not touch outputs");
        };
        auto bad = in; bad.pumps_ready = false; reject(bad, Reason::PumpUnavailable);
        bad = in; bad.duties_ready = false; reject(bad, Reason::PumpDutiesMissing);
        bad = in; bad.flows_verified = false; reject(bad, Reason::FlowCalibrationMissing);
        bad = in; bad.sample.sequence = 0; reject(bad, Reason::StaleSensors);
        bad = in; bad.now_ms += 3001; reject(bad, Reason::StaleSensors);
        auto boundary = in; boundary.now_ms += Machine::max_sample_age_ms;
        require(machine.start_blocker(boundary) == Reason::None, "exact age limit remains valid");
        bad = in; bad.now_ms = 999; reject(bad, Reason::StaleSensors);
        bad = in; bad.sample.sensors.turbidity = {}; reject(bad, Reason::SensorUnavailable);
        for (auto member : {&sensor_data::Snapshot::ph, &sensor_data::Snapshot::ec,
                            &sensor_data::Snapshot::turbidity}) {
            for (float value : {std::numeric_limits<float>::quiet_NaN(),
                                std::numeric_limits<float>::infinity(), -1.0f}) {
                bad = in;
                (bad.sample.sensors.*member).value = value;
                reject(bad, Reason::SensorUnavailable);
            }
        }
        bad = in; bad.sample.sensors.ec.status = sensor_data::Status::NeedsCalibration;
        reject(bad, Reason::SensorUncalibrated);
        bad = in; bad.interlocks_available = false; reject(bad, Reason::InterlocksUnavailable);
        bad = in; bad.interlocks_ok = false; reject(bad, Reason::InterlockOpen);
        Machine missing_power({start, stop, nullptr});
        require(missing_power.initialize(true), "monitoring allowed without power backend");
        actions.clear();
        require(!missing_power.handle(Event::Start, in) && missing_power.reason() == Reason::PowerUnavailable,
                "no power backend means no priming or running");
        require(actions.empty(), "no pump start without power backend");
        return 0;
    }
    if (scenario == "start_power_off_failure") {
        fail_power_off = true;
        actions.clear();
        require(!machine.handle(Event::Start, in) && machine.state() == State::Fault &&
                machine.reason() == Reason::ShutdownFailed, "unconfirmed power-off must block priming");
        for (const auto& action : actions) require(action != "pumps on", "do not start pumps after power-off failure");
        shutdown_order(); off(); return 0;
    }
    if (scenario == "pump_failure") fail_start = true;
    const bool started = machine.handle(Event::Start, in);
    if (scenario == "pump_failure") {
        require(!started && machine.state() == State::Fault && machine.reason() == Reason::PumpStartFailed,
                "pump failure must latch and shut down");
        shutdown_order(); off(); return 0;
    }
    require(started && machine.state() == State::Prime && pumps_on && !power_on, "prime with power disabled");
    if (scenario == "clock_rollback") {
        --in.now_ms;
        in.sample.sampled_at_ms = in.now_ms;
        require(!machine.handle(Event::FlowConfirmed, in) && machine.state() == State::Fault &&
                machine.reason() == Reason::PrimeTimeout, "clock rollback must not bypass priming timeout");
        shutdown_order(); off(); return 0;
    }
    if (scenario == "prime_sensor_failure") {
        in.sample.sensors.turbidity = {};
        require(!machine.handle(Event::FlowConfirmed, in) && machine.state() == State::Fault,
                "missing required turbidity during prime must prevent power-on");
        shutdown_order(); off(); return 0;
    }
    if (scenario == "stop_from_prime") {
        require(machine.handle(Event::Stop, in) && machine.state() == State::Idle, "STOP during prime");
        shutdown_order(); off(); return 0;
    }
    if (scenario == "timeout") {
        in.now_ms += Machine::prime_timeout_ms;
        in.sample.sampled_at_ms = in.now_ms; // Fresh sensors, but flow not confirmed in time.
        require(!machine.handle(Event::FlowConfirmed, in) && machine.reason() == Reason::PrimeTimeout,
                "late confirmation must not enable power");
        require(machine.state() == State::Fault, "timeout latches fault");
        shutdown_order(); off(); return 0;
    }
    if (scenario == "power_failure") fail_power_on = true;
    const bool running = machine.handle(Event::FlowConfirmed, in);
    if (scenario == "power_failure") {
        require(!running && machine.state() == State::Fault && machine.reason() == Reason::PowerStartFailed,
                "partial power-on failure shuts down and latches");
        shutdown_order(); off(); return 0;
    }
    require(running && machine.state() == State::Running && power_on, "confirmed flow starts running");
    require(!machine.handle(Event::Start, in), "duplicate START ignored");
    if (scenario == "lost_pump_running") {
        in.pumps_ready = false;
        require(!machine.handle(Event::Tick, in) && machine.state() == State::Fault &&
                machine.reason() == Reason::PumpUnavailable, "pump readiness loss must shut down power");
        shutdown_order(); off(); return 0;
    }
    if (scenario == "invalid_running") {
        in.sample.sensors.ec.value = std::numeric_limits<float>::quiet_NaN();
        require(!machine.handle(Event::Tick, in) && machine.state() == State::Fault &&
                machine.reason() == Reason::SensorUnavailable,
                "invalid numeric value must shut down despite an OK sensor status");
        shutdown_order(); off(); return 0;
    }
    if (scenario == "stale_running" || scenario == "interlock_running") {
        if (scenario == "stale_running") in.now_ms += 3001;
        else in.interlocks_ok = false;
        require(!machine.handle(Event::Tick, in) && machine.state() == State::Fault,
                "lost running readiness shuts down");
        require(machine.reason() == (scenario == "stale_running" ? Reason::StaleSensors : Reason::InterlockOpen),
                "preserve reason for running fault");
        shutdown_order(); off(); return 0;
    }
    if (scenario == "shutdown_failure") {
        fail_power_off = true;
        require(!machine.handle(Event::Stop, in) && machine.reason() == Reason::ShutdownFailed,
                "failed power-off is not success");
        require(!pumps_on, "pump stop attempted despite power-off error");
        shutdown_order();
        fail_power_off = false;
        fail_stop = true;
        require(!machine.handle(Event::Stop, in), "failed pump stop is not success");
        fail_stop = false;
        require(machine.handle(Event::Stop, in) && machine.state() == State::Fault, "retry stop preserves fault latch");
        off(); return 0;
    }
    if (scenario == "sensor_failure" || scenario == "reset") {
        in.sample.sensors.ec.status = sensor_data::Status::AdcError;
        require(!machine.handle(Event::Tick, in) && machine.state() == State::Fault, "required EC failure shuts down");
        shutdown_order(); off();
        require(!machine.handle(Event::Reset, in), "fault cause must clear before reset");
        in = healthy();
        require(!machine.handle(Event::Tick, in) && machine.state() == State::Fault, "recovery must not restart");
        if (scenario == "reset") {
            require(machine.handle(Event::Reset, in) && machine.state() == State::Init, "explicit reset to INIT");
            require(!machine.handle(Event::Start, in), "reset does not bypass INIT");
            require(machine.initialize(true) && machine.state() == State::Idle, "reinitialize after reset");
            off();
        }
        return 0;
    }
    if (scenario == "external_fault") {
        in.external_fault = true;
        require(machine.handle(Event::Tick, in) && machine.state() == State::Fault, "external fault takes priority");
        require(!machine.handle(Event::Reset, in), "cannot reset active fault");
        shutdown_order(); off(); return 0;
    }
    require(scenario == "happy", "unknown scenario");
    require(machine.handle(Event::Stop, in) && machine.state() == State::Idle, "normal STOP returns IDLE");
    shutdown_order(); off();
    require(!machine.handle(Event::FlowConfirmed, in), "cannot skip PRIME");
    return 0;
}
